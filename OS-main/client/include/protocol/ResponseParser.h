#pragma once

#include <string>
#include <map>

/**
 * 响应解析器
 * 负责解析Server返回的响应字符串
 */
class ResponseParser {
public:
    /**
     * 响应结构体
     */
    struct Response {
        bool success;                           // 是否成功（OK: 或 ERROR:）
        std::string message;                    // 主要消息内容
        std::map<std::string, std::string> data; // 结构化数据（如ROLE=ADMIN）
        std::string rawResponse;                // 原始响应

        Response() : success(false) {}
    };

    /**
     * 解析响应字符串
     * @param rawResponse 原始响应
     * @return 解析后的Response对象
     */
    static Response parse(const std::string& rawResponse);

    /**
     * 从响应中提取token（用于LOGIN响应）
     * @param response 响应对象
     * @return token字符串，失败返回空
     */
    static std::string extractToken(const Response& response);

    /**
     * 从响应中提取角色（用于LOGIN响应）
     * @param response 响应对象
     * @return 角色字符串，失败返回空
     */
    static std::string extractRole(const Response& response);

private:
    // 辅助函数：解析键值对（如ROLE=ADMIN）
    static void parseKeyValuePairs(const std::string& text, std::map<std::string, std::string>& data);
};

