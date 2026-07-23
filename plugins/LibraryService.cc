#include "LibraryService.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

using namespace drogon;
namespace fs = std::filesystem;

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
           fs::path(name).filename().string() == name &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos &&
           name.find('\0') == std::string::npos;
}

std::string LibraryService::normalizedFileName(const std::string &name)
{
    auto base = fs::path(name).filename().string();
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
    result.name = entry.path().filename().string();
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
            !entry.path().filename().string().starts_with("."))
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
    const fs::path cleanPath(cleanName);
    const auto stem = cleanPath.stem().string();
    const auto extension = cleanPath.extension().string();

    auto target = root_ / cleanName;
    unsigned int copyNumber = 1;
    while (fs::exists(target))
        target = root_ / (stem + " (" + std::to_string(copyNumber++) + ")" + extension);

    auto savePath = fs::relative(target, fs::current_path()).generic_string();
    if (!savePath.starts_with("."))
        savePath = "./" + savePath;
    if (file.saveAs(savePath) != 0)
        throw std::runtime_error("Cannot save uploaded file");
    return describe(fs::directory_entry(target));
}

bool LibraryService::removeFile(const std::string &name)
{
    if (!isSafeFileName(name))
        throw std::invalid_argument("Invalid file name");
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    const bool removed = fs::remove(root_ / name, ec);
    if (ec)
        throw std::runtime_error("Cannot delete file: " + ec.message());
    return removed;
}
