#pragma once

#include <drogon/drogon.h>
#include <vector>
#include <optional>
#include <cstdint>

struct Order
{
    int64_t id = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    int64_t total_price = 0; // Đã khởi tạo

    Json::Value toJson() const
    {
        Json::Value j;
        j["id"] = id;
        j["created_at"] = Json::Int64(created_at);
        j["updated_at"] = Json::Int64(updated_at);
        j["total_price"] = Json::Int64(total_price);
        return j;
    }
};

struct OrderItem
{
    int64_t id = 0;
    int64_t order_id = 0;
    int64_t product_id = 0;

    int quantity = 0;
    int64_t unit_price = 0;
    int64_t line_total = 0; // Giữ nguyên để khớp với logic cũ
    int64_t created_at = 0;

    Json::Value toJson() const
    {
        Json::Value j;
        j["product_id"] = product_id;
        j["quantity"] = quantity;
        j["unit_price"] = Json::Int64(unit_price);
        j["line_total"] = Json::Int64(line_total);
        return j;
    }
};

class OrderRepo
{
public:
    OrderRepo();

    void init_db();

    // =========================
    // TRANSACTIONS (bị cấm hoàn toàn)
    // =========================

    // =========================
    // ORDERS (HEADER)
    // =========================
    drogon::Task<int64_t> createOrder(int64_t total_price); // Thêm tham số
    drogon::Task<std::optional<Order>> getOrder(int64_t id);
    drogon::Task<std::vector<Order>> listOrders(int limit, int offset,
                                                std::optional<int64_t> from,
                                                std::optional<int64_t> to);
    drogon::Task<bool> deleteOrder(int64_t id);
    drogon::Task<void> updateOrderTotal(int64_t id, int64_t total_price); // Thêm method này

    // =========================
    // ORDER ITEMS (DETAIL)
    // =========================
    drogon::Task<int64_t> createItem(const OrderItem& item);

    drogon::Task<std::vector<OrderItem>>
    getItemsByOrderId(int64_t order_id);

    drogon::Task<bool>
    deleteItemsByOrderId(int64_t order_id);

    // =========================
    // FULL AGGREGATION QUERY
    // =========================
    drogon::Task<std::pair<Order, std::vector<OrderItem>>>
    getOrderFull(int64_t id);

private:
    drogon::orm::DbClientPtr db_;
};
