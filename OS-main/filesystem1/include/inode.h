// 在 include/inode.h 中添加目录项结构
#ifndef FS_INODE_H
#define FS_INODE_H

#include "disk.h"
#include "path.h"  // 添加这一行
#include <cstdint>

// inode类型定义
const int INODE_TYPE_FILE = 1;
const int INODE_TYPE_DIR = 2;

// 每个inode中直接块的数量
const int DIRECT_BLOCK_COUNT = 10;

// 目录项结构
// 注意：目录项名长度需要覆盖上层业务的 paperId（例如 concurrent_paper_*_timestamp）。
// 这里让 DirEntry 尺寸保持 64 字节对齐：4 (inode_id) + 60 (name) = 64。
const int DIR_NAME_SIZE = 60;
struct DirEntry {
    int inode_id;                      // inode编号
    char name[DIR_NAME_SIZE];          // 文件或目录名
};

const int DIRENT_PER_BLOCK = BLOCK_SIZE / sizeof(DirEntry);

// inode结构定义
struct Inode {
    int type;                           // 文件类型
    int size;                           // 文件大小(字节)
    int block_count;                    // 占用的数据块数量
    int direct_blocks[DIRECT_BLOCK_COUNT]; // 直接数据块指针
    int indirect_block;                 // 一级间接块指针
    // 可以添加更多字段如权限、时间戳等
};

#ifdef __cplusplus
extern "C" {
#endif

// inode操作函数声明
void init_inode(Inode* inode, int type);
int write_inode(int fd, int inode_id, const Inode* inode);
int read_inode(int fd, int inode_id, Inode* inode);
int inode_alloc_block(int fd, Inode* inode);
void inode_free_blocks(int fd, Inode* inode);

// 新增文件数据操作函数声明
int inode_write_data(int fd, Inode* inode, int inode_id, const char* data, int offset, int size);
int inode_read_data(int fd, const Inode* inode, char* buffer, int offset, int size);

// 新增目录操作函数声明
int dir_add_entry(int fd, Inode* dir_inode, int dir_inode_id, const char* name, int inode_id);
int dir_find_entry(int fd, const Inode* dir_inode, const char* name);
int dir_get_entry(int fd, const Inode* dir_inode, int index, DirEntry* entry);
int dir_remove_entry(int fd, Inode* dir_inode, int dir_inode_id, const char* name);

#ifdef __cplusplus
}
#endif

#endif