#include <drogon/drogon.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef THIMPOS_IGNORE_KEY_CHECK
#include "KeyManagerClient.h"
#endif

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

    //Load config file
    drogon::app().loadConfigFile("config.json");
    //Run HTTP framework,the method will block in the internal event loop
    drogon::app().run();

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
