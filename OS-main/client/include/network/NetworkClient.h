#pragma once

#include <string>

/**
 * 网络通信层
 * 负责与Server建立TCP连接，发送命令并接收响应
 */
class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    /**
     * 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool connect(const std::string& host, int port, std::string& errorMsg);

    /**
     * 发送命令到服务器
     * @param command 命令字符串
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool sendCommand(const std::string& command, std::string& errorMsg);

    /**
     * 接收服务器响应
     * @param response 响应内容输出
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool receiveResponse(std::string& response, std::string& errorMsg);

    /**
     * 发送命令并接收响应（组合操作）
     * @param command 命令字符串
     * @param response 响应内容输出
     * @param errorMsg 错误信息输出
     * @return 成功返回true
     */
    bool sendAndReceive(const std::string& command, std::string& response, std::string& errorMsg);

    /**
     * 断开连接
     */
    void disconnect();

    /**
     * 检查是否已连接
     */
    bool isConnected() const;

    /**
     * 设置默认服务器地址（用于后续连接）
     * @param host 服务器地址
     * @param port 服务器端口
     */
    void setDefaultServer(const std::string& host, int port);

private:
    int socket_fd_;
    bool connected_;
    std::string host_;
    int port_;

    // 内部辅助函数
    bool createSocket(std::string& errorMsg);
    void closeSocket();
};

