#pragma once

#include <string>
#include <string_view>

namespace thimpos::license
{
struct VerifiedLicense
{
    std::string registrantName;
    std::string expiresAt;
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

std::string formatLicenseError(std::string_view technicalError);
}  // namespace thimpos::license
