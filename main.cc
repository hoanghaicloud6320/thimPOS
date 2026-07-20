#include <drogon/drogon.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
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

void registerAuditLogger()
{
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr &request,
           const drogon::HttpResponsePtr &response) {
            const auto actor =
                auditAttribute(request, "username", "anonymous");
            const auto role =
                auditAttribute(request, "role", "anonymous");

            LOG_INFO << "[AUDIT]"
                     << " actor=" << actor
                     << " role=" << role
                     << " method=" << request->methodString()
                     << " path=" << auditValue(request->path())
                     << " status="
                     << static_cast<int>(response->statusCode())
                     << " ip=" << request->peerAddr().toIp()
                     << " target=" << auditTarget(request);
        });
}

std::shared_ptr<spdlog::logger> configureLogger()
{
    std::vector<spdlog::sink_ptr> sinks;
    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/app-recorder.log",
        kLogFileSize,
        kRotatedLogFiles,
        false));

    auto logger = std::make_shared<spdlog::logger>(
        "thimpos", sinks.begin(), sinks.end());
    logger->set_pattern("%Y%m%d %T.%f %6t %^%=8l%$ [%!] %v - %s:%#");
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);
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
    registerAuditLogger();

    // Run HTTP framework; this blocks in the internal event loop.
    drogon::app().run();

    // TerminateProcess() below bypasses static and DLL destructors, so flush
    // explicitly after plugin shutdown messages have been emitted.
    logger->flush();

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
