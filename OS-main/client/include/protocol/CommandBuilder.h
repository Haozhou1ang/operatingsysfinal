#pragma once

#include <string>

/**
 * 命令构建器
 * 负责构建符合Server协议格式的命令字符串
 */
class CommandBuilder {
public:
    // ========== 通用命令 ==========
    static std::string buildLogin(const std::string& username, const std::string& password);
    static std::string buildLogout(const std::string& token);
    static std::string buildHelp(const std::string& token);

    // ========== 文件操作命令 ==========
    static std::string buildRead(const std::string& token, const std::string& path);
    static std::string buildWrite(const std::string& token, const std::string& path, const std::string& content);
    static std::string buildMkdir(const std::string& token, const std::string& path);

    // ========== 作者命令 ==========
    static std::string buildPaperUpload(const std::string& token, const std::string& paperId, const std::string& content);
    static std::string buildPaperRevise(const std::string& token, const std::string& paperId, const std::string& content);
    static std::string buildStatus(const std::string& token, const std::string& paperId);
    static std::string buildReviewsDownload(const std::string& token, const std::string& paperId);

    // ========== 审稿人命令 ==========
    static std::string buildPaperDownload(const std::string& token, const std::string& paperId);
    static std::string buildReviewSubmit(const std::string& token, const std::string& paperId, const std::string& reviewContent);

    // ========== 编辑命令 ==========
    static std::string buildAssignReviewer(const std::string& token, const std::string& paperId, const std::string& reviewerUsername);
    static std::string buildDecide(const std::string& token, const std::string& paperId, const std::string& decision);

    // ========== 管理员命令 ==========
    static std::string buildUserAdd(const std::string& token, const std::string& username, const std::string& password, const std::string& role);
    static std::string buildUserDel(const std::string& token, const std::string& username);
    static std::string buildUserList(const std::string& token);
    static std::string buildBackupCreate(const std::string& token, const std::string& name = "");
    static std::string buildBackupList(const std::string& token);
    static std::string buildBackupRestore(const std::string& token, const std::string& name);
    static std::string buildSystemStatus(const std::string& token);
    static std::string buildCacheStats(const std::string& token, const std::string& paperId = "");
    static std::string buildCacheClear(const std::string& token);

    // ========== 审核流程命令 ==========
    static std::string buildSubmitReview(const std::string& token, const std::string& operation, const std::string& path);

private:
    // 辅助函数：转义特殊字符
    static std::string escapeContent(const std::string& content);
};

