#pragma once

#include "FSProtocol.h"
#include <string>
#include <mutex>
#include <unordered_map>

// 前向声明，避免包含整个 filesystem 头文件
struct Inode;

/**
 * RealFileSystemAdapter - 真实文件系统适配器
 * 
 * 将 FSProtocol 接口适配到实际的 filesystem 模块的 C API
 * 使用全局互斥锁保证多线程环境下的原子性
 */
class RealFileSystemAdapter : public FSProtocol {
public:
    /**
     * 构造函数
     * @param diskPath 磁盘镜像文件路径
     */
    explicit RealFileSystemAdapter(const std::string& diskPath);
    
    /**
     * 析构函数 - 关闭磁盘文件
     */
    ~RealFileSystemAdapter() override;

    // 禁止拷贝和移动
    RealFileSystemAdapter(const RealFileSystemAdapter&) = delete;
    RealFileSystemAdapter& operator=(const RealFileSystemAdapter&) = delete;

    // FSProtocol 接口实现
    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override;
    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override;
    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override;
    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override;
    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override;
    bool deleteFile(const std::string& path, std::string& errorMsg) override;
    bool createDirectory(const std::string& path, std::string& errorMsg) override;
    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override;
    std::string submitForReview(const std::string& operation, const std::string& path, 
                                const std::string& user, std::string& errorMsg) override;

    // 目录遍历/类型查询
    bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) override;
    bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) override;

    // 新增：获取论文访问统计
    size_t getPaperAccessCount(const std::string& paperId) const;
    
    // 新增：获取 block cache 统计
    void getBlockCacheStats(size_t& hits, size_t& misses, size_t& size, size_t& capacity) const;

private:
    int m_fd;                    // 磁盘文件描述符
    mutable std::mutex m_mutex;  // 全局互斥锁，保护所有 filesystem 操作
    
    // 论文访问统计（paperId -> 访问次数）
    mutable std::unordered_map<std::string, size_t> m_paperAccessCounts;
    
    // 辅助函数：路径解析
    int pathToInodeId(const std::string& path, std::string& errorMsg);
    
    // 辅助函数：获取父目录 inode ID 和文件名
    bool getParentAndName(const std::string& path, int& parentInodeId, std::string& name, std::string& errorMsg);
    
    // 辅助函数：递归创建目录
    bool ensureDirectoryExists(const std::string& path, std::string& errorMsg);
    
    // 内部函数（不加锁，调用者已持有锁）
    bool ensureDirectoryExistsInternal(const std::string& path, std::string& errorMsg);
    bool createDirectoryInternal(const std::string& path, std::string& errorMsg);
    
    // 辅助函数：规范化路径
    std::string normalizePath(const std::string& path);
};

