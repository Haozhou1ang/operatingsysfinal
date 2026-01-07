#pragma once
#include <string>

// 前向声明
class FSProtocol;
class Authenticator;
class PermissionChecker;
class BackupFlow;
class PaperService;
class ReviewFlow;
class ICacheStatsProvider;

class CLIProtocol {
public:
    // 构造函数，接收所有它需要与之交互的服务和流程
    CLIProtocol(FSProtocol* fs,
                Authenticator* auth,
                PermissionChecker* perm,
                BackupFlow* backup,
                PaperService* paper,
                ReviewFlow* review,
                ICacheStatsProvider* cacheStatsProvider = nullptr);

    // 解析并处理命令
    bool processCommand(const std::string& command, std::string& response);

private:
    FSProtocol* m_fs;
    Authenticator* m_auth;
    PermissionChecker* m_perm;
    BackupFlow* m_backupFlow;
    PaperService* m_paper;
    ReviewFlow* m_reviewFlow;
    ICacheStatsProvider* m_cacheStatsProvider;
};