#include "AccountController.h"
#include "../AuditLog.h"

using namespace drogon;

// --- Helper Functions ---

Json::Value AccountController::toJson(const AccountEntity& acc) {
    Json::Value j;
    j["username"] = acc.username;
    j["role"] = acc.role;
    j["email"] = acc.email;
    j["phone_number"] = acc.phone_number;
    j["full_name"] = acc.full_name;
    j["avatar_url"] = acc.avatar_url;
    j["date_of_birth"] = acc.date_of_birth;
    j["created_at"] = acc.created_at; 
    return j;
}

AccountEntity AccountController::fromJson(const Json::Value& json) {
    AccountEntity acc;
    acc.username = json.get("username", "").asString();
    acc.role = json.get("role", "").asString();
    acc.email = json.get("email", "").asString();
    acc.phone_number = json.get("phone_number", "").asString();
    acc.full_name = json.get("full_name", "").asString();
    acc.avatar_url = json.get("avatar_url", "").asString();
    acc.date_of_birth = json.get("date_of_birth", "").asString();
    return acc;
}

HttpResponsePtr AccountController::makeErrorResponse(int statusCode, const std::string& message) {
    Json::Value root;
    root["status"] = "error";
    root["code"] = statusCode;
    root["message"] = message;
    auto res = HttpResponse::newHttpJsonResponse(root);
    res->setStatusCode((HttpStatusCode)statusCode);
    return res;
}

// --- API Handlers ---

// [GET] /api/accounts
Task<HttpResponsePtr> AccountController::getAllAccounts(HttpRequestPtr req) {
    // AuthZFilter đã gán role vào attribute. Ta chỉ việc check.
    auto role = req->getAttributes()->get<std::string>("role");
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto* service = app().getPlugin<AccountManagementService>();
    auto result = co_await service->getAllAccounts();

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    Json::Value dataArr(Json::arrayValue);
    for (const auto& acc : result.data) {
        dataArr.append(toJson(acc));
    }
    root["data"] = dataArr;
    root["total"] = (int)result.data.size();

    co_return HttpResponse::newHttpJsonResponse(root);
}

// [POST] /api/accounts
Task<HttpResponsePtr> AccountController::createAccount(HttpRequestPtr req) {
    auto role = req->getAttributes()->get<std::string>("role");
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) co_return makeErrorResponse(400, "Invalid JSON");

    auto* service = app().getPlugin<AccountManagementService>();
    auto result = co_await service->createAccount(fromJson(*jsonPtr));

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["message"] = "Account created successfully";
    root["data"]["username"] = result.data.username;

    auto res = HttpResponse::newHttpJsonResponse(root);
    res->setStatusCode(k201Created);
    co_return res;
}

// [GET] /api/accounts/{username}
Task<HttpResponsePtr> AccountController::getDetail(HttpRequestPtr req, std::string username) {
    auto role = req->getAttributes()->get<std::string>("role");
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto* service = app().getPlugin<AccountManagementService>();
    auto result = co_await service->getAccountDetail(username);

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["data"] = toJson(result.data);
    co_return HttpResponse::newHttpJsonResponse(root);
}

// [PUT] /api/accounts/{username}
Task<HttpResponsePtr> AccountController::updateAccount(HttpRequestPtr req, std::string username) {
    auto role = req->getAttributes()->get<std::string>("role");
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) co_return makeErrorResponse(400, "Invalid JSON");

    AccountEntity entity = fromJson(*jsonPtr);
    entity.username = username; 

    auto* service = app().getPlugin<AccountManagementService>();
    auto previous = co_await service->getAccountDetail(username);
    auto result = co_await service->updateAccount(entity);

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["data"] = toJson(result.data);
    if (previous.success)
        writeChangeAudit(req, "SỬA TÀI KHOẢN", "account:" + username,
                         toJson(previous.data), toJson(result.data),
                         {"role", "email", "phone_number", "full_name", "avatar_url", "date_of_birth"});
    co_return HttpResponse::newHttpJsonResponse(root);
}

// [DELETE] /api/accounts/{username}
Task<HttpResponsePtr> AccountController::deleteAccount(HttpRequestPtr req, std::string username) {
    auto role = req->getAttributes()->get<std::string>("role");
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto* service = app().getPlugin<AccountManagementService>();
    auto previous = co_await service->getAccountDetail(username);
    auto result = co_await service->deleteAccount(username);

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["message"] = "Deleted";
    if (previous.success)
        writeChangeAudit(req, "XÓA TÀI KHOẢN", "account:" + username,
                         toJson(previous.data), Json::Value(Json::objectValue),
                         {"role", "email", "phone_number", "full_name", "avatar_url", "date_of_birth"});
    co_return HttpResponse::newHttpJsonResponse(root);
}

// [GET] /api/me
Task<HttpResponsePtr> AccountController::getCurrentUser(HttpRequestPtr req) {
    auto role = req->getAttributes()->get<std::string>("role");
    // Contract: manager hoặc staff mới được vào, rỗng ("") bị chặn
    if (role != "manager" && role != "staff") co_return makeErrorResponse(403, "không đủ quyền");

    auto currentUsername = req->getAttributes()->get<std::string>("username");
    auto* service = app().getPlugin<AccountManagementService>();
    auto result = co_await service->getAccountDetail(currentUsername);

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["data"] = toJson(result.data);
    co_return HttpResponse::newHttpJsonResponse(root);
}

// [PUT] /api/me
Task<HttpResponsePtr> AccountController::updateCurrentUser(HttpRequestPtr req) {
    auto role = req->getAttributes()->get<std::string>("role");
    // Contract: PUT /api/me chỉ dành cho manager
    if (role != "manager") co_return makeErrorResponse(403, "không đủ quyền");

    auto currentUsername = req->getAttributes()->get<std::string>("username");
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) co_return makeErrorResponse(400, "Invalid JSON");

    AccountEntity entity = fromJson(*jsonPtr);
    entity.username = currentUsername; 

    auto* service = app().getPlugin<AccountManagementService>();
    auto previous = co_await service->getAccountDetail(currentUsername);
    auto result = co_await service->updateAccount(entity);

    if (!result.success) {
        co_return makeErrorResponse(result.errorCode, result.message);
    }

    Json::Value root;
    root["status"] = "success";
    root["data"] = toJson(result.data);
    if (previous.success)
        writeChangeAudit(req, "SỬA HỒ SƠ", "account:" + currentUsername,
                         toJson(previous.data), toJson(result.data),
                         {"email", "phone_number", "full_name", "avatar_url", "date_of_birth"});
    co_return HttpResponse::newHttpJsonResponse(root);
}
