#include "../../include/protocol/CLIProtocol.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/protocol/RealFileSystemAdapter.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include "../../include/business/BackupFlow.h"
#include "../../include/business/PaperService.h"
#include "../../include/business/ReviewFlow.h"
#include "../../include/cache/CacheStatsProvider.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cctype>
#include <string_view>

namespace {

std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::ADMIN: return "ADMIN";
        case UserRole::EDITOR: return "EDITOR";
        case UserRole::REVIEWER: return "REVIEWER";
        case UserRole::AUTHOR: return "AUTHOR";
        case UserRole::GUEST: return "GUEST";
        default: return "UNKNOWN";
    }
}

UserRole parseRole(const std::string& s) {
    std::string up = s;
    for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (up == "ADMIN") return UserRole::ADMIN;
    if (up == "EDITOR") return UserRole::EDITOR;
    if (up == "REVIEWER") return UserRole::REVIEWER;
    if (up == "AUTHOR") return UserRole::AUTHOR;
    if (up == "GUEST") return UserRole::GUEST;
    return UserRole::UNKNOWN;
}

constexpr size_t kMaxTreeEntries = 2000;
constexpr int kMaxTreeDepth = 16;

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

std::string normalizePathForCli(std::string path) {
    if (path.empty()) return "/";
    for (auto& c : path) {
        if (c == '\\') c = '/';
    }
    if (path.front() != '/') path.insert(path.begin(), '/');
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

// 基础 base64 解码（仅用于测试/CLI，避免引入外部依赖）
bool base64Decode(const std::string& in, std::string& out, std::string& errorMsg) {
    static constexpr unsigned char kInvalid = 0xFF;
    static unsigned char table[256];
    static bool inited = false;
    if (!inited) {
        std::fill(std::begin(table), std::end(table), kInvalid);
        const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (size_t i = 0; i < alphabet.size(); i++) {
            table[static_cast<unsigned char>(alphabet[i])] = static_cast<unsigned char>(i);
        }
        table[static_cast<unsigned char>('=')] = 0;
        inited = true;
    }

    std::string s;
    s.reserve(in.size());
    for (unsigned char c : in) {
        if (std::isspace(c)) continue;
        s.push_back(static_cast<char>(c));
    }
    if (s.empty() || (s.size() % 4) != 0) {
        errorMsg = "Invalid base64 length.";
        return false;
    }

    out.clear();
    out.reserve((s.size() / 4) * 3);

    for (size_t i = 0; i < s.size(); i += 4) {
        unsigned char c0 = table[static_cast<unsigned char>(s[i])];
        unsigned char c1 = table[static_cast<unsigned char>(s[i + 1])];
        unsigned char c2 = table[static_cast<unsigned char>(s[i + 2])];
        unsigned char c3 = table[static_cast<unsigned char>(s[i + 3])];

        if (c0 == kInvalid || c1 == kInvalid || c2 == kInvalid || c3 == kInvalid) {
            errorMsg = "Invalid base64 character.";
            return false;
        }

        const bool pad2 = (s[i + 2] == '=');
        const bool pad3 = (s[i + 3] == '=');
        if (pad2 && !pad3) {
            errorMsg = "Invalid base64 padding.";
            return false;
        }

        unsigned int triple = (static_cast<unsigned int>(c0) << 18)
                            | (static_cast<unsigned int>(c1) << 12)
                            | (static_cast<unsigned int>(c2) << 6)
                            | static_cast<unsigned int>(c3);

        out.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (!pad2) out.push_back(static_cast<char>((triple >> 8) & 0xFF));
        if (!pad3) out.push_back(static_cast<char>(triple & 0xFF));
    }

    return true;
}

bool looksLikePdf(const std::string& bytes) {
    // 最小校验：PDF 文件头必须是 "%PDF-"
    return bytes.size() >= 5 && std::string_view(bytes.data(), 5) == std::string_view("%PDF-");
}

bool looksLikeZip(const std::string& bytes) {
    // ZIP / DOCX: 'PK\x03\x04'
    return bytes.size() >= 4
        && static_cast<unsigned char>(bytes[0]) == 0x50
        && static_cast<unsigned char>(bytes[1]) == 0x4B
        && static_cast<unsigned char>(bytes[2]) == 0x03
        && static_cast<unsigned char>(bytes[3]) == 0x04;
}

bool looksLikeOleDoc(const std::string& bytes) {
    // old .doc OLE header: D0 CF 11 E0 A1 B1 1A E1
    static const unsigned char sig[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    if (bytes.size() < 8) return false;
    for (int i = 0; i < 8; i++) {
        if (static_cast<unsigned char>(bytes[i]) != sig[i]) return false;
    }
    return true;
}

bool looksLikeRtf(const std::string& bytes) {
    return bytes.size() >= 5 && std::string_view(bytes.data(), 5) == std::string_view("{\\rtf");
}

std::string normalizeExt(std::string ext) {
    ext = trim(ext);
    if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool isSafeExt(const std::string& ext) {
    if (ext.empty() || ext.size() > 10) return false;
    for (unsigned char c : ext) {
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    return true;
}

bool validateByExt(const std::string& ext, const std::string& bytes, std::string& errorMsg) {
    // 允许的主流格式：pdf/docx/doc/rtf/tex/txt/md
    if (ext == "pdf") {
        if (!looksLikePdf(bytes)) {
            errorMsg = "Invalid PDF format (missing %PDF- header).";
            return false;
        }
        return true;
    }
    if (ext == "docx") {
        if (!looksLikeZip(bytes)) {
            errorMsg = "Invalid DOCX format (missing PK zip header).";
            return false;
        }
        return true;
    }
    if (ext == "doc") {
        // 允许老 doc，但做一个最小签名检查；不通过也允许（很多 doc 也可能带不同头）
        if (!looksLikeOleDoc(bytes)) {
            // 放宽：不阻止上传
            return true;
        }
        return true;
    }
    if (ext == "rtf") {
        if (!looksLikeRtf(bytes)) {
            errorMsg = "Invalid RTF format (missing {\\rtf header).";
            return false;
        }
        return true;
    }
    if (ext == "tex" || ext == "txt" || ext == "md") {
        // 纯文本类不做严格校验
        return true;
    }

    errorMsg = "Unsupported file format: " + ext;
    return false;
}

bool treeWalk(FSProtocol* fs,
              const std::string& path,
              int depth,
              std::ostringstream& oss,
              size_t& emitted,
              std::string& errorMsg) {
    if (depth > kMaxTreeDepth) return true;
    if (emitted >= kMaxTreeEntries) return true;

    bool isDir = false;
    if (!fs->isDirectory(path, isDir, errorMsg)) return false;
    if (!isDir) return true;

    std::vector<std::string> entries;
    if (!fs->listDirectory(path, entries, errorMsg)) return false;

    for (const auto& e : entries) {
        if (emitted >= kMaxTreeEntries) break;
        for (int i = 0; i < depth; i++) oss << "  ";
        oss << e << "\n";
        emitted++;

        if (!e.empty() && e.back() == '/') {
            std::string child = path;
            if (child.size() > 1 && child.back() != '/') child.push_back('/');
            child += e.substr(0, e.size() - 1);
            // 递归
            if (!treeWalk(fs, child, depth + 1, oss, emitted, errorMsg)) return false;
        }
    }
    return true;
}

}

CLIProtocol::CLIProtocol(FSProtocol* fs,
                         Authenticator* auth,
                         PermissionChecker* perm,
                         BackupFlow* backup,
                         PaperService* paper,
                                                 ReviewFlow* review,
                                                 ICacheStatsProvider* cacheStatsProvider)
        : m_fs(fs),
            m_auth(auth),
            m_perm(perm),
            m_backupFlow(backup),
            m_paper(paper),
            m_reviewFlow(review),
            m_cacheStatsProvider(cacheStatsProvider) {}

bool CLIProtocol::processCommand(const std::string& command, std::string& response) {
    std::stringstream ss(command);
    std::string cmd;
    ss >> cmd;

    std::string errorMsg;

    if (cmd == "LOGIN") {
        std::string user, pass;
        ss >> user >> pass;
        std::string sessionId = m_auth->login(user, pass, errorMsg);
        if (!sessionId.empty()) {
            const UserRole role = m_auth->getUserRole(sessionId);
            response = "OK: " + sessionId + " ROLE=" + roleToString(role);
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "LOGOUT") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: LOGOUT <sessionToken>";
            return false;
        }
        if (m_auth->logout(sessionId, errorMsg)) {
            response = "OK: Logged out.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "HELP") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "OK: Commands: LOGIN, HELP";
            return true;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        std::ostringstream oss;
        oss << "OK: ROLE=" << roleToString(role) << "\n";
        oss << "Common: READ WRITE MKDIR PWD LS TREE STATUS PAPER_DOWNLOAD\n";
        if (role == UserRole::AUTHOR) oss << "Author: PAPER_UPLOAD PAPER_UPLOAD_FILE_B64 PAPER_UPLOAD_PDF_B64 PAPER_REVISE REVIEWS_DOWNLOAD\n";
        if (role == UserRole::REVIEWER) oss << "Reviewer: REVIEW_SUBMIT\n";
        if (role == UserRole::EDITOR) oss << "Editor: ASSIGN_REVIEWER DECIDE REVIEWS_DOWNLOAD\n";
        if (role == UserRole::ADMIN) oss << "Admin: USER_ADD USER_DEL USER_LIST BACKUP_CREATE BACKUP_LIST BACKUP_RESTORE SYSTEM_STATUS CACHE_STATS CACHE_CLEAR\n";
        response = oss.str();
    } else if (cmd == "CACHE_STATS") {
        std::string sessionId, paperId;
        ss >> sessionId >> paperId;  // paperId 是可选的
        if (sessionId.empty()) {
            response = "ERROR: Usage: CACHE_STATS <sessionToken> [paperId]";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        std::ostringstream oss;
        oss << "OK:";

        // 如果指定了论文ID，返回论文级统计
        if (!paperId.empty()) {
#if defined(SERVER_USE_REAL_FS) && SERVER_USE_REAL_FS
            // 尝试从 RealFileSystemAdapter 获取论文访问统计
            if (auto* realFS = dynamic_cast<RealFileSystemAdapter*>(m_fs)) {
                size_t accessCount = realFS->getPaperAccessCount(paperId);
                oss << " paperId=" << paperId
                    << " access_count=" << accessCount;
            } else {
                oss << " paperId=" << paperId
                    << " access_count=N/A";
            }
#else
            oss << " paperId=" << paperId
                << " access_count=N/A";
#endif
        }

        // 总是返回缓存统计
#if defined(SERVER_USE_REAL_FS) && SERVER_USE_REAL_FS
        if (auto* realFS = dynamic_cast<RealFileSystemAdapter*>(m_fs)) {
            size_t hits, misses, size, capacity;
            realFS->getBlockCacheStats(hits, misses, size, capacity);
            size_t total = hits + misses;
            double hitRate = (total > 0) ? (100.0 * hits / total) : 0.0;

            oss << " block_cache_hits=" << hits
                << " block_cache_misses=" << misses
                << " block_cache_hit_rate=" << std::fixed << std::setprecision(2) << hitRate << "%"
                << " block_cache_size=" << size
                << " block_cache_capacity=" << capacity;
        } else if (m_cacheStatsProvider) {
#else
        if (m_cacheStatsProvider) {
#endif
            // 回退到旧的文件级缓存统计
            const CacheStats s = m_cacheStatsProvider->cacheStats();
            oss << " file_cache_hits=" << s.hits
                << " file_cache_misses=" << s.misses
                << " file_cache_size=" << s.size
                << " file_cache_capacity=" << s.capacity;
        }

        response = oss.str();
    } else if (cmd == "CACHE_CLEAR") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: CACHE_CLEAR <sessionToken>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (!m_cacheStatsProvider) {
            response = "ERROR: Cache stats not available.";
            return false;
        }

        m_cacheStatsProvider->clearCache();
        response = "OK: Cache cleared.";
    } else if (cmd == "READ") {
        std::string sessionId, path, content;
        ss >> sessionId >> path;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: READ <sessionToken> <path>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::READ_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->readFile(path, content, errorMsg)) {
            response = "OK: " + content;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "WRITE") {
        std::string sessionId, path, content;
        ss >> sessionId >> path;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);

        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: WRITE <sessionToken> <path> <content>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::WRITE_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->writeFile(path, content, errorMsg)) {
            response = "OK: File written.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "MKDIR") {
        std::string sessionId, path;
        ss >> sessionId >> path;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: MKDIR <sessionToken> <path>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::MKDIR)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->createDirectory(path, errorMsg)) {
            response = "OK: Directory created.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PWD") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: PWD <sessionToken>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        // 当前协议为“一条命令一个连接”，没有 CD 语义；PWD 固定返回根目录。
        response = "OK: /";
    } else if (cmd == "LS") {
        std::string sessionId, path;
        ss >> sessionId;
        ss >> path;
        if (sessionId.empty()) {
            response = "ERROR: Usage: LS <sessionToken> [path]";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::READ_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        const std::string normPath = normalizePathForCli(path);
        std::vector<std::string> entries;
        if (!m_fs->listDirectory(normPath, entries, errorMsg)) {
            response = "ERROR: " + errorMsg;
            return false;
        }
        std::ostringstream oss;
        oss << "OK:";
        for (const auto& e : entries) oss << "\n" << e;
        response = oss.str();
    } else if (cmd == "TREE") {
        std::string sessionId, path;
        ss >> sessionId;
        ss >> path;
        if (sessionId.empty()) {
            response = "ERROR: Usage: TREE <sessionToken> [path]";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::READ_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        const std::string normPath = normalizePathForCli(path);
        bool isDir = false;
        if (!m_fs->isDirectory(normPath, isDir, errorMsg)) {
            response = "ERROR: " + errorMsg;
            return false;
        }

        std::ostringstream oss;
        oss << "OK:\n";
        oss << normPath;
        oss << (isDir ? "/" : "") << "\n";
        size_t emitted = 0;
        if (isDir) {
            if (!treeWalk(m_fs, normPath, 1, oss, emitted, errorMsg)) {
                response = "ERROR: " + errorMsg;
                return false;
            }
        }
        response = oss.str();
    } else if (cmd == "BACKUP" || cmd == "BACKUP_CREATE") {
        std::string sessionId, name;
        ss >> sessionId >> name;
        if (sessionId.empty()) {
            response = "ERROR: Usage: BACKUP_CREATE <sessionToken> [name]";
            return false;
        }
        // name 为空时由 flow 生成默认名称
        // 快照是全局的，不需要路径参数
        if (m_backupFlow->createBackup(sessionId, "/", name, errorMsg)) {
            response = "OK: Backup created. (快照包含整个文件系统，不包括用户账户)";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "BACKUP_LIST") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: BACKUP_LIST <sessionToken>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::BACKUP_LIST)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        auto names = m_fs->listSnapshots("/", errorMsg);
        if (!errorMsg.empty() && names.empty()) {
            response = "ERROR: " + errorMsg;
            return false;
        }
        std::ostringstream oss;
        oss << "OK:";
        for (const auto& n : names) oss << " " << n;
        response = oss.str();
    } else if (cmd == "BACKUP_RESTORE") {
        std::string sessionId, name;
        ss >> sessionId >> name;
        if (sessionId.empty() || name.empty()) {
            response = "ERROR: Usage: BACKUP_RESTORE <sessionToken> <name>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::BACKUP_RESTORE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (m_fs->restoreSnapshot(name, errorMsg)) {
            response = "OK: Restored. (已恢复文件系统，用户账户不受影响)";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "SYSTEM_STATUS") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: SYSTEM_STATUS <sessionToken>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        response = "OK: Server running. (FS stats not available via interface yet)";
    } else if (cmd == "SUBMIT_REVIEW") {
        std::string sessionId, operation, path;
        ss >> sessionId >> operation >> path;
        if (sessionId.empty() || operation.empty() || path.empty()) {
            response = "ERROR: Usage: SUBMIT_REVIEW <sessionToken> <operation> <path>";
            return false;
        }
        std::string reviewId = m_reviewFlow->submitForReview(sessionId, operation, path, errorMsg);
        if (!reviewId.empty()) {
            response = "OK: Review submitted with ID " + reviewId;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PAPER_UPLOAD") {
        std::cout << "[CLIProtocol] PAPER_UPLOAD command received" << std::endl;
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        
        std::cout << "[CLIProtocol] Parsed - sessionId: " << sessionId.substr(0, 20) << "..." << std::endl;
        std::cout << "[CLIProtocol] Parsed - paperId: " << paperId << std::endl;
        std::cout << "[CLIProtocol] Parsed - content length: " << content.length() << " bytes" << std::endl;
        
        if (sessionId.empty() || paperId.empty()) {
            std::cout << "[CLIProtocol] ❌ Missing parameters" << std::endl;
            response = "ERROR: Usage: PAPER_UPLOAD <sessionToken> <paperId> <content>";
            return false;
        }
        
        std::cout << "[CLIProtocol] Calling m_paper->uploadPaper()..." << std::endl;
        if (m_paper->uploadPaper(sessionId, paperId, content, errorMsg)) {
            std::cout << "[CLIProtocol] ✓ Upload successful" << std::endl;
            response = "OK: Paper uploaded.";
        } else {
            std::cout << "[CLIProtocol] ❌ Upload failed: " << errorMsg << std::endl;
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PAPER_UPLOAD_FILE_B64") {
        // 统一的二进制文件上传：base64 + 扩展名
        std::string sessionId, paperId, ext;
        ss >> sessionId >> paperId >> ext;
        std::string b64;
        std::getline(ss, b64);
        b64 = trim(b64);

        if (sessionId.empty() || paperId.empty() || ext.empty() || b64.empty()) {
            response = "ERROR: Usage: PAPER_UPLOAD_FILE_B64 <sessionToken> <paperId> <ext> <base64>";
            return false;
        }

        ext = normalizeExt(ext);
        if (!isSafeExt(ext)) {
            response = "ERROR: Invalid file extension.";
            return false;
        }

        std::string bytes;
        if (!base64Decode(b64, bytes, errorMsg)) {
            response = "ERROR: base64 decode failed: " + errorMsg;
            return false;
        }

        // 避免超大 payload
        if (bytes.size() > 15 * 1024 * 1024) {
            response = "ERROR: File too large (max 15MB).";
            return false;
        }

        if (!validateByExt(ext, bytes, errorMsg)) {
            response = "ERROR: " + errorMsg;
            return false;
        }

        if (!m_paper->uploadPaperFile(sessionId, paperId, ext, bytes, errorMsg)) {
            response = "ERROR: " + errorMsg;
            return false;
        }

        response = "OK: File uploaded.";
    } else if (cmd == "PAPER_UPLOAD_PDF_B64") {
        // 兼容命令：等价于 PAPER_UPLOAD_FILE_B64 ... pdf
        std::string sessionId, paperId;
        ss >> sessionId >> paperId;
        std::string b64;
        std::getline(ss, b64);
        b64 = trim(b64);

        if (sessionId.empty() || paperId.empty() || b64.empty()) {
            response = "ERROR: Usage: PAPER_UPLOAD_PDF_B64 <sessionToken> <paperId> <base64>";
            return false;
        }

        std::string bytes;
        if (!base64Decode(b64, bytes, errorMsg)) {
            response = "ERROR: base64 decode failed: " + errorMsg;
            return false;
        }
        if (!looksLikePdf(bytes)) {
            response = "ERROR: Invalid PDF format (missing %PDF- header).";
            return false;
        }
        if (!m_paper->uploadPaperFile(sessionId, paperId, "pdf", bytes, errorMsg)) {
            response = "ERROR: " + errorMsg;
            return false;
        }

        response = "OK: PDF uploaded.";
    } else if (cmd == "PAPER_REVISE") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: PAPER_REVISE <sessionToken> <paperId> <content>";
            return false;
        }
        if (m_paper->submitRevision(sessionId, paperId, content, errorMsg)) {
            response = "OK: Revision submitted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PAPER_DOWNLOAD") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: PAPER_DOWNLOAD <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->downloadPaper(sessionId, paperId, content, errorMsg)) {
            response = "OK: " + content;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "STATUS") {
        std::string sessionId, paperId, out;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: STATUS <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->getStatus(sessionId, paperId, out, errorMsg)) {
            response = "OK:\n" + out;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "ASSIGN_REVIEWER") {
        std::string sessionId, paperId, reviewer;
        ss >> sessionId >> paperId >> reviewer;
        if (sessionId.empty() || paperId.empty() || reviewer.empty()) {
            response = "ERROR: Usage: ASSIGN_REVIEWER <sessionToken> <paperId> <reviewerUsername>";
            return false;
        }
        if (m_paper->assignReviewer(sessionId, paperId, reviewer, errorMsg)) {
            response = "OK: Reviewer assigned.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "REVIEW_SUBMIT") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: REVIEW_SUBMIT <sessionToken> <paperId> <reviewContent>";
            return false;
        }
        if (m_paper->submitReview(sessionId, paperId, content, errorMsg)) {
            response = "OK: Review submitted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "REVIEWS_DOWNLOAD") {
        std::string sessionId, paperId, out;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: REVIEWS_DOWNLOAD <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->downloadReviews(sessionId, paperId, out, errorMsg)) {
            response = "OK:\n" + out;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "DECIDE") {
        std::string sessionId, paperId, decision;
        ss >> sessionId >> paperId >> decision;
        if (sessionId.empty() || paperId.empty() || decision.empty()) {
            response = "ERROR: Usage: DECIDE <sessionToken> <paperId> <ACCEPT|REJECT>";
            return false;
        }
        if (m_paper->finalDecision(sessionId, paperId, decision, errorMsg)) {
            response = "OK: Decision recorded.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_ADD") {
        std::string sessionId, username, password, roleStr;
        ss >> sessionId >> username >> password >> roleStr;
        if (sessionId.empty() || username.empty() || password.empty() || roleStr.empty()) {
            response = "ERROR: Usage: USER_ADD <sessionToken> <username> <password> <ADMIN|EDITOR|REVIEWER|AUTHOR|GUEST>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        const UserRole role = parseRole(roleStr);
        if (role == UserRole::UNKNOWN) {
            response = "ERROR: Invalid role.";
            return false;
        }
        if (m_auth->addUser(username, password, role, errorMsg)) {
            response = "OK: User added.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_DEL") {
        std::string sessionId, username;
        ss >> sessionId >> username;
        if (sessionId.empty() || username.empty()) {
            response = "ERROR: Usage: USER_DEL <sessionToken> <username>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (m_auth->deleteUser(username, errorMsg)) {
            response = "OK: User deleted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_LIST") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: USER_LIST <sessionToken>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        auto users = m_auth->listUsers(errorMsg);
        if (!errorMsg.empty() && users.empty()) {
            response = "ERROR: " + errorMsg;
            return false;
        }
        std::ostringstream oss;
        oss << "OK:";
        for (const auto& [name, role] : users) {
            oss << "\n" << name << " " << roleToString(role);
        }
        response = oss.str();
    } else {
        response = "ERROR: Unknown command '" + cmd + "'";
        return false;
    }

    return true;
}