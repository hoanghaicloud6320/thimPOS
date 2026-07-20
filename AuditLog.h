#pragma once

#include <spdlog/spdlog.h>

#include <string>

inline void writeAuditLog(const std::string &message)
{
    const auto logger = spdlog::get("audit");
    if (logger)
    {
        logger->info("[NHẬT KÝ] {}", message);
    }
}
