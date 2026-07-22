#include "TxtPrinterService.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace drogon;

/**
 * @brief Plugin start
 */
void TxtPrinterService::initAndStart(const Json::Value &config)
{
    LOG_INFO << "[TxtPrinterService] 🚀 started";
}

/**
 * @brief Plugin shutdown
 */
void TxtPrinterService::shutdown()
{
    LOG_INFO << "[TxtPrinterService] 🛑 shutdown";
}

/**
 * @brief Lưu string vào file .txt
 */
Task<void> TxtPrinterService::saveStringToFile(const std::string &content,
                                               const std::string &filePath)
{
    // coroutine nhưng IO sync (đủ dùng cho POS nhỏ)
    std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);

    if (!ofs.is_open())
    {
        LOG_ERROR << "[TxtPrinterService] ❌ cannot open file: " << filePath;
        co_return;
    }

    ofs << content;
    ofs.close();

    //LOG_INFO << "[TxtPrinterService] 💾 saved file: " << filePath;

    co_return;
}

/**
 * @brief In file .txt bằng notepad
 */
Task<void> TxtPrinterService::printFile(const std::string &filePath)
{
    // escape path đơn giản (support path có space)
    std::string cmd = "notepad /p \"" + filePath + "\"";

    //LOG_INFO << "[TxtPrinterService] 🖨️ printing: " << filePath;

    // spawn process (non-blocking-ish, nhưng đủ cho use case)
    int ret = std::system(cmd.c_str());

    if (ret != 0)
    {
        LOG_ERROR << "[TxtPrinterService] ❌ printing "<<filePath<<" failed, code=" << ret;
    }
    else
    {
        LOG_INFO << "[TxtPrinterService] ✅ print command executed with "<<filePath;
    }

    co_return;
}