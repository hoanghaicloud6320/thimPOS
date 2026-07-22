#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace thimpos::license
{
struct VerifiedLicense
{
    std::string registrantName;
    std::string expiresAt;
    std::unordered_map<std::string, std::string> metadata;
};

class KeyManagerClient
{
  public:
    KeyManagerClient();
    VerifiedLicense verifyAtStartup() const;

  private:
    std::string apiUrl_;
    std::string product_;
};

void clearLicenseCache();
std::string formatLicenseError(std::string_view technicalError);
}  // namespace thimpos::license
