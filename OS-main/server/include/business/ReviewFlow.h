#pragma once
#include <string>

// 前向声明
class Authenticator;
class PermissionChecker;
class FSProtocol;

// 【业务流程：审核】
// 职责：封装提交操作审核请求的业务逻辑。
class ReviewFlow {
public:
    // 构造函数：依赖注入
    ReviewFlow(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs);

    // 提交一个操作以供审核。
    // 成功返回一个唯一的审核ID，失败返回空字符串，并通过 errorMsg 报告错误。
    std::string submitForReview(const std::string& sessionId,
                                const std::string& operation,
                                const std::string& path,
                                std::string& errorMsg);

private:
    Authenticator* authenticator_;
    PermissionChecker* permissionChecker_;
    FSProtocol* fsProtocol_;
};