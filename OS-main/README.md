# 学术论文审稿系统
**加油xdm，我们是最牛逼的组！**  
**SCUT 操作系统课程大作业**

一个基于 C++ 实现的完整学术论文同行评审系统，采用客户端-服务器架构，支持多用户角色、并发访问、文件系统管理和快照备份。

---

## 📋 目录

- [系统架构](#系统架构)
- [功能实现情况](#功能实现情况)
- [目录结构](#目录结构)
- [快速开始](#快速开始)
- [使用说明](#使用说明)


---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         Client (客户端)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ CLI 界面层    │  │  会话管理层   │  │  协议构建层   │          │
│  │ CLIInterface │  │SessionManager│  │CommandBuilder│          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                          ↓                                       │
│                  ┌──────────────┐                               │
│                  │  网络通信层   │                               │
│                  │NetworkClient │                               │
│                  └──────────────┘                               │
└─────────────────────────┬───────────────────────────────────────┘
                          │ TCP Socket (Port 8080)
                          ↓
┌─────────────────────────────────────────────────────────────────┐
│                         Server (服务器)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  协议解析层   │  │  认证授权层   │  │  业务逻辑层   │          │
│  │ CLIProtocol  │  │Authenticator │  │ PaperService │          │
│  │              │  │PermissionChk │  │  BackupFlow  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                          ↓                                       │
│                  ┌──────────────┐                               │
│                  │ 文件系统接口  │                               │
│                  │  FSProtocol  │                               │
│                  └──────────────┘                               │
│                          ↓                                       │
│                  ┌──────────────┐                               │
│                  │  LRU 缓存层   │                               │
│                  │  LRUCache    │                               │
│                  └──────────────┘                               │
└─────────────────────────┬───────────────────────────────────────┘
                          │ 函数调用
                          ↓
┌─────────────────────────────────────────────────────────────────┐
│                    FileSystem (文件系统)                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  磁盘管理层   │  │  Inode 层    │  │  目录管理层   │          │
│  │  disk.cpp    │  │  inode.cpp   │  │directory.cpp │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                          ↓                                       │
│                  ┌──────────────┐                               │
│                  │  快照管理层   │                               │
│                  │  Snapshot    │                               │
│                  └──────────────┘                               │
│                          ↓                                       │
│                  ┌──────────────┐                               │
│                  │  磁盘镜像文件 │                               │
│                  │  disk.img    │                               │
│                  └──────────────┘                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## ✅ 功能实现情况

### 核心要求

| 功能模块 | 要求 | 实现状态 | 说明 |
|---------|------|---------|------|
| **文件系统** | | | |
| └ 超级块 | ✅ 必需 | ✅ 已实现 | 包含块大小、inode 数量等元数据 |
| └ Inode 表 | ✅ 必需 | ✅ 已实现 | 支持文件和目录，10 直接块 + 1 间接块 |
| └ 数据块区域 | ✅ 必需 | ✅ 已实现 | 8069 个可用数据块（8MB 磁盘） |
| └ 空闲块位图 | ✅ 必需 | ✅ 已实现 | 位图机制管理 inode 和数据块分配 |
| └ 多级目录 | ✅ 必需 | ✅ 已实现 | 支持任意深度目录树 |
| └ 文件操作 | ✅ 必需 | ✅ 已实现 | 创建、读写、删除、路径解析 |
| └ LRU 缓存 | ✅ 必需 | ✅ 已实现 | 64 容量文件内容缓存，提供统计信息 |
| └ 快照备份 | ✅ 必需 | ✅ 已实现 | COW 快照，支持创建、列出、恢复 |
| **网络架构** | | | |
| └ 客户端发起操作 | ✅ 必需 | ✅ 已实现 | Client 主动发送命令，Server 响应 |
| └ 服务器处理逻辑 | ✅ 必需 | ✅ 已实现 | Server 负责所有业务逻辑和文件系统管理 |
| └ 多客户端并发 | ✅ 必需 | ✅ 已实现 | 多线程模型，每连接一线程 |
| └ 身份认证 | ✅ 必需 | ✅ 已实现 | Session Token 机制 |
| └ 权限控制 | ✅ 必需 | ✅ 已实现 | 基于角色的权限检查（RBAC） |
| └ CLI 客户端 | ✅ 必需 | ✅ 已实现 | 命令解析、协议构建、响应显示 |
| └ 清晰协议 | ✅ 必需 | ✅ 已实现 | 文本协议，格式明确可扩展 |
| └ 客户端隔离 | ✅ 必需 | ✅ 已实现 | Client 不访问本地文件系统 |
| **用户角色** | | | |
| └ 作者（Author） | ✅ 必需 | ✅ 已实现 | 上传、修订、查看状态、下载评审 |
| └ 审稿人（Reviewer） | ✅ 必需 | ✅ 已实现 | 下载论文、提交评审、查看状态 |
| └ 编辑（Editor） | ✅ 必需 | ✅ 已实现 | 分配审稿人、查看状态、最终决策 |
| └ 管理员（Admin） | ✅ 必需 | ✅ 已实现 | 用户管理、备份管理、系统监控 |

### 可选扩展

| 功能 | 实现状态 | 说明 |
|------|---------|------|
| 文件快照备份 | ✅ 已实现 | 基于 COW 机制的快照系统 |
| 自动评审人分配 | ❌ 未实现 | 当前需手动分配 |

### 通信协议说明

| 项目要求 | 当前实现 | 符合度 | 说明 |
|---------|---------|-------|------|
| SSH 通信 | TCP Socket | ⭐⭐⭐⭐☆ | 使用 TCP 替代 SSH，功能完整但无加密 |
| WebSocket (Server↔FS) | 函数调用 | ⭐⭐⭐⭐☆ | Server 与 FS 在同一进程，通过接口解耦 |

**设计说明**：
- **TCP vs SSH**：为简化实现，使用 TCP Socket 替代 SSH。协议清晰且功能完整，易于调试和扩展。
- **进程内调用 vs WebSocket**：Server 与 FileSystem 在同一进程通过 `FSProtocol` 接口通信，架构清晰，便于后续扩展为独立服务。

---

## 📁 目录结构

```
OS/
├── client/                         # 客户端模块
│   ├── CMakeLists.txt              # CMake 构建配置
│   ├── main.cpp                    # 客户端入口
│   ├── build.sh                    # 快速编译脚本
│   ├── include/                    # 头文件
│   │   ├── cli/
│   │   │   └── CLIInterface.h      # CLI 用户界面
│   │   ├── network/
│   │   │   └── NetworkClient.h     # TCP 网络通信
│   │   ├── protocol/
│   │   │   ├── CommandBuilder.h    # 命令构建器
│   │   │   └── ResponseParser.h    # 响应解析器
│   │   └── session/
│   │       └── SessionManager.h    # 会话管理（Token、角色）
│   ├── src/                        # 源文件实现
│   │   ├── cli/
│   │   │   └── CLIInterface.cpp
│   │   ├── network/
│   │   │   └── NetworkClient.cpp
│   │   ├── protocol/
│   │   │   ├── CommandBuilder.cpp
│   │   │   └── ResponseParser.cpp
│   │   └── session/
│   │       └── SessionManager.cpp
│   ├── doc/                        # 文档
│   │   ├── USAGE.md                # 使用说明
│   │   └── IMPLEMENTATION_SUMMARY.md
│   └── test/
│       └── test_commands.txt       # 测试命令集
│
├── server/                         # 服务器模块
│   ├── CMakeLists.txt              # CMake 构建配置
│   ├── main.cpp                    # 服务器入口（TCP 监听 + 多线程）
│   ├── include/                    # 头文件
│   │   ├── auth/                   # 认证授权
│   │   │   ├── Authenticator.h     # 用户认证、Session 管理
│   │   │   ├── PermissionChecker.h # 权限检查
│   │   │   └── RolePolicy.h        # 角色策略
│   │   ├── business/               # 业务逻辑
│   │   │   ├── PaperService.h      # 论文审稿核心业务
│   │   │   ├── BackupFlow.h        # 备份流程
│   │   │   └── ReviewFlow.h        # 审核流程
│   │   ├── cache/                  # 缓存
│   │   │   ├── LRUCache.h          # LRU 缓存模板
│   │   │   └── CacheStatsProvider.h
│   │   ├── protocol/               # 协议层
│   │   │   ├── CLIProtocol.h       # CLI 命令解析
│   │   │   ├── FSProtocol.h        # 文件系统接口契约
│   │   │   ├── ProtocolFactory.h   # 协议工厂
│   │   │   └── RealFileSystemAdapter.h
│   │   └── platform/
│   │       └── socket_compat.h     # 跨平台 Socket 兼容层
│   ├── src/                        # 源文件实现
│   │   ├── auth/
│   │   │   ├── Authenticator.cpp
│   │   │   ├── PermissionChecker.cpp
│   │   │   └── RolePolicy.cpp
│   │   ├── business/
│   │   │   ├── PaperService.cpp
│   │   │   ├── BackupFlow.cpp
│   │   │   └── ReviewFlow.cpp
│   │   ├── cache/
│   │   │   └── LRUCache.cpp
│   │   └── protocol/
│   │       ├── CLIProtocol.cpp
│   │       ├── FSProtocol.cpp      # 内存版 FS + LRU 装饰器
│   │       ├── ProtocolFactory.cpp
│   │       └── RealFileSystemAdapter.cpp
│   └── test/
│       └── test_client.py          # Python 测试客户端
│
├── filesystem/                     # 文件系统模块
│   ├── include/                    # 头文件
│   │   ├── disk.h                  # 磁盘操作、超级块、快照
│   │   ├── inode.h                 # Inode 结构、文件操作
│   │   ├── path.h                  # 路径解析
│   │   └── block_cache.h           # 块级缓存（可选）
│   ├── src/                        # 源文件
│   │   ├── disk.cpp                # 磁盘管理、位图、快照
│   │   ├── inode.cpp               # Inode 操作、文件读写
│   │   ├── directory.cpp           # 目录操作
│   │   ├── path.cpp                # 路径解析实现
│   │   ├── block_cache.cpp         # 块缓存实现
│   │   └── makefile                # 编译配置
│   ├── scripts/                    # 工具脚本
│   │   ├── mkfs.cpp                # 文件系统格式化工具
│   │   └── snapshot_tool.cpp       # 快照管理工具
│   ├── test/                       # 测试程序
│   │   ├── test_filesystem.cpp     # 文件系统功能测试
│   │   ├── test_snapshot.cpp       # 快照功能测试
│   │   └── test_block_cache.cpp    # 缓存测试
│   ├── bin/                        # 编译输出（运行时生成）
│   │   ├── mkfs
│   │   ├── test_filesystem
│   │   └── test_snapshot
│   └── disk/                       # 磁盘镜像目录
│       └── disk.img                # 8MB 磁盘镜像文件
│
├── Guide.md                        # 项目需求文档
├── README.md                       # 本文档
└── 项目实现分析报告.md              # 实现分析报告
```

---

## 🚀 快速开始

### 环境要求

- **编译器**：支持 C++11 的编译器（GCC 4.8+、Clang 3.4+、MSVC 2015+）
- **构建工具**：CMake 3.10+（Client/Server）、Make（FileSystem）
- **操作系统**：Linux、macOS、Windows

### 1. 编译文件系统

```bash
# 进入文件系统目录
cd filesystem/src

# 编译所有组件
make

# 创建磁盘镜像目录
mkdir -p ../disk

# 格式化文件系统
../bin/mkfs

# 运行测试（可选）
../bin/test_filesystem
../bin/test_snapshot
```

**预期输出**：
```
文件系统格式化完成！
磁盘大小: 8 MB
块大小: 1 KB
总块数: 8192
可用数据块: 8069
根目录已创建（inode 0）
```

### 2. 编译并启动服务器

```bash
# 进入服务器目录
cd server

# 创建构建目录
mkdir build
cd build

# 配置 CMake
cmake ..

# 编译
make

# 启动服务器（默认端口 8080）
./server
```

**预期输出**：
```
Application services initialized.
Server listening on port 8080...
```

### 3. 编译并启动客户端

**新开一个终端**：

```bash
# 进入客户端目录
cd client

# 创建构建目录
mkdir build
cd build

# 配置 CMake
cmake ..

# 编译
make

# 启动客户端（连接到 localhost:8080）
./client
```

**预期输出**：
```
========================================
  学术论文审稿系统 - 客户端
========================================
服务器地址: localhost:8080
输入 'help' 查看可用命令
输入 'exit' 退出程序
========================================

[未登录]$
```

### 4. 快速测试

在客户端中执行以下命令：

```bash
# 1. 管理员登录
login admin admin123

# 2. 查看帮助
help

# 3. 查看系统状态
system_status

# 4. 查看缓存统计
cache_stats

# 5. 列出所有用户
user_list

# 6. 登出
logout
```

---

## 📖 使用说明

### 内置测试账号

| 用户名 | 密码 | 角色 | 权限 |
|-------|------|------|------|
| `admin` | `admin123` | 管理员 | 用户管理、备份管理、系统监控 |
| `editor` | `editor123` | 编辑 | 分配审稿人、最终决策、查看评审 |
| `reviewer` | `reviewer123` | 审稿人 | 下载论文、提交评审 |
| `author` | `author123` | 作者 | 上传论文、修订、查看状态 |

### 命令列表

#### 通用命令

```bash
login <用户名> <密码>      # 登录系统
logout                     # 登出
help                       # 显示可用命令
exit                       # 退出程序
```

#### 文件操作（所有角色）

```bash
read <路径>                # 读取文件
write <路径> <内容>        # 写入文件
mkdir <路径>               # 创建目录
```

#### 作者命令

```bash
upload <论文ID>            # 上传论文（会提示输入内容）
revise <论文ID>            # 提交修订版本
status <论文ID>            # 查看论文状态
reviews <论文ID>           # 下载评审意见
```

**示例**：
```bash
[author@AUTHOR]$ upload paper001
请输入论文内容(单独一行输入END结束):
Title: Deep Learning for Computer Vision
Abstract: This paper presents a novel approach...
Introduction: In recent years, deep learning...
END
OK: Paper uploaded.
```

#### 审稿人命令

```bash
download <论文ID>          # 下载论文
review <论文ID>            # 提交评审意见（会提示输入）
status <论文ID>            # 查看论文状态
```

**示例**：
```bash
[reviewer@REVIEWER]$ download paper001
OK: Title: Deep Learning for Computer Vision...

[reviewer@REVIEWER]$ review paper001
请输入评审内容(单独一行输入END结束):
This paper is well-written and presents interesting results.
However, the experimental section needs more details.
Recommendation: Minor Revision
END
OK: Review submitted.
```

#### 编辑命令

```bash
assign <论文ID> <审稿人>   # 分配审稿人
decide <论文ID> <决定>     # 最终决定 (ACCEPT/REJECT)
status <论文ID>            # 查看论文状态
reviews <论文ID>           # 查看所有评审意见
```

**示例**：
```bash
[editor@EDITOR]$ assign paper001 reviewer
OK: Reviewer assigned.

[editor@EDITOR]$ decide paper001 ACCEPT
OK: Decision recorded.
```

#### 管理员命令

```bash
# 用户管理
user_add <用户名> <密码> <角色>    # 添加用户 (角色: ADMIN/EDITOR/REVIEWER/AUTHOR/GUEST)
user_del <用户名>                  # 删除用户
user_list                          # 列出所有用户

# 备份管理
backup_create <路径> [名称]        # 创建快照备份
backup_list                        # 列出所有备份
backup_restore <名称>              # 恢复备份

# 系统监控
system_status                      # 查看系统状态
cache_stats                        # 查看缓存统计（命中率、容量等）
cache_clear                        # 清空缓存
```

**示例**：
```bash
[admin@ADMIN]$ user_add testuser pass123 AUTHOR
OK: User added.

[admin@ADMIN]$ backup_create / backup_20260102
OK: Backup created.

[admin@ADMIN]$ cache_stats
OK: hits=42 misses=15 size=30/64
```

### 完整工作流程示例

#### 论文提交与评审流程

```bash
# 1. 作者上传论文
login author author123
upload paper001
# (输入论文内容，以 END 结束)
logout

# 2. 编辑分配审稿人
login editor editor123
assign paper001 reviewer
logout

# 3. 审稿人下载并评审
login reviewer reviewer123
download paper001
review paper001
# (输入评审意见，以 END 结束)
logout

# 4. 作者查看评审意见
login author author123
reviews paper001
logout

# 5. 作者提交修订版本
login author author123
revise paper001
# (输入修订内容，以 END 结束)
logout

# 6. 编辑做最终决定
login editor editor123
decide paper001 ACCEPT
logout
```

#### 系统管理流程

```bash
# 1. 管理员登录
login admin admin123

# 2. 添加新用户
user_add newauthor pass456 AUTHOR
user_add newreviewer pass789 REVIEWER

# 3. 查看所有用户
user_list

# 4. 创建系统备份
backup_create / backup_before_maintenance

# 5. 查看系统状态
system_status
cache_stats

# 6. 如需恢复备份
backup_list
backup_restore backup_before_maintenance

# 7. 登出
logout
```

---

## 🌟 技术亮点

### 1. 分层架构设计

- **Client 三层架构**：CLI 界面 → 会话管理 → 网络通信，职责清晰
- **Server 模块化设计**：协议解析 → 认证授权 → 业务逻辑 → 文件系统，易于扩展
- **FileSystem 独立性**：完全独立的文件系统实现，可单独测试和使用

### 2. 写时复制（COW）快照

- **零拷贝创建**：创建快照只复制元数据，不复制数据块
- **引用计数管理**：每个数据块维护引用计数，支持多快照共享
- **自动 COW**：写入时自动检测引用计数，必要时复制块
- **空间高效**：多个快照共享未修改的数据块

### 3. LRU 缓存机制

- **文件内容缓存**：Server 层缓存常用文件内容（容量 64）
- **命中率统计**：提供 hits、misses、size、capacity 统计
- **线程安全**：多线程访问时自动加锁保护
- **可观测性**：支持 `CACHE_STATS` 和 `CACHE_CLEAR` 命令

### 4. 并发访问支持

- **多线程模型**：Server 为每个客户端连接创建独立线程
- **线程分离**：使用 `detach()` 模式，支持多客户端同时访问
- **互斥保护**：关键数据结构使用 `std::mutex` 保护

### 5. 基于角色的权限控制（RBAC）

- **四种角色**：Admin、Editor、Reviewer、Author
- **命令级权限**：每个命令检查角色权限
- **资源级权限**：论文操作检查所有者权限
- **Session Token**：安全的会话管理机制

### 6. 清晰的通信协议

- **文本协议**：易于调试和扩展
- **统一格式**：`COMMAND <token> <args...>`
- **明确响应**：`OK:` 或 `ERROR:` 开头
- **可扩展性**：新增命令无需修改协议框架

### 7. 一致性保证

- **启动检查**：文件系统启动时自动检查一致性
- **自动修复**：检测到不一致时自动修复
- **引用计数验证**：验证引用计数与位图的一致性
- **原子操作**：关键操作保证原子性

---

