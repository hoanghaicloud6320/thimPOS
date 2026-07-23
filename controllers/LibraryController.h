#pragma once

#include <drogon/HttpController.h>

class LibraryController : public drogon::HttpController<LibraryController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LibraryController::list, "/api/library/files", drogon::Get, "AuthNFilter");
    ADD_METHOD_TO(LibraryController::upload, "/api/library/files", drogon::Post, "AuthNFilter");
    ADD_METHOD_TO(LibraryController::remove, "/api/library/files/{1}", drogon::Delete, "AuthNFilter");
    METHOD_LIST_END

    drogon::Task<drogon::HttpResponsePtr> list(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> upload(drogon::HttpRequestPtr req);
    drogon::Task<drogon::HttpResponsePtr> remove(drogon::HttpRequestPtr req,
                                                  std::string name);
};
