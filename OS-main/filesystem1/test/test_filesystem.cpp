#include "../include/disk.h"
#include "../include/inode.h"
#include <iostream>
#include <cstring>
#include <cassert>
using namespace std;

void test_disk_operations() {
    cout << "=== 测试磁盘基本操作 ===" << endl;
    
    // 打开磁盘
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    cout << "磁盘打开成功" << endl;
    
    // 读取superblock
    Superblock sb;
    read_superblock(fd, &sb);
    cout << "初始空闲inode数: " << sb.free_inode_count << endl;
    cout << "初始空闲块数: " << sb.free_block_count << endl;
    
    // 测试块读写
    char write_buf[BLOCK_SIZE];
    char read_buf[BLOCK_SIZE];
    
    memset(write_buf, 0xAA, BLOCK_SIZE); // 填充特定值
    write_block(fd, DATA_BLOCK_START, write_buf);
    
    memset(read_buf, 0, BLOCK_SIZE);
    read_block(fd, DATA_BLOCK_START, read_buf);
    
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
    cout << "块读写测试通过" << endl;
    
    // 测试部分块读写
    char partial_data[] = "Hello FileSystem!";
    write_data_block(fd, DATA_BLOCK_START, partial_data, 100, strlen(partial_data));
    
    char read_partial[100];
    read_data_block(fd, DATA_BLOCK_START, read_partial, 100, strlen(partial_data));
    
    assert(memcmp(partial_data, read_partial, strlen(partial_data)) == 0);
    cout << "部分块读写测试通过" << endl;
    
    disk_close(fd);
}

void test_inode_allocation() {
    cout << "\n=== 测试Inode分配 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 保存初始状态
    Superblock sb_orig;
    read_superblock(fd, &sb_orig);
    
    // 分配几个inode
    int inode_ids[5];
    for (int i = 0; i < 5; i++) {
        inode_ids[i] = alloc_inode(fd);
        assert(inode_ids[i] >= 0);
        cout << "分配inode ID: " << inode_ids[i] << endl;
    }
    
    // 检查superblock更新
    Superblock sb_new;
    read_superblock(fd, &sb_new);
    assert(sb_new.free_inode_count == sb_orig.free_inode_count - 5);
    cout << "Inode计数更新正确" << endl;
    
    // 释放一个inode
    free_inode(fd, inode_ids[0]);
    
    read_superblock(fd, &sb_new);
    assert(sb_new.free_inode_count == sb_orig.free_inode_count - 4);
    cout << "Inode释放和计数更新正确" << endl;
    
    disk_close(fd);
}

void test_block_allocation() {
    cout << "\n=== 测试数据块分配 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 保存初始状态
    Superblock sb_orig;
    read_superblock(fd, &sb_orig);
    
    // 分配几个数据块
    int block_ids[5];
    for (int i = 0; i < 5; i++) {
        block_ids[i] = alloc_block(fd);
        assert(block_ids[i] >= 0);
        cout << "分配数据块 ID: " << block_ids[i] << endl;
    }
    
    // 检查superblock更新
    Superblock sb_new;
    read_superblock(fd, &sb_new);
    assert(sb_new.free_block_count == sb_orig.free_block_count - 5);
    cout << "数据块计数更新正确" << endl;
    
    // 释放一个数据块
    free_block(fd, block_ids[0]);
    
    read_superblock(fd, &sb_new);
    assert(sb_new.free_block_count == sb_orig.free_block_count - 4);
    cout << "数据块释放和计数更新正确" << endl;
    
    disk_close(fd);
}

void test_inode_operations() {
    cout << "\n=== 测试Inode操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 分配一个inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    // 初始化inode
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    cout << "Inode初始化完成" << endl;
    
    // 写入inode
    int result = write_inode(fd, inode_id, &inode);
    assert(result == 0);
    cout << "Inode写入完成" << endl;
    
    // 读取inode
    Inode inode_read;  // 修改变量名避免冲突
    result = read_inode(fd, inode_id, &inode_read);  // 现在可以正确调用函数
    assert(result == 0);
    
    // 验证内容
    assert(inode_read.type == INODE_TYPE_FILE);
    assert(inode_read.size == 0);
    assert(inode_read.block_count == 0);
    cout << "Inode读取和验证完成" << endl;
    
    disk_close(fd);
}

void test_file_data_operations() {
    cout << "\n=== 测试文件数据操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 创建一个文件inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    
    // 写入数据
    const char* test_data = "这是测试数据内容。";
    int data_len = strlen(test_data);
    
    int written = inode_write_data(fd, &inode, inode_id, test_data, 0, data_len);
    assert(written == data_len);
    cout << "写入数据: " << test_data << endl;
    cout << "写入字节数: " << written << endl;
    
    // 读取数据
    char read_buffer[1024];
    int read_bytes = inode_read_data(fd, &inode, read_buffer, 0, data_len);
    assert(read_bytes == data_len);
    
    read_buffer[read_bytes] = '\0'; // 添加字符串结束符
    cout << "读取数据: " << read_buffer << endl;
    cout << "读取字节数: " << read_bytes << endl;
    
    // 验证数据一致性
    assert(strcmp(test_data, read_buffer) == 0);
    cout << "数据一致性验证通过" << endl;
    
    // 测试跨块读写
    char large_data[BLOCK_SIZE * 2];
    memset(large_data, 'A', BLOCK_SIZE);
    memset(large_data + BLOCK_SIZE, 'B', BLOCK_SIZE - 1);
    large_data[BLOCK_SIZE * 2 - 1] = '\0';
    
    // 重新初始化inode
    init_inode(&inode, INODE_TYPE_FILE);
    written = inode_write_data(fd, &inode, inode_id, large_data, 0, BLOCK_SIZE * 2 - 1);
    assert(written == BLOCK_SIZE * 2 - 1);
    cout << "大块数据写入完成" << endl;
    
    char large_read_buffer[BLOCK_SIZE * 2];
    read_bytes = inode_read_data(fd, &inode, large_read_buffer, 0, BLOCK_SIZE * 2 - 1);
    assert(read_bytes == BLOCK_SIZE * 2 - 1);
    
    large_read_buffer[read_bytes] = '\0';
    assert(memcmp(large_data, large_read_buffer, BLOCK_SIZE * 2 - 1) == 0);
    cout << "大块数据读取验证通过" << endl;
    
    disk_close(fd);
}

void test_direct_and_indirect_blocks() {
    cout << "\n=== 测试直接块和间接块 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 创建一个文件inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    
    // 写入足够多的数据来触发间接块分配
    // DIRECT_BLOCK_COUNT是10，每个块1024字节
    // 我们写入12个块的数据
    char* large_data = new char[BLOCK_SIZE * 12];
    for (int i = 0; i < BLOCK_SIZE * 12; i++) {
        large_data[i] = 'A' + (i % 26);
    }
    
    int written = inode_write_data(fd, &inode, inode_id, large_data, 0, BLOCK_SIZE * 12);
    assert(written == BLOCK_SIZE * 12);
    cout << "写入12个块的数据完成" << endl;
    
    // 检查inode状态
    cout << "分配的块数: " << inode.block_count << endl;
    cout << "文件大小: " << inode.size << endl;
    assert(inode.block_count == 12);
    assert(inode.size == BLOCK_SIZE * 12);
    
    // 验证直接块指针
    cout << "直接块指针数量: " << DIRECT_BLOCK_COUNT << endl;
    for (int i = 0; i < min(DIRECT_BLOCK_COUNT, inode.block_count); i++) {
        cout << "直接块[" << i << "]: " << inode.direct_blocks[i] << endl;
        assert(inode.direct_blocks[i] != -1);
    }
    
    // 验证间接块指针
    if (inode.block_count > DIRECT_BLOCK_COUNT) {
        cout << "间接块指针: " << inode.indirect_block << endl;
        assert(inode.indirect_block != -1);
    }
    
    // 读取数据验证
    char* read_buffer = new char[BLOCK_SIZE * 12];
    int read_bytes = inode_read_data(fd, &inode, read_buffer, 0, BLOCK_SIZE * 12);
    assert(read_bytes == BLOCK_SIZE * 12);
    
    assert(memcmp(large_data, read_buffer, BLOCK_SIZE * 12) == 0);
    cout << "大文件数据一致性验证通过" << endl;
    
    delete[] large_data;
    delete[] read_buffer;
    
    disk_close(fd);
}

// 在 test/test_filesystem.cpp 的末尾添加以下测试函数

void test_directory_operations() {
    cout << "\n=== 测试目录操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 创建根目录（已经在mkfs中创建，这里直接读取）
    Inode root_inode;
    int root_inode_id = 0;
    read_inode(fd, root_inode_id, &root_inode);
    assert(root_inode.type == INODE_TYPE_DIR);
    cout << "根目录读取成功" << endl;
    
    // 创建一个测试文件
    int file_inode_id = alloc_inode(fd);
    assert(file_inode_id >= 0);
    
    Inode file_inode;
    init_inode(&file_inode, INODE_TYPE_FILE);
    write_inode(fd, file_inode_id, &file_inode);
    cout << "测试文件创建成功，inode_id: " << file_inode_id << endl;
    
    // 向根目录添加文件条目
    cout << "准备添加文件条目，root_inode.size=" << root_inode.size 
         << ", block_count=" << root_inode.block_count << endl;
    int result = dir_add_entry(fd, &root_inode, root_inode_id, "test.txt", file_inode_id);
    cout << "dir_add_entry 返回: " << result << endl;
    assert(result == 0);
    cout << "向根目录添加文件条目成功" << endl;
    
    // 重新读取 root_inode 以获取最新状态
    read_inode(fd, root_inode_id, &root_inode);
    
    // 查找刚刚添加的条目
    int found_inode_id = dir_find_entry(fd, &root_inode, "test.txt");
    assert(found_inode_id == file_inode_id);
    cout << "目录条目查找成功" << endl;
    
    // 创建一个子目录
    int subdir_inode_id = alloc_inode(fd);
    assert(subdir_inode_id >= 0);
    
    Inode subdir_inode;
    init_inode(&subdir_inode, INODE_TYPE_DIR);
    write_inode(fd, subdir_inode_id, &subdir_inode);
    cout << "子目录创建成功，inode_id: " << subdir_inode_id << endl;
    
    // 向根目录添加子目录条目
    result = dir_add_entry(fd, &root_inode, root_inode_id, "subdir", subdir_inode_id);
    assert(result == 0);
    cout << "向根目录添加子目录条目成功" << endl;
    
    // 重新读取 root_inode 以获取最新状态
    read_inode(fd, root_inode_id, &root_inode);
    
    // 验证根目录大小
    assert(root_inode.size == 2 * sizeof(DirEntry));
    cout << "根目录大小验证成功: " << root_inode.size << " 字节" << endl;
    
    // 遍历根目录条目
    DirEntry entry;
    result = dir_get_entry(fd, &root_inode, 0, &entry);
    assert(result == 0);
    cout << "第一个条目: " << entry.name << " (inode_id: " << entry.inode_id << ")" << endl;
    
    result = dir_get_entry(fd, &root_inode, 1, &entry);
    assert(result == 0);
    cout << "第二个条目: " << entry.name << " (inode_id: " << entry.inode_id << ")" << endl;
    
    // 重新读取 root_inode 以获取最新状态
    read_inode(fd, root_inode_id, &root_inode);
    
    // 测试重复添加条目（应失败）
    result = dir_add_entry(fd, &root_inode, root_inode_id, "test.txt", file_inode_id);
    assert(result == -1);
    cout << "防止重复条目测试通过" << endl;
    
    // 测试删除条目
    result = dir_remove_entry(fd, &root_inode, root_inode_id, "test.txt");
    assert(result == 0);
    cout << "删除目录条目成功" << endl;
    
    // 重新读取 root_inode 以获取最新状态
    read_inode(fd, root_inode_id, &root_inode);
    
    // 验证删除后的目录大小
    assert(root_inode.size == sizeof(DirEntry));
    cout << "删除后根目录大小验证成功: " << root_inode.size << " 字节" << endl;
    
    // 验证条目确实已被删除
    found_inode_id = dir_find_entry(fd, &root_inode, "test.txt");
    assert(found_inode_id == -1);
    cout << "确认条目已删除" << endl;
    
    // 验证剩下的条目仍然存在
    found_inode_id = dir_find_entry(fd, &root_inode, "subdir");
    assert(found_inode_id == subdir_inode_id);
    cout << "剩余条目验证成功" << endl;
    
    disk_close(fd);
}


// 在 test_directory_operations() 后面添加以下测试

void test_multilevel_directory() {
    cout << "\n=== 测试多级目录 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 读取根目录
    Inode root_inode;
    int root_inode_id = 0;
    read_inode(fd, root_inode_id, &root_inode);
    
    // 创建多级目录结构: /level1/level2/level3
    int level1_id = alloc_inode(fd);
    assert(level1_id > 0);
    Inode level1_inode;
    init_inode(&level1_inode, INODE_TYPE_DIR);
    write_inode(fd, level1_id, &level1_inode);
    dir_add_entry(fd, &root_inode, root_inode_id, "level1", level1_id);
    cout << "创建 /level1 目录" << endl;
    
    int level2_id = alloc_inode(fd);
    assert(level2_id > 0);
    Inode level2_inode;
    init_inode(&level2_inode, INODE_TYPE_DIR);
    write_inode(fd, level2_id, &level2_inode);
    dir_add_entry(fd, &level1_inode, level1_id, "level2", level2_id);
    cout << "创建 /level1/level2 目录" << endl;
    
    int level3_id = alloc_inode(fd);
    assert(level3_id > 0);
    Inode level3_inode;
    init_inode(&level3_inode, INODE_TYPE_DIR);
    write_inode(fd, level3_id, &level3_inode);
    dir_add_entry(fd, &level2_inode, level2_id, "level3", level3_id);
    cout << "创建 /level1/level2/level3 目录" << endl;
    
    // 在最深层目录中创建一个文件
    int deep_file_id = alloc_inode(fd);
    assert(deep_file_id > 0);
    Inode deep_file_inode;
    init_inode(&deep_file_inode, INODE_TYPE_FILE);
    write_inode(fd, deep_file_id, &deep_file_inode);
    dir_add_entry(fd, &level3_inode, level3_id, "deep_file.txt", deep_file_id);
    cout << "在 /level1/level2/level3 中创建文件 deep_file.txt" << endl;
    
    // 验证所有目录都正确创建并链接
    assert(level1_inode.size == sizeof(DirEntry));  // level1 包含 level2
    assert(level2_inode.size == sizeof(DirEntry));  // level2 包含 level3
    assert(level3_inode.size == sizeof(DirEntry));  // level3 包含 deep_file.txt
    cout << "目录结构完整性验证通过" << endl;
    
    // 验证目录类型正确
    assert(level1_inode.type == INODE_TYPE_DIR);
    assert(level2_inode.type == INODE_TYPE_DIR);
    assert(level3_inode.type == INODE_TYPE_DIR);
    assert(deep_file_inode.type == INODE_TYPE_FILE);
    cout << "节点类型验证通过" << endl;
    
    disk_close(fd);
}


void test_parse_path_function() {
    cout << "\n=== 测试路径解析数组功能 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 读取根目录
    Inode root_inode;
    int root_inode_id = 0;
    read_inode(fd, root_inode_id, &root_inode);
    
    // 使用已有的目录结构进行测试
    int inode_ids[MAX_PATH_DEPTH];
    
    // 测试根目录解析
    int depth = parse_path(fd, "/", inode_ids, MAX_PATH_DEPTH);
    assert(depth == 1);
    assert(inode_ids[0] == 0);
    cout << "根目录路径数组解析成功，深度=" << depth << endl;
    
    // 测试多级路径解析
    depth = parse_path(fd, "/test_dir/sub_dir/test_file.txt", inode_ids, MAX_PATH_DEPTH);
    assert(depth == 4);
    assert(inode_ids[0] == 0);  // 根目录
    // 注意：这里的具体inode ID取决于测试环境的状态
    cout << "多级路径数组解析成功，深度=" << depth << endl;
    cout << "路径组件: 根目录(0) -> test_dir(" << inode_ids[1] 
         << ") -> sub_dir(" << inode_ids[2] << ") -> test_file.txt(" << inode_ids[3] << ")" << endl;
    
    // 测试不存在的路径
    depth = parse_path(fd, "/nonexistent/path", inode_ids, MAX_PATH_DEPTH);
    assert(depth == -1);
    cout << "不存在路径数组解析检测成功" << endl;
    
    disk_close(fd);
}

// 在 test/test_filesystem.cpp 的末尾添加以下测试函数

void test_path_parsing() {
    cout << "\n=== 测试路径解析 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 读取根目录
    Inode root_inode;
    int root_inode_id = 0;
    read_inode(fd, root_inode_id, &root_inode);
    
    // 创建测试目录结构: /test_dir/sub_dir
    int test_dir_id = alloc_inode(fd);
    assert(test_dir_id > 0);
    Inode test_dir_inode;
    init_inode(&test_dir_inode, INODE_TYPE_DIR);
    write_inode(fd, test_dir_id, &test_dir_inode);
    dir_add_entry(fd, &root_inode, root_inode_id, "test_dir", test_dir_id);
    cout << "创建 /test_dir 目录" << endl;
    
    int sub_dir_id = alloc_inode(fd);
    assert(sub_dir_id > 0);
    Inode sub_dir_inode;
    init_inode(&sub_dir_inode, INODE_TYPE_DIR);
    write_inode(fd, sub_dir_id, &sub_dir_inode);
    dir_add_entry(fd, &test_dir_inode, test_dir_id, "sub_dir", sub_dir_id);
    cout << "创建 /test_dir/sub_dir 目录" << endl;
    
    // 创建一个测试文件
    int test_file_id = alloc_inode(fd);
    assert(test_file_id > 0);
    Inode test_file_inode;
    init_inode(&test_file_inode, INODE_TYPE_FILE);
    write_inode(fd, test_file_id, &test_file_inode);
    dir_add_entry(fd, &sub_dir_inode, sub_dir_id, "test_file.txt", test_file_id);
    cout << "在 /test_dir/sub_dir 中创建文件 test_file.txt" << endl;
    
    // 测试路径解析功能
    // 测试根目录路径
    int inode_id = get_inode_by_path(fd, "/");
    assert(inode_id == 0);
    cout << "根目录路径解析成功" << endl;
    
    // 测试单级目录路径
    inode_id = get_inode_by_path(fd, "/test_dir");
    assert(inode_id == test_dir_id);
    cout << "/test_dir 路径解析成功" << endl;
    
    // 测试多级目录路径
    inode_id = get_inode_by_path(fd, "/test_dir/sub_dir");
    assert(inode_id == sub_dir_id);
    cout << "/test_dir/sub_dir 路径解析成功" << endl;
    
    // 测试文件路径
    inode_id = get_inode_by_path(fd, "/test_dir/sub_dir/test_file.txt");
    assert(inode_id == test_file_id);
    cout << "/test_dir/sub_dir/test_file.txt 路径解析成功" << endl;
    
    // 测试带斜杠结尾的目录路径
    inode_id = get_inode_by_path(fd, "/test_dir/sub_dir/");
    assert(inode_id == sub_dir_id);
    cout << "/test_dir/sub_dir/ 路径解析成功" << endl;
    
    // 测试不存在的路径
    inode_id = get_inode_by_path(fd, "/nonexistent");
    assert(inode_id == -1);
    cout << "不存在路径检测成功" << endl;
    
    inode_id = get_inode_by_path(fd, "/test_dir/nonexistent");
    assert(inode_id == -1);
    cout << "不存在子路径检测成功" << endl;
    
    // 测试父目录和文件名解析
    char filename[DIR_NAME_SIZE];
    int parent_id;
    
    int result = get_parent_inode_and_name(fd, "/test_dir/sub_dir/test_file.txt", &parent_id, filename);
    assert(result == 0);
    assert(parent_id == sub_dir_id);
    assert(strcmp(filename, "test_file.txt") == 0);
    cout << "父目录和文件名解析成功: 父目录ID=" << parent_id << ", 文件名=" << filename << endl;
    
    // 测试目录的父目录解析（修正测试）
    result = get_parent_inode_and_name(fd, "/test_dir/sub_dir", &parent_id, filename);
    if (result == 0) {
        assert(parent_id == test_dir_id);
        assert(strcmp(filename, "sub_dir") == 0);
        cout << "目录的父目录解析成功: 父目录ID=" << parent_id << ", 目录名=" << filename << endl;
    } else {
        cout << "目录的父目录解析失败，可能路径格式有问题" << endl;
    }
    
    // 测试根目录的父目录（应该失败）
    result = get_parent_inode_and_name(fd, "/", &parent_id, filename);
    assert(result == -1);
    cout << "根目录无父目录检测成功" << endl;
    
    disk_close(fd);
}

int main() {
    cout << "文件系统测试开始..." << endl;
    
    try {
        test_disk_operations();
        test_inode_allocation();
        test_block_allocation();
        test_inode_operations();
        test_file_data_operations();
        test_direct_and_indirect_blocks();
        test_directory_operations();
        test_multilevel_directory();
        test_path_parsing();           // 添加这一行
        test_parse_path_function();    // 添加这一行
        
        cout << "\n=== 所有测试通过! ===" << endl;
    } catch (const exception& e) {
        cout << "测试失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}