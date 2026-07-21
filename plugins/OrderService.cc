#include "OrderService.h"
#include "ProductManagerService.h"
#include "InventoryService.h"
#include <drogon/drogon.h>
#include <stdexcept>
#include <vector>

using namespace drogon;

// =============================================================================
// LIFECYCLE
// =============================================================================

void OrderService::initAndStart(const Json::Value& config)
{
    repo_.init_db();
    bill_tpl_filename=config["bill tpl file"].asString();
    LOG_INFO << "[OrderService] initialized and ready.";
}

void OrderService::shutdown()
{
    LOG_INFO << "[OrderService] shutting down.";
}

// =============================================================================
// BUSINESS LOGIC
// =============================================================================

/**
 * @brief Tạo đơn hàng mới.
 * Chiến lược: Validate toàn bộ -> Tạo Header -> Tạo Items.
 * Nếu tạo Items thất bại -> Cleanup Header và các Items đã tạo.
 */
drogon::Task<OrderDTO> OrderService::createOrder(const std::vector<OrderItemInput>& items)
{
    if (items.empty()) throw std::runtime_error("Order items cannot be empty");

    auto productSvc = app().getPlugin<ProductManagerService>();
    if (!productSvc) throw std::runtime_error("ProductManagerService unavailable");

    // 1. Giai đoạn Validate & Tính toán (KHÔNG GÂY ẢNH HƯỞNG DB)
    int64_t total_price = 0;
    std::vector<OrderItem> order_items;
    
    for (const auto& input : items) {
        auto prod = co_await productSvc->getProductById(input.product_id, true);
        if (!prod) throw std::runtime_error("Product ID not found: " + std::to_string(input.product_id));

        OrderItem item;
        item.product_id = prod->id;
        item.quantity = input.quantity;
        item.unit_price = prod->price;
        item.line_total = prod->price * input.quantity;
        
        total_price += item.line_total;
        order_items.push_back(item);
    }

    // 2. Giai đoạn Thực thi (Có thể gây lỗi)
    int64_t order_id = co_await repo_.createOrder(total_price);
    bool success = false;

    try {
        for (auto& item : order_items) {
            item.order_id = order_id;
            co_await repo_.createItem(item);
        }
        success = true; // Đánh dấu hoàn thành
    } catch (...) {
        // Lỗi xảy ra: Success vẫn là false
    }

    // 3. Giai đoạn Cleanup (Nếu không thành công)
    if (!success) {
        co_await repo_.deleteItemsByOrderId(order_id);
        co_await repo_.deleteOrder(order_id);
        throw std::runtime_error("Failed to create order items, rolled back order.");
    }

    // 4. Trả về kết quả
    OrderDTO dto;
    dto.id = order_id;
    dto.total_price = total_price;
    std::vector<std::pair<int64_t, int>> sold;
    for (const auto& input : items) sold.emplace_back(input.product_id, input.quantity);
    auto inventory = app().getPlugin<InventoryService>();
    if (inventory) co_await inventory->consumeOrder(order_id, sold);
    // Map thêm thông tin nếu cần
    co_return dto;
}

drogon::Task<std::optional<OrderDTO>> OrderService::getOrder(int64_t id)
{
    auto result = co_await repo_.getOrderFull(id);

    if (result.first.id == 0) {
        co_return std::nullopt;
    }

    auto productSvc = app().getPlugin<ProductManagerService>();
    if (!productSvc) throw std::runtime_error("ProductManagerService unavailable");

    OrderDTO dto;
    dto.id = result.first.id;
    dto.total_price = result.first.total_price;
    dto.created_at = result.first.created_at;
    dto.updated_at = result.first.updated_at;

    for (const auto& item : result.second) {
        // 🚀 query thẳng, không cache
        auto prod = co_await productSvc->getProductById(item.product_id, false);
        if (!prod) throw std::runtime_error("Product not found: " + std::to_string(item.product_id));

        OrderItemDTO itemDto;
        itemDto.product_id = item.product_id;
        itemDto.product_name = prod->name;  // ✅ fill trực tiếp
        itemDto.quantity = item.quantity;
        itemDto.unit_price = item.unit_price;
        itemDto.line_total = item.line_total;

        dto.items.push_back(itemDto);
    }

    co_return dto;
}

drogon::Task<std::vector<OrderDTO>> OrderService::listOrders(int limit, int offset)
{
    auto orders = co_await repo_.listOrders(limit, offset);
    std::vector<OrderDTO> dtos;
    
    for (const auto& o : orders) {
        OrderDTO dto;
        dto.id = o.id;
        dto.total_price = o.total_price;
        dto.created_at = o.created_at;
        dto.updated_at = o.updated_at;
        dtos.push_back(dto);
    }
    
    co_return dtos;
}

/**
 * @brief Cập nhật đơn hàng (Thay thế toàn bộ Items).
 * Lưu ý: Logic này xóa items cũ, cập nhật lại, rồi thêm items mới.
 */
drogon::Task<OrderDTO> OrderService::replaceOrder(int64_t id, const std::vector<OrderItemInput>& items)
{
    auto productSvc = app().getPlugin<ProductManagerService>();
    
    // 1. Validate & tính toán trước
    int64_t new_total = 0;
    std::vector<OrderItem> new_items;
    
    for (const auto& input : items) {
        auto prod = co_await productSvc->getProductById(input.product_id, true);
        if (!prod) throw std::runtime_error("Invalid product");
        
        OrderItem item;
        item.order_id = id;
        item.product_id = prod->id;
        item.quantity = input.quantity;
        item.unit_price = prod->price;
        item.line_total = prod->price * input.quantity;
        new_total += item.line_total;
        new_items.push_back(item);
    }

    // 2. Thực thi thay đổi
    bool success = false;
    try {
        co_await repo_.deleteItemsByOrderId(id);
        for(const auto& item : new_items) {
            co_await repo_.createItem(item);
        }
        co_await repo_.updateOrderTotal(id, new_total);
        success = true;
    } catch (...) {
        // Nếu fail, dữ liệu hiện tại đã bị corrupt (đã xóa items cũ). 
        // Trong trường hợp này, ta throw để báo lỗi hệ thống.
        throw; 
    }

    if (!success) {
        throw std::runtime_error("Failed to replace order");
    }

    // 3. Trả về kết quả mới nhất
    auto updated = co_await getOrder(id);
    std::vector<std::pair<int64_t, int>> sold;
    for (const auto& input : items) sold.emplace_back(input.product_id, input.quantity);
    if (auto inventory = app().getPlugin<InventoryService>())
        co_await inventory->consumeOrder(id, sold);
    co_return updated.value();
}

drogon::Task<bool> OrderService::deleteOrder(int64_t id)
{
    // Thứ tự xóa: Items trước, Order sau
    bool itemsDeleted = co_await repo_.deleteItemsByOrderId(id);
    bool orderDeleted = co_await repo_.deleteOrder(id);
    if (orderDeleted) {
        if (auto inventory = app().getPlugin<InventoryService>())
            co_await inventory->consumeOrder(id, {});
    }
    
    co_return (orderDeleted); // Chỉ quan tâm order đã xóa hay chưa
}
