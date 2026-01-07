// path.cpp
#include "../include/path.h"
#include "../include/inode.h"
#include <cstring>
#include <cstdlib>

// 路径解析函数
int parse_path(int fd, const char* path, int* inode_ids, int max_depth) {
    if (!path || !inode_ids || max_depth <= 0 || path[0] != '/') {
        return -1; // 路径必须以 '/' 开始
    }
    
    if (strlen(path) == 1) {
        // 根路径情况
        inode_ids[0] = 0; // 根目录inode ID通常为0
        return 1;
    }
    
    int depth = 0;
    int current_inode_id = 0; // 从根目录开始
    const char* p = path + 1; // 跳过开头的 '/'
    
    while (*p && depth < max_depth - 1) {
        // 提取下一个路径组件
        char component[DIR_NAME_SIZE];
        int i = 0;
        
        // 复制直到遇到'/'或字符串结束
        while (*p && *p != '/' && i < DIR_NAME_SIZE - 1) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        if (i == 0) {
            // 空组件，可能是连续的'/'
            if (*p == '/') p++;
            continue;
        }
        
        // 在当前目录中查找组件
        Inode current_inode;
        if (read_inode(fd, current_inode_id, &current_inode) != 0) {
            return -1; // 读取inode失败
        }
        
        if (current_inode.type != INODE_TYPE_DIR) {
            return -1; // 不是目录
        }
        
        int next_inode_id = dir_find_entry(fd, &current_inode, component);
        if (next_inode_id == -1) {
            return -1; // 找不到条目
        }
        
        inode_ids[depth++] = current_inode_id;
        current_inode_id = next_inode_id;
        
        // 跳过路径中的'/'
        if (*p == '/') {
            p++;
        }
    }
    
    // 添加最终目标的inode ID
    inode_ids[depth++] = current_inode_id;
    
    return depth;
}

// 根据路径获取文件或目录的inode ID
int get_inode_by_path(int fd, const char* path) {
    int inode_ids[MAX_PATH_DEPTH];
    int depth = parse_path(fd, path, inode_ids, MAX_PATH_DEPTH);
    
    if (depth <= 0) {
        return -1; // 路径解析失败
    }
    
    return inode_ids[depth - 1]; // 返回最后一个inode ID
}

// 根据路径获取父目录的inode ID和文件名
// 根据路径获取父目录的inode ID和文件名
int get_parent_inode_and_name(int fd, const char* path, int* parent_inode_id, char* name) {
    if (!path || path[0] != '/') {
        return -1;
    }
    
    // 处理根目录的特殊情况
    if (strcmp(path, "/") == 0) {
        return -1; // 根目录没有父目录
    }
    
    // 处理以'/'结尾的路径
    size_t path_len = strlen(path);
    const char* last_slash;
    
    // 如果路径以'/'结尾，需要找到倒数第二个'/'
    if (path_len > 1 && path[path_len - 1] == '/') {
        // 找到倒数第二个'/'
        const char* temp = path + path_len - 2;
        while (temp > path && *temp != '/') {
            temp--;
        }
        if (*temp == '/') {
            last_slash = temp;
        } else {
            return -1; // 路径格式错误
        }
    } else {
        // 找到最后一个'/'
        last_slash = strrchr(path, '/');
        if (!last_slash) {
            return -1;
        }
    }
    
    // 提取父路径
    char parent_path[MAX_PATH_LENGTH];
    int parent_len = last_slash - path;
    if (parent_len == 0) {
        // 父目录是根目录
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }
    
    // 提取文件名
    const char* filename = last_slash + 1;
    if (strlen(filename) == 0) {
        return -1; // 没有文件名
    }
    
    // 如果文件名以'/'结尾，去掉它
    char clean_filename[DIR_NAME_SIZE];
    strncpy(clean_filename, filename, DIR_NAME_SIZE - 1);
    clean_filename[DIR_NAME_SIZE - 1] = '\0';
    
    // 去掉末尾的'/'（如果有的话）
    size_t filename_len = strlen(clean_filename);
    if (filename_len > 0 && clean_filename[filename_len - 1] == '/') {
        clean_filename[filename_len - 1] = '\0';
    }
    
    strcpy(name, clean_filename);
    
    // 获取父目录的inode ID
    *parent_inode_id = get_inode_by_path(fd, parent_path);
    if (*parent_inode_id == -1) {
        return -1;
    }
    
    return 0;
}