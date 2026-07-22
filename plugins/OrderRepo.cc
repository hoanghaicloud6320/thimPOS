#include "OrderRepo.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <ctime>
#include <vector>
#include <optional>

OrderRepo::OrderRepo() 
{
    db_ = drogon::app().getDbClient();
}

void OrderRepo::init_db() 
{
    const std::string sqlOrders = R"(
        CREATE TABLE IF NOT EXISTS orders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            total_price INTEGER NOT NULL
        );
    )";

    const std::string sqlOrderItems = R"(
        CREATE TABLE IF NOT EXISTS order_items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            order_id   INTEGER NOT NULL,
            product_id INTEGER NOT NULL,
            quantity   INTEGER NOT NULL,
            unit_price INTEGER NOT NULL,
            line_total INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            FOREIGN KEY(order_id) REFERENCES orders(id)
        );
    )";

    db_->execSqlSync(sqlOrders);
    db_->execSqlSync(sqlOrderItems);
}

// =========================
// ORDERS
// =========================

drogon::Task<int64_t> OrderRepo::createOrder(int64_t total_price) 
{
    auto now = static_cast<int64_t>(std::time(nullptr));

    auto result = co_await db_->execSqlCoro(
        "INSERT INTO orders (total_price, created_at, updated_at) VALUES ($1, $2, $3) RETURNING id",
        total_price, now, now
    );

    co_return result[0]["id"].as<int64_t>();
}

drogon::Task<std::optional<Order>> OrderRepo::getOrder(int64_t id) 
{
    auto result = co_await db_->execSqlCoro(
        "SELECT * FROM orders WHERE id = $1",
        id
    );

    if (result.empty())
        co_return std::nullopt;

    Order o;
    o.id = result[0]["id"].as<int64_t>();
    o.created_at = result[0]["created_at"].as<int64_t>();
    o.updated_at = result[0]["updated_at"].as<int64_t>();
    o.total_price = result[0]["total_price"].as<int64_t>();

    co_return o;
}

drogon::Task<std::vector<Order>> OrderRepo::listOrders(int limit, int offset,
                                                       std::optional<int64_t> from,
                                                       std::optional<int64_t> to)
{
    std::string sql = "SELECT * FROM orders WHERE 1=1";
    if (from) sql += " AND created_at >= " + std::to_string(*from);
    if (to) sql += " AND created_at <= " + std::to_string(*to);
    sql += " ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?";
    auto result = co_await db_->execSqlCoro(sql, limit, offset);

    std::vector<Order> orders;

    for (const auto &row : result)
    {
        Order o;
        o.id = row["id"].as<int64_t>();
        o.created_at = row["created_at"].as<int64_t>();
        o.updated_at = row["updated_at"].as<int64_t>();
        o.total_price = row["total_price"].as<int64_t>();
        orders.push_back(o);
    }

    co_return orders;
}

drogon::Task<bool> OrderRepo::deleteOrder(int64_t id) 
{
    auto result = co_await db_->execSqlCoro(
        "DELETE FROM orders WHERE id = $1",
        id
    );

    co_return result.affectedRows() > 0;
}

drogon::Task<void> OrderRepo::updateOrderTotal(int64_t id, int64_t total_price) 
{
    auto now = static_cast<int64_t>(std::time(nullptr));

    co_await db_->execSqlCoro(
        "UPDATE orders SET total_price = $1, updated_at = $2 WHERE id = $3",
        total_price, now, id
    );

    co_return;
}

// =========================
// ORDER ITEMS
// =========================

drogon::Task<int64_t> OrderRepo::createItem(const OrderItem& item) 
{
    int64_t line_total = item.unit_price * item.quantity;

    auto result = co_await db_->execSqlCoro(
        "INSERT INTO order_items (order_id, product_id, quantity, unit_price, line_total, created_at) "
        "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
        item.order_id,
        item.product_id,
        item.quantity,
        item.unit_price,
        line_total,
        static_cast<int64_t>(std::time(nullptr))
    );

    co_return result[0]["id"].as<int64_t>();
}

drogon::Task<std::vector<OrderItem>> OrderRepo::getItemsByOrderId(int64_t order_id) 
{
    auto result = co_await db_->execSqlCoro(
        "SELECT * FROM order_items WHERE order_id = $1",
        order_id
    );

    std::vector<OrderItem> items;

    for (const auto &row : result)
    {
        OrderItem i;
        i.id = row["id"].as<int64_t>();
        i.order_id = row["order_id"].as<int64_t>();
        i.product_id = row["product_id"].as<int64_t>();
        i.quantity = row["quantity"].as<int>();
        i.unit_price = row["unit_price"].as<int64_t>();
        i.line_total = row["line_total"].as<int64_t>();
        i.created_at = row["created_at"].as<int64_t>();
        items.push_back(i);
    }

    co_return items;
}

drogon::Task<bool> OrderRepo::deleteItemsByOrderId(int64_t order_id) 
{
    auto result = co_await db_->execSqlCoro(
        "DELETE FROM order_items WHERE order_id = $1",
        order_id
    );

    co_return result.affectedRows() > 0;
}

// =========================
// FULL AGGREGATION
// =========================

drogon::Task<std::pair<Order, std::vector<OrderItem>>> OrderRepo::getOrderFull(int64_t id) 
{
    auto orderOpt = co_await getOrder(id);

    if (!orderOpt)
        co_return {{},{}};

    auto items = co_await getItemsByOrderId(id);

    co_return std::make_pair(*orderOpt, items);
}
