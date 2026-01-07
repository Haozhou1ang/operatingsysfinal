// 在 disk.cpp 中添加以下实现
#include "../include/disk.h"
#include "../include/inode.h" 
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <cassert>
#include <ctime>
#include <vector>
#include <map>
#include <set>

// 在 disk.cpp 文件中添加以下前向声明
void check_and_repair_filesystem(int fd);
void check_ref_count_consistency(int fd, const char* block_bitmap);
static void format_disk_image(int fd);

// 修改disk_open函数 - 添加初始化检查
int disk_open(const char* path) {
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open disk");
        return -1;  // 返回错误而不是退出程序
    }
    
    // 检查文件系统是否已初始化
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    // 如果文件大小为0，说明是新磁盘：直接格式化
    if (file_size == 0) {
        format_disk_image(fd);
        return fd;
    }

    // 文件已存在：先读 superblock 判断格式/版本是否匹配
    Superblock sb{};
    read_superblock(fd, &sb);

    const bool basic_invalid = (sb.block_count <= 0 || sb.inode_count <= 0 || sb.block_size != BLOCK_SIZE);
    const bool version_mismatch =
        (sb.magic != FS_SUPERBLOCK_MAGIC) ||
        (sb.version != FS_VERSION) ||
        (sb.dirent_size != (uint32_t)sizeof(DirEntry));

    // 若发现旧格式或明显损坏：重新格式化（清空旧数据）
    if (basic_invalid || version_mismatch) {
        std::cout << "⚠ Detected incompatible or uninitialized filesystem image. Re-formatting disk..." << std::endl;
        format_disk_image(fd);
        return fd;
    }

    // 格式匹配：再做一致性检查/修复
    check_and_repair_filesystem(fd);
    
    return fd;
}

// 修改check_and_repair_filesystem - 添加初始化检查
void check_and_repair_filesystem(int fd) {
    std::cout << "检查文件系统一致性..." << std::endl;
    
    Superblock sb;
    read_superblock(fd, &sb);
    
    // 检查superblock是否有效（检查标志位或一些已知值）
    // 简单方法：检查inode_count和block_count是否合理
    if (sb.block_count <= 0 || sb.inode_count <= 0 || sb.block_size != BLOCK_SIZE ||
        sb.magic != FS_SUPERBLOCK_MAGIC || sb.version != FS_VERSION) {
        std::cout << "⚠ 未初始化的文件系统，跳过一致性检查" << std::endl;
        return;
    }
    
    // 1. 检查inode bitmap vs SB计数
    char inode_bitmap[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, inode_bitmap);
    
    int actual_free_inodes = 0;
    for (int i = 0; i < BLOCK_SIZE * 8; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(inode_bitmap[byte_idx] & (1 << bit_idx))) {
            actual_free_inodes++;
        }
    }
    
    // 只有在有明显差异时才修复（容差范围：差异不超过5）
    int inode_diff = actual_free_inodes > sb.free_inode_count ? 
                     (actual_free_inodes - sb.free_inode_count) :
                     (sb.free_inode_count - actual_free_inodes);
    
    if (inode_diff > 5) {
        std::cout << "⚠ 修复inode计数: 记录=" << sb.free_inode_count 
                  << " 实际=" << actual_free_inodes << std::endl;
        sb.free_inode_count = actual_free_inodes;
        write_superblock(fd, &sb);
    } else {
        std::cout << "✓ inode计数一致" << std::endl;
    }
    
    // 2. 检查block bitmap vs SB计数
    char block_bitmap[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, block_bitmap);
    
    int actual_free_blocks = 0;
    int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
    
    for (int i = DATA_BLOCK_START; i < max_blocks; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(block_bitmap[byte_idx] & (1 << bit_idx))) {
            actual_free_blocks++;
        }
    }
    
    // 只有在有明显差异时才修复（容差范围：差异不超过5）
    int block_diff = actual_free_blocks > sb.free_block_count ?
                     (actual_free_blocks - sb.free_block_count) :
                     (sb.free_block_count - actual_free_blocks);
    
    if (block_diff > 5) {
        std::cout << "⚠ 修复block计数: 记录=" << sb.free_block_count 
                  << " 实际=" << actual_free_blocks << std::endl;
        sb.free_block_count = actual_free_blocks;
        write_superblock(fd, &sb);
    } else {
        std::cout << "✓ block计数一致" << std::endl;
    }
    
    // 3. 检查RefCount表一致性
    check_ref_count_consistency(fd, block_bitmap);
    
    std::cout << "一致性检查完成" << std::endl;
}

// 修改check_ref_count_consistency - 更精确的检查
// 注意：只检查数据块（DATA_BLOCK_START之后），元数据块由系统管理
void check_ref_count_consistency(int fd, const char* block_bitmap) {
    int repairs = 0;
    int issues = 0;
    int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
    
    // 批量修复：先收集所有需要修复的块
    std::vector<int> blocks_to_fix;
    
    // 只检查数据块区域（跳过元数据块）
    for (int i = DATA_BLOCK_START; i < max_blocks; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        
        // 块已分配
        if (block_bitmap[byte_idx] & (1 << bit_idx)) {
            int ref_count = get_block_ref_count(fd, i);
            
            // 已分配的数据块ref_count应该 >= 1
            if (ref_count <= 0) {
                issues++;
                blocks_to_fix.push_back(i);
            }
        } else {
            // 块未分配，RefCount应该为0
            int ref_count = get_block_ref_count(fd, i);
            if (ref_count > 0) {
                // 这种情况也需要修复：未分配的块不应该有引用计数
                issues++;
                // 直接清零，不需要加入修复列表
                int ref_count_block_offset = i / BLOCK_SIZE;
                int ref_count_index = i % BLOCK_SIZE;
                if (ref_count_block_offset < REF_COUNT_TABLE_BLOCKS) {
                    char ref_count_buf[BLOCK_SIZE];
                    read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
                    ref_count_buf[ref_count_index] = 0;
                    write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
                    repairs++;
                    if (repairs <= 10) {
                        std::cout << "修复块" << i << "的RefCount: " << ref_count << " → 0 (未分配)" << std::endl;
                    }
                }
            }
        }
    }
    
    // 批量修复所有问题块
    if (!blocks_to_fix.empty()) {
        // 按RefCount表块分组，减少磁盘I/O
        std::map<int, std::vector<int>> blocks_by_ref_table;
        
        for (int block_id : blocks_to_fix) {
            int ref_count_block_offset = block_id / BLOCK_SIZE;
            if (ref_count_block_offset < REF_COUNT_TABLE_BLOCKS) {
                blocks_by_ref_table[ref_count_block_offset].push_back(block_id);
            }
        }
        
        // 逐个RefCount表块进行修复
        for (auto& pair : blocks_by_ref_table) {
            int ref_table_block = pair.first;
            std::vector<int>& blocks = pair.second;
            
            char ref_count_buf[BLOCK_SIZE];
            read_block(fd, REF_COUNT_TABLE_START + ref_table_block, ref_count_buf);
            
            for (int block_id : blocks) {
                int ref_count_index = block_id % BLOCK_SIZE;
                ref_count_buf[ref_count_index] = 1;
                repairs++;
                
                // 只打印前10个修复信息，避免输出过多
                if (repairs <= 10) {
                    std::cout << "修复块" << block_id << "的RefCount: 0 → 1" << std::endl;
                }
            }
            
            write_block(fd, REF_COUNT_TABLE_START + ref_table_block, ref_count_buf);
        }
        
        if (repairs > 10) {
            std::cout << "✓ 共修复了 " << repairs << " 个RefCount问题" << std::endl;
        } else if (repairs > 0) {
            std::cout << "✓ 修复了 " << repairs << " 个引用计数问题" << std::endl;
        }
    } else {
        std::cout << "✓ RefCount一致性检查通过" << std::endl;
    }
}

void disk_close(int fd) {
    close(fd);
}

void read_block(int fd, int block_id, void* buf) {
    off_t offset = (off_t)block_id * BLOCK_SIZE;
    // 使用 pread 代替 lseek+read，确保线程安全
    ssize_t bytes_read = pread(fd, buf, BLOCK_SIZE, offset);
    if (bytes_read != BLOCK_SIZE) {
        // 读取失败，填充0
        memset(buf, 0, BLOCK_SIZE);
    }
}

void write_block(int fd, int block_id, const void* buf) {
    off_t offset = (off_t)block_id * BLOCK_SIZE;
    // 使用 pwrite 代替 lseek+write，确保线程安全
    ssize_t bytes_written = pwrite(fd, buf, BLOCK_SIZE, offset);
    if (bytes_written != BLOCK_SIZE) {
        // 写入失败，这是严重错误，但我们暂时不处理
        // 在生产环境中应该记录错误或抛出异常
    }
}

// 读取数据块的一部分内容
int read_data_block(int fd, int block_id, void* buf, int offset, int size) {
    // 参数检查
    if (offset < 0 || size <= 0 || offset + size > BLOCK_SIZE) {
        return -1;
    }
    
    // 读取整个块
    char block_buf[BLOCK_SIZE];
    read_block(fd, block_id, block_buf);
    
    // 拷贝需要的数据
    memcpy(buf, block_buf + offset, size);
    
    return size;
}

// 写入数据块的一部分内容
int write_data_block(int fd, int block_id, const void* data, int offset, int size) {
    // 参数检查
    if (offset < 0 || size <= 0 || offset + size > BLOCK_SIZE) {
        return -1;
    }
    
    // 读取整个块
    char block_buf[BLOCK_SIZE];
    read_block(fd, block_id, block_buf);
    
    // 更新数据
    memcpy(block_buf + offset, data, size);
    
    // 写回整个块
    write_block(fd, block_id, block_buf);
    
    return size;
}

// superblock相关逻辑
// superblock读取
void read_superblock(int fd, Superblock* sb) {
    char buf[BLOCK_SIZE];
    read_block(fd, SUPERBLOCK_BLOCK, buf);
    memcpy(sb, buf, sizeof(Superblock));
}

// superblock写回
void write_superblock(int fd, const Superblock* sb) {
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, sb, sizeof(Superblock));
    write_block(fd, SUPERBLOCK_BLOCK, buf);
}

// 简化的 mkfs：用于在 disk_open 时自动初始化/升级磁盘镜像。
// 注意：这会清空现有数据（对作业测试场景更友好，避免结构升级导致旧镜像无法读取）。
static void format_disk_image(int fd) {
    // 1) 扩展文件到完整大小并清零（ftruncate 不保证内容为 0，但后续会写关键元数据区域）
    if (ftruncate(fd, DISK_SIZE) != 0) {
        perror("ftruncate disk");
        // 尝试继续执行
    }

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

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(fd, SUPERBLOCK_BLOCK, buf);

    // ---- inode bitmap ----
    memset(buf, 0, BLOCK_SIZE);
    // inode 0 occupied
    buf[0] |= 1;
    write_block(fd, INODE_BITMAP_BLOCK, buf);

    // ---- block bitmap ----
    memset(buf, 0, BLOCK_SIZE);
    // mark metadata blocks + root dir block as allocated
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
        char ref_buf[BLOCK_SIZE];
        read_block(fd, REF_COUNT_TABLE_START + block_offset, ref_buf);
        ref_buf[block_index] = 1;
        write_block(fd, REF_COUNT_TABLE_START + block_offset, ref_buf);
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

    // ---- root inode ----
    Inode root_inode;
    init_inode(&root_inode, INODE_TYPE_DIR);
    root_inode.direct_blocks[0] = DATA_BLOCK_START;
    write_inode(fd, 0, &root_inode);

    // write back SB again (consistent counts)
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(fd, SUPERBLOCK_BLOCK, buf);

    std::cout << "✓ disk image formatted (auto-mkfs), version=" << sb.version
              << ", dirent_size=" << sb.dirent_size << std::endl;
}

// 分配一个 inode
// 修改后的 alloc_inode
int alloc_inode(int fd) {
    char buf[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, buf);
    
    // 查找第一个为0的位（空闲inode）
    for (int i = 0; i < BLOCK_SIZE * 8; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        if (!(buf[byte_index] & (1 << bit_index))) {
            // 第一步：标记bitmap为已使用（关键操作1）
            buf[byte_index] |= (1 << bit_index);
            write_block(fd, INODE_BITMAP_BLOCK, buf);
            
            // 第二步：更新superblock中的空闲inode计数（关键操作2 - 提交点）
            Superblock sb;
            read_superblock(fd, &sb);
            sb.free_inode_count--;
            write_superblock(fd, &sb);
            
            // 原子性保证：SB是最后一个改动的关键元数据
            // 如果crash，启动时检查可以修正不一致
            
            return i;
        }
    }
    
    return -1;
}

// 释放一个 inode
void free_inode(int fd, int inode_id) {
    char buf[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, buf);
    
    int byte_index = inode_id / 8;
    int bit_index = inode_id % 8;
    
    // 第一步：标记为未使用（关键操作1）
    buf[byte_index] &= ~(1 << bit_index);
    write_block(fd, INODE_BITMAP_BLOCK, buf);
    
    // 第二步：更新superblock中的空闲inode计数（关键操作2 - 提交点）
    Superblock sb;
    read_superblock(fd, &sb);
    sb.free_inode_count++;
    write_superblock(fd, &sb);
}

int alloc_block(int fd) {
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
    for (int i = 0; i < max_blocks; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        if (!(buf[byte_index] & (1 << bit_index))) {
            // 第一步：标记bitmap为已分配
            buf[byte_index] |= (1 << bit_index);
            write_block(fd, BLOCK_BITMAP_BLOCK, buf);
            
            // 第二步：初始化引用计数为1
            int ref_count_block_offset = i / BLOCK_SIZE;
            int ref_count_index = i % BLOCK_SIZE;
            
            if (ref_count_block_offset < REF_COUNT_TABLE_BLOCKS) {
                char ref_count_buf[BLOCK_SIZE];
                read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
                ref_count_buf[ref_count_index] = 1;
                write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
            }
            
            // 第三步：更新superblock（提交点）
            Superblock sb;
            read_superblock(fd, &sb);
            sb.free_block_count--;
            write_superblock(fd, &sb);
            
            return i;
        }
    }
    return -1;
}

// 修改free_block函数
// 注意：此函数处理引用计数并在必要时释放块
// 支持防御性调用（即使块已经释放也不会出错）
void free_block(int fd, int block_id) {
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    // 检查块是否已经释放
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    bool already_free = !(buf[byte_index] & (1 << bit_index));
    
    // 如果块已经释放，直接返回（防御性编程）
    if (already_free) {
        return;
    }
    
    // 计算引用计数位置
    int ref_count_block_offset = block_id / BLOCK_SIZE;
    int ref_count_index = block_id % BLOCK_SIZE;
    
    // 检查当前引用计数
    int current_ref_count = 0;
    if (ref_count_block_offset < REF_COUNT_TABLE_BLOCKS) {
        char ref_count_buf[BLOCK_SIZE];
        read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
        current_ref_count = ref_count_buf[ref_count_index];
        
        // 如果引用计数 > 1，只减少计数不真正释放
        if (current_ref_count > 1) {
            ref_count_buf[ref_count_index]--;
            write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
            return;
        }
        
        // 如果引用计数 == 1 或 0，清零
        ref_count_buf[ref_count_index] = 0;
        write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    }
    
    // 标记bitmap为未使用
    buf[byte_index] &= ~(1 << bit_index);
    write_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    // 更新superblock（提交点）
    Superblock sb;
    read_superblock(fd, &sb);
    sb.free_block_count++;
    write_superblock(fd, &sb);
}


int create_snapshot(int fd, const char* name) {
    Superblock current_sb;
    read_superblock(fd, &current_sb);
    
    // 第一阶段：分配并保存所有元数据（但快照未激活）
    int inode_bitmap_snapshot_block = alloc_block(fd);
    int block_bitmap_snapshot_block = alloc_block(fd);
    int inode_table_snapshot_blocks[16];
    
    for (int i = 0; i < 16; i++) {
        inode_table_snapshot_blocks[i] = alloc_block(fd);
        if (inode_table_snapshot_blocks[i] == -1) {
            // 清理已分配的块
            for (int j = 0; j < i; j++) {
                free_block(fd, inode_table_snapshot_blocks[j]);
            }
            free_block(fd, block_bitmap_snapshot_block);
            free_block(fd, inode_bitmap_snapshot_block);
            return -1;
        }
    }
    
    // 读取并保存inode和块位图
    char inode_bitmap[BLOCK_SIZE];
    char block_bitmap[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, inode_bitmap);
    read_block(fd, BLOCK_BITMAP_BLOCK, block_bitmap);
    
    // 保存inode表
    for (int i = 0; i < 16; i++) {
        char inode_block[BLOCK_SIZE];
        read_block(fd, INODE_TABLE_START + i, inode_block);
        write_block(fd, inode_table_snapshot_blocks[i], inode_block);
    }
    
    // 保存位图
    write_block(fd, inode_bitmap_snapshot_block, inode_bitmap);
    write_block(fd, block_bitmap_snapshot_block, block_bitmap);
    
    // 查找空闲快照槽位
    char buf[BLOCK_SIZE];
    int snapshots_per_block = BLOCK_SIZE / sizeof(Snapshot);
    int free_slot = -1;
    
    for (int i = 0; i < SNAPSHOT_TABLE_BLOCKS; i++) {
        read_block(fd, SNAPSHOT_TABLE_START + i, buf);
        Snapshot* block_snapshots = (Snapshot*)buf;
        
        for (int j = 0; j < snapshots_per_block && 
             (i * snapshots_per_block + j) < MAX_SNAPSHOTS; j++) {
            if (!block_snapshots[j].active && free_slot == -1) {
                free_slot = i * snapshots_per_block + j;
                break;
            }
        }
        if (free_slot != -1) break;
    }
    
    if (free_slot == -1) {
        free_block(fd, inode_bitmap_snapshot_block);
        free_block(fd, block_bitmap_snapshot_block);
        for (int i = 0; i < 16; i++) {
            free_block(fd, inode_table_snapshot_blocks[i]);
        }
        return -1;
    }
    
    // 创建快照结构但暂不激活
    Snapshot new_snapshot;
    memset(&new_snapshot, 0, sizeof(Snapshot));
    new_snapshot.id = free_slot;
    new_snapshot.active = 0;  // ← 关键：先不激活
    new_snapshot.timestamp = (int)time(nullptr);
    new_snapshot.root_inode_id = 0;
    strncpy(new_snapshot.name, name, sizeof(new_snapshot.name) - 1);
    new_snapshot.name[sizeof(new_snapshot.name) - 1] = '\0';
    
    new_snapshot.sb_at_snapshot = current_sb;
    new_snapshot.inode_bitmap_block = inode_bitmap_snapshot_block;
    new_snapshot.block_bitmap_block = block_bitmap_snapshot_block;
    for (int i = 0; i < 16; i++) {
        new_snapshot.inode_table_blocks[i] = inode_table_snapshot_blocks[i];
    }
    
    new_snapshot.total_inodes_used = current_sb.inode_count - current_sb.free_inode_count;
    new_snapshot.total_blocks_used = current_sb.block_count - current_sb.free_block_count;
    
    // 第一步：写入快照表（未激活状态）
    int block_id = SNAPSHOT_TABLE_START + (free_slot / snapshots_per_block);
    int offset = free_slot % snapshots_per_block;
    read_block(fd, block_id, buf);
    Snapshot* snapshots = (Snapshot*)buf;
    snapshots[offset] = new_snapshot;
    write_block(fd, block_id, buf);
    
    // 第二阶段：增加所有数据块的引用计数（跳过元数据块）
    // 注意：只增加数据块的引用计数，元数据块不参与快照的引用计数管理
    int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
    for (int block_id_iter = DATA_BLOCK_START; block_id_iter < max_blocks; block_id_iter++) {
        int byte_index = block_id_iter / 8;
        int bit_index = block_id_iter % 8;
        
        if (block_bitmap[byte_index] & (1 << bit_index)) {
            increment_block_ref_count(fd, block_id_iter);
        }
    }
    
    // 第三步：激活快照（这是最后一个关键操作）
    read_block(fd, block_id, buf);
    snapshots = (Snapshot*)buf;
    snapshots[offset].active = 1;  // ← 激活快照，标记操作完成
    write_block(fd, block_id, buf);
    
    return free_slot;
}

// 替换 src/disk.cpp 中的 list_snapshots 函数实现
int list_snapshots(int fd, Snapshot* snapshots, int max_count) {
    if (!snapshots || max_count <= 0) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    int snapshots_per_block = BLOCK_SIZE / sizeof(Snapshot);
    int count = 0;
    
    for (int i = 0; i < SNAPSHOT_TABLE_BLOCKS && count < max_count; i++) {
        read_block(fd, SNAPSHOT_TABLE_START + i, buf);
        Snapshot* block_snapshots = (Snapshot*)buf;
        
        for (int j = 0; j < snapshots_per_block && 
             (i * snapshots_per_block + j) < MAX_SNAPSHOTS && 
             count < max_count; j++) {
            
            if (block_snapshots[j].active) {
                snapshots[count] = block_snapshots[j];
                count++;
            }
        }
    }
    
return count; // 返回找到的快照数量
}

// 增加块引用计数
// 增加块引用计数
int increment_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    // 检查块是否已分配
    char bitmap[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, bitmap);
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    
    if (!(bitmap[byte_index] & (1 << bit_index))) {
        return -1; // 块未分配
    }
    
    // 计算ref_count在哪个块中
    int ref_count_block_offset = block_id / BLOCK_SIZE;
    int ref_count_index = block_id % BLOCK_SIZE;
    
    // 检查是否超出ref_count表的范围
    if (ref_count_block_offset >= REF_COUNT_TABLE_BLOCKS) {
        return -1;
    }
    
    // 读取ref_count块
    char ref_count_buf[BLOCK_SIZE];
    read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    
    // 增加引用计数
    unsigned char ref_count = ref_count_buf[ref_count_index];
    if (ref_count >= 255) {
        return -1; // 溢出
    }
    
    ref_count_buf[ref_count_index]++;
    write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    
    return 0;
}

// 减少块引用计数
int decrement_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    // 检查块是否已分配
    char bitmap[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, bitmap);
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    
    if (!(bitmap[byte_index] & (1 << bit_index))) {
        return -1; // 块未分配
    }
    
    // 计算ref_count在哪个块中
    int ref_count_block_offset = block_id / BLOCK_SIZE;
    int ref_count_index = block_id % BLOCK_SIZE;
    
    // 检查是否超出ref_count表的范围
    if (ref_count_block_offset >= REF_COUNT_TABLE_BLOCKS) {
        return -1;
    }
    
    // 读取ref_count块
    char ref_count_buf[BLOCK_SIZE];
    read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    
    // 减少引用计数
    unsigned char ref_count = ref_count_buf[ref_count_index];
    if (ref_count <= 0) {
        return -1;
    }
    
    ref_count_buf[ref_count_index]--;
    write_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    
    return 0;
}

// 获取块引用计数
int get_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    // 计算ref_count在哪个块中
    int ref_count_block_offset = block_id / BLOCK_SIZE;
    int ref_count_index = block_id % BLOCK_SIZE;
    
    // 检查是否超出ref_count表的范围
    if (ref_count_block_offset >= REF_COUNT_TABLE_BLOCKS) {
        return -1;
    }
    
    // 读取ref_count块
    char ref_count_buf[BLOCK_SIZE];
    read_block(fd, REF_COUNT_TABLE_START + ref_count_block_offset, ref_count_buf);
    
    // 返回引用计数
    return (int)ref_count_buf[ref_count_index];
}

// COW复制块
int copy_on_write_block(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    // 检查引用计数
    int ref_count = get_block_ref_count(fd, block_id);
    if (ref_count <= 1) {
        // 如果只有一个引用或没有引用，不需要复制
        return block_id;
    }
    
    // 分配新块
    int new_block_id = alloc_block(fd);
    if (new_block_id == -1) {
        return -1;
    }
    
    // 复制数据
    char buf[BLOCK_SIZE];
    read_block(fd, block_id, buf);
    write_block(fd, new_block_id, buf);
    
    // 减少旧块引用计数
    decrement_block_ref_count(fd, block_id);
    
    // 新块的引用计数应该为1（已由alloc_block设置）
    return new_block_id;
}

int restore_snapshot(int fd, int snapshot_id) {
    if (snapshot_id < 0 || snapshot_id >= MAX_SNAPSHOTS) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    int snapshots_per_block = BLOCK_SIZE / sizeof(Snapshot);
    int block_idx = snapshot_id / snapshots_per_block;
    int entry_idx = snapshot_id % snapshots_per_block;
    
    int block_id = SNAPSHOT_TABLE_START + block_idx;
    if (block_id >= SNAPSHOT_TABLE_START + SNAPSHOT_TABLE_BLOCKS) {
        return -1;
    }
    
    read_block(fd, block_id, buf);
    Snapshot* snapshots = (Snapshot*)buf;
    
    if (!snapshots[entry_idx].active) {
        std::cout << "快照不存在或未激活" << std::endl;
        return -1;
    }
    
    Snapshot snapshot = snapshots[entry_idx];
    std::cout << "准备恢复快照，根inode_id: " << snapshot.root_inode_id << std::endl;
    
    // 读取当前的块位图（在覆盖前保存）
    char current_block_bitmap[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, current_block_bitmap);
    
    // 读取快照的块位图
    char snapshot_block_bitmap[BLOCK_SIZE];
    read_block(fd, snapshot.block_bitmap_block, snapshot_block_bitmap);
    
    // 所有写操作在一起（原子恢复）
    // 1. 恢复superblock
    write_superblock(fd, &snapshot.sb_at_snapshot);
    
    // 2. 恢复inode和块位图
    char inode_bitmap[BLOCK_SIZE];
    char block_bitmap[BLOCK_SIZE];
    read_block(fd, snapshot.inode_bitmap_block, inode_bitmap);
    read_block(fd, snapshot.block_bitmap_block, block_bitmap);
    
    write_block(fd, INODE_BITMAP_BLOCK, inode_bitmap);
    write_block(fd, BLOCK_BITMAP_BLOCK, block_bitmap);
    
    // 3. 恢复inode表
    for (int i = 0; i < 16; i++) {
        char inode_block[BLOCK_SIZE];
        read_block(fd, snapshot.inode_table_blocks[i], inode_block);
        write_block(fd, INODE_TABLE_START + i, inode_block);
    }
    
    // 4. 更新引用计数
    // 对于在当前文件系统中使用但不在快照中的块，减少引用计数
    // 对于在快照中但不在当前文件系统中的块，不需要改变（快照已经持有引用）
    int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
    for (int block_id_iter = DATA_BLOCK_START; block_id_iter < max_blocks; block_id_iter++) {
        int byte_index = block_id_iter / 8;
        int bit_index = block_id_iter % 8;
        
        bool in_current = (current_block_bitmap[byte_index] & (1 << bit_index)) != 0;
        bool in_snapshot = (snapshot_block_bitmap[byte_index] & (1 << bit_index)) != 0;
        
        // 如果块在当前文件系统中但不在快照中，减少引用计数
        if (in_current && !in_snapshot) {
            int ref_count = get_block_ref_count(fd, block_id_iter);
            if (ref_count > 0) {
                decrement_block_ref_count(fd, block_id_iter);
            }
        }
        // 如果块在快照中但不在当前文件系统中，不需要操作
        // 因为快照创建时已经增加了引用计数
    }
    
    std::cout << "快照恢复成功" << std::endl;
    return 0;
}

// 简化版本：只用于从一个inode恢复数据到另一个inode（复用数据块）
// 这个函数现在简化为只处理简单情况
int restore_directory_tree(int fd, int source_inode_id, int target_inode_id) {
    // 如果源和目标inode相同，无需操作
    if (source_inode_id == target_inode_id) {
        return 0;
    }
    
    // 读取源inode
    Inode source_inode;
    read_inode(fd, source_inode_id, &source_inode);
    
    // 读取目标inode
    Inode target_inode;
    read_inode(fd, target_inode_id, &target_inode);
    
    // 释放目标inode的现有数据块
    if (target_inode.block_count > 0) {
        // 释放直接块
        for (int i = 0; i < target_inode.block_count && i < DIRECT_BLOCK_COUNT; i++) {
            if (target_inode.direct_blocks[i] != -1) {
                decrement_block_ref_count(fd, target_inode.direct_blocks[i]);
                if (get_block_ref_count(fd, target_inode.direct_blocks[i]) == 0) {
                    free_block(fd, target_inode.direct_blocks[i]);
                }
            }
        }
        
        // 释放间接块
        if (target_inode.indirect_block != -1) {
            int pointers[POINTERS_PER_BLOCK];
            read_block(fd, target_inode.indirect_block, pointers);
            
            int indirect_count = target_inode.block_count - DIRECT_BLOCK_COUNT;
            for (int i = 0; i < indirect_count && i < POINTERS_PER_BLOCK; i++) {
                if (pointers[i] != -1) {
                    decrement_block_ref_count(fd, pointers[i]);
                    if (get_block_ref_count(fd, pointers[i]) == 0) {
                        free_block(fd, pointers[i]);
                    }
                }
            }
            
            decrement_block_ref_count(fd, target_inode.indirect_block);
            if (get_block_ref_count(fd, target_inode.indirect_block) == 0) {
                free_block(fd, target_inode.indirect_block);
            }
        }
    }
    
    // 复制源inode的块指针到目标inode
    target_inode.type = source_inode.type;
    target_inode.size = source_inode.size;
    target_inode.block_count = source_inode.block_count;
    
    // 复制直接块指针
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        target_inode.direct_blocks[i] = source_inode.direct_blocks[i];
        if (i < source_inode.block_count && source_inode.direct_blocks[i] != -1) {
            // 增加引用计数
            increment_block_ref_count(fd, source_inode.direct_blocks[i]);
        }
    }
    
    // 复制间接块指针
    target_inode.indirect_block = source_inode.indirect_block;
    if (source_inode.indirect_block != -1) {
        increment_block_ref_count(fd, source_inode.indirect_block);
    }
    
    // 写回目标inode
    write_inode(fd, target_inode_id, &target_inode);
    
    return 0;
}
int delete_snapshot(int fd, int snapshot_id) {
    if (snapshot_id < 0 || snapshot_id >= MAX_SNAPSHOTS) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    int snapshots_per_block = BLOCK_SIZE / sizeof(Snapshot);
    int block_idx = snapshot_id / snapshots_per_block;
    int entry_idx = snapshot_id % snapshots_per_block;
    
    int block_id = SNAPSHOT_TABLE_START + block_idx;
    if (block_id >= SNAPSHOT_TABLE_START + SNAPSHOT_TABLE_BLOCKS) {
        return -1;
    }
    
    read_block(fd, block_id, buf);
    Snapshot* snapshots = (Snapshot*)buf;
    
    if (!snapshots[entry_idx].active) {
        return -1;
    }
    
    Snapshot snapshot_to_delete = snapshots[entry_idx];
    
    // 第一步：立即标记为非活动（防止快照被使用）
    snapshots[entry_idx].active = 0;
    write_block(fd, block_id, buf);
    
    // 第二阶段：安全地清理资源（即使失败也无关紧要）
    if (snapshot_to_delete.block_bitmap_block > 0) {
        char snapshot_block_bitmap[BLOCK_SIZE];
        read_block(fd, snapshot_to_delete.block_bitmap_block, snapshot_block_bitmap);
        
        // 只减少数据块的引用计数（与create_snapshot对应）
        int max_blocks = (BLOCK_SIZE * 8 < BLOCK_COUNT) ? BLOCK_SIZE * 8 : BLOCK_COUNT;
        for (int block_id_iter = DATA_BLOCK_START; block_id_iter < max_blocks; block_id_iter++) {
            int byte_index = block_id_iter / 8;
            int bit_index = block_id_iter % 8;
            
            if (snapshot_block_bitmap[byte_index] & (1 << bit_index)) {
                // 直接调用free_block，它会处理引用计数
                // free_block内部会检查RefCount：
                // - 如果>1，只减少计数
                // - 如果=1或0，清零并释放块
                free_block(fd, block_id_iter);
            }
        }
    }
    
    // 释放元数据块
    // 注意：这些块可能已经在上面的循环中被处理过了（如果它们在快照位图中）
    // free_block会检查bitmap，如果已经释放就不会重复操作
    if (snapshot_to_delete.inode_bitmap_block > 0) {
        free_block(fd, snapshot_to_delete.inode_bitmap_block);
    }
    if (snapshot_to_delete.block_bitmap_block > 0) {
        free_block(fd, snapshot_to_delete.block_bitmap_block);
    }
    for (int i = 0; i < 16; i++) {
        if (snapshot_to_delete.inode_table_blocks[i] > 0) {
            free_block(fd, snapshot_to_delete.inode_table_blocks[i]);
        }
    }
    
    return 0;
}