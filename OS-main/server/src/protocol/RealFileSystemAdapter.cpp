#include "../../include/protocol/RealFileSystemAdapter.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

// 引入 filesystem 的 C API
#include "disk.h"
#include "inode.h"
#include "path.h"
#include "block_cache.h"

// ==================== 构造和析构 ====================

RealFileSystemAdapter::RealFileSystemAdapter(const std::string& diskPath) {
    m_fd = disk_open(diskPath.c_str());
    if (m_fd < 0) {
        std::cerr << "❌ Failed to open disk image: " << diskPath << std::endl;
        throw std::runtime_error("Failed to open disk image: " + diskPath);
    }
    
    // 禁用块缓存以确保多线程环境下的数据一致性
    // 底层文件系统代码直接使用 read_block/write_block，绕过缓存
    // 在多线程环境下，缓存和磁盘数据可能不一致
    block_cache_init(0);  // 容量为 0 表示禁用缓存
    
    std::cout << "✅ Filesystem adapter initialized with disk: " << diskPath << std::endl;
}

RealFileSystemAdapter::~RealFileSystemAdapter() {
    if (m_fd >= 0) {
        // 刷新并销毁块缓存
        block_cache_flush(m_fd);
        block_cache_destroy();
        
        disk_close(m_fd);
        std::cout << "✅ Filesystem adapter closed" << std::endl;
    }
}

// ==================== 辅助函数 ====================

std::string RealFileSystemAdapter::normalizePath(const std::string& path) {
    if (path.empty()) return "/";
    
    std::string normalized = path;
    // 替换反斜杠为正斜杠
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // 确保以 / 开头
    if (normalized.front() != '/') {
        normalized.insert(normalized.begin(), '/');
    }
    
    // 移除末尾的 /（除非是根目录）
    while (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

int RealFileSystemAdapter::pathToInodeId(const std::string& path, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    // 根目录特殊处理
    if (normPath == "/") {
        return 0;  // 根目录的 inode ID 总是 0
    }
    
    int inodeId = get_inode_by_path(m_fd, normPath.c_str());
    if (inodeId < 0) {
        errorMsg = "Path not found: " + normPath;
    }
    return inodeId;
}

bool RealFileSystemAdapter::getParentAndName(const std::string& path, int& parentInodeId, 
                                             std::string& name, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    if (normPath == "/") {
        errorMsg = "Cannot get parent of root directory";
        return false;
    }
    
    char nameBuf[DIR_NAME_SIZE];
    int result = get_parent_inode_and_name(m_fd, normPath.c_str(), &parentInodeId, nameBuf);
    
    if (result < 0) {
        errorMsg = "Failed to get parent directory for: " + normPath;
        return false;
    }
    
    name = std::string(nameBuf);
    return true;
}

bool RealFileSystemAdapter::ensureDirectoryExists(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ensureDirectoryExistsInternal(path, errorMsg);
}

// 内部函数，不加锁（调用者已持有锁）
bool RealFileSystemAdapter::ensureDirectoryExistsInternal(const std::string& path, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    std::cout << "[ensureDir] Ensuring " << normPath << " exists" << std::endl;
    
    // 根目录总是存在
    if (normPath == "/") {
        std::cout << "[ensureDir] Root directory, returning true" << std::endl;
        return true;
    }
    
    // 检查目录是否已存在
    int inodeId = get_inode_by_path(m_fd, normPath.c_str());
    std::cout << "[ensureDir] get_inode_by_path returned: " << inodeId << std::endl;
    if (inodeId >= 0) {
        // 目录已存在，检查是否真的是目录
        Inode inode;
        if (read_inode(m_fd, inodeId, &inode) < 0) {
            errorMsg = "Failed to read inode for: " + normPath;
            std::cout << "[ensureDir] ❌ Failed to read inode" << std::endl;
            return false;
        }
        
        if (inode.type != INODE_TYPE_DIR) {
            errorMsg = "Path exists but is not a directory: " + normPath;
            std::cout << "[ensureDir] ❌ Path exists but not a directory" << std::endl;
            return false;
        }
        
        std::cout << "[ensureDir] ✓ Directory already exists" << std::endl;
        return true;  // 目录已存在
    }
    
    // 目录不存在，需要创建
    std::cout << "[ensureDir] Directory does not exist, need to create" << std::endl;
    // 首先确保父目录存在
    size_t lastSlash = normPath.find_last_of('/');
    if (lastSlash == std::string::npos) {
        errorMsg = "Invalid path: " + normPath;
        return false;
    }
    
    std::string parentPath = (lastSlash == 0) ? "/" : normPath.substr(0, lastSlash);
    std::cout << "[ensureDir] Recursively ensuring parent: " << parentPath << std::endl;
    if (!ensureDirectoryExistsInternal(parentPath, errorMsg)) {
        std::cout << "[ensureDir] ❌ Failed to ensure parent: " << errorMsg << std::endl;
        return false;
    }
    
    // 创建当前目录（使用内部函数，避免重复加锁）
    std::cout << "[ensureDir] Creating directory: " << normPath << std::endl;
    return createDirectoryInternal(normPath, errorMsg);
}

// ==================== FSProtocol 接口实现 ====================

bool RealFileSystemAdapter::createSnapshot(const std::string& path, const std::string& snapshotName, 
                                           std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    (void)path;  // 当前 filesystem 的快照是全局的，不支持路径级快照
    
    if (snapshotName.empty()) {
        errorMsg = "Snapshot name cannot be empty";
        return false;
    }
    
    int result = create_snapshot(m_fd, snapshotName.c_str());
    if (result < 0) {
        errorMsg = "Failed to create snapshot: " + snapshotName;
        return false;
    }
    
    std::cout << "✅ Created snapshot: " << snapshotName << " (ID: " << result << ")" << std::endl;
    return true;
}

bool RealFileSystemAdapter::restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (snapshotName.empty()) {
        errorMsg = "Snapshot name cannot be empty";
        return false;
    }
    
    // 查找快照 ID
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(m_fd, snapshots, MAX_SNAPSHOTS);
    
    if (count < 0) {
        errorMsg = "Failed to list snapshots";
        return false;
    }
    
    int snapshotId = -1;
    for (int i = 0; i < count; ++i) {
        if (snapshots[i].active && snapshotName == snapshots[i].name) {
            snapshotId = snapshots[i].id;
            break;
        }
    }
    
    if (snapshotId < 0) {
        errorMsg = "Snapshot not found: " + snapshotName;
        return false;
    }
    
    int result = restore_snapshot(m_fd, snapshotId);
    if (result < 0) {
        errorMsg = "Failed to restore snapshot: " + snapshotName;
        return false;
    }
    
    std::cout << "✅ Restored snapshot: " << snapshotName << " (ID: " << snapshotId << ")" << std::endl;
    return true;
}

std::vector<std::string> RealFileSystemAdapter::listSnapshots(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    (void)path;  // 当前 filesystem 的快照是全局的
    
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(m_fd, snapshots, MAX_SNAPSHOTS);
    
    if (count < 0) {
        errorMsg = "Failed to list snapshots";
        return {};
    }
    
    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) {
        if (snapshots[i].active) {
            names.push_back(snapshots[i].name);
        }
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

bool RealFileSystemAdapter::readFile(const std::string& path, std::string& content, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 跟踪论文访问：如果路径包含 /papers/，提取论文ID并增加计数
    if (normPath.find("/papers/") == 0) {
        size_t start = 8;  // "/papers/" 的长度
        size_t end = normPath.find('/', start);
        if (end != std::string::npos) {
            std::string paperId = normPath.substr(start, end - start);
            if (!paperId.empty()) {
                m_paperAccessCounts[paperId]++;
            }
        }
    }
    
    // 获取文件的 inode ID
    int inodeId = pathToInodeId(normPath, errorMsg);
    if (inodeId < 0) {
        return false;
    }
    
    // 读取 inode
    Inode inode;
    if (read_inode(m_fd, inodeId, &inode) < 0) {
        errorMsg = "Failed to read inode for: " + normPath;
        return false;
    }
    
    // 检查是否是文件
    if (inode.type != INODE_TYPE_FILE) {
        errorMsg = "Path is not a file: " + normPath;
        return false;
    }
    
    // 读取文件内容
    if (inode.size == 0) {
        content = "";
        return true;
    }
    
    std::vector<char> buffer(inode.size);
    int bytesRead = inode_read_data(m_fd, &inode, buffer.data(), 0, inode.size);
    
    if (bytesRead < 0 || bytesRead != inode.size) {
        errorMsg = "Failed to read file data: " + normPath;
        return false;
    }
    
    content.assign(buffer.begin(), buffer.end());
    return true;
}

bool RealFileSystemAdapter::writeFile(const std::string& path, const std::string& content, 
                                      std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 首先确保父目录存在（使用内部函数，避免重复加锁）
    size_t lastSlash = normPath.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        // 路径在根目录下
        lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash;
    }
    std::string parentPath = (lastSlash == 0) ? "/" : normPath.substr(0, lastSlash);
    std::string fileName = normPath.substr(lastSlash + 1);
    
    if (fileName.empty()) {
        errorMsg = "Invalid file path: no filename";
        return false;
    }
    
    if (!ensureDirectoryExistsInternal(parentPath, errorMsg)) {
        return false;
    }
    
    // 获取父目录 inode（目录现在一定存在）
    int parentInodeId = pathToInodeId(parentPath, errorMsg);
    if (parentInodeId < 0) {
        return false;
    }
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 检查文件是否已存在
    int fileInodeId = dir_find_entry(m_fd, &parentInode, fileName.c_str());
    
    Inode fileInode;
    if (fileInodeId < 0) {
        // 文件不存在，创建新文件
        fileInodeId = alloc_inode(m_fd);
        if (fileInodeId < 0) {
            errorMsg = "Failed to allocate inode for new file";
            return false;
        }
        
        init_inode(&fileInode, INODE_TYPE_FILE);
        
        // 添加目录条目
        int addResult = dir_add_entry(m_fd, &parentInode, parentInodeId, fileName.c_str(), fileInodeId);
        if (addResult < 0) {
            free_inode(m_fd, fileInodeId);
            if (addResult == -2) {
                errorMsg = "File entry already exists: " + fileName;
            } else if (addResult == -3) {
                errorMsg = "Failed to write directory entry (disk may be full)";
            } else {
                errorMsg = "Failed to add directory entry";
            }
            return false;
        }
    } else {
        // 文件已存在，读取 inode
        if (read_inode(m_fd, fileInodeId, &fileInode) < 0) {
            errorMsg = "Failed to read existing file inode";
            return false;
        }
        
        if (fileInode.type != INODE_TYPE_FILE) {
            errorMsg = "Path exists but is not a file: " + normPath;
            return false;
        }
        
        // 清空旧内容
        if (fileInode.block_count > 0) {
            inode_free_blocks(m_fd, &fileInode);
        }
    }
    
    // 写入新内容
    if (!content.empty()) {
        int bytesWritten = inode_write_data(m_fd, &fileInode, fileInodeId, 
                                            content.c_str(), 0, content.length());
        if (bytesWritten < 0 || bytesWritten != static_cast<int>(content.length())) {
            errorMsg = "Failed to write file data";
            return false;
        }
    } else {
        // 空文件，只需要更新 inode
        fileInode.size = 0;
        write_inode(m_fd, fileInodeId, &fileInode);
    }
    
    return true;
}

bool RealFileSystemAdapter::deleteFile(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 不能删除根目录
    if (normPath == "/") {
        errorMsg = "Cannot delete root directory";
        return false;
    }
    
    // 获取父目录和文件名
    int parentInodeId;
    std::string fileName;
    if (!getParentAndName(normPath, parentInodeId, fileName, errorMsg)) {
        return false;
    }
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 查找文件
    int fileInodeId = dir_find_entry(m_fd, &parentInode, fileName.c_str());
    if (fileInodeId < 0) {
        errorMsg = "File not found: " + normPath;
        return false;
    }
    
    // 读取文件 inode
    Inode fileInode;
    if (read_inode(m_fd, fileInodeId, &fileInode) < 0) {
        errorMsg = "Failed to read file inode";
        return false;
    }
    
    // 只能删除文件，不能删除目录（需要单独的 rmdir）
    if (fileInode.type == INODE_TYPE_DIR) {
        errorMsg = "Cannot delete directory using deleteFile: " + normPath;
        return false;
    }
    
    // 释放数据块
    if (fileInode.block_count > 0) {
        inode_free_blocks(m_fd, &fileInode);
    }
    
    // 释放 inode
    free_inode(m_fd, fileInodeId);
    
    // 从父目录中删除条目
    if (dir_remove_entry(m_fd, &parentInode, parentInodeId, fileName.c_str()) < 0) {
        errorMsg = "Failed to remove directory entry";
        return false;
    }
    
    return true;
}

bool RealFileSystemAdapter::isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::string normPath = normalizePath(path);
    int inodeId = pathToInodeId(normPath, errorMsg);
    if (inodeId < 0) return false;

    Inode inode;
    if (read_inode(m_fd, inodeId, &inode) < 0) {
        errorMsg = "Failed to read inode for: " + normPath;
        return false;
    }

    if (inode.type == INODE_TYPE_DIR) {
        isDirOut = true;
        return true;
    }
    if (inode.type == INODE_TYPE_FILE) {
        isDirOut = false;
        return true;
    }

    errorMsg = "Unknown inode type for: " + normPath;
    return false;
}

bool RealFileSystemAdapter::listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::string normPath = normalizePath(path);
    int dirInodeId = pathToInodeId(normPath, errorMsg);
    if (dirInodeId < 0) return false;

    Inode dirInode;
    if (read_inode(m_fd, dirInodeId, &dirInode) < 0) {
        errorMsg = "Failed to read inode for: " + normPath;
        return false;
    }
    if (dirInode.type != INODE_TYPE_DIR) {
        errorMsg = "Path is not a directory: " + normPath;
        return false;
    }

    const int entryCount = dirInode.size / static_cast<int>(sizeof(DirEntry));
    std::vector<std::string> out;
    out.reserve(std::max(0, entryCount));

    for (int i = 0; i < entryCount; i++) {
        DirEntry e{};
        if (dir_get_entry(m_fd, &dirInode, i, &e) < 0) {
            continue;
        }
        if (e.inode_id < 0) continue;
        if (e.name[0] == '\0') continue;

        std::string name(e.name);
        if (name.empty()) continue;

        Inode child;
        if (read_inode(m_fd, e.inode_id, &child) < 0) {
            // 读不到类型就按文件展示
            out.push_back(name);
            continue;
        }

        if (child.type == INODE_TYPE_DIR) {
            out.push_back(name + "/");
        } else {
            out.push_back(name);
        }
    }

    std::sort(out.begin(), out.end());
    entries = std::move(out);
    return true;
}

bool RealFileSystemAdapter::createDirectory(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return createDirectoryInternal(path, errorMsg);
}

// 内部函数，不加锁（调用者已持有锁）
bool RealFileSystemAdapter::createDirectoryInternal(const std::string& path, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    // 根目录已存在
    if (normPath == "/") {
        return true;
    }
    
    // 检查目录是否已存在
    int existingInodeId = get_inode_by_path(m_fd, normPath.c_str());
    if (existingInodeId >= 0) {
        // 目录已存在，这不是错误（幂等操作）
        std::cout << "[createDir] " << normPath << " already exists (inode " << existingInodeId << ")" << std::endl;
        return true;
    }
    
    // 解析路径：获取父目录路径和目录名
    size_t lastSlash = normPath.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : normPath.substr(0, lastSlash);
    std::string dirName = normPath.substr(lastSlash + 1);
    
    if (dirName.empty()) {
        errorMsg = "Invalid directory path: no directory name";
        return false;
    }
    
    std::cout << "[createDir] Creating " << normPath << " (parent=" << parentPath << ", name=" << dirName << ")" << std::endl;
    
    // 确保父目录存在（递归创建）
    if (!ensureDirectoryExistsInternal(parentPath, errorMsg)) {
        std::cout << "[createDir] Failed to ensure parent exists: " << errorMsg << std::endl;
        return false;
    }
    
    // 获取父目录 inode（父目录现在一定存在）
    int parentInodeId = pathToInodeId(parentPath, errorMsg);
    if (parentInodeId < 0) {
        std::cout << "[createDir] Parent not found after ensureExists: " << parentPath << std::endl;
        return false;
    }
    
    std::cout << "[createDir] Parent inode id: " << parentInodeId << std::endl;
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 分配新的 inode
    std::cout << "[createDir] Allocating inode for " << dirName << std::endl;
    int newDirInodeId = alloc_inode(m_fd);
    if (newDirInodeId < 0) {
        errorMsg = "Failed to allocate inode for new directory";
        std::cout << "[createDir] ❌ Failed to allocate inode" << std::endl;
        return false;
    }
    std::cout << "[createDir] ✓ Allocated inode " << newDirInodeId << std::endl;
    
    // 初始化目录 inode
    Inode newDirInode;
    init_inode(&newDirInode, INODE_TYPE_DIR);
    write_inode(m_fd, newDirInodeId, &newDirInode);
    std::cout << "[createDir] ✓ Initialized and wrote inode" << std::endl;
    
    // 添加到父目录
    std::cout << "[createDir] Adding entry '" << dirName << "' to parent inode " << parentInodeId << std::endl;
    int addResult = dir_add_entry(m_fd, &parentInode, parentInodeId, dirName.c_str(), newDirInodeId);
    std::cout << "[createDir] dir_add_entry result: " << addResult << " for " << dirName << std::endl;
    if (addResult < 0) {
        free_inode(m_fd, newDirInodeId);
        if (addResult == -2) {
            // 同名条目已存在，检查是否是目录（可能是并发创建）
            // 重新读取父目录 inode 以获取最新状态
            read_inode(m_fd, parentInodeId, &parentInode);
            int existingId = dir_find_entry(m_fd, &parentInode, dirName.c_str());
            std::cout << "[createDir] Entry already exists, existingId: " << existingId << std::endl;
            if (existingId >= 0) {
                Inode existingInode;
                if (read_inode(m_fd, existingId, &existingInode) == 0 && 
                    existingInode.type == INODE_TYPE_DIR) {
                    // 目录已存在，这是幂等操作，返回成功
                    std::cout << "[createDir] Concurrent creation detected, returning success" << std::endl;
                    return true;
                }
            }
            errorMsg = "Directory entry already exists: " + dirName;
        } else if (addResult == -3) {
            errorMsg = "Failed to write directory entry (disk may be full)";
        } else {
            errorMsg = "Failed to add directory entry";
        }
        return false;
    }
    
    // 验证目录已创建
    int verifyId = get_inode_by_path(m_fd, normPath.c_str());
    std::cout << "[createDir] Verification: " << normPath << " inode=" << verifyId << std::endl;
    
    return true;
}

std::string RealFileSystemAdapter::getFilePermission(const std::string& path, const std::string& user, 
                                                     std::string& errorMsg) {
    // 权限管理由 Server 层的 PermissionChecker 处理
    // FileSystem 层不维护权限信息
    (void)path;
    (void)user;
    (void)errorMsg;
    return "rwx";  // 默认返回全部权限
}

std::string RealFileSystemAdapter::submitForReview(const std::string& operation, const std::string& path, 
                                                   const std::string& user, std::string& errorMsg) {
    // 审核流程由 Server 层处理
    // FileSystem 层不维护审核信息
    (void)operation;
    (void)path;
    (void)user;
    (void)errorMsg;
    return "OK";  // 占位返回
}

// ==================== 统计接口实现 ====================

size_t RealFileSystemAdapter::getPaperAccessCount(const std::string& paperId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_paperAccessCounts.find(paperId);
    return (it != m_paperAccessCounts.end()) ? it->second : 0;
}

void RealFileSystemAdapter::getBlockCacheStats(size_t& hits, size_t& misses, size_t& size, size_t& capacity) const {
    // 调用 filesystem 的 C 接口获取 block cache 统计
    // filesystem 接口以 unsigned long* 返回，避免 MSVC 下 size_t* 不匹配
    unsigned long h = 0;
    unsigned long m = 0;
    unsigned long s = 0;
    unsigned long c = 0;
    block_cache_get_stats(&h, &m, &s, &c);
    hits = static_cast<size_t>(h);
    misses = static_cast<size_t>(m);
    size = static_cast<size_t>(s);
    capacity = static_cast<size_t>(c);
}

