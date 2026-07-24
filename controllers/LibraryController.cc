#include "LibraryController.h"
#include "../AuditLog.h"
#include "../plugins/LibraryService.h"

#include <drogon/MultiPart.h>

#include <algorithm>
#include <cctype>

using namespace drogon;

namespace
{
Json::Value fileJson(const LibraryFile &file)
{
    Json::Value value;
    value["name"] = file.name;
    value["url"] = file.url;
    value["size"] = Json::UInt64(file.size);
    value["modified_at"] = Json::Int64(file.modifiedAt);
    return value;
}

HttpResponsePtr errorResponse(const std::string &message, HttpStatusCode status)
{
    Json::Value body;
    body["success"] = false;
    body["error"] = message;
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

std::string mimeTypeFor(const std::string &name)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    auto extension = name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension == ".png")
        return "image/png";
    if (extension == ".jpg" || extension == ".jpeg")
        return "image/jpeg";
    if (extension == ".gif")
        return "image/gif";
    if (extension == ".webp")
        return "image/webp";
    if (extension == ".svg")
        return "image/svg+xml";
    if (extension == ".pdf")
        return "application/pdf";
    if (extension == ".txt")
        return "text/plain; charset=utf-8";
    if (extension == ".csv")
        return "text/csv; charset=utf-8";
    if (extension == ".json")
        return "application/json";
    if (extension == ".zip")
        return "application/zip";
    return "application/octet-stream";
}
}  // namespace

Task<HttpResponsePtr> LibraryController::list(HttpRequestPtr)
{
    try
    {
        Json::Value files(Json::arrayValue);
        for (const auto &file : app().getPlugin<LibraryService>()->listFiles())
            files.append(fileJson(file));
        Json::Value body;
        body["success"] = true;
        body["files"] = files;
        co_return HttpResponse::newHttpJsonResponse(body);
    }
    catch (const std::exception &error)
    {
        co_return errorResponse(error.what(), k500InternalServerError);
    }
}

Task<HttpResponsePtr> LibraryController::upload(HttpRequestPtr req)
{
    MultiPartParser parser;
    if (parser.parse(req) != 0 || parser.getFiles().empty())
        co_return errorResponse("Expected multipart/form-data with one or more files",
                                k400BadRequest);

    try
    {
        Json::Value saved(Json::arrayValue);
        auto service = app().getPlugin<LibraryService>();
        for (const auto &file : parser.getFiles())
            saved.append(fileJson(service->saveFile(file)));

        Json::Value body;
        body["success"] = true;
        body["files"] = saved;
        auto response = HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(k201Created);
        writeAuditLog("user=" + req->attributes()->get<std::string>("username") +
                      " | UPLOAD LIBRARY FILE | count=" +
                      std::to_string(saved.size()));
        co_return response;
    }
    catch (const std::invalid_argument &error)
    {
        co_return errorResponse(error.what(), k400BadRequest);
    }
    catch (const std::exception &error)
    {
        co_return errorResponse(error.what(), k500InternalServerError);
    }
}

Task<HttpResponsePtr> LibraryController::remove(HttpRequestPtr req, std::string name)
{
    try
    {
        if (!app().getPlugin<LibraryService>()->removeFile(name))
            co_return errorResponse("File not found", k404NotFound);
        Json::Value body;
        body["success"] = true;
        writeAuditLog("user=" + req->attributes()->get<std::string>("username") +
                      " | DELETE LIBRARY FILE | file=" + name);
        co_return HttpResponse::newHttpJsonResponse(body);
    }
    catch (const std::invalid_argument &error)
    {
        co_return errorResponse(error.what(), k400BadRequest);
    }
    catch (const std::exception &error)
    {
        co_return errorResponse(error.what(), k500InternalServerError);
    }
}

Task<HttpResponsePtr> LibraryController::download(HttpRequestPtr, std::string name)
{
    try
    {
        auto content = app().getPlugin<LibraryService>()->readFile(name);
        if (!content)
            co_return HttpResponse::newNotFoundResponse();
        auto response = HttpResponse::newHttpResponse();
        response->setContentTypeString(mimeTypeFor(name));
        response->setBody(std::move(*content));
        co_return response;
    }
    catch (const std::invalid_argument &)
    {
        co_return HttpResponse::newNotFoundResponse();
    }
    catch (const std::exception &error)
    {
        co_return errorResponse(error.what(), k500InternalServerError);
    }
}
