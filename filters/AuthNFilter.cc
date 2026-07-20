#include "AuthNFilter.h"
#include "../AuditLog.h"
#include "../plugins/AuthNService.h"
#include <json/json.h>

using namespace drogon;

namespace
{
std::string unauthorizedAction(const HttpRequestPtr &req)
{
    const auto &path = req->path();
    const auto method = req->method();

    if (path == "/api/orders" && method == Get)
        return "xem danh sách đơn hàng";
    if (path == "/api/orders" && method == Post)
        return "tạo đơn hàng";
    if (path.rfind("/api/orders/", 0) == 0)
    {
        const auto id = path.substr(std::string("/api/orders/").size());
        if (method == Get)
            return "xem đơn hàng số " + id;
        if (method == Put)
            return "sửa đơn hàng số " + id;
        if (method == Delete)
            return "xóa đơn hàng số " + id;
    }
    if (path == "/api/accounts")
        return method == Post ? "tạo tài khoản" :
                                "xem danh sách tài khoản";
    if (path.rfind("/api/accounts/", 0) == 0)
        return "truy cập thông tin tài khoản";
    if (path == "/api/me")
        return method == Put ? "sửa hồ sơ cá nhân" :
                               "xem hồ sơ cá nhân";
    if (path == "/api/products")
        return method == Post ? "thêm sản phẩm" :
                                "xem danh sách sản phẩm";
    if (path.rfind("/api/products/", 0) == 0)
        return "thay đổi sản phẩm";
    if (path == "/api/printbill")
        return "in hóa đơn";
    if (path == "/auth/logout")
        return "đăng xuất";
    if (path == "/auth/logout_all")
        return "đăng xuất khỏi mọi thiết bị";
    if (path == "/auth/me")
        return "xem trạng thái đăng nhập";
    if (path == "/auth/sessions" ||
        path.rfind("/auth/sessions/", 0) == 0)
        return "quản lý phiên đăng nhập";
    if (path == "/auth/users" ||
        path.rfind("/auth/users/", 0) == 0)
        return "quản lý tài khoản đăng nhập";
    if (path == "/auth/change_password")
        return "đổi mật khẩu tài khoản";
    return "truy cập chức năng được bảo vệ";
}
}  // namespace

void AuthNFilter::doFilter(const HttpRequestPtr &req,
                           FilterCallback &&fcb,
                           FilterChainCallback &&fccb)
{
    async_run([req,
               fcb = std::move(fcb),
               fccb = std::move(fccb)]() mutable -> Task<void> {

        auto authService = app().getPlugin<AuthNService>();

        // 🍪 đúng spec: sid
        auto token = req->getCookie("sid");

        auto reject = [&](const std::string &msg) {
            writeAuditLog(
                "Khách " + unauthorizedAction(req) +
                " — chưa đăng nhập.");

            Json::Value v;
            v["success"] = false;
            v["error"] = msg;

            auto resp = HttpResponse::newHttpJsonResponse(v);
            resp->setStatusCode(k401Unauthorized);

            fcb(resp);
        };

        if (token.empty()) {
            reject("UNAUTHORIZED");
            co_return;
        }

        auto usernameOpt = co_await authService->getUserByToken(token);

        if (!usernameOpt.has_value()) {
            reject("UNAUTHORIZED");
            co_return;
        }

        // 🧠 attach identity vào request context
        req->attributes()->insert("username", usernameOpt.value());

        fccb();
        co_return;
    });
}
