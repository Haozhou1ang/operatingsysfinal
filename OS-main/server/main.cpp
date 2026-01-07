#include "include/protocol/ProtocolFactory.h"
#include "include/platform/socket_compat.h"
#include "include/platform/ThreadPool.h"
#include <thread>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

// 客户端处理函数，将在线程池中运行
void HandleClientConnection(socket_t clientSocket) {
    std::cout << "Thread " << std::this_thread::get_id() << ": Handling new client on socket " << clientSocket << std::endl;
    ProtocolFactory::handleRequest(clientSocket);
    
    // 优雅地关闭连接
    shutdown(clientSocket, SHUTDOWN_SEND);
    CLOSE_SOCKET(clientSocket);
    std::cout << "Thread " << std::this_thread::get_id() << ": Connection closed for socket " << clientSocket << std::endl;
}

class Server {
public:
    // 构造函数：初始化线程池
    // 参数：
    // - numThreads: 工作线程数量（默认：硬件并发数，最大50）
    // - maxQueueSize: 最大任务队列大小（默认：100）
    Server(size_t numThreads = 0, size_t maxQueueSize = 100) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }
        // 限制最大线程数为50，避免资源耗尽
        if (numThreads > 50) {
            numThreads = 50;
        }
        m_threadPool = std::make_unique<ThreadPool>(numThreads, maxQueueSize);
    }

    ~Server() {
        // 先关闭线程池，等待所有任务完成
        if (m_threadPool) {
            m_threadPool->shutdown();
        }
        if (m_listenSocket != INVALID_SOCKET) CLOSE_SOCKET(m_listenSocket);
        WSACleanup();
    }

    bool start(int port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed: " << get_socket_error_string() << std::endl;
            return false;
        }

        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << get_socket_error_string() << std::endl;
            return false;
        }
        
        // 设置 SO_REUSEADDR 选项（跨平台）
        int opt = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << get_socket_error_string() << std::endl;
            return false;
        }

        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << get_socket_error_string() << std::endl;
            return false;
        }

        std::cout << "Server listening on port " << port << "..." << std::endl;
        std::cout << "线程池大小: " << m_threadPool->poolSize() 
                  << ", 最大队列: " << 100 << std::endl;

        size_t rejectedConnections = 0;
        while (true) {
            socket_t clientSocket = accept(m_listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed: " << get_socket_error_string() << std::endl;
                continue;
            }
            
            // 将客户端处理任务提交到线程池
            bool enqueued = m_threadPool->enqueue([clientSocket]() {
                HandleClientConnection(clientSocket);
            });
            
            if (!enqueued) {
                // 线程池队列已满，拒绝连接
                rejectedConnections++;
                std::cerr << "服务器繁忙，拒绝连接 (累计拒绝: " << rejectedConnections << ")" << std::endl;
                
                // 发送错误响应并关闭连接
                const char* busyMsg = "ERROR|Server busy, please try again later\n";
                send(clientSocket, busyMsg, strlen(busyMsg), 0);
                CLOSE_SOCKET(clientSocket);
            } else {
                // 定期输出线程池状态
                static size_t acceptCount = 0;
                acceptCount++;
                if (acceptCount % 10 == 0) {
                    std::cout << "线程池状态 - 活跃: " << m_threadPool->activeThreads() 
                             << "/" << m_threadPool->poolSize() 
                             << ", 队列: " << m_threadPool->queueSize() << std::endl;
                }
            }
        }
        return true;
    }

private:
    socket_t m_listenSocket = INVALID_SOCKET;
    std::unique_ptr<ThreadPool> m_threadPool;
};

int main() {
#ifdef _WIN32
    // 让 Windows 控制台按 UTF-8 解释输出，避免中文变成“鍒濆...”
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    // 创建服务器
    // 参数1: 线程池大小（0表示使用硬件并发数）
    // 参数2: 最大任务队列大小
    Server server(0, 100);
    
    std::cout << "========================================" << std::endl;
    std::cout << "论文审稿系统服务器 v2.0" << std::endl;
    std::cout << "多线程安全版本 - 使用线程池" << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (!server.start(8080)) {
        std::cerr << "Failed to start the server." << std::endl;
        return 1;
    }
    return 0;
}