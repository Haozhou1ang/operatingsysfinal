// inode.cpp
#include "../include/inode.h"
#include "../include/block_cache.h"
#include <cstring>
#include <vector>
#include <algorithm>
using std::vector;
using std::min;

// 初始化inode
void init_inode(Inode* inode, int type) {
    inode->type = type;
    inode->size = 0;
    inode->block_count = 0;
    
    // 初始化直接块指针
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = -1;
    }
    
    // 初始化间接块指针
    inode->indirect_block = -1;
}

// 将inode写入磁盘
int write_inode(int fd, int inode_id, const Inode* inode) {
    // 计算inode在inode表中的位置
    int inode_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_id = INODE_TABLE_START + (inode_id / inode_per_block);
    int offset = inode_id % inode_per_block;
    
    // 读取对应的块
    char buf[BLOCK_SIZE];
    read_block_cached(fd, block_id, buf);
    
    // 更新inode
    Inode* inodes = (Inode*)buf;
    inodes[offset] = *inode;
    
    // 写回磁盘
    write_block_cached(fd, block_id, buf);
    
    return 0;
}

// 从磁盘读取inode
int read_inode(int fd, int inode_id, Inode* inode) {
    // 计算inode在inode表中的位置
    int inode_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_id = INODE_TABLE_START + (inode_id / inode_per_block);
    int offset = inode_id % inode_per_block;
    
    // 读取对应的块
    char buf[BLOCK_SIZE];
    read_block_cached(fd, block_id, buf);
    
    // 获取inode
    Inode* inodes = (Inode*)buf;
    *inode = inodes[offset];
    
    return 0;
}

// 为inode分配一个数据块
int inode_alloc_block(int fd, Inode* inode) {
    // 分配一个数据块
    int block_id = alloc_block(fd);
    if (block_id == -1) {
        return -1; // 没有可用的数据块
    }
    
    // 如果直接块还有空间
    if (inode->block_count < DIRECT_BLOCK_COUNT) {
        inode->direct_blocks[inode->block_count] = block_id;
    } else {
        // 需要使用间接块
        // 如果还没有间接块，则分配一个
        if (inode->indirect_block == -1) {
            inode->indirect_block = alloc_block(fd);
            if (inode->indirect_block == -1) {
                // 回滚之前分配的数据块
                free_block(fd, block_id);
                return -1;
            }
            
            // 初始化间接块
            int pointers[POINTERS_PER_BLOCK];
            for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
                pointers[i] = -1;
            }
            pointers[0] = block_id;
            write_block_cached(fd, inode->indirect_block, pointers);
        } else {
            // 读取现有的间接块
            int pointers[POINTERS_PER_BLOCK];
            read_block_cached(fd, inode->indirect_block, pointers);
            
            // 在间接块中找到空闲位置
            int indirect_index = inode->block_count - DIRECT_BLOCK_COUNT;
            pointers[indirect_index] = block_id;
            
            // 写回间接块
            write_block_cached(fd, inode->indirect_block, pointers);
        }
    }
    
    inode->block_count++;
    // 注意：这里不直接增加size，而是在实际写入数据时增加
    
    return block_id;
}

// 释放inode占用的所有数据块
// 修正 inode_free_blocks 函数，确保正确处理引用计数
void inode_free_blocks(int fd, Inode* inode) {
    // 释放直接块
    for (int i = 0; i < inode->block_count && i < DIRECT_BLOCK_COUNT; i++) {
        if (inode->direct_blocks[i] != -1) {
            decrement_block_ref_count(fd, inode->direct_blocks[i]);
            if (get_block_ref_count(fd, inode->direct_blocks[i]) == 0) {
                free_block(fd, inode->direct_blocks[i]);
            }
        }
    }
    
    // 释放间接块指向的数据块
    if (inode->indirect_block != -1) {
        int pointers[POINTERS_PER_BLOCK];
        read_block_cached(fd, inode->indirect_block, pointers);
        
        int indirect_count = inode->block_count - DIRECT_BLOCK_COUNT;
        for (int i = 0; i < indirect_count && i < POINTERS_PER_BLOCK; i++) {
            if (pointers[i] != -1) {
                decrement_block_ref_count(fd, pointers[i]);
                if (get_block_ref_count(fd, pointers[i]) == 0) {
                    free_block(fd, pointers[i]);
                }
            }
        }
        
        // 释放间接块本身
        decrement_block_ref_count(fd, inode->indirect_block);
        if (get_block_ref_count(fd, inode->indirect_block) == 0) {
            free_block(fd, inode->indirect_block);
        }
    }
    
    // 重置inode
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = -1;
    }
    inode->indirect_block = -1;
    inode->block_count = 0;
    inode->size = 0;
}

// 修改inode_write_data函数以支持COW
// 在 inode.cpp 中修改
int inode_write_data(int fd, Inode* inode, int inode_id, 
                     const char* data, int offset, int size) {
    if (size <= 0) return 0;
    
    // 计算写入结束位置和需要的总块数
    int end_pos = offset + size;
    int blocks_needed = (end_pos + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // 如果需要更多块，分配它们
    while (inode->block_count < blocks_needed) {
        int block_id = alloc_block(fd);
        if (block_id == -1) {
            return -1; // 分配失败
        }
        
        // 清零新分配的块（重要！避免读取垃圾数据）
        char zero_buf[BLOCK_SIZE];
        memset(zero_buf, 0, BLOCK_SIZE);
        write_block_cached(fd, block_id, zero_buf);
        
        // 添加到 inode 的块列表
        if (inode->block_count < DIRECT_BLOCK_COUNT) {
            inode->direct_blocks[inode->block_count] = block_id;
        } else {
            // 需要间接块
            if (inode->block_count == DIRECT_BLOCK_COUNT) {
                // 第一次需要间接块，分配它
                inode->indirect_block = alloc_block(fd);
                if (inode->indirect_block == -1) {
                    free_block(fd, block_id);
                    return -1;
                }
                // 初始化间接块
                int pointers[POINTERS_PER_BLOCK];
                for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
                    pointers[i] = -1;
                }
                write_block_cached(fd, inode->indirect_block, (void*)pointers);
            }
            
            // 添加到间接块
            int pointers[POINTERS_PER_BLOCK];
            read_block_cached(fd, inode->indirect_block, pointers);
            pointers[inode->block_count - DIRECT_BLOCK_COUNT] = block_id;
            write_block_cached(fd, inode->indirect_block, (void*)pointers);
        }
        
        inode->block_count++;
    }
    
    // 写入数据到各个块
    int written = 0;
    int current_offset = offset;
    
    while (written < size) {
        int block_index = current_offset / BLOCK_SIZE;
        int block_offset = current_offset % BLOCK_SIZE;
        int to_write = std::min(size - written, BLOCK_SIZE - block_offset);
        
        // 获取块 ID
        int block_id;
        if (block_index < DIRECT_BLOCK_COUNT) {
            block_id = inode->direct_blocks[block_index];
        } else {
            int pointers[POINTERS_PER_BLOCK];
            read_block_cached(fd, inode->indirect_block, pointers);
            block_id = pointers[block_index - DIRECT_BLOCK_COUNT];
        }
        
        // COW检查：如果块的引用计数 > 1，需要复制块
        int ref_count = get_block_ref_count(fd, block_id);
        if (ref_count > 1) {
            // 执行COW：复制块
            int new_block_id = copy_on_write_block(fd, block_id);
            if (new_block_id == -1) {
                return written; // COW失败，返回已写入的字节数
            }
            
            // 更新inode中的块指针
            if (block_index < DIRECT_BLOCK_COUNT) {
                inode->direct_blocks[block_index] = new_block_id;
            } else {
                int pointers[POINTERS_PER_BLOCK];
                read_block_cached(fd, inode->indirect_block, pointers);
                pointers[block_index - DIRECT_BLOCK_COUNT] = new_block_id;
                write_block_cached(fd, inode->indirect_block, (void*)pointers);
            }
            
            block_id = new_block_id;
        }
        
        // 如果不是整块写入，需要先读取再写入
        if (block_offset != 0 || to_write != BLOCK_SIZE) {
            char temp_buf[BLOCK_SIZE];
            read_block_cached(fd, block_id, temp_buf);
            memcpy(temp_buf + block_offset, data + written, to_write);
            write_block_cached(fd, block_id, temp_buf);
        } else {
            write_block_cached(fd, block_id, (void*)(data + written));
        }
        
        written += to_write;
        current_offset += to_write;
    }
    
    // 更新文件大小（如果扩大了）
    if (end_pos > inode->size) {
        inode->size = end_pos;
    }
    
    // 写回 inode
    write_inode(fd, inode_id, inode);
    
    // 刷新缓存确保数据立即可见（对并发操作很重要）
    block_cache_flush(fd);
    
    return written;
}

// 从inode读取数据
int inode_read_data(int fd, const Inode* inode, char* buffer, int offset, int size) {
    if (size <= 0 || offset >= inode->size) return 0;
    
    // 限制读取范围不超过文件大小
    if (offset + size > inode->size) {
        size = inode->size - offset;
    }
    
    int bytes_read = 0;
    int current_offset = offset;
    int remaining = size;
    
    while (remaining > 0) {
        // 计算当前数据应该读取的逻辑块号和块内偏移
        int logical_block_num = current_offset / BLOCK_SIZE;
        int block_offset = current_offset % BLOCK_SIZE;
        int chunk_size = BLOCK_SIZE - block_offset;
        if (chunk_size > remaining) {
            chunk_size = remaining;
        }
        
        // 检查是否超出文件范围
        if (logical_block_num >= inode->block_count) {
            break;
        }
        
        // 获取物理块号
        int physical_block_id;
        if (logical_block_num < DIRECT_BLOCK_COUNT) {
            physical_block_id = inode->direct_blocks[logical_block_num];
        } else {
            // 处理间接块
            int indirect_index = logical_block_num - DIRECT_BLOCK_COUNT;
            int pointers[POINTERS_PER_BLOCK];
            read_block_cached(fd, inode->indirect_block, pointers);
            physical_block_id = pointers[indirect_index];
        }
        
        // 从块中读取数据
        read_data_block(fd, physical_block_id, buffer + bytes_read, block_offset, chunk_size);
        
        bytes_read += chunk_size;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }
    
    return bytes_read;
}