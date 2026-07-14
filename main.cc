#include <drogon/drogon.h>
#include <cstdlib>
#include <iostream>

#ifndef THIMPOS_IGNORE_KEY_CHECK
#include "KeyManagerClient.h"
#endif

int main()
{
#ifndef THIMPOS_IGNORE_KEY_CHECK
    try
    {
        thimpos::license::KeyManagerClient verifier;
        const auto license = verifier.verifyAtStartup();
        std::cout << "Bản quyền hợp lệ: " << license.registrantName
                  << " (hết hạn " << license.expiresAt << ")\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "\nLỖI BẢN QUYỀN: "
                  << thimpos::license::formatLicenseError(error.what()) << '\n';
        std::cerr << "ThimPOS chưa được khởi chạy.\n";
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
