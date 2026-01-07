#include "../../include/business/PaperService.h"

#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include "../../include/protocol/FSProtocol.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

namespace {

std::string normalizeId(std::string id) {
    // 最小清洗：去掉空格
    id.erase(std::remove_if(id.begin(), id.end(), [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }), id.end());
    return id;
}

std::string paperRoot(const std::string& paperId) {
    return "/papers/" + paperId;
}

std::string metaPath(const std::string& paperId) {
    return paperRoot(paperId) + "/meta.txt";
}

std::string currentPath(const std::string& paperId) {
    return paperRoot(paperId) + "/current.txt";
}

std::string currentBinaryPath(const std::string& paperId, const std::string& ext) {
    return paperRoot(paperId) + "/current." + ext;
}

std::string reviewsDir(const std::string& paperId) {
    return paperRoot(paperId) + "/reviews";
}

std::string reviewPath(const std::string& paperId, const std::string& reviewer) {
    return reviewsDir(paperId) + "/" + reviewer + ".txt";
}

std::string revisionsDir(const std::string& paperId) {
    return paperRoot(paperId) + "/revisions";
}

std::string nowRevisionName() {
    using Clock = std::chrono::system_clock;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
    return std::to_string(ms);
}

struct Meta {
    std::string author;
    std::string status;     // SUBMITTED/UNDER_REVIEW/ACCEPTED/REJECTED
    std::string decision;   // ACCEPT/REJECT
    std::vector<std::string> reviewers;
};

std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string joinCsv(const std::vector<std::string>& v) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ',';
        oss << v[i];
    }
    return oss.str();
}

bool readMeta(FSProtocol* fs, const std::string& paperId, Meta& meta, std::string& errorMsg) {
    std::string text;
    if (!fs->readFile(metaPath(paperId), text, errorMsg)) {
        return false;
    }

    Meta m;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const std::string key = line.substr(0, pos);
        const std::string val = line.substr(pos + 1);
        if (key == "author") m.author = val;
        else if (key == "status") m.status = val;
        else if (key == "decision") m.decision = val;
        else if (key == "reviewers") m.reviewers = splitCsv(val);
    }

    meta = std::move(m);
    return true;
}

bool writeMeta(FSProtocol* fs, const std::string& paperId, const Meta& meta, std::string& errorMsg) {
    std::ostringstream oss;
    oss << "author=" << meta.author << "\n";
    oss << "status=" << meta.status << "\n";
    oss << "decision=" << meta.decision << "\n";
    oss << "reviewers=" << joinCsv(meta.reviewers) << "\n";
    return fs->writeFile(metaPath(paperId), oss.str(), errorMsg);
}

bool isReviewerAssigned(const Meta& meta, const std::string& reviewer) {
    return std::find(meta.reviewers.begin(), meta.reviewers.end(), reviewer) != meta.reviewers.end();
}

bool validateToken(Authenticator* auth, const std::string& token, std::string& username, std::string& errorMsg) {
    if (!auth->validateSession(token, username, errorMsg)) {
        errorMsg = "Not authenticated: " + errorMsg;
        return false;
    }
    return true;
}

}

PaperService::PaperService(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs)
    : authenticator_(auth), permissionChecker_(perm), fsProtocol_(fs) {}

bool PaperService::uploadPaperFile(const std::string& sessionToken,
                                  const std::string& paperIdRaw,
                                  const std::string& fileExt,
                                  const std::string& bytes,
                                  std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }
    if (fileExt.empty()) {
        errorMsg = "fileExt is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::PAPER_UPLOAD)) {
        errorMsg = "Permission denied.";
        return false;
    }

    // 创建目录结构
    const std::string rootPath = paperRoot(paperId);
    if (!fsProtocol_->createDirectory(rootPath, errorMsg)) return false;
    if (!fsProtocol_->createDirectory(reviewsDir(paperId), errorMsg)) return false;
    if (!fsProtocol_->createDirectory(revisionsDir(paperId), errorMsg)) return false;

    // 防止重复 paperId
    std::string existing;
    if (fsProtocol_->readFile(metaPath(paperId), existing, errorMsg)) {
        errorMsg = "paperId already exists.";
        return false;
    }

    Meta meta;
    meta.author = username;
    meta.status = "SUBMITTED";
    meta.decision.clear();
    meta.reviewers.clear();

    if (!writeMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    // current.txt 写一个可读的指针，保持兼容/便于诊断
    {
        std::ostringstream oss;
        oss << "[BINARY_FILE] current." << fileExt;
        fsProtocol_->writeFile(currentPath(paperId), oss.str(), errorMsg);
    }

    // 写入二进制文件
    if (!fsProtocol_->writeFile(currentBinaryPath(paperId, fileExt), bytes, errorMsg)) return false;

    // 记录一个 revision
    const std::string revPath = revisionsDir(paperId) + "/" + nowRevisionName() + "." + fileExt;
    fsProtocol_->writeFile(revPath, bytes, errorMsg);

    return true;
}

bool PaperService::uploadPaper(const std::string& sessionToken,
                              const std::string& paperIdRaw,
                              const std::string& content,
                              std::string& errorMsg) {
    std::cout << "[PaperService] uploadPaper called for paperId: " << paperIdRaw << std::endl;
    
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::cout << "[PaperService] Validating token..." << std::endl;
    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    std::cout << "[PaperService] Checking permissions..." << std::endl;
    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::PAPER_UPLOAD)) {
        errorMsg = "Permission denied.";
        return false;
    }

    std::cout << "[PaperService] Creating directory structure..." << std::endl;
    // 创建目录结构
    const std::string rootPath = paperRoot(paperId);
    std::cout << "[PaperService] Creating paperRoot: " << rootPath << std::endl;
    if (!fsProtocol_->createDirectory(rootPath, errorMsg)) {
        std::cout << "[PaperService] ❌ Failed to create paperRoot: " << rootPath << std::endl;
        std::cout << "[PaperService] Error message: " << errorMsg << std::endl;
        return false;
    }
    std::cout << "[PaperService] ✓ paperRoot created successfully" << std::endl;
    
    const std::string revDir = reviewsDir(paperId);
    std::cout << "[PaperService] Creating reviewsDir: " << revDir << std::endl;
    if (!fsProtocol_->createDirectory(revDir, errorMsg)) {
        std::cout << "[PaperService] ❌ Failed to create reviewsDir: " << revDir << std::endl;
        std::cout << "[PaperService] Error message: " << errorMsg << std::endl;
        return false;
    }
    std::cout << "[PaperService] ✓ reviewsDir created successfully" << std::endl;
    
    const std::string revsDir = revisionsDir(paperId);
    std::cout << "[PaperService] Creating revisionsDir: " << revsDir << std::endl;
    if (!fsProtocol_->createDirectory(revsDir, errorMsg)) {
        std::cout << "[PaperService] ❌ Failed to create revisionsDir: " << revsDir << std::endl;
        std::cout << "[PaperService] Error message: " << errorMsg << std::endl;
        return false;
    }
    std::cout << "[PaperService] ✓ revisionsDir created successfully" << std::endl;

    std::cout << "[PaperService] Checking for duplicate paperId..." << std::endl;
    // 防止重复 paperId
    std::string existing;
    if (fsProtocol_->readFile(metaPath(paperId), existing, errorMsg)) {
        errorMsg = "paperId already exists.";
        return false;
    }

    std::cout << "[PaperService] Writing metadata..." << std::endl;
    Meta meta;
    meta.author = username;
    meta.status = "SUBMITTED";
    meta.decision.clear();
    meta.reviewers.clear();

    if (!writeMeta(fsProtocol_, paperId, meta, errorMsg)) return false;
    
    std::cout << "[PaperService] Writing current content..." << std::endl;
    if (!fsProtocol_->writeFile(currentPath(paperId), content, errorMsg)) return false;

    std::cout << "[PaperService] Writing revision..." << std::endl;
    // 记录一个 revision
    const std::string revPath = revisionsDir(paperId) + "/" + nowRevisionName() + ".txt";
    fsProtocol_->writeFile(revPath, content, errorMsg);

    std::cout << "[PaperService] Upload completed successfully" << std::endl;
    return true;
}

bool PaperService::submitRevision(const std::string& sessionToken,
                                 const std::string& paperIdRaw,
                                 const std::string& content,
                                 std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::PAPER_REVISE)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;
    if (meta.author != username) {
        errorMsg = "Only author can submit revision.";
        return false;
    }

    if (!fsProtocol_->writeFile(currentPath(paperId), content, errorMsg)) return false;
    const std::string revPath = revisionsDir(paperId) + "/" + nowRevisionName() + ".txt";
    fsProtocol_->writeFile(revPath, content, errorMsg);

    // 若已经分配审稿人，则进入 UNDER_REVIEW
    if (!meta.reviewers.empty()) {
        meta.status = "UNDER_REVIEW";
        if (!writeMeta(fsProtocol_, paperId, meta, errorMsg)) return false;
    }

    return true;
}

bool PaperService::downloadPaper(const std::string& sessionToken,
                                const std::string& paperIdRaw,
                                std::string& contentOut,
                                std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::PAPER_DOWNLOAD)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    // 资源级校验
    if (role == UserRole::AUTHOR && meta.author != username) {
        errorMsg = "Author can only download own paper.";
        return false;
    }
    if (role == UserRole::REVIEWER && !isReviewerAssigned(meta, username)) {
        errorMsg = "Reviewer not assigned.";
        return false;
    }

    return fsProtocol_->readFile(currentPath(paperId), contentOut, errorMsg);
}

bool PaperService::submitReview(const std::string& sessionToken,
                               const std::string& paperIdRaw,
                               const std::string& reviewContent,
                               std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::REVIEW_SUBMIT)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    if (role == UserRole::REVIEWER && !isReviewerAssigned(meta, username)) {
        errorMsg = "Reviewer not assigned.";
        return false;
    }
    if (role == UserRole::AUTHOR) {
        errorMsg = "Author cannot submit review.";
        return false;
    }

    fsProtocol_->createDirectory(reviewsDir(paperId), errorMsg);
    if (!fsProtocol_->writeFile(reviewPath(paperId, username), reviewContent, errorMsg)) return false;

    // 写入后标记为 UNDER_REVIEW
    if (meta.status == "SUBMITTED") {
        meta.status = "UNDER_REVIEW";
        if (!writeMeta(fsProtocol_, paperId, meta, errorMsg)) return false;
    }

    return true;
}

bool PaperService::downloadReviews(const std::string& sessionToken,
                                  const std::string& paperIdRaw,
                                  std::string& reviewsOut,
                                  std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::REVIEW_DOWNLOAD)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    // 作者只能下载自己的
    if (role == UserRole::AUTHOR && meta.author != username) {
        errorMsg = "Author can only download own reviews.";
        return false;
    }

    // 演示用：按已分配 reviewers 列表拼接；如果 reviewer 文件不存在就跳过
    std::ostringstream oss;
    bool any = false;

    for (const auto& reviewer : meta.reviewers) {
        std::string content;
        std::string tmpErr;
        if (fsProtocol_->readFile(reviewPath(paperId, reviewer), content, tmpErr)) {
            any = true;
            oss << "--- reviewer=" << reviewer << " ---\n";
            oss << content << "\n";
        }
    }

    if (!any) {
        reviewsOut = "(no reviews yet)";
    } else {
        reviewsOut = oss.str();
    }

    return true;
}

bool PaperService::getStatus(const std::string& sessionToken,
                            const std::string& paperIdRaw,
                            std::string& statusOut,
                            std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    if (paperId.empty()) {
        errorMsg = "paperId is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::PAPER_STATUS)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    // 资源级校验：审稿人只能看自己被分配的；作者只能看自己
    if (role == UserRole::AUTHOR && meta.author != username) {
        errorMsg = "Author can only view own status.";
        return false;
    }
    if (role == UserRole::REVIEWER && !isReviewerAssigned(meta, username)) {
        errorMsg = "Reviewer not assigned.";
        return false;
    }

    std::ostringstream oss;
    oss << "paperId=" << paperId << "\n";
    oss << "author=" << meta.author << "\n";
    oss << "status=" << meta.status << "\n";
    oss << "reviewers=" << joinCsv(meta.reviewers) << "\n";
    oss << "decision=" << meta.decision << "\n";
    statusOut = oss.str();
    return true;
}

bool PaperService::assignReviewer(const std::string& sessionToken,
                                 const std::string& paperIdRaw,
                                 const std::string& reviewerUsernameRaw,
                                 std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    const std::string reviewerUsername = normalizeId(reviewerUsernameRaw);
    if (paperId.empty() || reviewerUsername.empty()) {
        errorMsg = "paperId/reviewerUsername is empty.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::ASSIGN_REVIEWER)) {
        errorMsg = "Permission denied.";
        return false;
    }

    // 验证审稿人用户是否存在
    if (!authenticator_->userExists(reviewerUsername)) {
        errorMsg = "Reviewer user '" + reviewerUsername + "' does not exist.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    if (!isReviewerAssigned(meta, reviewerUsername)) {
        meta.reviewers.push_back(reviewerUsername);
        std::sort(meta.reviewers.begin(), meta.reviewers.end());
    }

    // 一旦分配审稿人，进入 UNDER_REVIEW
    if (meta.status == "SUBMITTED") {
        meta.status = "UNDER_REVIEW";
    }

    return writeMeta(fsProtocol_, paperId, meta, errorMsg);
}

bool PaperService::finalDecision(const std::string& sessionToken,
                                const std::string& paperIdRaw,
                                const std::string& decisionRaw,
                                std::string& errorMsg) {
    const std::string paperId = normalizeId(paperIdRaw);
    std::string decision = decisionRaw;
    std::transform(decision.begin(), decision.end(), decision.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (paperId.empty() || decision.empty()) {
        errorMsg = "paperId/decision is empty.";
        return false;
    }
    if (decision != "ACCEPT" && decision != "REJECT") {
        errorMsg = "decision must be ACCEPT or REJECT.";
        return false;
    }

    std::string username;
    if (!validateToken(authenticator_, sessionToken, username, errorMsg)) return false;

    const UserRole role = authenticator_->getUserRole(sessionToken);
    if (!permissionChecker_->hasPermission(role, Permission::FINAL_DECISION)) {
        errorMsg = "Permission denied.";
        return false;
    }

    Meta meta;
    if (!readMeta(fsProtocol_, paperId, meta, errorMsg)) return false;

    meta.decision = decision;
    meta.status = (decision == "ACCEPT") ? "ACCEPTED" : "REJECTED";

    return writeMeta(fsProtocol_, paperId, meta, errorMsg);
}
