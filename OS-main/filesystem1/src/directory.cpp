// directory.cpp
#include "../include/inode.h"
#include <cstring>
#include <cstdio>

// 最大重试次数（用于处理 COW 相关的临时失败）
static const int MAX_RETRY_COUNT = 3;

// 向目录中添加条目
// 返回值：0 成功，-1 一般错误，-2 同名条目已存在，-3 写入失败
int dir_add_entry(int fd, Inode* dir_inode, int dir_inode_id, 
                  const char* name, int inode_id) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        fprintf(stderr, "[dir_add_entry] ERROR: not a directory (type=%d)\n", dir_inode->type);
        return -1;
    }
    
    // 使用重试逻辑来处理可能的 COW 相关临时失败
    for (int retry = 0; retry < MAX_RETRY_COUNT; retry++) {
        // 第一步：重新读取最新的目录inode以获取最新的size（并发安全）
        Inode fresh_dir_inode;
        if (read_inode(fd, dir_inode_id, &fresh_dir_inode) != 0) {
            fprintf(stderr, "[dir_add_entry] ERROR: failed to read inode %d\n", dir_inode_id);
            return -1;
        }
        
        // 验证 inode 类型
        if (fresh_dir_inode.type != INODE_TYPE_DIR) {
            fprintf(stderr, "[dir_add_entry] ERROR: fresh inode not a dir (type=%d)\n", fresh_dir_inode.type);
            return -1;
        }
        
        // 第二步：验证不存在同名条目（使用最新的inode）
        if (dir_find_entry(fd, &fresh_dir_inode, name) != -1) {
            fprintf(stderr, "[dir_add_entry] Entry '%s' already exists\n", name);
            return -2;  // 同名条目已存在
        }
        
        // 创建新目录项
        DirEntry new_entry;
        new_entry.inode_id = inode_id;
        strncpy(new_entry.name, name, DIR_NAME_SIZE - 1);
        new_entry.name[DIR_NAME_SIZE - 1] = '\0';
        
        // 第三步：写入目录项（使用最新的inode）
        int entry_index = fresh_dir_inode.size / sizeof(DirEntry);
        int offset = entry_index * sizeof(DirEntry);
        
        fprintf(stderr, "[dir_add_entry] Writing '%s' to dir %d at offset %d (size before=%d)\n", 
                name, dir_inode_id, offset, fresh_dir_inode.size);
        
        int result = inode_write_data(fd, &fresh_dir_inode, dir_inode_id, 
                                      (const char*)&new_entry, 
                                      offset, sizeof(DirEntry));
        
        fprintf(stderr, "[dir_add_entry] Write result: %d (expected %zu), size after=%d\n", 
                result, sizeof(DirEntry), fresh_dir_inode.size);
        
        // 写入成功
        if (result == (int)sizeof(DirEntry)) {
            // 验证写入（使用更新后的inode）
            fprintf(stderr, "[dir_add_entry] Before verify - entry_count=%d, offset=%d\n", 
                    fresh_dir_inode.size / (int)sizeof(DirEntry), offset);
            
            // 尝试直接读取刚写入的数据
            DirEntry verify_entry;
            int read_result = inode_read_data(fd, &fresh_dir_inode, (char*)&verify_entry, offset, sizeof(DirEntry));
            fprintf(stderr, "[dir_add_entry] Direct read at offset %d: result=%d, name='%s', inode_id=%d\n",
                    offset, read_result, verify_entry.name, verify_entry.inode_id);
            
            int found = dir_find_entry(fd, &fresh_dir_inode, name);
            fprintf(stderr, "[dir_add_entry] Verify: found=%d, fresh_size=%d\n", found, fresh_dir_inode.size);
            
            // 更新调用者的inode
            *dir_inode = fresh_dir_inode;
            return 0;
        }
        
        // 写入失败，检查是否因为并发修改导致需要重试
        // 重新读取 inode 检查 size 是否已被其他操作修改
        Inode check_inode;
        read_inode(fd, dir_inode_id, &check_inode);
        
        fprintf(stderr, "[dir_add_entry] Write failed, retry=%d, check_size=%d, fresh_size=%d\n", 
                retry, check_inode.size, fresh_dir_inode.size);
        
        if (check_inode.size != fresh_dir_inode.size) {
            // size 已改变，说明有并发修改，重试
            continue;
        }
        
        // 如果 size 没变但写入仍失败，可能是资源不足，不再重试
        break;
    }
    
    fprintf(stderr, "[dir_add_entry] FAILED after retries for '%s'\n", name);
    return -3;  // 写入失败
}

// 在目录中查找条目
int dir_find_entry(int fd, const Inode* dir_inode, const char* name) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    
    for (int i = 0; i < entry_count; i++) {
        DirEntry entry;
        int offset = i * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&entry, offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            continue;
        }
        
        if (strcmp(entry.name, name) == 0) {
            return entry.inode_id; // 返回找到的inode id
        }
    }
    
    return -1; // 未找到
}

// 获取目录中的第index个条目
int dir_get_entry(int fd, const Inode* dir_inode, int index, DirEntry* entry) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    if (index >= entry_count) {
        return -1; // 索引超出范围
    }
    
    int offset = index * sizeof(DirEntry);
    int bytes_read = inode_read_data(fd, dir_inode, (char*)entry, offset, sizeof(DirEntry));
    
    return (bytes_read == sizeof(DirEntry)) ? 0 : -1;
}

// 从目录中删除条目
int dir_remove_entry(int fd, Inode* dir_inode, int dir_inode_id, const char* name) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    int found_index = -1;
    
    // 查找要删除的条目
    for (int i = 0; i < entry_count; i++) {
        DirEntry entry;
        int offset = i * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&entry, offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            continue;
        }
        
        if (strcmp(entry.name, name) == 0) {
            found_index = i;
            break;
        }
    }
    
    if (found_index == -1) {
        return -1; // 条目不存在
    }
    
    // 如果不是最后一个条目，需要用最后一个条目覆盖它
    if (found_index < entry_count - 1) {
        DirEntry last_entry;
        int last_offset = (entry_count - 1) * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&last_entry, last_offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            return -1;  // 读取失败
        }
        
        int target_offset = found_index * sizeof(DirEntry);
        int written = inode_write_data(fd, dir_inode, dir_inode_id, (char*)&last_entry, 
                                       target_offset, sizeof(DirEntry));
        if (written != sizeof(DirEntry)) {
            return -1;  // 写入失败，不修改目录大小
        }
    }
    
    // 只有在成功写入后才缩小目录大小
    dir_inode->size -= sizeof(DirEntry);
    
    // 写回inode
    write_inode(fd, dir_inode_id, dir_inode);
    
    return 0;
}