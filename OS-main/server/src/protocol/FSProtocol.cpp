#include "../../include/protocol/FSProtocol.h"
#include <memory>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../../include/cache/LRUCache.h"
#include "../../include/cache/CacheStatsProvider.h"

namespace {

std::string normalizePath(std::string path) {
    if (path.empty()) return "/";
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.front() != '/') path.insert(path.begin(), '/');
    // 移除末尾多余的 '/'
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

std::string parentDir(const std::string& path) {
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0) return "/";
    return path.substr(0, pos);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string makeId(const char* prefix) {
    using Clock = std::chrono::system_clock;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << prefix << ms;
    return oss.str();
}

}

// 具体的文件系统协议实现类
class RealFSProtocol : public FSProtocol {
public:
    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        if (snapshotName.empty()) {
            errorMsg = "snapshotName is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        std::unordered_map<std::string, std::string> captured;

        // 捕获该路径前缀下的所有文件（演示用：简单前缀匹配）
        const std::string prefix = (normPath == "/") ? "/" : (normPath + "/");
        for (const auto& [filePath, content] : m_files) {
            if (normPath == "/" || filePath == normPath || startsWith(filePath, prefix)) {
                captured[filePath] = content;
            }
        }

        m_snapshots[snapshotName] = std::move(captured);
        return true;
    }

    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_snapshots.find(snapshotName);
        if (it == m_snapshots.end()) {
            errorMsg = "Snapshot not found.";
            return false;
        }

        // 演示用：恢复为快照内容（覆盖同名文件；不会删除快照里不存在但当前存在的文件）
        for (const auto& [filePath, content] : it->second) {
            m_dirs.insert(parentDir(filePath));
            m_files[filePath] = content;
        }

        return true;
    }

    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override {
        (void)path;
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        std::vector<std::string> names;
        names.reserve(m_snapshots.size());
        for (const auto& [name, _] : m_snapshots) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        auto it = m_files.find(normPath);
        if (it == m_files.end()) {
            errorMsg = "File not found.";
            return false;
        }
        content = it->second;
        return true;
    }

    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        const std::string dir = parentDir(normPath);
        std::scoped_lock lock(m_mutex);

        // 演示用：自动创建父目录
        m_dirs.insert(dir);
        m_files[normPath] = content;
        (void)errorMsg;
        return true;
    }

    bool deleteFile(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        if (m_files.erase(normPath) == 0) {
            errorMsg = "File not found.";
            return false;
        }
        return true;
    }

    bool createDirectory(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);
        m_dirs.insert(normPath);
        (void)errorMsg;
        return true;
    }

    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override {
        (void)path;
        (void)user;
        (void)errorMsg;
        // 目前权限由 server 的 PermissionChecker 统一管理；FS层接口先返回占位。
        return "managed_by_server";
    }

    std::string submitForReview(const std::string& operation, const std::string& path, 
                                       const std::string& user, std::string& errorMsg) override {
        if (operation.empty()) {
            errorMsg = "operation is empty.";
            return {};
        }

        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        const std::string reviewId = makeId("review_");
        m_reviews[reviewId] = ReviewRequest{operation, normPath, user};
        (void)errorMsg;
        return reviewId;
    }

    bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        if (m_dirs.find(normPath) == m_dirs.end()) {
            errorMsg = "Directory not found: " + normPath;
            return false;
        }

        std::unordered_set<std::string> seen;
        std::vector<std::string> out;

        // 子目录
        for (const auto& d : m_dirs) {
            if (d == "/") continue;
            if (parentDir(d) != normPath) continue;
            const auto pos = d.find_last_of('/');
            const std::string name = (pos == std::string::npos) ? d : d.substr(pos + 1);
            if (name.empty()) continue;
            const std::string decorated = name + "/";
            if (seen.insert(decorated).second) out.push_back(decorated);
        }

        // 子文件
        for (const auto& [p, _content] : m_files) {
            if (parentDir(p) != normPath) continue;
            const auto pos = p.find_last_of('/');
            const std::string name = (pos == std::string::npos) ? p : p.substr(pos + 1);
            if (name.empty()) continue;
            if (seen.insert(name).second) out.push_back(name);
        }

        std::sort(out.begin(), out.end());
        entries = std::move(out);
        return true;
    }

    bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        if (m_dirs.find(normPath) != m_dirs.end()) {
            isDirOut = true;
            return true;
        }
        if (m_files.find(normPath) != m_files.end()) {
            isDirOut = false;
            return true;
        }
        errorMsg = "Path not found: " + normPath;
        return false;
    }

private:
    struct ReviewRequest {
        std::string operation;
        std::string path;
        std::string user;
    };

    std::mutex m_mutex;
    std::unordered_set<std::string> m_dirs{"/"};
    std::unordered_map<std::string, std::string> m_files;
    // snapshotName -> (filePath -> content)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_snapshots;
    std::unordered_map<std::string, ReviewRequest> m_reviews;
};

class CachingFSProtocol : public FSProtocol, public ICacheStatsProvider {
public:
    explicit CachingFSProtocol(std::unique_ptr<FSProtocol> inner, size_t capacity)
        : m_inner(std::move(inner)), m_cache(capacity), m_capacity(capacity) {}

    CacheStats cacheStats() const override {
        // LRUCache 内部已加锁，无需外部锁
        return CacheStats{
            m_cache.hits(),
            m_cache.misses(),
            m_cache.size(),
            m_capacity,
        };
    }

    void clearCache() override {
        // LRUCache 内部已加锁，无需外部锁
        m_cache.clear();
    }

    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override {
        return m_inner->createSnapshot(path, snapshotName, errorMsg);
    }

    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override {
        // 恢复会大规模改变内容：直接清空缓存
        // LRUCache 内部已加锁，无需外部锁
        m_cache.clear();
        return m_inner->restoreSnapshot(snapshotName, errorMsg);
    }

    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override {
        return m_inner->listSnapshots(path, errorMsg);
    }

    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override {
        const std::string key = normalizePath(path);

        // LRUCache 内部已加锁，无需外部锁
        if (auto v = m_cache.tryGet(key)) {
            content = *v;
            return true;
        }

        if (!m_inner->readFile(key, content, errorMsg)) {
            return false;
        }

        // LRUCache 内部已加锁，无需外部锁
        m_cache.put(key, content);
        return true;
    }

    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override {
        const std::string key = normalizePath(path);
        if (!m_inner->writeFile(key, content, errorMsg)) return false;
        // LRUCache 内部已加锁，无需外部锁
        m_cache.put(key, content);
        return true;
    }

    bool deleteFile(const std::string& path, std::string& errorMsg) override {
        const std::string key = normalizePath(path);
        // LRUCache 内部已加锁，无需外部锁
        m_cache.erase(key);
        return m_inner->deleteFile(key, errorMsg);
    }

    bool createDirectory(const std::string& path, std::string& errorMsg) override {
        return m_inner->createDirectory(path, errorMsg);
    }

    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override {
        return m_inner->getFilePermission(path, user, errorMsg);
    }

    std::string submitForReview(const std::string& operation, const std::string& path,
                                const std::string& user, std::string& errorMsg) override {
        return m_inner->submitForReview(operation, path, user, errorMsg);
    }

    bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) override {
        return m_inner->listDirectory(path, entries, errorMsg);
    }

    bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) override {
        return m_inner->isDirectory(path, isDirOut, errorMsg);
    }

private:
    std::unique_ptr<FSProtocol> m_inner;
    LRUCache<std::string, std::string> m_cache;  // 线程安全的LRU缓存
    size_t m_capacity;
    // 注意：m_cacheMutex 已移除，因为 LRUCache 内部已实现线程安全
};

// 引入真实文件系统适配器
#include "../../include/protocol/RealFileSystemAdapter.h"

// 工厂函数：供 ProtocolFactory 使用
std::unique_ptr<FSProtocol> createFSProtocol() {
    // server侧默认启用一个小容量文件内容缓存，以匹配架构设计中的 Cache(LRU)

#if defined(SERVER_USE_REAL_FS) && SERVER_USE_REAL_FS
    // 使用真实的 FileSystem 适配器
    // 磁盘镜像路径：相对于 server 可执行文件的位置
    const std::string diskPath = "../../filesystem/disk/disk.img";

    try {
        auto real = std::make_unique<RealFileSystemAdapter>(diskPath);
        return std::make_unique<CachingFSProtocol>(std::move(real), 64);
    } catch (const std::exception& e) {
        std::cerr << "⚠️  Failed to initialize real filesystem, falling back to in-memory: "
                  << e.what() << std::endl;
        // 如果真实文件系统初始化失败，回退到内存版本
        auto real = std::make_unique<RealFSProtocol>();
        return std::make_unique<CachingFSProtocol>(std::move(real), 64);
    }
#else
    // Windows/MSVC 默认使用内存版本，避免引入 filesystem 源码的 POSIX 依赖
    auto real = std::make_unique<RealFSProtocol>();
    return std::make_unique<CachingFSProtocol>(std::move(real), 64);
#endif
}