#pragma once
#include <string>

// 前向声明以避免循环依赖和减少编译时间
class Authenticator;
class PermissionChecker;
class FSProtocol;

// 【业务流程：备份】
// 职责：封装创建备份的完整业务逻辑，包括身份验证、权限检查和文件系统操作。
class BackupFlow {
public:
    // 构造函数：通过依赖注入传入所需的服务模块。
    BackupFlow(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs);

    // 执行创建备份的操作。
    // 成功返回 true, 失败返回 false，并通过 errorMsg 返回错误信息。
    bool createBackup(const std::string& sessionId, 
                      const std::string& path, 
                      const std::string& snapshotName,
                      std::string& errorMsg);

private:
    // 持有对其他服务模块的指针（不负责生命周期管理）
    Authenticator* authenticator_;
    PermissionChecker* permissionChecker_;
    FSProtocol* fsProtocol_;
};