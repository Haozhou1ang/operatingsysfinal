#include "protocol/CommandBuilder.h"
#include <sstream>

// ========== 通用命令 ==========
std::string CommandBuilder::buildLogin(const std::string& username, const std::string& password) {
    return "LOGIN " + username + " " + password;
}

std::string CommandBuilder::buildLogout(const std::string& token) {
    return "LOGOUT " + token;
}

std::string CommandBuilder::buildHelp(const std::string& token) {
    return "HELP " + token;
}

// ========== 文件操作命令 ==========
std::string CommandBuilder::buildRead(const std::string& token, const std::string& path) {
    return "READ " + token + " " + path;
}

std::string CommandBuilder::buildWrite(const std::string& token, const std::string& path, const std::string& content) {
    return "WRITE " + token + " " + path + " " + content;
}

std::string CommandBuilder::buildMkdir(const std::string& token, const std::string& path) {
    return "MKDIR " + token + " " + path;
}

// ========== 作者命令 ==========
std::string CommandBuilder::buildPaperUpload(const std::string& token, const std::string& paperId, const std::string& content) {
    return "PAPER_UPLOAD " + token + " " + paperId + " " + content;
}

std::string CommandBuilder::buildPaperRevise(const std::string& token, const std::string& paperId, const std::string& content) {
    return "PAPER_REVISE " + token + " " + paperId + " " + content;
}

std::string CommandBuilder::buildStatus(const std::string& token, const std::string& paperId) {
    return "STATUS " + token + " " + paperId;
}

std::string CommandBuilder::buildReviewsDownload(const std::string& token, const std::string& paperId) {
    return "REVIEWS_DOWNLOAD " + token + " " + paperId;
}

// ========== 审稿人命令 ==========
std::string CommandBuilder::buildPaperDownload(const std::string& token, const std::string& paperId) {
    return "PAPER_DOWNLOAD " + token + " " + paperId;
}

std::string CommandBuilder::buildReviewSubmit(const std::string& token, const std::string& paperId, const std::string& reviewContent) {
    return "REVIEW_SUBMIT " + token + " " + paperId + " " + reviewContent;
}

// ========== 编辑命令 ==========
std::string CommandBuilder::buildAssignReviewer(const std::string& token, const std::string& paperId, const std::string& reviewerUsername) {
    return "ASSIGN_REVIEWER " + token + " " + paperId + " " + reviewerUsername;
}

std::string CommandBuilder::buildDecide(const std::string& token, const std::string& paperId, const std::string& decision) {
    return "DECIDE " + token + " " + paperId + " " + decision;
}

// ========== 管理员命令 ==========
std::string CommandBuilder::buildUserAdd(const std::string& token, const std::string& username, const std::string& password, const std::string& role) {
    return "USER_ADD " + token + " " + username + " " + password + " " + role;
}

std::string CommandBuilder::buildUserDel(const std::string& token, const std::string& username) {
    return "USER_DEL " + token + " " + username;
}

std::string CommandBuilder::buildUserList(const std::string& token) {
    return "USER_LIST " + token;
}

std::string CommandBuilder::buildBackupCreate(const std::string& token, const std::string& name) {
    if (name.empty()) {
        return "BACKUP_CREATE " + token;
    }
    return "BACKUP_CREATE " + token + " " + name;
}

std::string CommandBuilder::buildBackupList(const std::string& token) {
    return "BACKUP_LIST " + token;
}

std::string CommandBuilder::buildBackupRestore(const std::string& token, const std::string& name) {
    return "BACKUP_RESTORE " + token + " " + name;
}

std::string CommandBuilder::buildSystemStatus(const std::string& token) {
    return "SYSTEM_STATUS " + token;
}

std::string CommandBuilder::buildCacheStats(const std::string& token, const std::string& paperId) {
    std::string cmd = "CACHE_STATS " + token;
    if (!paperId.empty()) {
        cmd += " " + paperId;
    }
    return cmd;
}

std::string CommandBuilder::buildCacheClear(const std::string& token) {
    return "CACHE_CLEAR " + token;
}

// ========== 审核流程命令 ==========
std::string CommandBuilder::buildSubmitReview(const std::string& token, const std::string& operation, const std::string& path) {
    return "SUBMIT_REVIEW " + token + " " + operation + " " + path;
}

// ========== 辅助函数 ==========
std::string CommandBuilder::escapeContent(const std::string& content) {
    // 简单实现：暂不做转义，因为server协议是空格分隔
    // 如果需要支持包含空格的内容，需要更复杂的转义机制
    return content;
}

