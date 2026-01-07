#include "protocol/ResponseParser.h"
#include <sstream>
#include <algorithm>

ResponseParser::Response ResponseParser::parse(const std::string& rawResponse) {
    Response resp;
    resp.rawResponse = rawResponse;

    if (rawResponse.empty()) {
        resp.success = false;
        resp.message = "Empty response";
        return resp;
    }

    // 判断是否成功
    if (rawResponse.substr(0, 3) == "OK:") {
        resp.success = true;
        resp.message = rawResponse.substr(3);
    } else if (rawResponse.substr(0, 6) == "ERROR:") {
        resp.success = false;
        resp.message = rawResponse.substr(6);
    } else {
        // 未知格式
        resp.success = false;
        resp.message = rawResponse;
    }

    // 去除前导空格
    size_t start = resp.message.find_first_not_of(" \t\n\r");
    if (start != std::string::npos) {
        resp.message = resp.message.substr(start);
    }

    // 解析键值对（如ROLE=ADMIN）
    parseKeyValuePairs(resp.message, resp.data);

    return resp;
}

std::string ResponseParser::extractToken(const Response& response) {
    if (!response.success) {
        return "";
    }

    // LOGIN响应格式: "OK: <token> ROLE=..."
    // 提取第一个单词作为token
    std::istringstream iss(response.message);
    std::string token;
    iss >> token;

    // 检查token是否包含ROLE=（如果包含说明解析错误）
    if (token.find("ROLE=") != std::string::npos) {
        return "";
    }

    return token;
}

std::string ResponseParser::extractRole(const Response& response) {
    auto it = response.data.find("ROLE");
    if (it != response.data.end()) {
        return it->second;
    }
    return "";
}

void ResponseParser::parseKeyValuePairs(const std::string& text, std::map<std::string, std::string>& data) {
    std::istringstream iss(text);
    std::string word;

    while (iss >> word) {
        size_t pos = word.find('=');
        if (pos != std::string::npos) {
            std::string key = word.substr(0, pos);
            std::string value = word.substr(pos + 1);
            data[key] = value;
        }
    }
}

