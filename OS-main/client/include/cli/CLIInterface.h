#pragma once

#include <string>
#include <vector>

// 前向声明
class NetworkClient;
class SessionManager;

/**
 * CLI用户界面
 * 负责用户交互、命令解析、输出格式化
 */
class CLIInterface {
public:
    CLIInterface(NetworkClient* network, SessionManager* session);
    ~CLIInterface();

    /**
     * 运行主循环
     */
    void run();

    /**
     * 设置服务器地址和端口
     */
    void setServerAddress(const std::string& host, int port);

private:
    NetworkClient* network_;
    SessionManager* session_;
    std::string serverHost_;
    int serverPort_;
    bool running_;

    // ========== 主循环相关 ==========
    void showWelcome();
    void showPrompt();
    std::string readUserInput();
    void handleCommand(const std::string& input);

    // ========== 命令处理 ==========
    void handleLogin(const std::vector<std::string>& args);
    void handleLogout();
    void handleHelp();
    void handleExit();

    // 文件操作
    void handleRead(const std::vector<std::string>& args);
    void handleWrite(const std::vector<std::string>& args);
    void handleMkdir(const std::vector<std::string>& args);

    // 作者命令
    void handlePaperUpload(const std::vector<std::string>& args);
    void handlePaperRevise(const std::vector<std::string>& args);
    void handleStatus(const std::vector<std::string>& args);
    void handleReviewsDownload(const std::vector<std::string>& args);

    // 审稿人命令
    void handlePaperDownload(const std::vector<std::string>& args);
    void handleReviewSubmit(const std::vector<std::string>& args);

    // 编辑命令
    void handleAssignReviewer(const std::vector<std::string>& args);
    void handleDecide(const std::vector<std::string>& args);

    // 管理员命令
    void handleUserAdd(const std::vector<std::string>& args);
    void handleUserDel(const std::vector<std::string>& args);
    void handleUserList();
    void handleBackupCreate(const std::vector<std::string>& args);
    void handleBackupList();
    void handleBackupRestore(const std::vector<std::string>& args);
    void handleSystemStatus();
    void handleCacheStats(const std::vector<std::string>& args);
    void handleCacheClear();

    // ========== 辅助函数 ==========
    std::vector<std::string> parseCommand(const std::string& input);
    std::string readMultilineContent(const std::string& prompt);
    void displayResponse(const std::string& response);
    void displayError(const std::string& error);
    void displaySuccess(const std::string& message);

    // 帮助信息
    void showGeneralHelp();
    void showAuthorHelp();
    void showReviewerHelp();
    void showEditorHelp();
    void showAdminHelp();

    // 发送命令的通用函数
    bool sendCommandAndDisplay(const std::string& command);
};

