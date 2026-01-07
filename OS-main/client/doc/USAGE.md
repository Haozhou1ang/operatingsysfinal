# Client端使用指南

## 快速开始

### 1. 编译

```bash
# 方法1: 使用构建脚本
./build.sh

# 方法2: 手动编译
mkdir build 
cd build
cmake ..
make
```

### 2. 运行

```bash
# 连接到默认服务器 (localhost:8080)
./build/client

# 连接到指定服务器
./build/client -h 192.168.1.100 -p 8080
```

### 3. 登录

```
[未登录]$ login admin admin123
成功: 登录成功! 角色: ADMIN
[admin@ADMIN]$
```

## 测试账号

| 用户名   | 密码        | 角色     |
| -------- | ----------- | -------- |
| admin    | admin123    | ADMIN    |
| editor   | editor123   | EDITOR   |
| reviewer | reviewer123 | REVIEWER |
| author   | author123   | AUTHOR   |

## 使用示例

### 示例1: 作者上传论文

```
[未登录]$ login author author123
成功: 登录成功! 角色: AUTHOR

[author@AUTHOR]$ upload paper001
请输入论文内容(单独一行输入END结束):
Title: Deep Learning for Computer Vision
Abstract: This paper presents a novel approach...
Introduction: In recent years, deep learning has...
Methodology: We propose a new architecture...
Results: Our experiments show significant improvements...
Conclusion: This work demonstrates the effectiveness...
END
OK: Paper uploaded successfully

[author@AUTHOR]$ status paper001
OK: Paper status: SUBMITTED

[author@AUTHOR]$ logout
成功: 已登出
```

### 示例2: 编辑分配审稿人

```
[未登录]$ login editor editor123
成功: 登录成功! 角色: EDITOR

[editor@EDITOR]$ assign paper001 reviewer
OK: Reviewer assigned successfully

[editor@EDITOR]$ status paper001
OK: Paper status: UNDER_REVIEW, Reviewers: reviewer

[editor@EDITOR]$ logout
成功: 已登出
```

### 示例3: 审稿人提交评审

```
[未登录]$ login reviewer reviewer123
成功: 登录成功! 角色: REVIEWER

[reviewer@REVIEWER]$ download paper001
OK: Title: Deep Learning for Computer Vision
Abstract: This paper presents a novel approach...
(论文内容...)

[reviewer@REVIEWER]$ review paper001
请输入评审内容(单独一行输入END结束):
Strengths:
- Novel approach to the problem
- Comprehensive experiments
- Clear presentation

Weaknesses:
- Limited comparison with recent methods
- Missing ablation studies

Overall: This is a solid paper with interesting ideas.
Recommendation: Accept with minor revisions
END
OK: Review submitted successfully

[reviewer@REVIEWER]$ logout
成功: 已登出
```

### 示例4: 作者查看评审意见

```
[未登录]$ login author author123
成功: 登录成功! 角色: AUTHOR

[author@AUTHOR]$ reviews paper001
OK: Reviews for paper001:
Reviewer: reviewer
Strengths:
- Novel approach to the problem
- Comprehensive experiments
- Clear presentation
(评审内容...)

[author@AUTHOR]$ revise paper001
请输入修订内容(单独一行输入END结束):
Revision v2:
- Added comparison with recent methods (Section 4.2)
- Added ablation studies (Section 4.3)
- Addressed all reviewer comments
END
OK: Revision submitted successfully

[author@AUTHOR]$ logout
成功: 已登出
```

### 示例5: 编辑做最终决定

```
[未登录]$ login editor editor123
成功: 登录成功! 角色: EDITOR

[editor@EDITOR]$ reviews paper001
OK: Reviews for paper001:
(所有评审意见...)

[editor@EDITOR]$ decide paper001 ACCEPT
OK: Decision recorded: ACCEPT

[editor@EDITOR]$ status paper001
OK: Paper status: ACCEPTED

[editor@EDITOR]$ logout
成功: 已登出
```

### 示例6: 管理员管理用户

```
[未登录]$ login admin admin123
成功: 登录成功! 角色: ADMIN

[admin@ADMIN]$ user_list
OK: Users:
- admin (ADMIN)
- editor (EDITOR)
- reviewer (REVIEWER)
- author (AUTHOR)

[admin@ADMIN]$ user_add newauthor pass123 AUTHOR
OK: User added successfully

[admin@ADMIN]$ user_list
OK: Users:
- admin (ADMIN)
- editor (EDITOR)
- reviewer (REVIEWER)
- author (AUTHOR)
- newauthor (AUTHOR)

[admin@ADMIN]$ user_del newauthor
OK: User deleted successfully

[admin@ADMIN]$ logout
成功: 已登出
```

### 示例7: 管理员备份管理

```
[未登录]$ login admin admin123
成功: 登录成功! 角色: ADMIN

[admin@ADMIN]$ backup_create / snapshot_20231215
OK: Backup created: snapshot_20231215

[admin@ADMIN]$ backup_list
OK: Available backups:
- snapshot_20231215 (2023-12-15 14:30:00)

[admin@ADMIN]$ system_status
OK: System Status:
- Uptime: 2 hours
- Active sessions: 1
- Total papers: 5
- Total users: 4

[admin@ADMIN]$ cache_stats
OK: Cache Statistics:
- Hits: 42
- Misses: 15
- Hit rate: 73.7%
- Size: 30/64

[admin@ADMIN]$ logout
成功: 已登出
```

## 常见问题

### Q1: 如何输入多行内容？

A: 对于需要输入论文、评审等多行内容的命令，系统会提示您输入。输入完成后，单独一行输入 `END` 结束。

### Q2: 如何退出程序？

A: 输入 `exit` 或 `quit` 命令。如果已登录，程序会自动登出。

### Q3: 忘记密码怎么办？

A: 联系管理员使用 `user_del` 删除用户后重新添加，或者直接修改Server端的用户数据。

### Q4: 连接不上服务器？

A: 检查：

1. Server是否已启动
2. 服务器地址和端口是否正确
3. 网络连接是否正常
4. 防火墙是否允许连接

### Q5: 命令执行失败？

A: 常见原因：

1. 未登录或token过期 - 重新登录
2. 权限不足 - 检查当前角色是否有权限执行该命令
3. 参数错误 - 检查命令格式是否正确
4. Server端错误 - 查看Server日志

## 命令速查表

### 通用命令

- `login <用户名> <密码>` - 登录
- `logout` - 登出
- `help` - 帮助
- `exit` - 退出

### 作者命令

- `upload <论文ID>` - 上传论文
- `revise <论文ID>` - 提交修订
- `status <论文ID>` - 查看状态
- `reviews <论文ID>` - 查看评审

### 审稿人命令

- `download <论文ID>` - 下载论文
- `review <论文ID>` - 提交评审
- `status <论文ID>` - 查看状态

### 编辑命令

- `assign <论文ID> <审稿人>` - 分配审稿人
- `decide <论文ID> <ACCEPT|REJECT>` - 做决定
- `status <论文ID>` - 查看状态
- `reviews <论文ID>` - 查看所有评审

### 管理员命令

- `user_add <用户名> <密码> <角色>` - 添加用户
- `user_del <用户名>` - 删除用户
- `user_list` - 列出用户
- `backup_create <路径> [名称]` - 创建备份
- `backup_list` - 列出备份
- `backup_restore <名称>` - 恢复备份
- `system_status` - 系统状态
- `cache_stats` - 缓存统计
- `cache_clear` - 清空缓存

### 文件操作

- `read <路径>` - 读文件
- `write <路径> <内容>` - 写文件
- `mkdir <路径>` - 创建目录

## 技巧提示

1. **命令简写**: 某些命令支持简写，如 `upload` 可以简写为 `upload`
2. **Tab补全**: 未来版本可能支持Tab键补全命令
3. **历史记录**: 使用上下箭头键查看命令历史（依赖终端支持）
4. **批量操作**: 可以将命令写入文件，然后通过重定向执行：`./client < commands.txt`

## 开发与调试

### 查看详细日志

目前Client端输出较简洁。如需调试，可以：

1. 查看Server端日志
2. 使用网络抓包工具（如tcpdump、Wireshark）
3. 在代码中添加调试输出

### 修改源码后重新编译

```bash
cd build
make
```

如果修改了CMakeLists.txt或添加了新文件：

```bash
cd build
cmake ..
make
```

## 更多信息

- 详细架构设计: 参见 `README.md`
- Server端协议: 参见 `../server/readme.md`
- 测试命令集: 参见 `test/test_commands.txt`
