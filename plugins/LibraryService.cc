#include "LibraryService.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>

using namespace drogon;
namespace fs = std::filesystem;

namespace
{
fs::path pathFromUtf8(const std::string &value)
{
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return fs::path(std::u8string(begin, begin + value.size()));
}

std::string pathToUtf8(const fs::path &value)
{
    const auto utf8 = value.u8string();
    return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

std::pair<std::string, std::string> splitExtension(const std::string &name)
{
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
        return {name, ""};
    return {name.substr(0, dot), name.substr(dot)};
}
}  // namespace

void LibraryService::initAndStart(const Json::Value &)
{
    root_ = fs::absolute(fs::path("static") / "user_library_files").lexically_normal();
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec)
        throw std::runtime_error("Cannot create library storage: " + ec.message());
    LOG_INFO << "[LibraryService] storage: " << root_.string();
}

void LibraryService::shutdown()
{
}

bool LibraryService::isSafeFileName(const std::string &name)
{
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos &&
           name.find('\0') == std::string::npos;
}

std::string LibraryService::normalizedFileName(const std::string &name)
{
    const auto separator = name.find_last_of("/\\");
    auto base = separator == std::string::npos ? name : name.substr(separator + 1);
    if (base.empty() || base == "." || base == "..")
        base = "file";

    for (auto &ch : base)
    {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte < 32 || ch == '/' || ch == '\\' || ch == ':' ||
            ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
            ch == '>' || ch == '|')
            ch = '_';
    }
    while (!base.empty() && (base.back() == '.' || base.back() == ' '))
        base.pop_back();
    return base.empty() ? "file" : base;
}

LibraryFile LibraryService::describe(const fs::directory_entry &entry) const
{
    LibraryFile result;
    result.name = pathToUtf8(entry.path().filename());
    result.url = "/user_library_files/" + result.name;
    std::error_code ec;
    result.size = entry.file_size(ec);
    const auto writeTime = entry.last_write_time(ec);
    if (!ec)
    {
        const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        result.modifiedAt = std::chrono::duration_cast<std::chrono::seconds>(
                                systemTime.time_since_epoch())
                                .count();
    }
    return result;
}

std::vector<LibraryFile> LibraryService::listFiles() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LibraryFile> result;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(root_, ec))
    {
        if (entry.is_regular_file() &&
            !pathToUtf8(entry.path().filename()).starts_with("."))
            result.push_back(describe(entry));
    }
    if (ec)
        throw std::runtime_error("Cannot read library: " + ec.message());
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.modifiedAt > b.modifiedAt;
    });
    return result;
}

LibraryFile LibraryService::saveFile(const HttpFile &file)
{
    if (file.getFileName().empty() || file.fileLength() == 0)
        throw std::invalid_argument("File is empty");

    std::lock_guard<std::mutex> lock(mutex_);
    const auto cleanName = normalizedFileName(file.getFileName());
    const auto [stem, extension] = splitExtension(cleanName);

    auto target = root_ / pathFromUtf8(cleanName);
    unsigned int copyNumber = 1;
    while (fs::exists(target))
        target = root_ / pathFromUtf8(
                             stem + " (" + std::to_string(copyNumber++) + ")" + extension);

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    const auto content = file.fileContent();
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output)
    {
        std::error_code ignored;
        fs::remove(target, ignored);
        throw std::runtime_error("Cannot save uploaded file");
    }
    return describe(fs::directory_entry(target));
}

std::optional<std::string> LibraryService::readFile(const std::string &name) const
{
    if (!isSafeFileName(name))
        throw std::invalid_argument("Invalid file name");

    std::lock_guard<std::mutex> lock(mutex_);
    const auto target = root_ / pathFromUtf8(name);
    std::error_code ec;
    if (!fs::is_regular_file(target, ec))
    {
        if (ec)
            throw std::runtime_error("Cannot read file: " + ec.message());
        return std::nullopt;
    }

    std::ifstream input(target, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read file");
    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    if (input.bad())
        throw std::runtime_error("Cannot read file");
    return content;
}

bool LibraryService::removeFile(const std::string &name)
{
    if (!isSafeFileName(name))
        throw std::invalid_argument("Invalid file name");
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    const bool removed = fs::remove(root_ / pathFromUtf8(name), ec);
    if (ec)
        throw std::runtime_error("Cannot delete file: " + ec.message());
    return removed;
}
