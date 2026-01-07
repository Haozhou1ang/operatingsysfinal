#pragma once

#include <string>

class Authenticator;
class PermissionChecker;
class FSProtocol;

// 论文审稿系统的核心业务服务（server侧编排 + 权限/资源校验 + 通过FSProtocol落库）
class PaperService {
public:
    PaperService(Authenticator* auth, PermissionChecker* perm, FSProtocol* fs);

    bool uploadPaper(const std::string& sessionToken,
                    const std::string& paperId,
                    const std::string& content,
                    std::string& errorMsg);

    // 新增：上传二进制论文文件（pdf/docx/rtf/doc/tex 等）
    // fileExt 不带点，例如 "pdf"、"docx"。
    bool uploadPaperFile(const std::string& sessionToken,
                         const std::string& paperId,
                         const std::string& fileExt,
                         const std::string& bytes,
                         std::string& errorMsg);

    bool submitRevision(const std::string& sessionToken,
                        const std::string& paperId,
                        const std::string& content,
                        std::string& errorMsg);

    bool downloadPaper(const std::string& sessionToken,
                       const std::string& paperId,
                       std::string& contentOut,
                       std::string& errorMsg);

    bool submitReview(const std::string& sessionToken,
                      const std::string& paperId,
                      const std::string& reviewContent,
                      std::string& errorMsg);

    bool downloadReviews(const std::string& sessionToken,
                         const std::string& paperId,
                         std::string& reviewsOut,
                         std::string& errorMsg);

    bool getStatus(const std::string& sessionToken,
                   const std::string& paperId,
                   std::string& statusOut,
                   std::string& errorMsg);

    bool assignReviewer(const std::string& sessionToken,
                        const std::string& paperId,
                        const std::string& reviewerUsername,
                        std::string& errorMsg);

    bool finalDecision(const std::string& sessionToken,
                       const std::string& paperId,
                       const std::string& decision,
                       std::string& errorMsg);

private:
    Authenticator* authenticator_;
    PermissionChecker* permissionChecker_;
    FSProtocol* fsProtocol_;
};
