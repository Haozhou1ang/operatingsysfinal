#include "network/NetworkClient.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

NetworkClient::NetworkClient() 
    : socket_fd_(-1), connected_(false), host_(""), port_(0) {
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& host, int port, std::string& errorMsg) {
    if (connected_) {
        disconnect();
    }

    host_ = host;
    port_ = port;

    // 创建socket
    if (!createSocket(errorMsg)) {
        return false;
    }

    // 设置服务器地址
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // 转换IP地址或主机名
    // 先尝试作为IP地址解析
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        // 如果不是IP地址，尝试解析主机名
        struct hostent* he = gethostbyname(host.c_str());
        if (he == nullptr || he->h_addr_list[0] == nullptr) {
            errorMsg = "Invalid address or hostname: " + host;
            closeSocket();
            return false;
        }
        // 使用解析出的第一个IP地址
        std::memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    // 连接到服务器
    if (::connect(socket_fd_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        errorMsg = "Connection failed: " + std::string(std::strerror(errno));
        closeSocket();
        return false;
    }

    connected_ = true;
    return true;
}

bool NetworkClient::sendCommand(const std::string& command, std::string& errorMsg) {
    if (!connected_) {
        errorMsg = "Not connected to server";
        return false;
    }

    // 发送命令
    ssize_t sent = send(socket_fd_, command.c_str(), command.length(), 0);
    if (sent < 0) {
        errorMsg = "Send failed: " + std::string(std::strerror(errno));
        connected_ = false;
        return false;
    }

    // 关闭写端，告诉服务器发送完毕
    shutdown(socket_fd_, SHUT_WR);

    return true;
}

bool NetworkClient::receiveResponse(std::string& response, std::string& errorMsg) {
    if (!connected_) {
        errorMsg = "Not connected to server";
        return false;
    }

    response.clear();
    char buffer[4096];
    ssize_t received;

    // 循环接收直到连接关闭
    while ((received = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[received] = '\0';
        response += buffer;
    }

    if (received < 0) {
        errorMsg = "Receive failed: " + std::string(std::strerror(errno));
        connected_ = false;
        return false;
    }

    // 接收完毕后断开连接（server采用一次连接一次命令模式）
    disconnect();

    return true;
}

bool NetworkClient::sendAndReceive(const std::string& command, std::string& response, std::string& errorMsg) {
    // 每次都重新连接（因为server是一次连接一次命令）
    if (!connect(host_, port_, errorMsg)) {
        return false;
    }

    if (!sendCommand(command, errorMsg)) {
        disconnect();
        return false;
    }

    if (!receiveResponse(response, errorMsg)) {
        return false;
    }

    return true;
}

void NetworkClient::disconnect() {
    if (socket_fd_ >= 0) {
        closeSocket();
    }
    connected_ = false;
}

bool NetworkClient::isConnected() const {
    return connected_;
}

void NetworkClient::setDefaultServer(const std::string& host, int port) {
    host_ = host;
    port_ = port;
}

bool NetworkClient::createSocket(std::string& errorMsg) {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        errorMsg = "Socket creation failed: " + std::string(std::strerror(errno));
        return false;
    }
    return true;
}

void NetworkClient::closeSocket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

