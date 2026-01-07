// 修改 scripts/mkfs.cpp
#include "../include/disk.h"
#include "../include/inode.h"
#include <unistd.h>
#include <iostream>
#include <cstring>
using namespace std;

// ...前面的代码...

// 修改 scripts/mkfs.cpp 中的main函数:

int main() {
    // 第1步：创建/打开文件（不扩展）
    int fd = disk_open("../disk/disk.img");

    char buf[BLOCK_SIZE];

    // ---- Superblock ----
    Superblock sb{};
    sb.block_size = BLOCK_SIZE;
    sb.block_count = BLOCK_COUNT;
    sb.inode_count = BLOCK_SIZE * 8;
    sb.free_inode_count = BLOCK_SIZE * 8 - 1;
    sb.free_block_count = BLOCK_COUNT - DATA_BLOCK_START - 1;
    sb.magic = FS_SUPERBLOCK_MAGIC;
    sb.version = FS_VERSION;
    sb.dirent_size = (uint32_t)sizeof(DirEntry);
    sb.reserved = 0;

    // ---- 初始化阶段：直接写块，不用alloc ----
    
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(fd, SUPERBLOCK_BLOCK, buf);

    // ---- inode bitmap ----
    memset(buf, 0, BLOCK_SIZE);
    write_block(fd, INODE_BITMAP_BLOCK, buf);

    // ---- block bitmap ----
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < DATA_BLOCK_START + 1; i++) {  // +1 for root dir block
        buf[i / 8] |= (1 << (i % 8));
    }
    write_block(fd, BLOCK_BITMAP_BLOCK, buf);

    // ---- ref_count table ----
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < REF_COUNT_TABLE_BLOCKS; i++) {
        write_block(fd, REF_COUNT_TABLE_START + i, buf);
    }
    
    // 设置元数据块ref_count=1
    for (int i = 0; i <= DATA_BLOCK_START; i++) {
        int block_offset = i / BLOCK_SIZE;
        int block_index = i % BLOCK_SIZE;
        
        read_block(fd, REF_COUNT_TABLE_START + block_offset, buf);
        buf[block_index] = 1;
        write_block(fd, REF_COUNT_TABLE_START + block_offset, buf);
    }

    // ---- inode table ----
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < INODE_TABLE_BLOCK_COUNT; i++) {
        write_block(fd, INODE_TABLE_START + i, buf);
    }
    
    // ---- snapshot table ----
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < SNAPSHOT_TABLE_BLOCKS; i++) {
        write_block(fd, SNAPSHOT_TABLE_START + i, buf);
    }

    // ---- 创建根目录inode（标记bitmap）----
    memset(buf, 0, BLOCK_SIZE);
    buf[0 / 8] |= (1 << (0 % 8));  // inode 0已占用
    write_block(fd, INODE_BITMAP_BLOCK, buf);
    
    Inode root_inode;
    init_inode(&root_inode, INODE_TYPE_DIR);
    root_inode.direct_blocks[0] = DATA_BLOCK_START;
    write_inode(fd, 0, &root_inode);

    // ---- 更新SB的free_inode_count ----
    sb.free_inode_count = BLOCK_SIZE * 8 - 1;
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(fd, SUPERBLOCK_BLOCK, buf);

    // 现在扩展文件到完整大小
    lseek(fd, DISK_SIZE - 1, SEEK_SET);
    write(fd, "\0", 1);

    cout << "✓ disk.img 格式化完成！" << endl;
    cout << "  总块数: " << sb.block_count << endl;
    cout << "  元数据块: " << DATA_BLOCK_START << endl;
    cout << "  数据块: " << sb.free_block_count << endl;
    cout << "  总inode数: " << sb.inode_count << endl;
    cout << "  空闲inode数: " << sb.free_inode_count << endl;
    cout << "✓ 根目录创建成功！" << endl;

    disk_close(fd);
    return 0;
}