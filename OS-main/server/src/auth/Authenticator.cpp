#include "../../include/auth/Authenticator.h"
#include <iostream>
#include <memory>

#include <chrono>
#include <algorithm>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

using Clock = std::chrono::steady_clock;

struct UserRecord {
    std::string password;
    UserRole role;
};

struct SessionRecord {
    std::string username;
    UserRole role;
    Clock::time_point expiresAt;
};

constexpr std::chrono::minutes kSessionTtl{120};

std::string generateToken() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << std::hex;
    oss << dist(rng);
    oss << dist(rng);
    return oss.str();
}

}

class RealAuthenticator : public Authenticator {
public:
    std::string login(const std::string& username, const std::string& password, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "Unknown user.";
            return {};
        }

        if (it->second.password != password) {
            errorMsg = "Invalid password.";
            return {};
        }

        const std::string token = generateToken();
        m_sessions[token] = SessionRecord{
            username,
            it->second.role,
            Clock::now() + kSessionTtl,
        };

        return token;
    }

    bool validateSession(const std::string& sessionId, std::string& username, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) {
            errorMsg = "Session not found.";
            return false;
        }

        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            errorMsg = "Session expired.";
            return false;
        }

        // sliding expiration：每次验证都续期
        it->second.expiresAt = Clock::now() + kSessionTtl;
        username = it->second.username;
        return true;
    }

    bool logout(const std::string& sessionId, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) {
            errorMsg = "Session not found.";
            return false;
        }

        m_sessions.erase(it);
        return true;
    }

    UserRole getUserRole(const std::string& sessionToken) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionToken);
        if (it == m_sessions.end()) {
            return UserRole::UNKNOWN;
        }

        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            return UserRole::UNKNOWN;
        }

        return it->second.role;
    }

    bool addUser(const std::string& username,
                 const std::string& password,
                 UserRole role,
                 std::string& errorMsg) override {
        if (username.empty() || password.empty()) {
            errorMsg = "username/password is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        if (m_users.find(username) != m_users.end()) {
            errorMsg = "User already exists.";
            return false;
        }
        m_users[username] = UserRecord{password, role};
        return true;
    }

    bool deleteUser(const std::string& username, std::string& errorMsg) override {
        if (username.empty()) {
            errorMsg = "username is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "User not found.";
            return false;
        }

        m_users.erase(it);

        // 清理该用户的会话
        for (auto sit = m_sessions.begin(); sit != m_sessions.end();) {
            if (sit->second.username == username) sit = m_sessions.erase(sit);
            else ++sit;
        }

        return true;
    }

    std::vector<std::pair<std::string, UserRole>> listUsers(std::string& errorMsg) override {
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        std::vector<std::pair<std::string, UserRole>> out;
        out.reserve(m_users.size());
        for (const auto& [name, rec] : m_users) {
            out.emplace_back(name, rec.role);
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        return out;
    }

    bool userExists(const std::string& username) override {
        std::scoped_lock lock(m_mutex);
        return m_users.find(username) != m_users.end();
    }

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, UserRecord> m_users{
        // 先用硬编码账号便于联调；后续可替换为从 FSProtocol 读取用户/角色文件
        {"admin", {"admin123", UserRole::ADMIN}},
        {"editor", {"editor123", UserRole::EDITOR}},
        {"reviewer", {"reviewer123", UserRole::REVIEWER}},
        {"author", {"author123", UserRole::AUTHOR}},
        {"guest", {"guest", UserRole::GUEST}},
    };
    std::unordered_map<std::string, SessionRecord> m_sessions;
};

// 【工厂函数实现】
// 定义 createAuthenticator 函数，这是链接器正在寻找的函数
std::unique_ptr<Authenticator> createAuthenticator() {
    return std::make_unique<RealAuthenticator>();
}