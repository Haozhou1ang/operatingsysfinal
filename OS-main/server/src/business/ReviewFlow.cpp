#include "../../include/business/ReviewFlow.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include <string>

// 构造函数实现
ReviewFlow::ReviewFlow(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs)
    : authenticator_(auth), permissionChecker_(perm), fsProtocol_(fs) {}

// submitForReview 方法实现
std::string ReviewFlow::submitForReview(const std::string& sessionId,
                                      const std::string& operation,
                                      const std::string& path,
                                      std::string& errorMsg) {
    // 1. 身份确认：验证会话
    std::string username;
    if (!authenticator_->validateSession(sessionId, username, errorMsg)) {
        errorMsg = "Invalid session ID: " + errorMsg;
        return ""; // 返回空字符串表示失败
    }

    // 2. 权限确认：检查用户是否有权对该路径发起此操作的审核
    //    例如，提交 "DELETE" 审核前，用户至少得有读取权限才能看到文件
    UserRole userRole = authenticator_->getUserRole(sessionId);
    if (!permissionChecker_->hasPermission(userRole, Permission::REVIEW_SUBMIT)) {
        errorMsg = "Permission denied to access path for review: " + errorMsg;
        return "";
    }

    // 3. 【调用 FileSystem API】提交审核请求
    //    FS层会处理请求的创建和存储
    std::string reviewId = fsProtocol_->submitForReview(operation, path, username, errorMsg);
    
    if (reviewId.empty()) {
        errorMsg = "Failed to submit for review: " + errorMsg;
        return "";
    }

    return reviewId; // 返回审核请求的唯一 ID
}