#include "ProductManagerController.h"
#include "../plugins/ProductManagerService.h"
#include "../AuditLog.h"

using namespace drogon;

// Helper tạo JSON response thành công
static HttpResponsePtr jsonResponse(const Json::Value& data, HttpStatusCode code = k200OK)
{
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(code);
    return resp;
}

// Helper tạo JSON response lỗi
static HttpResponsePtr errorResponse(const std::string& msg, HttpStatusCode code = k400BadRequest)
{
    Json::Value json;
    json["error"] = msg;
    return jsonResponse(json, code);
}

// ================= CREATE =================
Task<HttpResponsePtr> ProductManagerController::create(const HttpRequestPtr req)
{
    // Role Enforcement: Theo contract 1.3, create chỉ dành cho manager
    const std::string& role = req->attributes()->get<std::string>("role");
    if (role != "manager") {
        co_return errorResponse("Permission denied", k403Forbidden);
    }

    auto json = req->getJsonObject();
    if (!json)
        co_return errorResponse("Invalid JSON");

    if (!json->isMember("name") || !json->isMember("price") || !json->isMember("category"))
        co_return errorResponse("Missing required fields (name, price, category)");

    std::string name = (*json)["name"].asString();
    int64_t price = (*json)["price"].asInt64();
    std::string description = (*json).get("description", "").asString();
    std::string image_url = (*json).get("image_url", "").asString();
    std::string category = (*json)["category"].asString();

    auto svc = app().getPlugin<ProductManagerService>();
    try {
        int64_t id = co_await svc->createProduct(name, price, description, image_url, category);
        Json::Value resp;
        resp["id"] = id;
        co_return jsonResponse(resp, k201Created);
    } catch (const std::exception& e) {
        co_return errorResponse(e.what(), k500InternalServerError);
    }
}

// ================= GET BY ID =================
Task<HttpResponsePtr> ProductManagerController::getById(const HttpRequestPtr req, int64_t id)
{
    // Public access - No role check
    bool activeOnly = (req->getParameter("active_only") != "false");

    auto svc = app().getPlugin<ProductManagerService>();
    auto productOpt = co_await svc->getProductById(id, activeOnly);
    
    if (!productOpt)
        co_return errorResponse("Product not found", k404NotFound);

    const auto& p = *productOpt;
    Json::Value json;
    json["id"] = p.id;
    json["name"] = p.name;
    json["price"] = p.price;
    json["description"] = p.description;
    json["image_url"] = p.imageUrl;
    json["category"] = p.category;
    json["is_active"] = p.isActive;
    json["created_at"] = p.createdAt;
    json["updated_at"] = p.updatedAt;

    co_return jsonResponse(json);
}

// ================= LIST =================
Task<HttpResponsePtr> ProductManagerController::list(const HttpRequestPtr req)
{
    // Public access - No role check
    int limit = 20;
    int offset = 0;
    bool activeOnly = (req->getParameter("active_only") != "false");

    if (!req->getParameter("limit").empty()) limit = std::stoi(req->getParameter("limit"));
    if (!req->getParameter("offset").empty()) offset = std::stoi(req->getParameter("offset"));

    auto svc = app().getPlugin<ProductManagerService>();
    auto list = co_await svc->listProducts(limit, offset, activeOnly);

    Json::Value arr(Json::arrayValue);
    for (const auto& p : list) {
        Json::Value item;
        item["id"] = p.id;
        item["name"] = p.name;
        item["price"] = p.price;
        item["description"] = p.description;
        item["image_url"] = p.imageUrl;
        item["category"] = p.category;
        item["is_active"] = p.isActive;
        item["created_at"] = p.createdAt;
        item["updated_at"] = p.updatedAt;
        arr.append(item);
    }

    Json::Value resp;
    resp["data"] = arr;
    resp["paging"]["limit"] = limit;
    resp["paging"]["offset"] = offset;

    co_return jsonResponse(resp);
}

// ================= SEARCH (ENHANCED) =================
Task<HttpResponsePtr> ProductManagerController::search(const HttpRequestPtr req)
{
    // Public access - No role check
    std::string keyword = req->getParameter("q");
    int limit = 20;
    int offset = 0;
    bool activeOnly = (req->getParameter("active_only") != "false");

    if (!req->getParameter("limit").empty()) limit = std::stoi(req->getParameter("limit"));
    if (!req->getParameter("offset").empty()) offset = std::stoi(req->getParameter("offset"));

    std::optional<std::string> category;
    if (!req->getParameter("category").empty()) category = req->getParameter("category");

    std::optional<int64_t> from;
    if (!req->getParameter("from").empty()) from = std::stoll(req->getParameter("from"));

    std::optional<int64_t> to;
    if (!req->getParameter("to").empty()) to = std::stoll(req->getParameter("to"));

    auto svc = app().getPlugin<ProductManagerService>();
    auto list = co_await svc->searchProducts(keyword, category, from, to, limit, offset, activeOnly);

    Json::Value arr(Json::arrayValue);
    for (const auto& p : list) {
        Json::Value item;
        item["id"] = p.id;
        item["name"] = p.name;
        item["price"] = p.price;
        item["description"] = p.description;
        item["image_url"] = p.imageUrl;
        item["category"] = p.category;
        item["is_active"] = p.isActive;
        item["created_at"] = p.createdAt;
        item["updated_at"] = p.updatedAt;
        arr.append(item);
    }

    Json::Value resp;
    resp["data"] = arr;
    resp["paging"]["limit"] = limit;
    resp["paging"]["offset"] = offset;

    co_return jsonResponse(resp);
}

// ================= UPDATE =================
Task<HttpResponsePtr> ProductManagerController::update(const HttpRequestPtr req, int64_t id)
{
    // Role Enforcement: manager only
    const std::string& role = req->attributes()->get<std::string>("role");
    if (role != "manager") {
        co_return errorResponse("Permission denied", k403Forbidden);
    }

    auto json = req->getJsonObject();
    if (!json)
        co_return errorResponse("Invalid JSON");

    if (!json->isMember("name") || !json->isMember("price") || 
        !json->isMember("category") || !json->isMember("is_active")) {
        co_return errorResponse("Missing required fields");
    }

    std::string name = (*json)["name"].asString();
    int64_t price = (*json)["price"].asInt64();
    std::string description = (*json).get("description", "").asString();
    std::string image_url = (*json).get("image_url", "").asString();
    std::string category = (*json)["category"].asString();
    bool isActive = (*json)["is_active"].asBool();

    auto svc = app().getPlugin<ProductManagerService>();
    auto previous = co_await svc->getProductById(id, false);
    bool ok = co_await svc->updateProduct(id, name, price, description, image_url, category, isActive);

    if (!ok)
        co_return errorResponse("Product not found", k404NotFound);

    Json::Value resp;
    resp["success"] = true;
    Json::Value before;
    if (previous) {
        before["name"] = previous->name; before["price"] = previous->price;
        before["description"] = previous->description; before["image_url"] = previous->imageUrl;
        before["category"] = previous->category; before["is_active"] = previous->isActive;
    }
    writeChangeAudit(req, "SỬA SẢN PHẨM", "product#" + std::to_string(id), before, *json,
                     {"name", "price", "description", "image_url", "category", "is_active"});
    co_return jsonResponse(resp);
}

// ================= DELETE =================
Task<HttpResponsePtr> ProductManagerController::remove(const HttpRequestPtr req, int64_t id)
{
    // Role Enforcement: manager only
    const std::string& role = req->attributes()->get<std::string>("role");
    if (role != "manager") {
        co_return errorResponse("Permission denied", k403Forbidden);
    }

    auto svc = app().getPlugin<ProductManagerService>();
    auto previous = co_await svc->getProductById(id, false);
    bool ok = co_await svc->deleteProduct(id);

    if (!ok)
        co_return errorResponse("Delete failed: Product not found", k404NotFound);

    Json::Value resp;
    resp["success"] = true;
    if (previous) {
        Json::Value before, after;
        before["is_active"] = previous->isActive; after["is_active"] = false;
        writeChangeAudit(req, "XÓA SẢN PHẨM", "product#" + std::to_string(id), before, after,
                         {"is_active"});
    }
    co_return jsonResponse(resp);
}
