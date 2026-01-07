# 论文审稿系统 - Client端

## 项目简介

这是操作系统课程大作业的Client端实现，提供命令行界面(CLI)与Server进行交互，实现论文审稿系统的各项功能。

## 架构设计

Client端采用分层架构设计：

```
┌─────────────────────────────────┐
│      CLI界面层 (CLIInterface)    │  ← 用户交互、命令解析
├─────────────────────────────────┤
│   会话管理层 (SessionManager)    │  ← 登录状态、Token管理
├─────────────────────────────────┤
│      协议层 (CommandBuilder)     │  ← 命令构建、响应解析
│           (ResponseParser)       │
├─────────────────────────────────┤
│    网络层 (NetworkClient)        │  ← TCP连接、消息收发
└─────────────────────────────────┘
           ↓ TCP Socket
    ┌──────────────┐
    │    Server    │
    └──────────────┘
```

## 目录结构

```
client/
├── CMakeLists.txt              # 构建配置
├── main.cpp                    # 程序入口
├── README.md                   # 本文档
├── include/                    # 头文件
│   ├── network/
│   │   └── NetworkClient.h     # 网络通信层接口
│   ├── protocol/
│   │   ├── CommandBuilder.h    # 命令构建器
│   │   └── ResponseParser.h    # 响应解析器
│   ├── session/
│   │   └── SessionManager.h    # 会话管理器
│   └── cli/
│       └── CLIInterface.h      # CLI用户界面
├── src/                        # 源文件实现
│   ├── network/
│   │   └── NetworkClient.cpp
│   ├── protocol/
│   │   ├── CommandBuilder.cpp
│   │   └── ResponseParser.cpp
│   ├── session/
│   │   └── SessionManager.cpp
│   └── cli/
│       └── CLIInterface.cpp
└── test/                       # 测试文件
```

## 编译与运行

### 编译步骤

```bash
# 进入client目录
cd client

# 创建构建目录
mkdir build
cd build

# 配置CMake
cmake ..

# 编译
make

# 编译完成后，可执行文件为 ./client
```

### 运行

```bash
# 使用默认配置（localhost:8080）
./client

# 指定服务器地址和端口
./client -h 192.168.1.100 -p 8080

# 查看帮助
./client --help
```

## 使用说明

### 登录

启动程序后，首先需要登录：

```
[未登录]$ login admin admin123
成功: 登录成功! 角色: ADMIN
[admin@ADMIN]$
```

内置测试账号（与Server端一致）：
- `admin/admin123` - 管理员
- `editor/editor123` - 编辑
- `reviewer/reviewer123` - 审稿人
- `author/author123` - 作者

### 命令概览

输入 `help` 可查看当前角色可用的命令。

#### 作者 (AUTHOR) 命令

```bash
upload <论文ID>          # 上传论文（会提示输入内容）
revise <论文ID>          # 提交修订版本
status <论文ID>          # 查看论文状态
reviews <论文ID>         # 下载评审意见
```

示例：
```
[author@AUTHOR]$ upload paper001
请输入论文内容(单独一行输入END结束):
This is my paper abstract...
Introduction...
END
OK: Paper uploaded successfully
```

#### 审稿人 (REVIEWER) 命令

```bash
download <论文ID>        # 下载论文
review <论文ID>          # 提交评审意见（会提示输入内容）
status <论文ID>          # 查看论文状态
```

#### 编辑 (EDITOR) 命令

```bash
assign <论文ID> <审稿人>    # 分配审稿人
decide <论文ID> <决定>      # 做最终决定 (ACCEPT/REJECT)
status <论文ID>             # 查看论文状态
reviews <论文ID>            # 查看所有评审意见
```

示例：
```
[editor@EDITOR]$ assign paper001 reviewer
OK: Reviewer assigned

[editor@EDITOR]$ decide paper001 ACCEPT
OK: Decision recorded
```

#### 管理员 (ADMIN) 命令

```bash
# 用户管理
user_add <用户名> <密码> <角色>    # 添加用户
user_del <用户名>                  # 删除用户
user_list                          # 列出所有用户

# 备份管理
backup_create <路径> [名称]        # 创建备份
backup_list                        # 列出所有备份
backup_restore <名称>              # 恢复备份

# 系统监控
system_status                      # 查看系统状态
cache_stats                        # 查看缓存统计
cache_clear                        # 清空缓存
```

示例：
```
[admin@ADMIN]$ user_add testuser pass123 AUTHOR
OK: User added

[admin@ADMIN]$ backup_create / snapshot_20231215
OK: Backup created

[admin@ADMIN]$ cache_stats
OK: Cache hits: 42, misses: 15, size: 30/64
```

#### 通用命令

```bash
read <路径>              # 读取文件
write <路径> <内容>      # 写入文件
mkdir <路径>             # 创建目录
logout                   # 登出
help                     # 显示帮助
exit                     # 退出程序
```

### 多行内容输入

对于需要输入多行内容的命令（如上传论文、提交评审），系统会提示您输入，单独一行输入 `END` 结束：

```
[author@AUTHOR]$ upload paper001
请输入论文内容(单独一行输入END结束):
Title: My Research Paper
Abstract: This paper presents...
Introduction: In recent years...
END
OK: Paper uploaded successfully
```

### 退出程序

```
[admin@ADMIN]$ exit
正在登出...
再见!
```

## 功能特性

### 1. 网络通信层 (NetworkClient)

- **TCP连接**: 使用POSIX socket API实现
- **自动重连**: 每次命令自动建立新连接（适配Server的一次连接一次命令模式）
- **错误处理**: 完善的错误信息反馈

### 2. 协议层 (CommandBuilder & ResponseParser)

- **命令构建**: 自动构建符合Server协议格式的命令
- **响应解析**: 解析 `OK:` 和 `ERROR:` 格式的响应
- **数据提取**: 自动提取token、角色等结构化信息

### 3. 会话管理层 (SessionManager)

- **登录状态维护**: 保存token和用户信息
- **自动token注入**: 所有命令自动携带token
- **角色识别**: 根据角色显示不同的命令菜单

### 4. CLI界面层 (CLIInterface)

- **友好交互**: 清晰的提示符和错误信息
- **命令补全**: 支持简写命令（如 `upload` 可简写为 `upload`）
- **角色相关帮助**: 根据当前角色显示可用命令
- **多行输入**: 支持论文内容等多行文本输入

## 与Server对接

### 协议兼容性

Client完全兼容Server的CLI协议：

| 命令格式 | 示例 |
|---------|------|
| LOGIN | `LOGIN admin admin123` |
| 其他命令 | `COMMAND <token> <args...>` |

| 响应格式 | 示例 |
|---------|------|
| 成功 | `OK: <message>` |
| 失败 | `ERROR: <message>` |

### 测试验证

1. 启动Server（参考 `../server/README.md`）
2. 启动Client并连接
3. 执行测试用例：

```bash
# 测试用例1: 管理员功能
login admin admin123
user_list
system_status
logout

# 测试用例2: 论文全流程
login author author123
upload paper001
# (输入论文内容)
logout

login editor editor123
assign paper001 reviewer
logout

login reviewer reviewer123
download paper001
review paper001
# (输入评审意见)
logout

login editor editor123
decide paper001 ACCEPT
logout
```

## 技术亮点

1. **分层架构**: 清晰的职责划分，易于维护和扩展
2. **RAII管理**: 自动资源管理，防止内存泄漏
3. **错误处理**: 完善的错误传播机制
4. **用户体验**: 友好的交互提示和帮助信息
5. **协议兼容**: 严格遵循Server协议规范

## 已知限制

1. **文件访问**: 按照需求，Client不能访问本地文件，所有内容需手动输入或从stdin重定向
2. **单会话**: 当前版本仅支持单一会话（未实现多会话切换）
3. **内容转义**: 暂不支持包含特殊字符的内容（因为Server协议是空格分隔）

