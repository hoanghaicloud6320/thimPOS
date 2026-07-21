#include <drogon/drogon.h>
#include "AuditLog.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef THIMPOS_IGNORE_KEY_CHECK
#include "KeyManagerClient.h"
#endif

namespace
{
constexpr std::size_t kLogFileSize = 5 * 1024 * 1024;
constexpr std::size_t kRotatedLogFiles = 4;

std::string auditValue(std::string value)
{
    constexpr std::size_t kMaxAuditValueLength = 256;
    if (value.size() > kMaxAuditValueLength)
    {
        value.resize(kMaxAuditValueLength);
    }

    for (auto &character : value)
    {
        if (character == '\r' || character == '\n' ||
            character == '\t')
        {
            character = ' ';
        }
    }
    return value;
}

std::string auditAttribute(const drogon::HttpRequestPtr &request,
                           const std::string &name,
                           const std::string &fallback)
{
    try
    {
        const auto value =
            request->attributes()->get<std::string>(name);
        return value.empty() ? fallback : auditValue(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::string auditTarget(const drogon::HttpRequestPtr &request)
{
    const auto &path = request->path();
    if (path != "/auth/login" && path != "/auth/register")
    {
        return "-";
    }

    const auto json = request->getJsonObject();
    if (!json || !json->isMember("username") ||
        !(*json)["username"].isString())
    {
        return "-";
    }

    // Only the claimed username is useful for these anonymous actions.
    // Never write passwords, cookies, session tokens, or the request body.
    return auditValue((*json)["username"].asString());
}

std::string auditJsonString(const drogon::HttpRequestPtr &request,
                            const std::string &field)
{
    const auto json = request->getJsonObject();
    if (!json || !json->isMember(field) || !(*json)[field].isString())
    {
        return "";
    }
    return auditValue((*json)[field].asString());
}

bool startsWith(const std::string &value, const std::string &prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::string auditActor(const drogon::HttpRequestPtr &request)
{
    const auto username =
        auditAttribute(request, "username", "");
    if (username.empty())
    {
        return "Khách";
    }

    const auto role = auditAttribute(request, "role", "");
    if (role == "manager")
    {
        return username + " (quản lý)";
    }
    if (role == "staff")
    {
        return username + " (nhân viên)";
    }
    return username;
}

std::string auditAction(const drogon::HttpRequestPtr &request)
{
    const auto &path = request->path();
    const auto method = request->method();
    const auto target = auditTarget(request);

    if (path == "/auth/login")
        return "đăng nhập với tên " + target;
    if (path == "/auth/register")
        return "đăng ký tài khoản " + target;
    if (path == "/auth/logout")
        return "đăng xuất";
    if (path == "/auth/logout_all")
        return "đăng xuất khỏi mọi thiết bị";
    if (path == "/auth/change_password")
    {
        const auto username = auditJsonString(request, "username");
        return "đổi mật khẩu tài khoản" +
               (username.empty() ? "" : " " + username);
    }
    if (path == "/auth/me")
        return "xem trạng thái đăng nhập";
    if (path == "/auth/sessions")
        return "xem các phiên đăng nhập";
    if (startsWith(path, "/auth/sessions/"))
        return "xóa một phiên đăng nhập";
    if (path == "/auth/users")
        return "xem danh sách tài khoản đăng nhập";
    if (startsWith(path, "/auth/users/"))
        return "xóa thông tin đăng nhập của " +
               auditValue(path.substr(std::string("/auth/users/").size()));

    if (path == "/api/printbill")
    {
        const auto id = auditValue(request->getParameter("id"));
        return "in hóa đơn" + (id.empty() ? "" : " số " + id);
    }

    if (path == "/api/accounts" && method == drogon::Get)
        return "xem danh sách tài khoản";
    if (path == "/api/accounts" && method == drogon::Post)
    {
        const auto username = auditJsonString(request, "username");
        return "tạo tài khoản" +
               (username.empty() ? "" : " " + username);
    }
    if (startsWith(path, "/api/accounts/"))
    {
        const auto username =
            auditValue(path.substr(std::string("/api/accounts/").size()));
        if (method == drogon::Get)
            return "xem tài khoản " + username;
        if (method == drogon::Put)
            return "sửa tài khoản " + username;
        if (method == drogon::Delete)
            return "xóa tài khoản " + username;
    }
    if (path == "/api/me" && method == drogon::Get)
        return "xem hồ sơ cá nhân";
    if (path == "/api/me" && method == drogon::Put)
        return "sửa hồ sơ cá nhân";

    if (path == "/api/products" && method == drogon::Get)
        return "xem danh sách sản phẩm";
    if (path == "/api/products" && method == drogon::Post)
    {
        const auto name = auditJsonString(request, "name");
        return "thêm sản phẩm" +
               (name.empty() ? "" : " “" + name + "”");
    }
    if (startsWith(path, "/api/products/search"))
        return "tìm kiếm sản phẩm";
    if (startsWith(path, "/api/products/"))
    {
        const auto id =
            auditValue(path.substr(std::string("/api/products/").size()));
        if (method == drogon::Get)
            return "xem sản phẩm số " + id;
        if (method == drogon::Put)
            return "sửa sản phẩm số " + id;
        if (method == drogon::Delete)
            return "xóa sản phẩm số " + id;
    }

    if (path == "/api/orders" && method == drogon::Get)
        return "xem danh sách đơn hàng";
    if (path == "/api/orders" && method == drogon::Post)
        return "tạo đơn hàng";
    if (startsWith(path, "/api/orders/"))
    {
        const auto id =
            auditValue(path.substr(std::string("/api/orders/").size()));
        if (method == drogon::Get)
            return "xem đơn hàng số " + id;
        if (method == drogon::Put)
            return "sửa đơn hàng số " + id;
        if (method == drogon::Delete)
            return "xóa đơn hàng số " + id;
    }

    return "truy cập " + auditValue(path);
}

std::string auditResult(drogon::HttpStatusCode status)
{
    const auto code = static_cast<int>(status);
    if (code >= 200 && code < 400)
        return "thành công";
    if (code == 401)
        return "chưa đăng nhập";
    if (code == 403)
        return "không đủ quyền";
    if (code == 404)
        return "không tìm thấy";
    return "không thành công";
}

// Audit request/view cũ được giữ lại để tham khảo nhưng không biên dịch.
#if 0
void registerAuditLogger()
{
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr &request,
           const drogon::HttpResponsePtr &response) {
            const auto &path = request->path();
            if (!startsWith(path, "/api/") &&
                !startsWith(path, "/auth/"))
            {
                return;
            }

            writeAuditLog(
                auditActor(request) + " " +
                auditAction(request) + " — " +
                auditResult(response->statusCode()) + ".");
        });
}
#endif

std::string newLogFilePath()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream path;
    path << "logs/app-recorder-"
         << std::put_time(&localTime, "%Y%m%d-%H%M%S")
#ifdef _WIN32
         << "-" << GetCurrentProcessId()
#endif
         << ".log";
    return path.str();
}

std::shared_ptr<spdlog::logger> configureLogger()
{
    std::vector<spdlog::sink_ptr> sinks;
    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        newLogFilePath(),
        kLogFileSize,
        kRotatedLogFiles,
        false));

    auto logger = std::make_shared<spdlog::logger>(
        "thimpos", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);

    auto auditLogger = std::make_shared<spdlog::logger>(
        "audit", sinks.begin(), sinks.end());
    auditLogger->set_pattern("%Y-%m-%d %H:%M:%S | %v");
    auditLogger->set_level(spdlog::level::info);
    auditLogger->flush_on(spdlog::level::info);

    spdlog::register_logger(logger);
    spdlog::register_logger(auditLogger);
    trantor::Logger::enableSpdLog(logger);
    return logger;
}
}  // namespace

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

#ifndef THIMPOS_IGNORE_KEY_CHECK
    if (argc == 2 && std::strcmp(argv[1], "--clear-license-cache") == 0)
    {
        try
        {
            thimpos::license::clearLicenseCache();
            std::cout << "Đã xóa cache bản quyền. / License cache cleared.\n";
            return EXIT_SUCCESS;
        }
        catch (const std::exception &error)
        {
            std::cout << "Không thể xóa cache bản quyền. / Unable to clear license cache: "
                      << thimpos::license::formatLicenseError(error.what()) << '\n';
            return EXIT_FAILURE;
        }
    }
#else
    if (argc == 2 && std::strcmp(argv[1], "--clear-license-cache") == 0)
    {
        std::cerr << "Không thể xóa cache bản quyền từ bản build này. / "
                     "License cache clearing is unavailable in this build.\n";
        return EXIT_FAILURE;
    }
#endif

    std::error_code logDirectoryError;
    std::filesystem::create_directories("logs", logDirectoryError);
    if (logDirectoryError)
    {
        std::cout << "Không thể tạo thư mục logs. / Unable to create the logs directory.\n";
        std::cout << "Chi tiết / Details: " << logDirectoryError.message() << '\n';
        return EXIT_FAILURE;
    }

#ifndef THIMPOS_IGNORE_KEY_CHECK
    try
    {
        thimpos::license::KeyManagerClient verifier;
        const auto license = verifier.verifyAtStartup();
        std::cout << "Bản quyền hợp lệ / License valid: " << license.registrantName
                  << " (hết hạn / expires " << license.expiresAt << ")\n";
    }
    catch (const std::exception &error)
    {
        std::cout << "\nLỖI BẢN QUYỀN / LICENSE ERROR: "
                  << thimpos::license::formatLicenseError(error.what()) << '\n';
        std::cout << "ThimPOS chưa được khởi chạy. / ThimPOS was not started.\n";
        return EXIT_FAILURE;
    }
#else
    std::cerr << "WARNING: license-key verification is disabled in this build\n";
#endif

    // Load config first, then install a dual-sink logger before Drogon starts
    // plugins. Drogon keeps this logger instead of replacing it with a
    // file-only sink in setupFileLogger().
    drogon::app().loadConfigFile("config.json");
    auto logger = configureLogger();
    drogon::app().registerBeginningAdvice([] {
        const auto auditLogger = spdlog::get("audit");
        if (auditLogger)
        {
            auditLogger->set_pattern("%Y-%m-%d %H:%M:%S | %v");
        }
    });
    // Không audit request xem/list/search. Các controller chỉ ghi thao tác
    // sửa/xóa thành công, kèm diff field mà người dùng có thể chỉnh.
    // registerAuditLogger();

    // Run HTTP framework; this blocks in the internal event loop.
    drogon::app().run();

    // Close the log file while normal code is still running. Do not leave the
    // file handle for the DLL/static destructor phase on Windows.
    logger->flush();
    trantor::Logger::disableSpdLog();
    spdlog::drop("audit");
    spdlog::drop("thimpos");
    logger.reset();

#ifdef _WIN32
    // Drogon/Trantor's AsyncFileLogger destructor can deadlock while MinGW
    // unloads DLLs after a clean shutdown. Plugins and event loops have already
    // stopped when run() returns, so bypass only the faulty DLL destructors.
    std::cout.flush();
    std::cerr.flush();
    TerminateProcess(GetCurrentProcess(), EXIT_SUCCESS);
#endif

    return EXIT_SUCCESS;
}
