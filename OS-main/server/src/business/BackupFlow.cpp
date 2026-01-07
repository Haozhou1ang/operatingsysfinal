#include "../../include/business/BackupFlow.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include <string>
#include <ctime> // 用于生成时间戳
#include <iostream> // 用于调试输出

// 构造函数实现
BackupFlow::BackupFlow(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs)
    : authenticator_(auth), permissionChecker_(perm), fsProtocol_(fs) {}


// createBackup 方法实现
bool BackupFlow::createBackup(const std::string& sessionId, 
                             const std::string& path, 
                             const std::string& snapshotName,
                             std::string& errorMsg) {
    // 1. 身份确认：验证会话
    std::string username;
    if (!authenticator_->validateSession(sessionId, username, errorMsg)) {
        errorMsg = "Invalid session ID: " + errorMsg;
        return false;
    }

    UserRole userRole = authenticator_->getUserRole(sessionId);
    if (!permissionChecker_->hasPermission(userRole, Permission::BACKUP_CREATE)) {
        errorMsg = "Permission denied: User does not have BACKUP_CREATE permission.";
        return false;
    }

    // 3. 【调用 FileSystem API】创建快照
    const std::string actualName = snapshotName.empty() ? ("backup_" + std::to_string(time(nullptr))) : snapshotName;
    if (!fsProtocol_->createSnapshot(path, actualName, errorMsg)) {
        errorMsg = "Failed to create snapshot: " + errorMsg;
        return false;
    }
    
    return true;
}