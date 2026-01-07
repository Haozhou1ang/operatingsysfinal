#include "network/NetworkClient.h"
#include "session/SessionManager.h"
#include "cli/CLIInterface.h"
#include <iostream>
#include <cstring>

void printUsage(const char* programName) {
    std::cout << "用法: " << programName << " [选项]\n";
    std::cout << "选项:\n";
    std::cout << "  -h, --host <地址>    服务器地址 (默认: localhost)\n";
    std::cout << "  -p, --port <端口>    服务器端口 (默认: 8080)\n";
    std::cout << "  --help               显示此帮助信息\n";
    std::cout << "\n示例:\n";
    std::cout << "  " << programName << "\n";
    std::cout << "  " << programName << " -h 192.168.1.100 -p 9000\n";
}

int main(int argc, char* argv[]) {
    // 默认配置
    std::string host = "localhost";
    int port = 8080;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "错误: " << arg << " 需要一个参数\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[++i]);
                    if (port <= 0 || port > 65535) {
                        std::cerr << "错误: 端口号必须在 1-65535 之间\n";
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "错误: 无效的端口号\n";
                    return 1;
                }
            } else {
                std::cerr << "错误: " << arg << " 需要一个参数\n";
                printUsage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "错误: 未知选项 " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // 创建各层对象
    NetworkClient network;
    SessionManager session(&network);
    CLIInterface cli(&network, &session);

    // 设置服务器地址
    cli.setServerAddress(host, port);

    // 运行CLI主循环
    try {
        cli.run();
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

