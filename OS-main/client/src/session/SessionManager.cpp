#include "session/SessionManager.h"
#include "network/NetworkClient.h"
#include "protocol/CommandBuilder.h"
#include "protocol/ResponseParser.h"
#include <algorithm>
#include <cctype>

SessionManager::SessionManager(NetworkClient* network)
    : network_(network), sessionToken_(""), username_(""), 
      role_(UserRole::UNKNOWN), loggedIn_(false) {
}

SessionManager::~SessionManager() {
    // 如果还在登录状态，尝试登出
    if (loggedIn_) {
        std::string errorMsg;
        logout(errorMsg);
    }
}

bool SessionManager::login(const std::string& username, const std::string& password, std::string& errorMsg) {
    if (loggedIn_) {
        errorMsg = "Already logged in as " + username_;
        return false;
    }

    // 构建登录命令
    std::string command = CommandBuilder::buildLogin(username, password);
    std::string response;

    // 发送命令并接收响应
    if (!network_->sendAndReceive(command, response, errorMsg)) {
        return false;
    }

    // 解析响应
    ResponseParser::Response resp = ResponseParser::parse(response);
    if (!resp.success) {
        errorMsg = resp.message;
        return false;
    }

    // 提取token和角色
    sessionToken_ = ResponseParser::extractToken(resp);
    std::string roleStr = ResponseParser::extractRole(resp);

    if (sessionToken_.empty()) {
        errorMsg = "Failed to extract session token from response";
        return false;
    }

    username_ = username;
    role_ = stringToRole(roleStr);
    loggedIn_ = true;

    return true;
}

bool SessionManager::logout(std::string& errorMsg) {
    if (!loggedIn_) {
        errorMsg = "Not logged in";
        return false;
    }

    // 构建登出命令
    std::string command = CommandBuilder::buildLogout(sessionToken_);
    std::string response;

    // 发送命令并接收响应
    if (!network_->sendAndReceive(command, response, errorMsg)) {
        // 即使网络失败，也清除本地会话
        clearSession();
        return false;
    }

    // 解析响应
    ResponseParser::Response resp = ResponseParser::parse(response);
    
    // 清除会话信息
    clearSession();

    if (!resp.success) {
        errorMsg = resp.message;
        return false;
    }

    return true;
}

std::string SessionManager::getCurrentToken() const {
    return sessionToken_;
}

std::string SessionManager::getCurrentUsername() const {
    return username_;
}

UserRole SessionManager::getCurrentRole() const {
    return role_;
}

std::string SessionManager::getRoleString() const {
    return roleToString(role_);
}

bool SessionManager::isLoggedIn() const {
    return loggedIn_;
}

void SessionManager::clearSession() {
    sessionToken_.clear();
    username_.clear();
    role_ = UserRole::UNKNOWN;
    loggedIn_ = false;
}

std::string SessionManager::roleToString(UserRole role) {
    switch (role) {
        case UserRole::ADMIN: return "ADMIN";
        case UserRole::EDITOR: return "EDITOR";
        case UserRole::REVIEWER: return "REVIEWER";
        case UserRole::AUTHOR: return "AUTHOR";
        case UserRole::GUEST: return "GUEST";
        default: return "UNKNOWN";
    }
}

UserRole SessionManager::stringToRole(const std::string& roleStr) {
    std::string upper = roleStr;
    std::transform(upper.begin(), upper.end(), upper.begin(), 
                   [](unsigned char c) { return std::toupper(c); });

    if (upper == "ADMIN") return UserRole::ADMIN;
    if (upper == "EDITOR") return UserRole::EDITOR;
    if (upper == "REVIEWER") return UserRole::REVIEWER;
    if (upper == "AUTHOR") return UserRole::AUTHOR;
    if (upper == "GUEST") return UserRole::GUEST;
    return UserRole::UNKNOWN;
}

