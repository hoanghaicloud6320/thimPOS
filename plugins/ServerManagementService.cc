#include "ServerManagementService.h"

#include <ctime>
#include <utility>

std::string ServerManagementService::registrantName_;
std::string ServerManagementService::expiresAt_;
bool ServerManagementService::verificationEnabled_ = false;

void ServerManagementService::configureLicense(
    std::string registrantName,
    std::string expiresAt,
    bool verificationEnabled)
{
    registrantName_ = std::move(registrantName);
    expiresAt_ = std::move(expiresAt);
    verificationEnabled_ = verificationEnabled;
}

void ServerManagementService::initAndStart(const Json::Value &)
{
    startedAt_ = std::chrono::steady_clock::now();
    shuttingDown_.store(false);
}

void ServerManagementService::shutdown()
{
}

Json::Value ServerManagementService::status() const
{
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - startedAt_)
                            .count();

    Json::Value result;
    result["status"] =
        shuttingDown_.load() ? "shutting_down" : "running";
    result["uptime_seconds"] = Json::Int64(uptime);
    result["server_time"] = Json::Int64(std::time(nullptr));
    return result;
}

Json::Value ServerManagementService::licenseInfo() const
{
    Json::Value result;
    result["verification_enabled"] = verificationEnabled_;
    result["valid"] =
        verificationEnabled_ && !registrantName_.empty() &&
        !expiresAt_.empty();
    result["registrant_name"] = registrantName_;
    result["expires_at"] = expiresAt_;
    return result;
}

bool ServerManagementService::beginShutdown()
{
    bool expected = false;
    return shuttingDown_.compare_exchange_strong(expected, true);
}
