#pragma once

#include <drogon/HttpController.h>

class ServerManagementController
    : public drogon::HttpController<ServerManagementController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ServerManagementController::status,
                  "/api/server-management/status", drogon::Get,
                  "AuthNFilter", "AuthZFilter");
    ADD_METHOD_TO(ServerManagementController::license,
                  "/api/server-management/license", drogon::Get,
                  "AuthNFilter", "AuthZFilter");
    ADD_METHOD_TO(ServerManagementController::shutdown,
                  "/api/server-management/shutdown", drogon::Post,
                  "AuthNFilter", "AuthZFilter");
    METHOD_LIST_END

    drogon::Task<drogon::HttpResponsePtr>
    status(drogon::HttpRequestPtr request);

    drogon::Task<drogon::HttpResponsePtr>
    license(drogon::HttpRequestPtr request);

    void shutdown(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
