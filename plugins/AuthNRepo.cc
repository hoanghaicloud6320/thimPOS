#include "AuthNRepo.h"
#include <drogon/drogon.h>

using namespace drogon;

void AuthNRepo::init() {
    auto db = app().getDbClient();

    const auto credentialsTable = db->execSqlSync(
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' AND name = 'credentials'");
    const bool shouldCreateBootstrapCredential = credentialsTable.empty();

    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS credentials ("
        "username TEXT PRIMARY KEY, "
        "password TEXT NOT NULL);"
    );

    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "token TEXT PRIMARY KEY, "
        "username TEXT NOT NULL, "
        "expire_at TIMESTAMP NOT NULL);"
    );

    if (shouldCreateBootstrapCredential) {
        db->execSqlSync(
            "INSERT INTO credentials (username, password) "
            "VALUES ('admin', 'mmbbmg')"
        );
    }
}

Task<std::optional<std::string>> AuthNRepo::login(std::string username, std::string password) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "SELECT username FROM credentials WHERE username = $1 AND password = $2",
        username, password
    );

    if (result.empty()) {
        co_return std::nullopt;
    }

    std::string token = utils::getUuid();

    co_await db->execSqlCoro(
        "INSERT INTO sessions (token, username, expire_at) "
        "VALUES ($1, $2, datetime('now', '+30 day'))",
        token, username
    );

    co_return token;
}

Task<std::optional<std::string>> AuthNRepo::getUserByToken(std::string token) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "SELECT username FROM sessions WHERE token = $1 AND expire_at > datetime('now')",
        token
    );

    if (result.empty()) {
        co_return std::nullopt;
    }

    co_return result[0]["username"].as<std::string>();
}

Task<bool> AuthNRepo::insertUser(std::string username, std::string password) {
    auto db = app().getDbClient();
    try {
        co_await db->execSqlCoro(
            "INSERT INTO credentials (username, password) VALUES ($1, $2)",
            username, password
        );
        co_return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Insert user failed: " << e.what();
        co_return false;
    }
}

Task<bool> AuthNRepo::deleteUser(std::string username) {
    auto db = app().getDbClient();

    co_await db->execSqlCoro("DELETE FROM sessions WHERE username = $1", username);
    auto result = co_await db->execSqlCoro("DELETE FROM credentials WHERE username = $1", username);

    co_return result.affectedRows() > 0;
}

Task<bool> AuthNRepo::updatePassword(std::string username, std::string password) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "UPDATE credentials SET password = $1 WHERE username = $2",
        password, username
    );

    co_return result.affectedRows() > 0;
}

Task<bool> AuthNRepo::invalidateSession(std::string token) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "DELETE FROM sessions WHERE token = $1",
        token
    );

    co_return result.affectedRows() > 0;
}

Task<int> AuthNRepo::invalidateAllSessions(std::string username) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "DELETE FROM sessions WHERE username = $1",
        username
    );

    co_return static_cast<int>(result.affectedRows());
}

Task<std::vector<std::string>> AuthNRepo::getSessionByUser(std::string username) {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "SELECT token FROM sessions WHERE username = $1 AND expire_at > datetime('now')",
        username
    );

    std::vector<std::string> tokens;
    for (const auto& row : result) {
        tokens.push_back(row["token"].as<std::string>());
    }

    co_return tokens;
}

Task<std::vector<std::string>> AuthNRepo::getAllUsers() {
    auto db = app().getDbClient();

    auto result = co_await db->execSqlCoro(
        "SELECT username FROM credentials"
    );

    std::vector<std::string> users;
    users.reserve(result.size());

    for (const auto& row : result) {
        users.push_back(row["username"].as<std::string>());
    }

    co_return users;
}
