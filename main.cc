#include <drogon/drogon.h>
#include <cstdlib>
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

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

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
    return EXIT_SUCCESS;
}
