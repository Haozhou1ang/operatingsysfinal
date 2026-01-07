#include "cli/CLIInterface.h"
#include "network/NetworkClient.h"
#include "session/SessionManager.h"
#include "protocol/CommandBuilder.h"
#include "protocol/ResponseParser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

CLIInterface::CLIInterface(NetworkClient* network, SessionManager* session)
    : network_(network), session_(session), serverHost_("localhost"), 
      serverPort_(8080), running_(false) {
}

CLIInterface::~CLIInterface() {
}

void CLIInterface::setServerAddress(const std::string& host, int port) {
    serverHost_ = host;
    serverPort_ = port;
    // 同时设置NetworkClient的默认服务器地址
    network_->setDefaultServer(host, port);
}

void CLIInterface::run() {
    showWelcome();
    running_ = true;

    while (running_) {
        showPrompt();
        std::string input = readUserInput();
        
        if (input.empty()) {
            continue;
        }

        handleCommand(input);
    }
}

void CLIInterface::showWelcome() {
    std::cout << "========================================\n";
    std::cout << "    论文审稿系统 Client\n";
    std::cout << "    服务器: " << serverHost_ << ":" << serverPort_ << "\n";
    std::cout << "========================================\n";
    std::cout << "输入 'help' 查看帮助信息\n";
    std::cout << "输入 'exit' 退出程序\n\n";
}

void CLIInterface::showPrompt() {
    if (session_->isLoggedIn()) {
        std::cout << "[" << session_->getCurrentUsername() 
                  << "@" << session_->getRoleString() << "]$ ";
    } else {
        std::cout << "[未登录]$ ";
    }
    std::cout.flush();
}

std::string CLIInterface::readUserInput() {
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void CLIInterface::handleCommand(const std::string& input) {
    std::vector<std::string> args = parseCommand(input);
    if (args.empty()) {
        return;
    }

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    // 通用命令（无需登录）
    if (cmd == "login") {
        handleLogin(args);
    } else if (cmd == "help") {
        handleHelp();
    } else if (cmd == "exit" || cmd == "quit") {
        handleExit();
    }
    // 需要登录的命令
    else if (!session_->isLoggedIn()) {
        displayError("请先登录。使用 'login <用户名> <密码>'");
    }
    // 登出
    else if (cmd == "logout") {
        handleLogout();
    }
    // 文件操作
    else if (cmd == "read") {
        handleRead(args);
    } else if (cmd == "write") {
        handleWrite(args);
    } else if (cmd == "mkdir") {
        handleMkdir(args);
    }
    // 作者命令
    else if (cmd == "upload" || cmd == "paper_upload") {
        handlePaperUpload(args);
    } else if (cmd == "revise" || cmd == "paper_revise") {
        handlePaperRevise(args);
    } else if (cmd == "status") {
        handleStatus(args);
    } else if (cmd == "reviews" || cmd == "reviews_download") {
        handleReviewsDownload(args);
    }
    // 审稿人命令
    else if (cmd == "download" || cmd == "paper_download") {
        handlePaperDownload(args);
    } else if (cmd == "review" || cmd == "review_submit") {
        handleReviewSubmit(args);
    }
    // 编辑命令
    else if (cmd == "assign" || cmd == "assign_reviewer") {
        handleAssignReviewer(args);
    } else if (cmd == "decide") {
        handleDecide(args);
    }
    // 管理员命令
    else if (cmd == "user_add" || cmd == "useradd") {
        handleUserAdd(args);
    } else if (cmd == "user_del" || cmd == "userdel") {
        handleUserDel(args);
    } else if (cmd == "user_list" || cmd == "userlist") {
        handleUserList();
    } else if (cmd == "backup_create") {
        handleBackupCreate(args);
    } else if (cmd == "backup_list") {
        handleBackupList();
    } else if (cmd == "backup_restore") {
        handleBackupRestore(args);
    } else if (cmd == "system_status") {
        handleSystemStatus();
    } else if (cmd == "cache_stats") {
        handleCacheStats(args);
    } else if (cmd == "cache_clear") {
        handleCacheClear();
    }
    else {
        displayError("未知命令: " + cmd + "。输入 'help' 查看帮助");
    }
}

// ========== 命令处理实现 ==========

void CLIInterface::handleLogin(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("用法: login <用户名> <密码>");
        return;
    }

    std::string username = args[1];
    std::string password = args[2];
    std::string errorMsg;

    if (session_->login(username, password, errorMsg)) {
        displaySuccess("登录成功! 角色: " + session_->getRoleString());
    } else {
        displayError("登录失败: " + errorMsg);
    }
}

void CLIInterface::handleLogout() {
    std::string errorMsg;
    if (session_->logout(errorMsg)) {
        displaySuccess("已登出");
    } else {
        displayError("登出失败: " + errorMsg);
    }
}

void CLIInterface::handleHelp() {
    if (!session_->isLoggedIn()) {
        showGeneralHelp();
        return;
    }

    UserRole role = session_->getCurrentRole();
    switch (role) {
        case UserRole::AUTHOR:
            showAuthorHelp();
            break;
        case UserRole::REVIEWER:
            showReviewerHelp();
            break;
        case UserRole::EDITOR:
            showEditorHelp();
            break;
        case UserRole::ADMIN:
            showAdminHelp();
            break;
        default:
            showGeneralHelp();
            break;
    }
}

void CLIInterface::handleExit() {
    if (session_->isLoggedIn()) {
        std::cout << "正在登出...\n";
        std::string errorMsg;
        session_->logout(errorMsg);
    }
    std::cout << "再见!\n";
    running_ = false;
}

void CLIInterface::handleRead(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: read <路径>");
        return;
    }
    std::string command = CommandBuilder::buildRead(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleWrite(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("用法: write <路径> <内容>");
        std::cout << "提示: 如需输入多行内容，使用 write <路径> - 然后输入多行\n";
        return;
    }

    std::string path = args[1];
    std::string content;

    if (args[2] == "-") {
        content = readMultilineContent("请输入文件内容(单独一行输入END结束):");
    } else {
        // 将剩余参数合并为内容
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2) content += " ";
            content += args[i];
        }
    }

    std::string command = CommandBuilder::buildWrite(session_->getCurrentToken(), path, content);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleMkdir(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: mkdir <路径>");
        return;
    }
    std::string command = CommandBuilder::buildMkdir(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handlePaperUpload(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: upload <论文ID>");
        std::cout << "提示: 将提示您输入论文内容\n";
        return;
    }

    std::string paperId = args[1];
    std::string content = readMultilineContent("请输入论文内容(单独一行输入END结束):");

    std::string command = CommandBuilder::buildPaperUpload(session_->getCurrentToken(), paperId, content);
    sendCommandAndDisplay(command);
}

void CLIInterface::handlePaperRevise(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: revise <论文ID>");
        std::cout << "提示: 将提示您输入修订内容\n";
        return;
    }

    std::string paperId = args[1];
    std::string content = readMultilineContent("请输入修订内容(单独一行输入END结束):");

    std::string command = CommandBuilder::buildPaperRevise(session_->getCurrentToken(), paperId, content);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleStatus(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: status <论文ID>");
        return;
    }
    std::string command = CommandBuilder::buildStatus(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleReviewsDownload(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: reviews <论文ID>");
        return;
    }
    std::string command = CommandBuilder::buildReviewsDownload(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handlePaperDownload(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: download <论文ID>");
        return;
    }
    std::string command = CommandBuilder::buildPaperDownload(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleReviewSubmit(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: review <论文ID>");
        std::cout << "提示: 将提示您输入评审内容\n";
        return;
    }

    std::string paperId = args[1];
    std::string content = readMultilineContent("请输入评审内容(单独一行输入END结束):");

    std::string command = CommandBuilder::buildReviewSubmit(session_->getCurrentToken(), paperId, content);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleAssignReviewer(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("用法: assign <论文ID> <审稿人用户名>");
        return;
    }
    std::string command = CommandBuilder::buildAssignReviewer(session_->getCurrentToken(), args[1], args[2]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleDecide(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        displayError("用法: decide <论文ID> <ACCEPT|REJECT>");
        return;
    }
    std::string command = CommandBuilder::buildDecide(session_->getCurrentToken(), args[1], args[2]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleUserAdd(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        displayError("用法: user_add <用户名> <密码> <角色>");
        std::cout << "角色: ADMIN, EDITOR, REVIEWER, AUTHOR, GUEST\n";
        return;
    }
    std::string command = CommandBuilder::buildUserAdd(session_->getCurrentToken(), args[1], args[2], args[3]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleUserDel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: user_del <用户名>");
        return;
    }
    std::string command = CommandBuilder::buildUserDel(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleUserList() {
    std::string command = CommandBuilder::buildUserList(session_->getCurrentToken());
    sendCommandAndDisplay(command);
}

void CLIInterface::handleBackupCreate(const std::vector<std::string>& args) {
    // 快照是全局的，只需要可选的名称参数
    std::string name = (args.size() >= 2) ? args[1] : "";
    std::string command = CommandBuilder::buildBackupCreate(session_->getCurrentToken(), name);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleBackupList() {
    std::string command = CommandBuilder::buildBackupList(session_->getCurrentToken());
    sendCommandAndDisplay(command);
}

void CLIInterface::handleBackupRestore(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayError("用法: backup_restore <快照名称>");
        return;
    }
    std::string command = CommandBuilder::buildBackupRestore(session_->getCurrentToken(), args[1]);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleSystemStatus() {
    std::string command = CommandBuilder::buildSystemStatus(session_->getCurrentToken());
    sendCommandAndDisplay(command);
}

void CLIInterface::handleCacheStats(const std::vector<std::string>& args) {
    std::string paperId = (args.size() >= 2) ? args[1] : "";
    std::string command = CommandBuilder::buildCacheStats(session_->getCurrentToken(), paperId);
    sendCommandAndDisplay(command);
}

void CLIInterface::handleCacheClear() {
    std::string command = CommandBuilder::buildCacheClear(session_->getCurrentToken());
    sendCommandAndDisplay(command);
}

// ========== 辅助函数 ==========

std::vector<std::string> CLIInterface::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

std::string CLIInterface::readMultilineContent(const std::string& prompt) {
    std::cout << prompt << "\n";
    std::string content;
    std::string line;
    
    while (std::getline(std::cin, line)) {
        if (line == "END") {
            break;
        }
        content += line + "\n";
    }
    
    return content;
}

void CLIInterface::displayResponse(const std::string& response) {
    std::cout << response << "\n";
}

void CLIInterface::displayError(const std::string& error) {
    std::cout << "错误: " << error << "\n";
}

void CLIInterface::displaySuccess(const std::string& message) {
    std::cout << "成功: " << message << "\n";
}

bool CLIInterface::sendCommandAndDisplay(const std::string& command) {
    std::string response, errorMsg;
    
    if (!network_->sendAndReceive(command, response, errorMsg)) {
        displayError("网络错误: " + errorMsg);
        return false;
    }
    
    displayResponse(response);
    return true;
}

// ========== 帮助信息 ==========

void CLIInterface::showGeneralHelp() {
    std::cout << "\n========== 通用命令 ==========\n";
    std::cout << "  login <用户名> <密码>  - 登录系统\n";
    std::cout << "  help                   - 显示帮助信息\n";
    std::cout << "  exit                   - 退出程序\n";
    std::cout << "\n提示: 登录后可查看角色相关命令\n\n";
}

void CLIInterface::showAuthorHelp() {
    std::cout << "\n========== 作者命令 ==========\n";
    std::cout << "  upload <论文ID>        - 上传论文\n";
    std::cout << "  revise <论文ID>        - 提交修订版本\n";
    std::cout << "  status <论文ID>        - 查看论文状态\n";
    std::cout << "  reviews <论文ID>       - 下载评审意见\n";
    std::cout << "\n========== 通用命令 ==========\n";
    std::cout << "  read <路径>            - 读取文件\n";
    std::cout << "  write <路径> <内容>    - 写入文件\n";
    std::cout << "  mkdir <路径>           - 创建目录\n";
    std::cout << "  logout                 - 登出\n";
    std::cout << "  help                   - 显示帮助\n";
    std::cout << "  exit                   - 退出\n\n";
}

void CLIInterface::showReviewerHelp() {
    std::cout << "\n========== 审稿人命令 ==========\n";
    std::cout << "  download <论文ID>      - 下载论文\n";
    std::cout << "  review <论文ID>        - 提交评审意见\n";
    std::cout << "  status <论文ID>        - 查看论文状态\n";
    std::cout << "\n========== 通用命令 ==========\n";
    std::cout << "  read <路径>            - 读取文件\n";
    std::cout << "  logout                 - 登出\n";
    std::cout << "  help                   - 显示帮助\n";
    std::cout << "  exit                   - 退出\n\n";
}

void CLIInterface::showEditorHelp() {
    std::cout << "\n========== 编辑命令 ==========\n";
    std::cout << "  assign <论文ID> <审稿人>  - 分配审稿人\n";
    std::cout << "  decide <论文ID> <决定>    - 做最终决定(ACCEPT/REJECT)\n";
    std::cout << "  status <论文ID>           - 查看论文状态\n";
    std::cout << "  reviews <论文ID>          - 查看所有评审意见\n";
    std::cout << "\n========== 通用命令 ==========\n";
    std::cout << "  read <路径>               - 读取文件\n";
    std::cout << "  logout                    - 登出\n";
    std::cout << "  help                      - 显示帮助\n";
    std::cout << "  exit                      - 退出\n\n";
}

void CLIInterface::showAdminHelp() {
    std::cout << "\n========== 管理员命令 ==========\n";
    std::cout << "  user_add <用户名> <密码> <角色>  - 添加用户\n";
    std::cout << "  user_del <用户名>                - 删除用户\n";
    std::cout << "  user_list                        - 列出所有用户\n";
    std::cout << "  backup_create [名称]             - 创建全局快照（不含用户账户）\n";
    std::cout << "  backup_list                      - 列出所有快照\n";
    std::cout << "  backup_restore <名称>            - 恢复快照（不影响用户账户）\n";
    std::cout << "  system_status                    - 查看系统状态\n";
    std::cout << "  cache_stats [论文ID]             - 查看缓存统计（可选指定论文ID）\n";
    std::cout << "  cache_clear                      - 清空缓存\n";
    std::cout << "\n========== 通用命令 ==========\n";
    std::cout << "  read <路径>                      - 读取文件\n";
    std::cout << "  write <路径> <内容>              - 写入文件\n";
    std::cout << "  mkdir <路径>                     - 创建目录\n";
    std::cout << "  logout                           - 登出\n";
    std::cout << "  help                             - 显示帮助\n";
    std::cout << "  exit                             - 退出\n\n";
}

