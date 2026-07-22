#include "AiController.h"

#include "../plugins/AiService.h"

using namespace drogon;

namespace
{
HttpResponsePtr errorResponse(const std::string &code,
                              const std::string &message,
                              HttpStatusCode status)
{
    Json::Value body;
    body["success"] = false;
    body["error"]["code"] = code;
    body["error"]["message"] = message;
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}
}  // namespace

Task<HttpResponsePtr> AiController::generate(HttpRequestPtr request)
{
    const auto role = request->attributes()->get<std::string>("role");
    if (role != "manager")
        co_return errorResponse("FORBIDDEN", "Only managers may use AI features.",
                                k403Forbidden);

    const auto json = request->getJsonObject();
    if (!json || !json->isObject())
        co_return errorResponse("INVALID_JSON", "Request body must be a JSON object.",
                                k400BadRequest);
    if (!json->isMember("messages") || !(*json)["messages"].isArray() ||
        (*json)["messages"].empty())
        co_return errorResponse("AI_MESSAGES_REQUIRED",
                                "messages must be a non-empty array.", k400BadRequest);

    std::vector<AiMessage> messages;
    std::size_t totalCharacters = 0;
    for (const auto &item : (*json)["messages"])
    {
        if (!item.isObject() || !item["role"].isString() ||
            !item["content"].isString())
            co_return errorResponse("INVALID_AI_MESSAGE",
                                    "Each message requires string fields role and content.",
                                    k400BadRequest);
        const auto messageRole = item["role"].asString();
        if (messageRole != "system" && messageRole != "user" &&
            messageRole != "assistant" && messageRole != "model")
            co_return errorResponse("INVALID_AI_ROLE",
                                    "role must be system, user, assistant, or model.",
                                    k400BadRequest);
        const auto content = item["content"].asString();
        if (content.empty())
            co_return errorResponse("INVALID_AI_MESSAGE", "Message content must not be empty.",
                                    k400BadRequest);
        totalCharacters += content.size();
        messages.push_back({messageRole == "model" ? "assistant" : messageRole, content});
    }
    if (messages.size() > 100 || totalCharacters > 100000)
        co_return errorResponse("AI_REQUEST_TOO_LARGE",
                                "A request supports at most 100 messages and 100000 characters.",
                                k413RequestEntityTooLarge);

    const float temperature = json->get("temperature", 0.7).asFloat();
    if (temperature < 0.0F || temperature > 2.0F)
        co_return errorResponse("INVALID_TEMPERATURE",
                                "temperature must be between 0 and 2.", k400BadRequest);

    try
    {
        const auto reply = app().getPlugin<AiService>()->generate(messages, temperature);
        Json::Value body;
        body["success"] = true;
        body["data"]["model"] = reply.model;
        body["data"]["message"]["role"] = "assistant";
        body["data"]["message"]["content"] = reply.content;
        co_return HttpResponse::newHttpJsonResponse(body);
    }
    catch (const AiServiceError &error)
    {
        co_return errorResponse(error.code(), error.what(),
                                static_cast<HttpStatusCode>(error.httpStatus()));
    }
    catch (const std::exception &error)
    {
        co_return errorResponse("AI_INTERNAL_ERROR", error.what(),
                                k500InternalServerError);
    }
}
