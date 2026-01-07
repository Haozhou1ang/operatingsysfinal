#pragma once

#include <string>

// 前向声明
class NetworkClient;
class CommandBuilder;
class ResponseParser;

/**
 * 用户角色枚举
 */
enum class UserRole {
    ADMIN,
    EDITOR,
    REVIEWER,
    AUTHOR,
    GUEST,
    UNKNOWN
};

/**
 * 会话管理器
 * 负责维护登录状态、token管理
 */
class SessionManager {
public:
    SessionManager(NetworkClient* network);
    ~SessionManager();

    /**
     * 登录
     * @param username 用户名
     * @param password 密码
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool login(const std::string& username, const std::string& password, std::string& errorMsg);

    /**
     * 登出
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool logout(std::string& errorMsg);

    /**
     * 获取当前token
     */
    std::string getCurrentToken() const;

    /**
     * 获取当前用户名
     */
    std::string getCurrentUsername() const;

    /**
     * 获取当前角色
     */
    UserRole getCurrentRole() const;

    /**
     * 获取角色字符串
     */
    std::string getRoleString() const;

    /**
     * 检查是否已登录
     */
    bool isLoggedIn() const;

    /**
     * 清除会话信息
     */
    void clearSession();

    /**
     * 角色转字符串
     */
    static std::string roleToString(UserRole role);

    /**
     * 字符串转角色
     */
    static UserRole stringToRole(const std::string& roleStr);

private:
    NetworkClient* network_;
    std::string sessionToken_;
    std::string username_;
    UserRole role_;
    bool loggedIn_;
};

