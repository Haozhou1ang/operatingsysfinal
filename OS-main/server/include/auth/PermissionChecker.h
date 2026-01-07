#ifndef PERMISSION_CHECKER_H
#define PERMISSION_CHECKER_H

// 引入 Authenticator.h 是为了使用其中定义的 UserRole 枚举
#include "Authenticator.h" 

// 定义操作所需的权限类型
enum class Permission {
    READ_FILE,
    WRITE_FILE,
    MKDIR,
    BACKUP_CREATE,
    BACKUP_LIST,
    BACKUP_RESTORE,
    SYSTEM_STATUS,
    USER_MANAGE,
    PAPER_UPLOAD,
    PAPER_REVISE,
    PAPER_DOWNLOAD,
    PAPER_STATUS,
    REVIEW_SUBMIT,
    REVIEW_DOWNLOAD,
    ASSIGN_REVIEWER,
    FINAL_DECISION
};

// 权限检查器类
class PermissionChecker {
public:
    // 检查一个用户角色是否拥有所需权限的核心函数
    // 这是一个简化实现。真实世界的应用会更复杂，可能会从配置文件或数据库中加载角色与权限的对应关系。
    bool hasPermission(UserRole role, Permission requiredPermission) {
        switch (role) {
            // 管理员拥有所有权限
            case UserRole::ADMIN:
                return true; 
            // 编辑：可以读/写/建目录；需要审核流（后续可扩展为分配审稿人、最终决定等）
            case UserRole::EDITOR:
                return requiredPermission == Permission::READ_FILE
                    || requiredPermission == Permission::WRITE_FILE
                    || requiredPermission == Permission::MKDIR
                    || requiredPermission == Permission::PAPER_DOWNLOAD
                    || requiredPermission == Permission::PAPER_STATUS
                    || requiredPermission == Permission::REVIEW_DOWNLOAD
                    || requiredPermission == Permission::ASSIGN_REVIEWER
                    || requiredPermission == Permission::FINAL_DECISION;
            // 审稿人：读论文、提交评审（此处用 REVIEW_SUBMIT 表示）
            case UserRole::REVIEWER:
                return requiredPermission == Permission::READ_FILE
                    || requiredPermission == Permission::PAPER_DOWNLOAD
                    || requiredPermission == Permission::PAPER_STATUS
                    || requiredPermission == Permission::REVIEW_SUBMIT;
            // 作者：写论文/修订、查看状态（当前先用写/读抽象）
            case UserRole::AUTHOR:
                return requiredPermission == Permission::READ_FILE
                    || requiredPermission == Permission::WRITE_FILE
                    || requiredPermission == Permission::MKDIR
                    || requiredPermission == Permission::PAPER_UPLOAD
                    || requiredPermission == Permission::PAPER_REVISE
                    || requiredPermission == Permission::PAPER_DOWNLOAD
                    || requiredPermission == Permission::PAPER_STATUS
                    || requiredPermission == Permission::REVIEW_DOWNLOAD;
            // 访客只拥有读权限（如果你们希望“未登录不可读”，可在 CLIProtocol 直接禁止）
            case UserRole::GUEST:
                return requiredPermission == Permission::READ_FILE;
            // 其他未知角色没有任何权限
            default:
                return false;
        }
    }
};

#endif // PERMISSION_CHECKER_H