#pragma once

#include <drogon/HttpController.h>

class AiController : public drogon::HttpController<AiController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AiController::generate, "/api/ai/generate", drogon::Post,
                  "AuthNFilter", "AuthZFilter");
    METHOD_LIST_END

    drogon::Task<drogon::HttpResponsePtr> generate(drogon::HttpRequestPtr request);
};
