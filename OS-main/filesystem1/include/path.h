// path.h
#ifndef FS_PATH_H
#define FS_PATH_H

// 前向声明
struct Inode;
struct DirEntry;

// 路径解析相关常量
const int MAX_PATH_DEPTH = 32;
const int MAX_PATH_LENGTH = 256;

#ifdef __cplusplus
extern "C" {
#endif

// 函数声明
int parse_path(int fd, const char* path, int* inode_ids, int max_depth);
int get_inode_by_path(int fd, const char* path);
int get_parent_inode_and_name(int fd, const char* path, int* parent_inode_id, char* name);

// inode.h中函数的前向声明
int read_inode(int fd, int inode_id, Inode* inode);
int dir_find_entry(int fd, const Inode* dir_inode, const char* name);

#ifdef __cplusplus
}
#endif

#endif