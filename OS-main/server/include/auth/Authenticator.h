#pragma once
#include <string>
#include <vector>

// 定义用户角色
enum class UserRole {
    ADMIN,
    EDITOR,
    REVIEWER,
    AUTHOR,
    GUEST,
    UNKNOWN
};

// 【身份认证占位】不填充技术细节
class Authenticator {
public:
    // 登录：返回会话令牌 (Session ID)
    virtual std::string login(const std::string& username, 
                                            const std::string& password, 
                                            std::string& errorMsg) = 0;
    
    // 验证会话有效性
    virtual bool validateSession(const std::string& sessionId, 
                               std::string& username, 
                               std::string& errorMsg) = 0;
    
    // 注销会话
    virtual bool logout(const std::string& sessionId, std::string& errorMsg) = 0;

    // 获取用户角色
    virtual UserRole getUserRole(const std::string& sessionToken) = 0;

    // --- 管理员：用户管理 ---
    virtual bool addUser(const std::string& username,
                         const std::string& password,
                         UserRole role,
                         std::string& errorMsg) = 0;

    virtual bool deleteUser(const std::string& username, std::string& errorMsg) = 0;

    virtual std::vector<std::pair<std::string, UserRole>> listUsers(std::string& errorMsg) = 0;

    // 检查用户是否存在
    virtual bool userExists(const std::string& username) = 0;
};