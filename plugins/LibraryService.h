#pragma once

#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct LibraryFile
{
    std::string name;
    std::string url;
    std::uintmax_t size{0};
    std::int64_t modifiedAt{0};
};

class LibraryService : public drogon::Plugin<LibraryService>
{
  public:
    void initAndStart(const Json::Value &) override;
    void shutdown() override;

    std::vector<LibraryFile> listFiles() const;
    LibraryFile saveFile(const drogon::HttpFile &file);
    std::optional<std::string> readFile(const std::string &name) const;
    bool removeFile(const std::string &name);

  private:
    static bool isSafeFileName(const std::string &name);
    static std::string normalizedFileName(const std::string &name);
    LibraryFile describe(const std::filesystem::directory_entry &entry) const;

    std::filesystem::path root_;
    mutable std::mutex mutex_;
};
