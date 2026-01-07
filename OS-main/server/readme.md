## 目录结构（Server）
```
server/
├── CMakeLists.txt                # 构建配置
├── main.cpp                      # 入口：TCP监听 + 每连接一线程处理
├── note.txt                      # 简要运行/测试记录
├── readme.md                     # 本文档（server 交付与对接说明）
├── include/
│   ├── auth/                     # 用户/会话/权限
│   │   ├── Authenticator.h
│   │   ├── PermissionChecker.h
│   │   └── RolePolicy.h          # 预留
│   ├── business/                 # 业务编排
│   │   ├── BackupFlow.h
│   │   ├── PaperService.h        # 论文审稿业务核心（作者/审稿人/编辑）
│   │   └── ReviewFlow.h          # 预留：通用“提交审核”能力
│   ├── cache/
│   │   └── LRUCache.h            # LRU缓存模板（server侧用于缓存文件内容）
│   └── protocol/
│       ├── CLIProtocol.h         # 文本协议命令解析
│       ├── FSProtocol.h          # server -> filesystem 的统一接口契约
│       └── ProtocolFactory.h     # 组合根 + 请求调度
├── src/
│   ├── auth/
│   │   └── Authenticator.cpp
│   ├── business/
│   │   ├── BackupFlow.cpp
│   │   └── PaperService.cpp
│   └── protocol/
│       ├── CLIProtocol.cpp
│       ├── FSProtocol.cpp        # 当前为“内存版FS + LRU缓存装饰器”（演示用）
│       └── ProtocolFactory.cpp
└── test/
    └── test_client.py            # Python CLI 客户端（联调/演示）
```

---

## 架构与数据流
1) Client 通过 TCP 发送一条命令文本到 server（当前客户端模型：一条命令一个连接）
2) server/main.cpp 接收连接并创建线程
3) ProtocolFactory::handleRequest 读取命令 → 创建 CLIProtocol → 解析命令
4) CLIProtocol 调用：
   - Authenticator：登录/校验token/登出
   - PermissionChecker：命令级权限判定
   - PaperService：论文业务（作者/审稿人/编辑）
   - BackupFlow：管理员备份
   - FSProtocol：所有数据最终落到文件系统（当前演示为内存版实现）

---

## 与 filesystem 模块对接接口（契约）
server 只依赖 include/protocol/FSProtocol.h 定义的接口；filesystem 组需要提供一个“真实适配实现”，并让 createFSProtocol() 返回该实现。

### FSProtocol 需要实现的接口
- bool createSnapshot(path, snapshotName, errorMsg)
- bool restoreSnapshot(snapshotName, errorMsg)
- vector<string> listSnapshots(path, errorMsg)
- bool readFile(path, contentOut, errorMsg)
- bool writeFile(path, content, errorMsg)
- bool deleteFile(path, errorMsg)
- bool createDirectory(path, errorMsg)
- string getFilePermission(path, user, errorMsg)
- string submitForReview(operation, path, user, errorMsg)

说明：当前 server 的权限主路径是“命令级 RBAC（PermissionChecker）+ 论文资源级校验（PaperService）”；
getFilePermission / submitForReview 更偏向 filesystem 侧 ACL/审核扩展与兼容点，若你们暂不需要可先返回占位信息。

### 语义约定（建议）
- path 使用“类Unix路径”，例如 /papers/p1/current.txt
- readFile：不存在返回 false，errorMsg 填 "File not found."（或等价信息）
- writeFile：父目录不存在时建议 server 先 createDirectory；filesystem 也可选择自动创建
- snapshot：建议语义为“对某个 path 前缀下的数据做一致性快照”

### server 侧缓存（LRU）
- server/src/protocol/FSProtocol.cpp 提供 CachingFSProtocol（文件内容缓存，容量64）
- 缓存键会做路径归一化（统一 / 分隔符与前导 /），并对多线程访问做互斥保护（适配 main.cpp 的并发模型）
- 对接真实 filesystem 时：建议保留该装饰器，让缓存继续工作（不侵入底层FS，实现可替换）

---

## CLI 协议（命令集）
为支持断线重连，除 LOGIN 外所有命令都需要携带 sessionToken。

### 通用
- LOGIN <user> <pass>
- LOGOUT <token>
- HELP <token>

### 文件（通用调试能力）
- READ <token> <path>
- WRITE <token> <path> <content...>
- MKDIR <token> <path>

### 作者（Author）
- PAPER_UPLOAD <token> <paperId> <content...>
- PAPER_REVISE <token> <paperId> <content...>
- STATUS <token> <paperId>
- REVIEWS_DOWNLOAD <token> <paperId>

### 审稿人（Reviewer）
- PAPER_DOWNLOAD <token> <paperId>
- STATUS <token> <paperId>
- REVIEW_SUBMIT <token> <paperId> <reviewContent...>

### 编辑（Editor）
- ASSIGN_REVIEWER <token> <paperId> <reviewerUsername>
- DECIDE <token> <paperId> <ACCEPT|REJECT>
- STATUS <token> <paperId>
- REVIEWS_DOWNLOAD <token> <paperId>

### 管理员（Admin）
- USER_ADD <token> <username> <password> <ADMIN|EDITOR|REVIEWER|AUTHOR|GUEST>
- USER_DEL <token> <username>
- USER_LIST <token>
- BACKUP_CREATE <token> <path> [name]
- BACKUP_LIST <token>
- BACKUP_RESTORE <token> <name>
- SYSTEM_STATUS <token>

### 缓存（LRU，可观测性/测试用）
仅当 server 侧启用了缓存装饰器时可用（默认启用）。
- CACHE_STATS <token>    # 查看缓存统计：hits/misses/size/capacity
- CACHE_CLEAR <token>    # 清空缓存

内置测试账号（可在 src/auth/Authenticator.cpp 修改）：
- admin/admin123
- editor/editor123
- reviewer/reviewer123
- author/author123

---

## 完成情况表（server 交付范围）
| 模块 | 事项 | 状态 | 入口文件 |
|---|---|---|---|
| 网络并发 | TCP监听 + 每连接线程 | 已完成 | main.cpp |
| 协议层 | 文本命令解析与响应 | 已完成 | include/protocol/CLIProtocol.h + src/protocol/CLIProtocol.cpp |
| 认证 | 登录/生成token | 已完成 | include/auth/Authenticator.h + src/auth/Authenticator.cpp |
| 会话 | validate/续期/登出（断线保持） | 已完成 | src/auth/Authenticator.cpp |
| 权限 | 角色+命令级权限 | 已完成 | include/auth/PermissionChecker.h |
| 论文业务 | 上传/修订/下载/状态/分配/决定/评审提交与下载 | 已完成（存储依赖FSProtocol） | include/business/PaperService.h + src/business/PaperService.cpp |
| 管理员 | 用户管理（add/del/list） | 已完成 | include/auth/Authenticator.h + src/auth/Authenticator.cpp |
| 备份 | 创建/列出/恢复快照 | 已完成（依赖FSProtocol快照能力） | include/business/BackupFlow.h + src/business/BackupFlow.cpp |
| 对接接口 | FSProtocol 契约定义 | 已完成 | include/protocol/FSProtocol.h |
| 演示FS | 内存版FS + LRU装饰器 | 已完成（演示用） | src/protocol/FSProtocol.cpp |

---

## 本地测试（Windows）
1) 构建（注意：新增 .cpp 后需重新运行 cmake -S/-B）：
- cmake -S server -B server/build
- cmake --build server/build --config Release --target server

2) 启动服务端：运行 server/build/Release/server.exe（默认端口 8080）

3) 启动客户端：python server/test/test_client.py

建议演示脚本：
1) admin 登录并创建用户：USER_ADD ...
2) author 上传论文：PAPER_UPLOAD ...
3) editor 分配 reviewer：ASSIGN_REVIEWER ...
4) reviewer 下载并提交评审：PAPER_DOWNLOAD / REVIEW_SUBMIT
5) author 下载评审意见：REVIEWS_DOWNLOAD
6) editor 最终决定：DECIDE
7) admin 备份与恢复：BACKUP_CREATE / BACKUP_LIST / BACKUP_RESTORE