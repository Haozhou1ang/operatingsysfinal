#include "../../include/protocol/ProtocolFactory.h"
#include "../../include/protocol/CLIProtocol.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include "../../include/business/BackupFlow.h"
#include "../../include/business/PaperService.h"
#include "../../include/business/ReviewFlow.h"
#include "../../include/cache/CacheStatsProvider.h"
#include <memory>
#include <string>
#include <iostream>

// 声明在其他 .cpp 文件中定义的工厂函数
std::unique_ptr<FSProtocol> createFSProtocol();
std::unique_ptr<Authenticator> createAuthenticator();

// 【应用服务中心 - Composition Root】
// 这是所有主要组件被实例化的地方。我们使用单例模式确保整个应用只有一个服务实例集合。
class AppServices {
public:
    // 提供对所有服务和流程的访问
    Authenticator* getAuthenticator() { return authenticator.get(); }
    PermissionChecker* getPermissionChecker() { return permissionChecker.get(); }
    FSProtocol* getFSProtocol() { return fsProtocol.get(); }
    ICacheStatsProvider* getCacheStatsProvider() { return cacheStatsProvider; }
    BackupFlow* getBackupFlow() { return backupFlow.get(); }
    PaperService* getPaperService() { return paperService.get(); }
    ReviewFlow* getReviewFlow() { return reviewFlow.get(); }

    // 获取单例实例的静态方法
    static AppServices& instance() {
        static AppServices services;
        return services;
    }

private:
    // 私有构造函数，在此处完成所有对象的创建和依赖注入
    AppServices() {
        // 1. 创建核心服务
        fsProtocol = createFSProtocol();
        cacheStatsProvider = dynamic_cast<ICacheStatsProvider*>(fsProtocol.get());
        authenticator = createAuthenticator();
        permissionChecker = std::make_unique<PermissionChecker>();

        // 2. 创建业务流程，并注入它们所依赖的服务
        backupFlow = std::make_unique<BackupFlow>(authenticator.get(), permissionChecker.get(), fsProtocol.get());
        paperService = std::make_unique<PaperService>(authenticator.get(), permissionChecker.get(), fsProtocol.get());
        reviewFlow = std::make_unique<ReviewFlow>(authenticator.get(), permissionChecker.get(), fsProtocol.get());
        std::cout << "Application services initialized." << std::endl;
    }

    // 使用智能指针管理所有对象的生命周期
    std::unique_ptr<FSProtocol> fsProtocol;
    ICacheStatsProvider* cacheStatsProvider = nullptr;
    std::unique_ptr<Authenticator> authenticator;
    std::unique_ptr<PermissionChecker> permissionChecker;
    std::unique_ptr<BackupFlow> backupFlow;
    std::unique_ptr<PaperService> paperService;
    std::unique_ptr<ReviewFlow> reviewFlow;
};


void ProtocolFactory::handleRequest(socket_t clientSocket) {
    // 1. 读取客户端命令
    std::string commandStr;
    char buffer[4096];
    
    // 循环读取直到连接关闭或没有更多数据
    while (true) {
        int bytesRecv = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRecv < 0) {
            std::cerr << "Recv failed: " << get_socket_error_string() << std::endl;
            return;
        }
        
        if (bytesRecv == 0) {
            // 客户端关闭了连接（或写端）
            break;
        }
        
        buffer[bytesRecv] = '\0';
        commandStr += std::string(buffer, bytesRecv);
    }
    
    if (commandStr.empty()) {
        std::cout << "Received empty command" << std::endl;
        return;
    }
    
    std::cout << "Received command (" << commandStr.length() << " bytes): " 
              << commandStr.substr(0, 100) << (commandStr.length() > 100 ? "..." : "") << std::endl;

    // 2. 从服务中心获取所有服务实例
    auto& services = AppServices::instance();

    // 3. 创建 CLI 协议处理器，并注入所有依赖项
    CLIProtocol cliProtocol(
        services.getFSProtocol(),
        services.getAuthenticator(),
        services.getPermissionChecker(),
        services.getBackupFlow(),
        services.getPaperService(),
        services.getReviewFlow(),
        services.getCacheStatsProvider()
    );

    // 4. 处理命令
    std::string response;
    std::cout << "Processing command..." << std::endl;
    cliProtocol.processCommand(commandStr, response);
    std::cout << "Command processed. Response length: " << response.length() << " bytes" << std::endl;
    
    if (!response.empty()) {
        std::cout << "Response: " << response.substr(0, 100) << (response.length() > 100 ? "..." : "") << std::endl;
    }

    // 5. 将响应发回客户端
    if (!response.empty()) {
        int sent = send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
        if (sent < 0) {
            std::cerr << "Failed to send response: " << get_socket_error_string() << std::endl;
        } else {
            std::cout << "Response sent successfully (" << sent << " bytes)" << std::endl;
        }
    } else {
        std::cout << "Warning: Empty response generated" << std::endl;
    }
}