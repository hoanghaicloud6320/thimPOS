#pragma once

#include <drogon/HttpRequest.h>
#include <json/json.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

inline void writeAuditLog(const std::string &message)
{
    const auto logger = spdlog::get("audit");
    if (logger)
        logger->warn("[!!! THAY ĐỔI DỮ LIỆU !!!] {}", message);
}

inline std::string changeActor(const drogon::HttpRequestPtr &request)
{
    std::string username = "unknown", role = "unknown";
    try { username = request->attributes()->get<std::string>("username"); } catch (...) {}
    try { role = request->attributes()->get<std::string>("role"); } catch (...) {}
    return username + " (" + role + ")";
}

inline std::string auditJsonValue(const Json::Value &value)
{
    if (value.isNull()) return "<đã xóa>";
    if (value.isString()) return "\"" + value.asString() + "\"";
    if (value.isBool()) return value.asBool() ? "true" : "false";
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

inline void writeChangeAudit(const drogon::HttpRequestPtr &request,
                             const std::string &action,
                             const std::string &target,
                             const Json::Value &before,
                             const Json::Value &after,
                             const std::vector<std::string> &editableFields)
{
    std::string diff;
    for (const auto &field : editableFields)
    {
        const auto oldValue = before.isMember(field) ? before[field] : Json::Value();
        const auto newValue = after.isMember(field) ? after[field] : Json::Value();
        if (oldValue == newValue) continue;
        if (!diff.empty()) diff += "; ";
        diff += field + ": " + auditJsonValue(oldValue) + " -> " + auditJsonValue(newValue);
    }
    if (diff.empty()) return;
    writeAuditLog(changeActor(request) + " | " + action + " | " + target +
                  " | DIFF { " + diff + " }");
}
