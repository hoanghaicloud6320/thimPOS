#include "OrderController.h"
#include "../AuditLog.h"

// --- STATIC HELPER FUNCTIONS ---

static std::vector<OrderItemInput> toOrderItemInputs(const Json::Value& jsonItems) {
    std::vector<OrderItemInput> items;
    if (!jsonItems.isArray()) return items;
    for (const auto& item : jsonItems) {
        items.push_back({
            item.get("product_id", 0).asInt64(),
            item.get("quantity", 0).asInt()
        });
    }
    return items;
}

static Json::Value orderToJson(const OrderDTO& order) {
    Json::Value ret;
    ret["id"] = (Json::Int64)order.id;
    ret["total_price"] = (Json::Int64)order.total_price;
    ret["created_at"] = (Json::Int64)order.created_at;
    ret["updated_at"] = (Json::Int64)order.updated_at;
    
    Json::Value itemsArray(Json::arrayValue);
    for (const auto& item : order.items) {
        Json::Value itemJson;
        itemJson["product_id"] = (Json::Int64)item.product_id;
        itemJson["product_name"] = item.product_name;
        itemJson["quantity"] = item.quantity;
        itemJson["unit_price"] = (Json::Int64)item.unit_price;
        itemJson["line_total"] = (Json::Int64)item.line_total;
        itemsArray.append(itemJson);
    }
    ret["items"] = itemsArray;
    return ret;
}

static Json::Value orderListToJson(const std::vector<OrderDTO>& orders) {
    Json::Value ret(Json::arrayValue);
    for (const auto& order : orders) {
        Json::Value item;
        item["id"] = (Json::Int64)order.id;
        item["total_price"] = (Json::Int64)order.total_price;
        item["created_at"] = (Json::Int64)order.created_at;
        ret.append(item);
    }
    return ret;
}

static Json::Value editableOrderJson(const OrderDTO& order) {
    Json::Value root, items(Json::arrayValue);
    for (const auto& item : order.items) {
        Json::Value row;
        row["product_id"] = Json::Int64(item.product_id);
        row["quantity"] = item.quantity;
        items.append(row);
    }
    root["items"] = items;
    return root;
}

static bool isAuthorized(const drogon::HttpRequestPtr& req) {
    auto role = req->getAttributes()->get<std::string>("role");
    return (role == "manager" || role == "staff");
}

// --- CONTROLLER IMPLEMENTATION ---

drogon::Task<drogon::HttpResponsePtr> OrderController::createOrder(drogon::HttpRequestPtr req)
{
    if (!isAuthorized(req)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        co_return resp;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("items")) {
        Json::Value err;
        err["error"] = "INVALID_REQUEST";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }

    try {
        auto items = toOrderItemInputs((*json)["items"]);
        auto order = co_await service_->createOrder(items);
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(orderToJson(order));
        resp->setStatusCode(drogon::k201Created);
        co_return resp;
    } catch (const std::exception& e) {
        Json::Value err;
        err["error"] = e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> OrderController::getOrderDetail(drogon::HttpRequestPtr req, int64_t id)
{
    if (!isAuthorized(req)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        co_return resp;
    }

    try {
        auto order = co_await service_->getOrder(id);
        if (!order) {
            Json::Value err;
            err["error"] = "ORDER_NOT_FOUND";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k404NotFound);
            co_return resp;
        }
        co_return drogon::HttpResponse::newHttpJsonResponse(orderToJson(*order));
    }
    catch (const std::exception& e) {
        Json::Value err;
        err["error"] = "INTERNAL_ERROR";
        err["message"] = e.what();

        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k500InternalServerError);
        co_return resp;
    }
    catch (...) {
        Json::Value err;
        err["error"] = "INTERNAL_ERROR";
        err["message"] = "unknown exception";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k500InternalServerError);
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> OrderController::listOrders(drogon::HttpRequestPtr req)
{
    if (!isAuthorized(req)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        co_return resp;
    }

    auto limitParam = req->getParameter("limit");
    auto offsetParam = req->getParameter("offset");
    int limit = limitParam.empty() ? 20 : std::stoi(limitParam);
    int offset = offsetParam.empty() ? 0 : std::stoi(offsetParam);

    try {
        auto orders = co_await service_->listOrders(limit, offset);

        Json::Value ret;
        ret["items"] = orderListToJson(orders);
        ret["limit"] = limit;
        ret["offset"] = offset;

        co_return drogon::HttpResponse::newHttpJsonResponse(ret);
    } catch (...) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k500InternalServerError);
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> OrderController::replaceOrder(drogon::HttpRequestPtr req, int64_t id)
{
    if (!isAuthorized(req)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        co_return resp;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("items")) {
        Json::Value err;
        err["error"] = "INVALID_REQUEST";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }

    try {
        auto items = toOrderItemInputs((*json)["items"]);
        auto previous = co_await service_->getOrder(id);
        auto order = co_await service_->replaceOrder(id, items);
        if (previous)
            writeChangeAudit(req, "SỬA ĐƠN HÀNG", "order#" + std::to_string(id),
                             editableOrderJson(*previous), editableOrderJson(order), {"items"});
        co_return drogon::HttpResponse::newHttpJsonResponse(orderToJson(order));
    } catch (const std::exception& e) {
        Json::Value err;
        err["error"] = e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k400BadRequest);
        co_return resp;
    }
}

drogon::Task<drogon::HttpResponsePtr> OrderController::deleteOrder(drogon::HttpRequestPtr req, int64_t id)
{
    if (!isAuthorized(req)) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k403Forbidden);
        co_return resp;
    }

    auto previous = co_await service_->getOrder(id);
    bool success = co_await service_->deleteOrder(id);
    if (success) {
        Json::Value ret;
        ret["success"] = true;
        if (previous)
            writeChangeAudit(req, "XÓA ĐƠN HÀNG", "order#" + std::to_string(id),
                             editableOrderJson(*previous), Json::Value(Json::objectValue), {"items"});
        co_return drogon::HttpResponse::newHttpJsonResponse(ret);
    }
    
    Json::Value err;
    err["error"] = "ORDER_NOT_FOUND";
    auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(drogon::k404NotFound);
    co_return resp;
}
