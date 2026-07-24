#pragma once

#include <drogon/plugins/Plugin.h>

#include <atomic>
#include <chrono>
#include <string>

class ServerManagementService
    : public drogon::Plugin<ServerManagementService>
{
  public:
    static void configureLicense(std::string registrantName,
                                 std::string expiresAt,
                                 bool verificationEnabled);

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    Json::Value status() const;
    Json::Value licenseInfo() const;
    bool beginShutdown();

  private:
    static std::string registrantName_;
    static std::string expiresAt_;
    static bool verificationEnabled_;

    std::chrono::steady_clock::time_point startedAt_;
    std::atomic_bool shuttingDown_{false};
};
