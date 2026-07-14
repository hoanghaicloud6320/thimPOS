#include "KeyManagerClient.h"

#include <curl/curl.h>
#include <json/json.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace thimpos::license
{
namespace
{
constexpr double kRequestTimeoutSeconds = 10.0;
constexpr auto kMaximumOfflineWindow = std::chrono::hours(24 * 7);
constexpr std::string_view kApiUrl =
    "https://keymanager-cloud.thuanvatlyhy.workers.dev";

using JsonPtr = std::shared_ptr<Json::Value>;

struct HttpResult
{
    bool transportSucceeded{false};
    int status{0};
    std::string body;
};

struct Cache
{
    Json::Value root;
    std::string key;
    std::string deviceId;
};

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

Json::Value parseJson(const std::string &text, std::string_view context)
{
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::Value value;
    std::string errors;
    std::istringstream input(text);
    if (!Json::parseFromStream(builder, input, &value, &errors))
    {
        throw std::runtime_error(std::string(context) + ": invalid JSON: " + errors);
    }
    return value;
}

std::string compactJson(const Json::Value &value)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::vector<unsigned char> decodeBase64Url(std::string_view encoded)
{
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, 256> table{};
    table.fill(-1);
    for (std::size_t i = 0; i < alphabet.size(); ++i)
        table[static_cast<unsigned char>(alphabet[i])] = static_cast<int>(i);

    std::vector<unsigned char> result;
    int accumulator = 0;
    int bits = -8;
    for (char character : encoded)
    {
        if (character == '=')
            break;
        if (character == '-')
            character = '+';
        else if (character == '_')
            character = '/';
        const int value = table[static_cast<unsigned char>(character)];
        if (value < 0)
            throw std::runtime_error("LICENSE_BASE64_INVALID");
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 0)
        {
            result.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xff));
            bits -= 8;
        }
    }
    return result;
}

std::chrono::system_clock::time_point parseUtc(const std::string &value,
                                               std::string_view field)
{
    if (value.size() < 20 || value.back() != 'Z')
        throw std::runtime_error("LICENSE_TIME_INVALID: " + std::string(field));
    std::tm tm{};
    std::istringstream input(value.substr(0, 19));
    input >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (input.fail())
        throw std::runtime_error("LICENSE_TIME_INVALID: " + std::string(field));
#ifdef _WIN32
    const auto seconds = _mkgmtime(&tm);
#else
    const auto seconds = timegm(&tm);
#endif
    if (seconds < 0)
        throw std::runtime_error("LICENSE_TIME_INVALID: " + std::string(field));
    return std::chrono::system_clock::from_time_t(seconds);
}

std::vector<unsigned char> rawEcdsaSignatureToDer(
    const std::vector<unsigned char> &signature)
{
    if (signature.size() != 64)
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");
    using SignaturePtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
    SignaturePtr parsed(ECDSA_SIG_new(), ECDSA_SIG_free);
    BIGNUM *r = BN_bin2bn(signature.data(), 32, nullptr);
    BIGNUM *s = BN_bin2bn(signature.data() + 32, 32, nullptr);
    if (!parsed || !r || !s || ECDSA_SIG_set0(parsed.get(), r, s) != 1)
    {
        BN_free(r);
        BN_free(s);
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");
    }
    const int length = i2d_ECDSA_SIG(parsed.get(), nullptr);
    if (length <= 0)
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");
    std::vector<unsigned char> der(static_cast<std::size_t>(length));
    auto *cursor = der.data();
    if (i2d_ECDSA_SIG(parsed.get(), &cursor) != length)
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");
    return der;
}

bool verifyEs256(const Json::Value &jwk,
                 const std::vector<unsigned char> &payload,
                 const std::vector<unsigned char> &rawSignature)
{
    if (jwk["kty"].asString() != "EC" || jwk["crv"].asString() != "P-256")
        throw std::runtime_error("LICENSE_PUBLIC_KEY_INVALID");
    const auto x = decodeBase64Url(jwk["x"].asString());
    const auto y = decodeBase64Url(jwk["y"].asString());
    if (x.size() != 32 || y.size() != 32)
        throw std::runtime_error("LICENSE_PUBLIC_KEY_INVALID");

    using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)>;
    using BigNumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
    using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using DigestPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    EcKeyPtr ec(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), EC_KEY_free);
    BigNumPtr xCoordinate(BN_bin2bn(x.data(), static_cast<int>(x.size()), nullptr), BN_free);
    BigNumPtr yCoordinate(BN_bin2bn(y.data(), static_cast<int>(y.size()), nullptr), BN_free);
    if (!ec || !xCoordinate || !yCoordinate ||
        EC_KEY_set_public_key_affine_coordinates(ec.get(), xCoordinate.get(), yCoordinate.get()) != 1)
        throw std::runtime_error("LICENSE_PUBLIC_KEY_INVALID");

    PkeyPtr key(EVP_PKEY_new(), EVP_PKEY_free);
    if (!key || EVP_PKEY_assign_EC_KEY(key.get(), ec.release()) != 1)
        throw std::runtime_error("LICENSE_PUBLIC_KEY_INVALID");
    const auto derSignature = rawEcdsaSignatureToDer(rawSignature);
    DigestPtr digest(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!digest || EVP_DigestVerifyInit(digest.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");
    return EVP_DigestVerify(digest.get(),
                            derSignature.data(),
                            derSignature.size(),
                            payload.data(),
                            payload.size()) == 1;
}

Json::Value requireObject(const Json::Value &parent,
                          const char *field,
                          std::string_view error)
{
    if (!parent.isMember(field) || !parent[field].isObject())
        throw std::runtime_error(std::string(error));
    return parent[field];
}

VerifiedLicense verifySignedLicense(const Json::Value &file,
                                    const Json::Value &publicKey,
                                    const std::string &expectedProduct,
                                    const std::optional<std::string> &expectedKey)
{
    if (file["version"].asInt() != 1 || file["alg"].asString() != "ES256")
        throw std::runtime_error("UNSUPPORTED_LICENSE_FORMAT");
    const auto payloadBytes = decodeBase64Url(file["payload"].asString());
    const auto signature = decodeBase64Url(file["signature"].asString());
    if (!verifyEs256(publicKey, payloadBytes, signature))
        throw std::runtime_error("LICENSE_SIGNATURE_INVALID");

    const std::string payloadText(payloadBytes.begin(), payloadBytes.end());
    const auto payload = parseJson(payloadText, "signed license payload");
    if (payload["product"].asString() != expectedProduct)
        throw std::runtime_error("LICENSE_PRODUCT_MISMATCH");
    if (payload["status"].asString() != "active")
        throw std::runtime_error("LICENSE_REVOKED");
    if (expectedKey && payload["license_key"].asString() != *expectedKey)
        throw std::runtime_error("LICENSE_KEY_MISMATCH");

    const auto now = std::chrono::system_clock::now();
    const auto startsAt = parseUtc(payload["starts_at"].asString(), "starts_at");
    const auto expiresAt = parseUtc(payload["expires_at"].asString(), "expires_at");
    const auto issuedAt = parseUtc(payload["issued_at"].asString(), "issued_at");
    const auto offlineUntil = parseUtc(payload["offline_until"].asString(), "offline_until");
    if (startsAt > now)
        throw std::runtime_error("LICENSE_NOT_STARTED");
    if (expiresAt <= now)
        throw std::runtime_error("LICENSE_EXPIRED");
    if (offlineUntil <= now)
        throw std::runtime_error("OFFLINE_CACHE_EXPIRED");
    if (offlineUntil > issuedAt + kMaximumOfflineWindow)
        throw std::runtime_error("OFFLINE_WINDOW_INVALID");
    return {payload["registrant_name"].asString(), payload["expires_at"].asString()};
}

HttpResult postJson(const std::string &apiUrl,
                    const std::string &path,
                    const Json::Value &body)
{
    HttpResult output;
    static const bool curlReady = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    if (!curlReady)
        return output;
    using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    using HeaderPtr = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
    CurlPtr client(curl_easy_init(), curl_easy_cleanup);
    HeaderPtr headers(curl_slist_append(nullptr, "content-type: application/json"),
                      curl_slist_free_all);
    if (!client || !headers)
        return output;
    const auto requestBody = compactJson(body);
    const auto url = apiUrl + path;
    curl_easy_setopt(client.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(client.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(client.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(client.get(), CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(client.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));
    curl_easy_setopt(client.get(), CURLOPT_TIMEOUT, static_cast<long>(kRequestTimeoutSeconds));
    curl_easy_setopt(client.get(), CURLOPT_WRITEFUNCTION,
                     +[](char *data, std::size_t size, std::size_t count, void *context) {
                         const auto bytes = size * count;
                         static_cast<std::string *>(context)->append(data, bytes);
                         return bytes;
                     });
    curl_easy_setopt(client.get(), CURLOPT_WRITEDATA, &output.body);
    output.transportSucceeded = curl_easy_perform(client.get()) == CURLE_OK;
    long status = 0;
    curl_easy_getinfo(client.get(), CURLINFO_RESPONSE_CODE, &status);
    output.status = static_cast<int>(status);
    return output;
}

HttpResult get(const std::string &apiUrl, const std::string &path)
{
    HttpResult output;
    static const bool curlReady = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    if (!curlReady)
        return output;
    using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    CurlPtr client(curl_easy_init(), curl_easy_cleanup);
    if (!client)
        return output;
    const auto url = apiUrl + path;
    curl_easy_setopt(client.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(client.get(), CURLOPT_TIMEOUT, static_cast<long>(kRequestTimeoutSeconds));
    curl_easy_setopt(client.get(), CURLOPT_WRITEFUNCTION,
                     +[](char *data, std::size_t size, std::size_t count, void *context) {
                         const auto bytes = size * count;
                         static_cast<std::string *>(context)->append(data, bytes);
                         return bytes;
                     });
    curl_easy_setopt(client.get(), CURLOPT_WRITEDATA, &output.body);
    output.transportSucceeded = curl_easy_perform(client.get()) == CURLE_OK;
    long status = 0;
    curl_easy_getinfo(client.get(), CURLINFO_RESPONSE_CODE, &status);
    output.status = static_cast<int>(status);
    return output;
}

std::string apiError(const HttpResult &result)
{
    try
    {
        const auto body = parseJson(result.body, "KeyManager error");
        const auto code = body["error"]["code"].asString();
        const auto message = body["error"]["message"].asString();
        if (!code.empty())
            return code + (message.empty() ? "" : ": " + message);
    }
    catch (...)
    {
    }
    return "KEY_MANAGER_HTTP_" + std::to_string(result.status);
}

#ifdef _WIN32
constexpr wchar_t kRegistryPath[] = L"Software\\ThimPOS";
constexpr wchar_t kRegistryValue[] = L"LicenseCache";
constexpr char kDpapiEntropy[] = "ThimPOS license cache v1";

struct LocalMemoryDeleter
{
    void operator()(unsigned char *value) const
    {
        if (value)
            LocalFree(value);
    }
};

std::optional<std::string> readStoredCache()
{
    HKEY key = nullptr;
    const auto opened = RegOpenKeyExW(
        HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key);
    if (opened == ERROR_FILE_NOT_FOUND)
        return std::nullopt;
    if (opened != ERROR_SUCCESS)
        throw std::runtime_error("LICENSE_STORAGE_READ_FAILED");
    std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> registryKey(
        key, RegCloseKey);

    DWORD type = 0;
    DWORD size = 0;
    auto status = RegQueryValueExW(
        registryKey.get(), kRegistryValue, nullptr, &type, nullptr, &size);
    if (status == ERROR_FILE_NOT_FOUND)
        return std::nullopt;
    if (status != ERROR_SUCCESS || type != REG_BINARY || size == 0)
        throw std::runtime_error("LICENSE_STORAGE_READ_FAILED");
    std::vector<unsigned char> encrypted(size);
    status = RegQueryValueExW(registryKey.get(),
                              kRegistryValue,
                              nullptr,
                              &type,
                              encrypted.data(),
                              &size);
    if (status != ERROR_SUCCESS)
        throw std::runtime_error("LICENSE_STORAGE_READ_FAILED");

    DATA_BLOB input{static_cast<DWORD>(encrypted.size()), encrypted.data()};
    DATA_BLOB entropy{static_cast<DWORD>(sizeof(kDpapiEntropy) - 1),
                      reinterpret_cast<BYTE *>(const_cast<char *>(kDpapiEntropy))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(
            &input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        throw std::runtime_error("LICENSE_STORAGE_DECRYPT_FAILED");
    std::unique_ptr<unsigned char, LocalMemoryDeleter> decrypted(output.pbData);
    return std::string(reinterpret_cast<const char *>(output.pbData), output.cbData);
}

void writeStoredCache(const std::string &plainText)
{
    DATA_BLOB input{static_cast<DWORD>(plainText.size()),
                    reinterpret_cast<BYTE *>(const_cast<char *>(plainText.data()))};
    DATA_BLOB entropy{static_cast<DWORD>(sizeof(kDpapiEntropy) - 1),
                      reinterpret_cast<BYTE *>(const_cast<char *>(kDpapiEntropy))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input,
                          L"ThimPOS License",
                          &entropy,
                          nullptr,
                          nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN,
                          &output))
        throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
    std::unique_ptr<unsigned char, LocalMemoryDeleter> encrypted(output.pbData);

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        kRegistryPath,
                        0,
                        nullptr,
                        0,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS)
        throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
    std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> registryKey(
        key, RegCloseKey);
    if (RegSetValueExW(registryKey.get(),
                       kRegistryValue,
                       0,
                       REG_BINARY,
                       output.pbData,
                       output.cbData) != ERROR_SUCCESS)
        throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
}

void deleteStoredCache()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_SET_VALUE, &key) !=
        ERROR_SUCCESS)
        return;
    std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> registryKey(
        key, RegCloseKey);
    RegDeleteValueW(registryKey.get(), kRegistryValue);
}
#else
std::filesystem::path cachePath()
{
    const auto *user = getpwuid(getuid());
    if (!user || !user->pw_dir)
        throw std::runtime_error("LICENSE_STORAGE_READ_FAILED");
    return std::filesystem::path(user->pw_dir) / ".thimpos-license.json";
}

std::optional<std::string> readStoredCache()
{
    std::ifstream input(cachePath(), std::ios::binary);
    if (!input)
        return std::nullopt;
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof())
        throw std::runtime_error("LICENSE_STORAGE_READ_FAILED");
    return contents.str();
}

void writeStoredCache(const std::string &plainText)
{
    const auto path = cachePath();
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
        output << plainText;
        if (!output)
            throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
    }
    chmod(temporary.c_str(), S_IRUSR | S_IWUSR);
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
    if (error)
        throw std::runtime_error("LICENSE_STORAGE_WRITE_FAILED");
}

void deleteStoredCache()
{
    std::error_code error;
    std::filesystem::remove(cachePath(), error);
}
#endif

std::optional<Cache> loadCache()
{
    const auto stored = readStoredCache();
    if (!stored)
        return std::nullopt;
    Json::Value root;
    try
    {
        root = parseJson(*stored, "license cache");
    }
    catch (...)
    {
        throw std::runtime_error("LICENSE_CACHE_INVALID");
    }
    const auto key = trim(root["key"].asString());
    const auto deviceId = trim(root["device_id"].asString());
    if (key.empty() || deviceId.empty())
        throw std::runtime_error("LICENSE_CACHE_INVALID");
    return Cache{root, key, deviceId};
}

void saveCache(const Json::Value &cache)
{
    writeStoredCache(compactJson(cache));
}

std::string promptForKey(std::string_view prompt)
{
    std::cout << '\n' << prompt << "\nLicense key: " << std::flush;
    std::string key;
    if (!std::getline(std::cin, key) || trim(key).empty())
        throw std::runtime_error("LICENSE_KEY_REQUIRED");
    return trim(std::move(key));
}

std::string generateDeviceId()
{
    std::array<unsigned char, 16> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
        throw std::runtime_error("DEVICE_ID_GENERATION_FAILED");
    std::ostringstream output;
    output << "thimpos-" << std::hex << std::setfill('0');
    for (const auto byte : bytes)
        output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}
}  // namespace

KeyManagerClient::KeyManagerClient()
    : apiUrl_(kApiUrl), product_("thimpos")
{
}

VerifiedLicense KeyManagerClient::verifyAtStartup() const
{
    std::optional<Cache> cache;
    try
    {
        cache = loadCache();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Cảnh báo bản quyền: " << formatLicenseError(error.what()) << '\n';
        deleteStoredCache();
    }

    std::string requestedKey =
        cache ? cache->key
              : promptForKey("ThimPOS chưa được kích hoạt. Vui lòng nhập key bản quyền được cấp.");
    const auto deviceId = cache ? cache->deviceId : generateDeviceId();

    const auto useOfflineCache = [&]() -> VerifiedLicense {
        if (cache && cache->key == requestedKey)
            return verifySignedLicense(
                requireObject(cache->root, "license_file", "LICENSE_CACHE_INVALID"),
                requireObject(cache->root, "public_key", "LICENSE_CACHE_INVALID"),
                product_,
                requestedKey);
        throw std::runtime_error("KEY_MANAGER_UNREACHABLE");
    };

    while (true)
    {
        const auto keyResponse = get(apiUrl_, "/v1/public-key");
        if (!keyResponse.transportSucceeded || keyResponse.status >= 500)
            return useOfflineCache();
        if (keyResponse.status < 200 || keyResponse.status >= 300)
            throw std::runtime_error(apiError(keyResponse));

        Json::Value activationBody;
        activationBody["key"] = requestedKey;
        activationBody["device_id"] = deviceId;
        const auto activationResponse =
            postJson(apiUrl_, "/v1/licenses/activate", activationBody);
        if (!activationResponse.transportSucceeded || activationResponse.status >= 500)
            return useOfflineCache();
        if (activationResponse.status < 200 || activationResponse.status >= 300)
        {
            const auto error = apiError(activationResponse);
            std::cerr << "\nKey không được chấp nhận: " << formatLicenseError(error) << '\n';
            deleteStoredCache();
            cache.reset();
            requestedKey = promptForKey(
                "Vui lòng nhập key bản quyền khác, hoặc để trống để thoát.");
            continue;
        }

        const auto keyJson = parseJson(keyResponse.body, "public key response");
        const auto activationJson = parseJson(activationResponse.body, "activation response");
        const auto publicKey =
            requireObject(keyJson, "public_key", "LICENSE_PUBLIC_KEY_INVALID");
        const auto licenseFile =
            requireObject(activationJson, "license_file", "LICENSE_ACTIVATION_INVALID");
        const auto verified =
            verifySignedLicense(licenseFile, publicKey, product_, requestedKey);

        Json::Value newCache;
        newCache["key"] = requestedKey;
        newCache["device_id"] = deviceId;
        newCache["session_id"] = activationJson["session_id"].asString();
        newCache["public_key"] = publicKey;
        newCache["license_file"] = licenseFile;
        saveCache(newCache);
        return verified;
    }
}

std::string formatLicenseError(std::string_view technicalError)
{
    const auto separator = technicalError.find_first_of(": (");
    const std::string code(technicalError.substr(0, separator));
    std::string message;
    if (code == "LICENSE_KEY_REQUIRED")
        message = "Chưa có key bản quyền. Hãy chạy lại và nhập key được cấp.";
    else if (code == "LICENSE_NOT_FOUND")
        message = "Key không tồn tại hoặc đã nhập sai.";
    else if (code == "LICENSE_REVOKED")
        message = "Key đã bị thu hồi. Vui lòng liên hệ nơi cấp bản quyền.";
    else if (code == "LICENSE_NOT_STARTED")
        message = "Bản quyền chưa đến ngày bắt đầu sử dụng.";
    else if (code == "LICENSE_EXPIRED")
        message = "Bản quyền đã hết hạn. Vui lòng gia hạn để tiếp tục sử dụng.";
    else if (code == "LOGIN_LIMIT_REACHED")
        message = "Key đã đạt giới hạn số lần kích hoạt.";
    else if (code == "SESSION_LIMIT_REACHED")
        message = "Key đã đạt giới hạn thiết bị hoặc phiên đang hoạt động.";
    else if (code == "LICENSE_PRODUCT_MISMATCH")
        message = "Key này không dành cho sản phẩm ThimPOS.";
    else if (code == "OFFLINE_CACHE_EXPIRED")
        message = "Thời gian sử dụng offline đã hết. Hãy kết nối Internet rồi mở lại ThimPOS.";
    else if (code == "KEY_MANAGER_UNREACHABLE")
        message = "Không thể kết nối máy chủ bản quyền và chưa có dữ liệu offline hợp lệ. Hãy kiểm tra Internet.";
    else if (code == "LICENSE_SIGNATURE_INVALID" ||
             code == "LICENSE_PUBLIC_KEY_INVALID" ||
             code == "LICENSE_KEY_MISMATCH" || code == "LICENSE_CACHE_INVALID" ||
             code == "UNSUPPORTED_LICENSE_FORMAT" ||
             code == "OFFLINE_WINDOW_INVALID" || code == "LICENSE_BASE64_INVALID" ||
             code == "LICENSE_TIME_INVALID")
        message = "Dữ liệu bản quyền không hợp lệ hoặc đã bị thay đổi.";
    else if (code == "LICENSE_STORAGE_READ_FAILED" ||
             code == "LICENSE_STORAGE_DECRYPT_FAILED")
        message = "Không đọc được dữ liệu bản quyền đã lưu trên máy. Hệ thống sẽ yêu cầu nhập lại key.";
    else if (code == "LICENSE_STORAGE_WRITE_FAILED")
        message = "Không thể lưu dữ liệu bản quyền trên máy. Hãy kiểm tra quyền của tài khoản đang chạy.";
    else if (code == "DEVICE_ID_GENERATION_FAILED")
        message = "Không thể tạo mã nhận diện thiết bị.";
    else
        message = "Không thể xác minh bản quyền. Vui lòng liên hệ hỗ trợ.";
    return message + " [" + (code.empty() ? "LICENSE_ERROR" : code) + "]";
}
}  // namespace thimpos::license
