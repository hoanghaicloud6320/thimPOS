#pragma once

#include <drogon/plugins/Plugin.h>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct AiMessage
{
    std::string role;
    std::string content;
};

struct AiReply
{
    std::string model;
    std::string content;
};

class AiServiceError : public std::runtime_error
{
  public:
    AiServiceError(std::string code, std::string message, int httpStatus);

    const std::string &code() const noexcept;
    int httpStatus() const noexcept;

  private:
    std::string code_;
    int httpStatus_;
};

class AiService : public drogon::Plugin<AiService>
{
  public:
    static void configureFromLicense(
        const std::unordered_map<std::string, std::string> &metadata);

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    AiReply generate(const std::vector<AiMessage> &messages, float temperature) const;

  private:
    static std::string apiKey_;
    static std::string modelName_;
};
