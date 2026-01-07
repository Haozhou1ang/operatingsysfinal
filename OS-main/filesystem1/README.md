# FileSystem 模块

## 项目简介

这是操作系统课程大作业的 FileSystem 模块，实现了一个类 Unix 的文件系统，支持多级目录、文件读写、快照备份等功能。该文件系统基于磁盘镜像文件运行，提供完整的文件和目录管理能力。

## 核心特性

### ✅ 已实现功能

- **完整的磁盘管理**：基于磁盘镜像文件的块级读写
- **Inode 系统**：支持文件和目录的索引节点管理
- **位图分配**：基于位图的 inode 和数据块分配机制
- **多级目录**：支持任意深度的目录树结构
- **路径解析**：类 Unix 的路径解析（支持 `/path/to/file` 格式）
- **文件读写**：支持文件的创建、读取、写入、删除
- **直接块 + 间接块**：10 个直接块 + 1 个一级间接块
- **COW 快照**：写时复制（Copy-on-Write）快照机制
- **引用计数**：块级引用计数，支持快照共享数据块
- **一致性检查**：启动时自动检查和修复文件系统一致性

---

## 目录结构

```
filesystem/
├── include/                    # 头文件
│   ├── disk.h                  # 磁盘操作、超级块、快照结构定义
│   ├── inode.h                 # Inode 结构、目录项定义
│   └── path.h                  # 路径解析函数声明
├── src/                        # 源文件
│   ├── disk.cpp                # 磁盘管理、位图分配、快照实现
│   ├── inode.cpp               # Inode 操作、文件读写
│   ├── directory.cpp           # 目录操作（增删查）
│   ├── path.cpp                # 路径解析实现
│   └── makefile                # 编译配置
├── scripts/                    # 工具脚本
│   ├── mkfs.cpp                # 文件系统格式化工具
│   └── snapshot_tool.cpp       # 快照管理工具（可选）
├── test/                       # 测试程序
│   ├── test_filesystem.cpp     # 文件系统功能测试
│   └── test_snapshot.cpp       # 快照功能测试
└── disk/                       # 磁盘镜像目录（运行时创建）
    └── disk.img                # 磁盘镜像文件
```

---

## 磁盘布局

### 整体结构

```
┌─────────────────────────────────────────────────────────────┐
│  Block 0         │  Superblock (超级块)                      │
├─────────────────────────────────────────────────────────────┤
│  Block 1         │  Inode Bitmap (inode 位图)                │
├─────────────────────────────────────────────────────────────┤
│  Block 2         │  Block Bitmap (数据块位图)                │
├─────────────────────────────────────────────────────────────┤
│  Block 3-18      │  Inode Table (inode 表, 16 blocks)        │
├─────────────────────────────────────────────────────────────┤
│  Block 19-22     │  Snapshot Table (快照表, 4 blocks)        │
├─────────────────────────────────────────────────────────────┤
│  Block 23-122    │  Reference Count Table (引用计数, 100 块) │
├─────────────────────────────────────────────────────────────┤
│  Block 123+      │  Data Blocks (数据块区域)                 │
└─────────────────────────────────────────────────────────────┘
```

### 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| **磁盘大小** | 8 MB | `DISK_SIZE = 8 * 1024 * 1024` |
| **块大小** | 1 KB | `BLOCK_SIZE = 1024` |
| **总块数** | 8192 | `BLOCK_COUNT = 8192` |
| **Inode 表大小** | 16 块 | `INODE_TABLE_BLOCK_COUNT = 16` |
| **快照表大小** | 4 块 | `SNAPSHOT_TABLE_BLOCKS = 4` |
| **引用计数表** | 100 块 | `REF_COUNT_TABLE_BLOCKS = 100` |
| **数据块起始** | 123 | `DATA_BLOCK_START = 123` |
| **可用数据块** | 8069 | `8192 - 123 = 8069` |

### 关键数据结构

#### Superblock（超级块）

```cpp
struct Superblock {
    int block_size;         // 块大小（1024 字节）
    int block_count;        // 总块数（8192）
    int inode_count;        // 总 inode 数
    int free_inode_count;   // 空闲 inode 数
    int free_block_count;   // 空闲块数
};
```

#### Inode（索引节点）

```cpp
struct Inode {
    int type;                           // 文件类型（FILE=1, DIR=2）
    int size;                           // 文件大小（字节）
    int block_count;                    // 占用的数据块数量
    int direct_blocks[10];              // 10 个直接块指针
    int indirect_block;                 // 1 个一级间接块指针
};
```

**容量计算**：
- 直接块：`10 × 1KB = 10 KB`
- 间接块：`(1024 / 4) × 1KB = 256 KB`
- **单文件最大**：`10 KB + 256 KB = 266 KB`

#### DirEntry（目录项）

```cpp
struct DirEntry {
    int inode_id;           // inode 编号
    char name[28];          // 文件或目录名（最长 27 字符）
};
```

- 每个块可存储：`1024 / 32 = 32` 个目录项
- 单目录最大文件数：`266 KB / 32 B ≈ 8500` 个

#### Snapshot（快照）

```cpp
struct Snapshot {
    int id;                     // 快照 ID
    int active;                 // 是否激活（1=active, 0=inactive）
    int timestamp;              // 时间戳
    int root_inode_id;          // 快照时的根 inode ID
    char name[32];              // 快照名称
    Superblock sb_at_snapshot;  // 快照时的超级块状态
    int inode_bitmap_block;     // 快照时的 inode 位图块 ID
    int block_bitmap_block;     // 快照时的块位图块 ID
    int inode_table_blocks[16]; // 快照时的 inode 表块 ID
    int total_inodes_used;      // 快照时使用的 inode 数量
    int total_blocks_used;      // 快照时使用的块数量
};
```

---

## 核心功能实现

### 1. 磁盘管理（disk.cpp）

#### 基础操作

```cpp
// 打开磁盘镜像（自动进行一致性检查）
int disk_open(const char* path);

// 关闭磁盘
void disk_close(int fd);

// 读取块
void read_block(int fd, int block_id, void* buf);

// 写入块
void write_block(int fd, int block_id, const void* buf);

// 部分块读写
int read_data_block(int fd, int block_id, void* buf, int offset, int size);
int write_data_block(int fd, int block_id, const void* data, int offset, int size);
```

#### 位图分配

```cpp
// 分配 inode
int alloc_inode(int fd);

// 释放 inode
void free_inode(int fd, int inode_id);

// 分配数据块
int alloc_block(int fd);

// 释放数据块
void free_block(int fd, int block_id);
```

**位图操作原理**：
- 每个位表示一个 inode/块的分配状态（0=空闲，1=已分配）
- 使用位运算进行高效的分配和释放：
  - 检查位：`buf[byte_index] & (1 << bit_index)`
  - 设置位：`buf[byte_index] |= (1 << bit_index)`
  - 清除位：`buf[byte_index] &= ~(1 << bit_index)`

#### 快照功能

```cpp
// 创建快照
int create_snapshot(int fd, const char* name);

// 恢复快照
int restore_snapshot(int fd, int snapshot_id);

// 删除快照
int delete_snapshot(int fd, int snapshot_id);

// 列出快照
int list_snapshots(int fd, Snapshot* snapshots, int max_count);
```

**COW 机制**：
1. 创建快照时，复制元数据（superblock、位图、inode 表）
2. 数据块不复制，只增加引用计数
3. 写入数据块时，检查引用计数：
   - 如果 `ref_count == 1`，直接写入
   - 如果 `ref_count > 1`，先复制块（Copy-on-Write），再写入

```cpp
// 引用计数管理
int increment_block_ref_count(int fd, int block_id);
int decrement_block_ref_count(int fd, int block_id);
int get_block_ref_count(int fd, int block_id);
int copy_on_write_block(int fd, int block_id);
```

---

### 2. Inode 管理（inode.cpp）

#### Inode 操作

```cpp
// 初始化 inode
void init_inode(Inode* inode, int type);

// 读取 inode
int read_inode(int fd, int inode_id, Inode* inode);

// 写入 inode
int write_inode(int fd, int inode_id, const Inode* inode);

// 为 inode 分配数据块
int inode_alloc_block(int fd, Inode* inode);

// 释放 inode 的所有数据块
void inode_free_blocks(int fd, Inode* inode);
```

#### 文件数据操作

```cpp
// 写入文件数据
int inode_write_data(int fd, Inode* inode, int inode_id, 
                     const char* data, int offset, int size);

// 读取文件数据
int inode_read_data(int fd, const Inode* inode, 
                    char* buffer, int offset, int size);
```

**实现细节**：
- 自动处理直接块和间接块的切换
- 支持任意偏移量的读写
- 自动分配新块（写入时）
- 更新文件大小和块计数

---

### 3. 目录管理（directory.cpp）

```cpp
// 添加目录项
int dir_add_entry(int fd, Inode* dir_inode, int dir_inode_id, 
                  const char* name, int inode_id);

// 查找目录项（返回 inode_id）
int dir_find_entry(int fd, const Inode* dir_inode, const char* name);

// 获取第 index 个目录项
int dir_get_entry(int fd, const Inode* dir_inode, int index, DirEntry* entry);

// 删除目录项
int dir_remove_entry(int fd, Inode* dir_inode, int dir_inode_id, const char* name);
```

**实现特点**：
- 目录本质上是一个特殊的文件，内容是 `DirEntry` 数组
- 添加时检查重名
- 删除时用最后一个条目覆盖被删除的条目（避免空洞）

---

### 4. 路径解析（path.cpp）

```cpp
// 解析路径，返回每一级的 inode_id
int parse_path(int fd, const char* path, int* inode_ids, int max_depth);

// 根据路径获取 inode_id
int get_inode_by_path(int fd, const char* path);

// 获取父目录 inode_id 和文件名
int get_parent_inode_and_name(int fd, const char* path, 
                               int* parent_inode_id, char* name);
```

**示例**：
```cpp
// 解析路径 "/papers/paper001/current.txt"
int inode_id = get_inode_by_path(fd, "/papers/paper001/current.txt");

// 获取父目录和文件名
int parent_id;
char filename[DIR_NAME_SIZE];
get_parent_inode_and_name(fd, "/papers/paper001/current.txt", &parent_id, filename);
// parent_id -> /papers/paper001 的 inode_id
// filename -> "current.txt"
```

---

## 编译与运行

### 编译步骤

```bash
# 进入 src 目录
cd filesystem/src

# 编译所有源文件和测试程序
make

# 编译结果：
# - ../bin/mkfs              # 格式化工具
# - ../bin/test_filesystem   # 文件系统测试
# - ../bin/test_snapshot     # 快照测试（如果存在）
```

### 初始化文件系统

```bash
# 创建磁盘镜像目录
mkdir -p ../disk

# 运行格式化工具
../bin/mkfs

# 输出示例：
# 文件系统格式化完成！
# 磁盘大小: 8 MB
# 块大小: 1 KB
# 总块数: 8192
# 可用数据块: 8069
# 根目录已创建（inode 0）
```

### 运行测试

```bash
# 运行文件系统基础测试
../bin/test_filesystem

# 测试内容：
# - 磁盘基本操作
# - Inode 分配
# - 数据块分配
# - 文件读写
# - 目录操作
# - 路径解析

# 运行快照测试
../bin/test_snapshot

# 测试内容：
# - 快照创建
# - 快照恢复
# - 快照删除
# - COW 机制
# - 引用计数
```

---

## API 使用示例

### 示例 1：创建文件并写入数据

```cpp
#include "disk.h"
#include "inode.h"
#include "path.h"

int main() {
    // 1. 打开磁盘
    int fd = disk_open("disk/disk.img");
    
    // 2. 分配 inode
    int file_inode_id = alloc_inode(fd);
    
    // 3. 初始化 inode
    Inode file_inode;
    init_inode(&file_inode, INODE_TYPE_FILE);
    
    // 4. 写入数据
    const char* data = "Hello, FileSystem!";
    inode_write_data(fd, &file_inode, file_inode_id, data, 0, strlen(data));
    
    // 5. 将 inode 写回磁盘
    write_inode(fd, file_inode_id, &file_inode);
    
    // 6. 添加到根目录
    Inode root_inode;
    read_inode(fd, 0, &root_inode);
    dir_add_entry(fd, &root_inode, 0, "hello.txt", file_inode_id);
    
    // 7. 关闭磁盘
    disk_close(fd);
    
    return 0;
}
```

### 示例 2：读取文件

```cpp
int main() {
    int fd = disk_open("disk/disk.img");
    
    // 1. 通过路径获取 inode_id
    int file_inode_id = get_inode_by_path(fd, "/hello.txt");
    if (file_inode_id < 0) {
        printf("文件不存在\n");
        return -1;
    }
    
    // 2. 读取 inode
    Inode file_inode;
    read_inode(fd, file_inode_id, &file_inode);
    
    // 3. 读取文件内容
    char buffer[1024];
    int bytes_read = inode_read_data(fd, &file_inode, buffer, 0, file_inode.size);
    buffer[bytes_read] = '\0';
    
    printf("文件内容: %s\n", buffer);
    
    disk_close(fd);
    return 0;
}
```

### 示例 3：创建目录

```cpp
int main() {
    int fd = disk_open("disk/disk.img");
    
    // 1. 分配 inode
    int dir_inode_id = alloc_inode(fd);
    
    // 2. 初始化为目录类型
    Inode dir_inode;
    init_inode(&dir_inode, INODE_TYPE_DIR);
    write_inode(fd, dir_inode_id, &dir_inode);
    
    // 3. 添加到根目录
    Inode root_inode;
    read_inode(fd, 0, &root_inode);
    dir_add_entry(fd, &root_inode, 0, "papers", dir_inode_id);
    
    disk_close(fd);
    return 0;
}
```

### 示例 4：创建和恢复快照

```cpp
int main() {
    int fd = disk_open("disk/disk.img");
    
    // 1. 创建快照
    int snapshot_id = create_snapshot(fd, "backup_20260102");
    printf("快照创建成功，ID: %d\n", snapshot_id);
    
    // 2. 修改一些数据...
    // （修改文件内容）
    
    // 3. 恢复快照
    restore_snapshot(fd, snapshot_id);
    printf("快照恢复成功\n");
    
    // 4. 删除快照
    delete_snapshot(fd, snapshot_id);
    
    disk_close(fd);
    return 0;
}
```

---

## 与 Server 模块对接

### 对接接口（FSProtocol）

Server 模块通过 `FSProtocol` 接口调用文件系统功能。需要创建一个适配器类：

```cpp
// server/src/protocol/RealFileSystemAdapter.h
#pragma once
#include "FSProtocol.h"
#include <mutex>

class RealFileSystemAdapter : public FSProtocol {
public:
    RealFileSystemAdapter(const char* diskPath);
    ~RealFileSystemAdapter();
    
    bool createSnapshot(const std::string& path, const std::string& snapshotName, 
                       std::string& errorMsg) override;
    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override;
    std::vector<std::string> listSnapshots(const std::string& path, 
                                          std::string& errorMsg) override;
    bool readFile(const std::string& path, std::string& content, 
                 std::string& errorMsg) override;
    bool writeFile(const std::string& path, const std::string& content, 
                  std::string& errorMsg) override;
    bool deleteFile(const std::string& path, std::string& errorMsg) override;
    bool createDirectory(const std::string& path, std::string& errorMsg) override;
    std::string getFilePermission(const std::string& path, const std::string& user, 
                                  std::string& errorMsg) override;
    std::string submitForReview(const std::string& operation, const std::string& path, 
                               const std::string& user, std::string& errorMsg) override;

private:
    int m_fd;               // 磁盘文件描述符
    std::mutex m_mutex;     // 多线程保护
    
    // 辅助函数
    int pathToInodeId(const std::string& path);
    bool ensureParentDirectory(const std::string& path);
};
```

### 实现示例

```cpp
// server/src/protocol/RealFileSystemAdapter.cpp
#include "RealFileSystemAdapter.h"
extern "C" {
    #include "../../filesystem/include/disk.h"
    #include "../../filesystem/include/inode.h"
    #include "../../filesystem/include/path.h"
}

RealFileSystemAdapter::RealFileSystemAdapter(const char* diskPath) {
    m_fd = disk_open(diskPath);
    if (m_fd < 0) {
        throw std::runtime_error("Failed to open disk");
    }
}

RealFileSystemAdapter::~RealFileSystemAdapter() {
    if (m_fd >= 0) {
        disk_close(m_fd);
    }
}

bool RealFileSystemAdapter::readFile(const std::string& path, 
                                     std::string& content, 
                                     std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. 路径解析
    int inode_id = get_inode_by_path(m_fd, path.c_str());
    if (inode_id < 0) {
        errorMsg = "File not found.";
        return false;
    }
    
    // 2. 读取 inode
    Inode inode;
    if (read_inode(m_fd, inode_id, &inode) < 0) {
        errorMsg = "Failed to read inode.";
        return false;
    }
    
    // 3. 检查是否为文件
    if (inode.type != INODE_TYPE_FILE) {
        errorMsg = "Not a file.";
        return false;
    }
    
    // 4. 读取内容
    content.resize(inode.size);
    int bytes_read = inode_read_data(m_fd, &inode, &content[0], 0, inode.size);
    if (bytes_read != inode.size) {
        errorMsg = "Failed to read file content.";
        return false;
    }
    
    return true;
}

bool RealFileSystemAdapter::writeFile(const std::string& path, 
                                      const std::string& content, 
                                      std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. 确保父目录存在
    if (!ensureParentDirectory(path)) {
        errorMsg = "Failed to create parent directory.";
        return false;
    }
    
    // 2. 检查文件是否存在
    int inode_id = get_inode_by_path(m_fd, path.c_str());
    
    if (inode_id < 0) {
        // 文件不存在，创建新文件
        inode_id = alloc_inode(m_fd);
        if (inode_id < 0) {
            errorMsg = "Failed to allocate inode.";
            return false;
        }
        
        Inode new_inode;
        init_inode(&new_inode, INODE_TYPE_FILE);
        
        // 写入数据
        int written = inode_write_data(m_fd, &new_inode, inode_id, 
                                       content.c_str(), 0, content.size());
        if (written != (int)content.size()) {
            errorMsg = "Failed to write file content.";
            free_inode(m_fd, inode_id);
            return false;
        }
        
        // 添加到父目录
        int parent_id;
        char filename[DIR_NAME_SIZE];
        get_parent_inode_and_name(m_fd, path.c_str(), &parent_id, filename);
        
        Inode parent_inode;
        read_inode(m_fd, parent_id, &parent_inode);
        dir_add_entry(m_fd, &parent_inode, parent_id, filename, inode_id);
    } else {
        // 文件存在，覆盖内容
        Inode inode;
        read_inode(m_fd, inode_id, &inode);
        
        // 释放旧的数据块
        inode_free_blocks(m_fd, &inode);
        
        // 重新初始化
        init_inode(&inode, INODE_TYPE_FILE);
        
        // 写入新数据
        int written = inode_write_data(m_fd, &inode, inode_id, 
                                       content.c_str(), 0, content.size());
        if (written != (int)content.size()) {
            errorMsg = "Failed to write file content.";
            return false;
        }
    }
    
    return true;
}

// ... 其他方法的实现
```

---

## 技术亮点

### 1. 写时复制（COW）快照

- **零拷贝创建**：创建快照时只复制元数据，不复制数据块
- **引用计数**：每个数据块维护引用计数，支持多个快照共享
- **自动 COW**：写入时自动检测引用计数，必要时复制块
- **空间高效**：多个快照共享未修改的数据块

### 2. 一致性保证

- **启动检查**：每次打开磁盘时自动检查一致性
- **自动修复**：检测到不一致时自动修复（位图 vs 超级块计数）
- **引用计数验证**：验证引用计数与位图的一致性
- **原子操作**：关键操作（如目录项添加）保证原子性

### 3. 灵活的块管理

- **直接块 + 间接块**：平衡小文件性能和大文件支持
- **按需分配**：写入时才分配数据块
- **自动扩展**：文件增长时自动分配新块
- **高效释放**：删除文件时自动释放所有数据块

### 4. 类 Unix 接口

- **路径解析**：支持 `/path/to/file` 格式
- **目录树**：支持任意深度的目录结构
- **inode 抽象**：统一的文件和目录抽象
- **位图分配**：高效的空间管理

---

## 性能特点

### 优势

1. **小文件性能好**：直接块访问，无需间接寻址
2. **快照创建快**：COW 机制，零拷贝创建
3. **空间利用率高**：引用计数共享数据块
4. **一致性强**：自动检查和修复

### 限制

1. **单文件大小**：最大 266 KB（10 直接块 + 256 间接块）
2. **文件名长度**：最长 27 字符
3. **无缓存**：每次读写都访问磁盘（可在上层添加 LRU 缓存）
4. **无并发控制**：需要上层（Server）提供锁机制

---

## 待优化功能

### 高优先级

1. **块级 LRU 缓存** ⭐⭐⭐
   - 缓存常用数据块
   - 提供命中率统计
   - 可配置缓存大小

2. **多线程安全** ⭐⭐
   - 添加文件级锁
   - 支持并发读
   - 互斥写操作

### 中优先级

1. **二级间接块**
   - 支持更大的文件（最大 64 MB）
   
2. **目录索引**
   - 加速大目录的查找
   - 使用哈希表或 B 树

3. **文件权限**
   - 添加 owner、group、mode 字段
   - 实现权限检查

### 低优先级

1. **符号链接**
2. **硬链接**
3. **文件时间戳**（创建、修改、访问时间）
4. **磁盘配额**

---

## 常见问题

### Q1: 如何增加磁盘大小？

修改 `include/disk.h` 中的 `DISK_SIZE` 常量，然后重新格式化：

```cpp
const int DISK_SIZE = 100 * 1024 * 1024;  // 改为 100MB
```

### Q2: 如何支持更大的文件？

添加二级间接块支持。修改 `Inode` 结构：

```cpp
struct Inode {
    int type;
    int size;
    int block_count;
    int direct_blocks[10];
    int indirect_block;
    int double_indirect_block;  // 新增
};
```

### Q3: 如何添加 LRU 缓存？

参考 Server 模块的 `LRUCache.h`，创建块缓存：

```cpp
LRUCache<int, char[BLOCK_SIZE]> blockCache(64);  // 缓存 64 个块

void cached_read_block(int fd, int block_id, void* buf) {
    auto cached = blockCache.tryGet(block_id);
    if (cached) {
        memcpy(buf, cached.value(), BLOCK_SIZE);
    } else {
        read_block(fd, block_id, buf);
        blockCache.put(block_id, buf);
    }
}
```

### Q4: 如何处理并发访问？

在 Server 层添加互斥锁：

```cpp
std::mutex fs_mutex;

bool readFile(...) {
    std::lock_guard<std::mutex> lock(fs_mutex);
    // 调用文件系统 API
}
```

---

## 测试覆盖

### 已测试功能

- ✅ 磁盘基本操作（读写块）
- ✅ Inode 分配和释放
- ✅ 数据块分配和释放
- ✅ 文件创建、读写、删除
- ✅ 目录创建、添加条目、删除条目
- ✅ 路径解析（单级、多级、根目录）
- ✅ 快照创建、恢复、删除
- ✅ COW 机制
- ✅ 引用计数
- ✅ 一致性检查

### 测试用例

运行 `test_filesystem` 和 `test_snapshot` 可以看到详细的测试输出。

