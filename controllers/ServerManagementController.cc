#include "ServerManagementController.h"

#include "../plugins/ServerManagementService.h"

#include <chrono>

using namespace drogon;

namespace
{
HttpResponsePtr response(Json::Value body,
                         HttpStatusCode status = k200OK)
{
    auto result = HttpResponse::newHttpJsonResponse(body);
    result->setStatusCode(status);
    return result;
}

HttpResponsePtr forbidden()
{
    Json::Value body;
    body["success"] = false;
    body["error"]["code"] = "FORBIDDEN";
    body["error"]["message"] =
        "Only managers may manage the server.";
    return response(std::move(body), k403Forbidden);
}

bool isManager(const HttpRequestPtr &request)
{
    try
    {
        return request->attributes()->get<std::string>("role") == "manager";
    }
    catch (...)
    {
        return false;
    }
}
}  // namespace

Task<HttpResponsePtr>
ServerManagementController::status(HttpRequestPtr request)
{
    if (!isManager(request))
        co_return forbidden();

    const auto service = app().getPlugin<ServerManagementService>();
    Json::Value body;
    body["success"] = true;
    body["data"] = service->status();
    co_return response(std::move(body));
}

Task<HttpResponsePtr>
ServerManagementController::license(HttpRequestPtr request)
{
    if (!isManager(request))
        co_return forbidden();

    const auto service = app().getPlugin<ServerManagementService>();
    Json::Value body;
    body["success"] = true;
    body["data"] = service->licenseInfo();
    co_return response(std::move(body));
}

void ServerManagementController::shutdown(
    const HttpRequestPtr &request,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (!isManager(request))
    {
        callback(forbidden());
        return;
    }

    const auto json = request->getJsonObject();
    if (!json || !json->isObject() ||
        json->get("confirmation", "").asString() != "SHUTDOWN")
    {
        Json::Value body;
        body["success"] = false;
        body["error"]["code"] = "SHUTDOWN_CONFIRMATION_REQUIRED";
        body["error"]["message"] =
            "Request body must contain confirmation: SHUTDOWN.";
        callback(response(std::move(body), k400BadRequest));
        return;
    }

    const auto service = app().getPlugin<ServerManagementService>();
    if (!service->beginShutdown())
    {
        Json::Value body;
        body["success"] = false;
        body["error"]["code"] = "SHUTDOWN_IN_PROGRESS";
        body["error"]["message"] = "The server is already shutting down.";
        callback(response(std::move(body), k409Conflict));
        return;
    }

    std::string username = "unknown";
    try
    {
        username = request->attributes()->get<std::string>("username");
    }
    catch (...)
    {
    }
    LOG_WARN << "[ServerManagement] shutdown requested by manager "
             << username;

    Json::Value body;
    body["success"] = true;
    body["data"]["status"] = "shutting_down";
    body["data"]["message"] = "Server is shutting down.";
    callback(response(std::move(body), k202Accepted));

    // Leave enough time for Drogon to flush the HTTP response before stopping
    // its event loops and plugins.
    app().getLoop()->runAfter(0.5, [] {
        LOG_WARN << "[ServerManagement] stopping server";
        app().quit();
    });
}
