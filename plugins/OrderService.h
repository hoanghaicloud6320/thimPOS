#pragma once

#include <drogon/drogon.h>
#include "OrderRepo.h"

#include <optional>
#include <vector>
#include <cstdint>

/**
 * @struct OrderItemInput
 * @brief Dữ liệu đầu vào cho một mục hàng trong đơn hàng.
 */
struct OrderItemInput
{
    int64_t product_id = 0;
    int quantity = 0;
};

/**
 * @struct OrderItemDTO
 * @brief Dữ liệu hiển thị chi tiết cho một mục hàng.
 */
struct OrderItemDTO
{
    int64_t product_id = 0;
    std::string product_name;
    int quantity = 0;
    int64_t unit_price = 0;
    int64_t line_total = 0;
};

/**
 * @struct OrderDTO
 * @brief Dữ liệu tổng thể của một đơn hàng.
 */
struct OrderDTO
{
    int64_t id = 0;
    std::vector<OrderItemDTO> items;
    int64_t total_price = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

/**
 * @class OrderService
 * @brief Service tầng nghiệp vụ chịu trách nhiệm xử lý logic đơn hàng.
 * @details Sử dụng ProductManagerService (thông qua Plugin pattern) để truy xuất thông tin sản phẩm.
 */
class OrderService : public drogon::Plugin<OrderService>
{
public:
    OrderService() = default;

    /**
     * @brief Khởi tạo và bắt đầu plugin.
     * @param config Cấu hình JSON cho plugin.
     */
    void initAndStart(const Json::Value& config) override;

    /**
     * @brief Dừng hoạt động của plugin.
     */
    void shutdown() override;

    /**
     * @brief Tạo một đơn hàng mới.
     * @param items Danh sách các mục hàng (OrderItemInput).
     * @return drogon::Task<OrderDTO> Dữ liệu đơn hàng vừa tạo.
     */
    drogon::Task<OrderDTO> createOrder(
        const std::vector<OrderItemInput>& items
    );

    /**
     * @brief Lấy chi tiết của một đơn hàng theo ID.
     * @param id ID của đơn hàng cần truy vấn.
     * @return drogon::Task<std::optional<OrderDTO>> Đơn hàng tìm thấy hoặc std::nullopt.
     */
    [[nodiscard]] drogon::Task<std::optional<OrderDTO>> getOrder(int64_t id);

    /**
     * @brief Liệt kê danh sách đơn hàng mới nhất với phân trang.
     * @param limit Số lượng bản ghi tối đa.
     * @param offset Vị trí bắt đầu (offset).
     * @return drogon::Task<std::vector<OrderDTO>> Danh sách các đơn hàng.
     */
    [[nodiscard]] drogon::Task<std::vector<OrderDTO>> listOrders(
        int limit,
        int offset,
        std::optional<int64_t> from = std::nullopt,
        std::optional<int64_t> to = std::nullopt,
        bool includeItems = false
    );

    /**
     * @brief Thay thế nội dung của một đơn hàng hiện có.
     * @param id ID của đơn hàng cần cập nhật.
     * @param items Danh sách mục hàng mới.
     * @return drogon::Task<OrderDTO> Dữ liệu đơn hàng sau khi cập nhật.
     */
    drogon::Task<OrderDTO> replaceOrder(
        int64_t id,
        const std::vector<OrderItemInput>& items
    );

    /**
     * @brief Xóa một đơn hàng khỏi hệ thống.
     * @param id ID của đơn hàng cần xóa.
     * @return drogon::Task<bool> Trả về true nếu xóa thành công, ngược lại false.
     */
    drogon::Task<bool> deleteOrder(int64_t id);

    
    /**
     * @brief Chuyển đổi OrderDTO thành chuỗi dạng hóa đơn (bill).
     * @details Hàm này tạo một chuỗi text biểu diễn đầy đủ thông tin đơn hàng,
     *          bao gồm danh sách sản phẩm, số lượng, đơn giá, thành tiền từng dòng
     *          và tổng tiền cuối cùng.
     *
     *          Chuỗi kết quả phù hợp để:
     *          - In ra file / máy in (thông qua system hoặc tool ngoài)
     *          - Hiển thị nhanh trên console (debug)
     *          - Gửi qua các service text-based (log, message queue, ...)
     *
     *          Định dạng chuỗi có thể thay đổi tùy nhu cầu hiển thị (plain text, aligned columns, ...).
     *
     * @param order Đối tượng OrderDTO chứa toàn bộ dữ liệu đơn hàng.
     * @return std::string Chuỗi biểu diễn hóa đơn của đơn hàng.
     */
    std::string convertOrderToBillString(OrderDTO order);

private:
    OrderRepo repo_;
    std::string bill_tpl_filename;
};
