"""server/test/test_client.py

用途：
  这是一个“手动测试客户端”，用于在命令行里逐条发送 server 的 CLI 指令，查看返回结果。

特点：
  1) 全中文交互提示 + 详细中文注释，便于当作测试说明书使用
  2) 自动处理：除 LOGIN 外，其他命令会自动注入当前会话 token
  3) 支持多会话（多账号）切换：可同时登录 admin/editor/reviewer/author/guest，随时切换活跃账号
  4) 内置：
     - “可用指令总览”（本地打印，不依赖 server）
     - “手动测试用例清单”（按角色/场景给出建议命令序列）

前置条件：
  - server 已启动并监听 host:port（默认 localhost:8080）
  - 本脚本仅做 client 侧辅助，不修改 server 状态机逻辑
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import socket
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional


# 单次 recv 的读取上限。
# 由于 server 可能返回多行文本（例如 STATUS/USER_LIST），所以需要循环 recv 直到对端关闭。
BUFFER_SIZE = 4096


@dataclass
class ClientConfig:
    """客户端连接配置。"""

    host: str = "localhost"
    port: int = 8080
    timeout_sec: float = 5.0
    retries: int = 3


class ServerClient:
    """一个最小的 TCP 文本协议客户端。

    注意：server 的实现是“一条命令一个连接”（见 server/main.cpp），
    所以这里每次 send 都会创建一次连接，发送命令后等待 server 关闭连接。
    """

    def __init__(self, cfg: ClientConfig):
        self.cfg = cfg

        # 支持多账号：alias -> token
        # 例如：sessions["admin"] = "<token>"
        self.sessions: Dict[str, str] = {}
        self.active_alias: Optional[str] = None

    # ----------------------------
    # 网络层：发送/接收
    # ----------------------------
    def send_raw(self, command: str) -> str:
        """发送原始命令（不做 token 注入）。"""

        command = command.strip()
        if not command:
            return "ERROR: 指令不能为空"

        last_exc: Optional[Exception] = None

        for attempt in range(self.cfg.retries):
            try:
                with socket.create_connection((self.cfg.host, self.cfg.port), timeout=self.cfg.timeout_sec) as s:
                    s.settimeout(self.cfg.timeout_sec)

                    # 发送 UTF-8 文本
                    s.sendall(command.encode("utf-8"))

                    # 告诉 server “我发完了”，便于对端尽快返回并关闭
                    try:
                        s.shutdown(socket.SHUT_WR)
                    except OSError:
                        pass

                    # 循环接收，直到 server 关闭连接
                    chunks: list[bytes] = []
                    while True:
                        data = s.recv(BUFFER_SIZE)
                        if not data:
                            break
                        chunks.append(data)

                return b"".join(chunks).decode("utf-8", errors="replace")
            except (ConnectionRefusedError, socket.timeout) as e:
                last_exc = e
                # 简单重试：短暂 sleep（递增）
                if attempt < self.cfg.retries - 1:
                    time.sleep(0.2 * (attempt + 1))
                    continue
            except Exception as e:
                last_exc = e
                break

        return f"ERROR: 连接/发送失败：{last_exc}"

    def send_raw_bytes(self, command: str) -> bytes:
        """发送原始命令并返回原始 bytes（不做 token 注入）。

        用途：
          - READ 二进制文件（例如 PDF）时，不应先 decode 成字符串，否则会出现乱码/替换字符。
        """

        command = command.strip()
        if not command:
            return b"ERROR: empty command"

        last_exc: Optional[Exception] = None

        for attempt in range(self.cfg.retries):
            try:
                with socket.create_connection((self.cfg.host, self.cfg.port), timeout=self.cfg.timeout_sec) as s:
                    s.settimeout(self.cfg.timeout_sec)
                    s.sendall(command.encode("utf-8"))
                    try:
                        s.shutdown(socket.SHUT_WR)
                    except OSError:
                        pass

                    chunks: list[bytes] = []
                    while True:
                        data = s.recv(BUFFER_SIZE)
                        if not data:
                            break
                        chunks.append(data)

                return b"".join(chunks)
            except (ConnectionRefusedError, socket.timeout) as e:
                last_exc = e
                if attempt < self.cfg.retries - 1:
                    time.sleep(0.2 * (attempt + 1))
                    continue
            except Exception as e:
                last_exc = e
                break

        return f"ERROR: 连接/发送失败：{last_exc}".encode("utf-8", errors="replace")

    def _get_active_token(self) -> Optional[str]:
        if not self.active_alias:
            return None
        return self.sessions.get(self.active_alias)

    def send(self, command: str, *, auto_token: bool = True) -> str:
        """发送命令（默认自动注入 token）。

        规则：
          - LOGIN 不注入 token
          - 其他命令：若用户没手动写 token，则自动插入为第二个参数
        """

        command = command.strip()
        if not command:
            return "ERROR: 指令不能为空"

        cmd = command.split(" ", 1)[0].upper()
        if not auto_token or cmd == "LOGIN":
            return self.send_raw(command)

        token = self._get_active_token()
        if not token:
            return "ERROR: 未登录（没有可用 token）。请先执行：LOGIN <用户名> <密码>"

        parts = command.split()

        # 用户已经手动带了 token（第二段就是 token）则不重复注入
        if len(parts) >= 2 and parts[1] == token:
            return self.send_raw(command)

        # 否则注入 token 到第二段
        rest = command[len(parts[0]) :].lstrip()
        injected = f"{cmd} {token} {rest}" if rest else f"{cmd} {token}"
        return self.send_raw(injected)

    def send_bytes(self, command: str, *, auto_token: bool = True) -> bytes:
        """发送命令并返回原始 bytes（默认自动注入 token）。"""

        command = command.strip()
        if not command:
            return b"ERROR: empty command"

        cmd = command.split(" ", 1)[0].upper()
        if not auto_token or cmd == "LOGIN":
            return self.send_raw_bytes(command)

        token = self._get_active_token()
        if not token:
            return "ERROR: 未登录（没有可用 token）。请先执行：LOGIN <用户名> <密码>".encode("utf-8")

        parts = command.split()
        if len(parts) >= 2 and parts[1] == token:
            return self.send_raw_bytes(command)

        rest = command[len(parts[0]) :].lstrip()
        injected = f"{cmd} {token} {rest}" if rest else f"{cmd} {token}"
        return self.send_raw_bytes(injected)

    # ----------------------------
    # 会话管理：登录/登出/切换
    # ----------------------------
    def login(self, username: str, password: str, *, alias: Optional[str] = None) -> str:
        """登录并保存 token。

        server 返回格式：OK: <token> ROLE=...
        """

        resp = self.send_raw(f"LOGIN {username} {password}")
        if not resp.startswith("OK:"):
            return resp

        # 解析 token
        token = None
        try:
            token = resp.split("OK:", 1)[1].strip().split()[0]
        except Exception:
            token = None

        if not token:
            return "ERROR: 登录成功但无法解析 token（响应格式异常）"

        key = alias or username
        self.sessions[key] = token
        self.active_alias = key
        return resp + f"\n[i] 已保存会话：{key}"

    def logout(self, alias: Optional[str] = None) -> str:
        """登出并清理本地 token 记录。"""

        key = alias or self.active_alias
        if not key:
            return "ERROR: 当前没有活跃会话可登出"

        token = self.sessions.get(key)
        if not token:
            return f"ERROR: 会话不存在：{key}"

        resp = self.send_raw(f"LOGOUT {token}")
        if resp.startswith("OK:"):
            self.sessions.pop(key, None)
            if self.active_alias == key:
                self.active_alias = None
        return resp

    def use(self, alias: str) -> str:
        if alias not in self.sessions:
            return f"ERROR: 未找到会话：{alias}（请先 LOGIN 或用 :sessions 查看）"
        self.active_alias = alias
        return f"OK: 已切换到会话 {alias}"


# ----------------------------
# 本地帮助：指令总览 + 手动测试清单
# ----------------------------

def print_local_help() -> None:
        """统一帮助。

        设计目标：
            - 你输入 :help 与输入 HELP/help，看到的内容完全一致
            - 提示语全部中文；命令关键字（LOGIN/READ/...）保留英文大写，便于与 server 匹配
        """

        print(
                """\
==================== 帮助 ====================

[如何使用]
1) 先登录：LOGIN <用户名> <密码>
     - 登录成功后，客户端会自动保存 token，并设为“当前活跃会话”。


[本地指令（不会发给 server，以冒号 : 开头）]
    :help              显示本帮助（与 HELP/help 完全一致）
    :commands          显示 server 全部可用指令总览（按角色分类）
    :cases             显示“手动测试用例清单”（建议命令序列，可复制粘贴）
    :sessions          显示当前保存的会话列表
    :use <alias>       切换活跃会话（alias 默认是用户名；也可 LOGIN 第三个参数自定义别名）
    :logout [alias]    登出（默认登出活跃会话）
    :serverhelp        获取并显示 server 实际返回的 HELP（会做中文化展示；需要已登录）
    :exit              退出

===============================================================
"""
    )


def _translate_server_help_to_chinese(resp: str) -> str:
    """将 server 返回的 HELP 文本做最小中文化。

    注意：
      - 命令关键字保持原样（LOGIN/READ/...）
      - 仅把提示语/标签翻译成中文，保证“全部中文输出”的体验
    """

    if not resp:
        return "(空响应)"

    if resp.startswith("ERROR:"):
        return "错误：" + resp[len("ERROR:") :].lstrip()

    if not resp.startswith("OK:"):
        return resp

    text = resp
    text = text.replace("OK:", "成功：")
    text = text.replace("ROLE=", "角色=")
    text = text.replace("Commands:", "可用命令：")
    text = text.replace("Common:", "通用：")
    text = text.replace("Author:", "作者：")
    text = text.replace("Reviewer:", "审稿人：")
    text = text.replace("Editor:", "编辑：")
    text = text.replace("Admin:", "管理员：")
    return text


def print_all_server_commands() -> None:
    """打印 server 侧支持的所有命令（来自 server/readme.md 与 CLIProtocol 约定）。"""

    print(
        """\
======== server 可用指令总览 ========

通用：
  LOGIN <user> <pass>
  LOGOUT <token>
  HELP <token>

文件（通用调试）：
  READ <token> <path>
  WRITE <token> <path> <content...>
  MKDIR <token> <path>

作者（AUTHOR）：
  PAPER_UPLOAD <token> <paperId> <content...>
  PAPER_REVISE <token> <paperId> <content...>
  STATUS <token> <paperId>
  REVIEWS_DOWNLOAD <token> <paperId>

审稿人（REVIEWER）：
  PAPER_DOWNLOAD <token> <paperId>
  STATUS <token> <paperId>
  REVIEW_SUBMIT <token> <paperId> <reviewContent...>

编辑（EDITOR）：
  ASSIGN_REVIEWER <token> <paperId> <reviewerUsername>
  DECIDE <token> <paperId> <ACCEPT|REJECT>
  STATUS <token> <paperId>
  REVIEWS_DOWNLOAD <token> <paperId>

管理员（ADMIN）：
  USER_ADD <token> <username> <password> <ADMIN|EDITOR|REVIEWER|AUTHOR|GUEST>
  USER_DEL <token> <username>
  USER_LIST <token>
  BACKUP_CREATE <token> <path> [name]
  BACKUP_LIST <token>
  BACKUP_RESTORE <token> <name>
  SYSTEM_STATUS <token>
    CACHE_STATS <token>
    CACHE_CLEAR <token>

审核（ReviewFlow，当前实现为“提交审核请求”）：
  SUBMIT_REVIEW <token> <operation> <path>

====================================
"""
    )


def print_manual_test_cases() -> None:
    """打印“手动测试用例清单”。

    说明：
      - 这是“建议命令序列”，你可以复制粘贴到本客户端逐条执行。
      - paperId / snapshotName 等建议带时间戳避免冲突。
    """

    print(
        """\
======== 手动测试用例清单（建议）========

[用例 0] 基础连通性
  - 输入一个不存在的命令，应该返回 Unknown command
    NO_SUCH_CMD

[用例 1] 管理员（admin）基本能力：用户管理 + 系统状态
  LOGIN admin admin123
  HELP
  USER_LIST
  USER_ADD test_u1 p123 GUEST
  USER_LIST
  USER_DEL test_u1
  USER_LIST
  SYSTEM_STATUS

[用例 1.1] LRU 缓存（命中/未命中/淘汰）——建议 admin 执行
    说明：CACHE_STATS / CACHE_CLEAR 仅用于测试观测，要求管理员权限。
    (1) 查看初始统计
        CACHE_STATS
    (2) 清空缓存（确保统计从干净状态开始）
        CACHE_CLEAR
        CACHE_STATS
    (3) 命中测试：重复读取同一个文件
        MKDIR /lru
        WRITE /lru/a hello
        CACHE_CLEAR
        READ /lru/a          # 第一次：miss
        READ /lru/a          # 第二次：hit
        CACHE_STATS
    (4) 淘汰测试：读超过 capacity 的不同 key（默认 capacity=64）
        # 先准备文件（写入后会写入缓存，所以要再清空一次）
        WRITE /lru/f01 v01
        WRITE /lru/f02 v02
        ...
        WRITE /lru/f70 v70
        CACHE_CLEAR
        # 依次读取 f01~f70（此时 miss 会快速增长，cache size 最终应为 64）
        READ /lru/f01
        READ /lru/f02
        ...
        READ /lru/f70
        CACHE_STATS
        # 再读一次 f01（大概率已被淘汰）与 f70（大概率仍在缓存）
        READ /lru/f01
        READ /lru/f70
        CACHE_STATS

[用例 2] 文件读写（通用调试）
  MKDIR /tmp
  WRITE /tmp/a hello
  READ /tmp/a

[用例 3] 备份/恢复（建议先用 admin）
  BACKUP_CREATE / snap_123
  BACKUP_LIST
  WRITE /tmp/a changed
  READ /tmp/a
  BACKUP_RESTORE snap_123
  READ /tmp/a

[用例 4] 提交审核请求（ReviewFlow）
  SUBMIT_REVIEW DELETE /tmp/a

[用例 5] Guest 权限验证（读允许/写拒绝）
  LOGIN guest guest
  READ /tmp/a
  WRITE /tmp/a should_fail

[用例 6] 论文全流程（建议多账号切换执行）
  (1) 作者上传
    LOGIN author author123
    PAPER_UPLOAD p1001 content_v1
    STATUS p1001

  (2) 编辑分配审稿人
    LOGIN editor editor123
    ASSIGN_REVIEWER p1001 reviewer

  (3) 审稿人下载/提交评审
    LOGIN reviewer reviewer123
    PAPER_DOWNLOAD p1001
    REVIEW_SUBMIT p1001 looks_good
    STATUS p1001

  (4) 作者/编辑下载评审
    (author) REVIEWS_DOWNLOAD p1001
    (editor) REVIEWS_DOWNLOAD p1001

  (5) 编辑最终决定
    (editor) DECIDE p1001 ACCEPT
    (editor) STATUS p1001

========================================
"""
    )


def run_interactive_shell(client: ServerClient) -> int:
    """交互式手动测试入口。"""

    print("=" * 60)
    print(f"手动测试客户端已启动：{client.cfg.host}:{client.cfg.port}")
    print("输入 :help 查看本地帮助；输入 :exit 退出")
    print("=" * 60)

    while True:
        try:
            prompt = client.active_alias or "(未登录)"
            line = input(f"[{prompt}]> ").strip()
            if not line:
                continue

            # 处理本地指令（以 : 开头）
            if line.startswith(":"):
                parts = line[1:].strip().split()
                cmd = parts[0].lower() if parts else ""
                args = parts[1:]

                if cmd in {"exit", "quit"}:
                    return 0
                if cmd in {"help", "h"}:
                    print_local_help()
                    continue
                if cmd in {"commands", "cmds"}:
                    print_all_server_commands()
                    continue
                if cmd in {"cases", "case"}:
                    print_manual_test_cases()
                    continue
                if cmd in {"sessions", "sess"}:
                    if not client.sessions:
                        print("(空) 当前没有保存任何会话。")
                    else:
                        print("已保存会话：")
                        for k in sorted(client.sessions.keys()):
                            flag = "*" if k == client.active_alias else " "
                            print(f"  {flag} {k}")
                    continue
                if cmd == "use":
                    if len(args) != 1:
                        print("用法：:use <alias>")
                        continue
                    print(client.use(args[0]))
                    continue
                if cmd == "logout":
                    alias = args[0] if args else None
                    print(client.logout(alias))
                    continue
                if cmd == "serverhelp":
                    # 调用 server 的 HELP（需要 token），并把输出做中文化
                    token = client._get_active_token()
                    if not token:
                        print("ERROR: 未登录，无法调用 server 的 HELP。请先 LOGIN。")
                        continue
                    raw = client.send_raw(f"HELP {token}")
                    print(_translate_server_help_to_chinese(raw))
                    continue

                print("ERROR: 未知本地指令。输入 :help 查看可用本地指令。")
                continue

            # 普通情况：当作 server 指令发送
            upper = line.split(" ", 1)[0].upper()

            # 统一：HELP/help 直接显示本地“统一帮助”（不发给 server）
            if upper == "HELP":
                print_local_help()
                continue

            # 1) LOGIN：走专门逻辑，保存 token 并设置活跃会话
            if upper == "LOGIN":
                parts = line.split()
                if len(parts) < 3:
                    print("ERROR: 用法：LOGIN <user> <pass>")
                    continue
                username, password = parts[1], parts[2]
                alias = parts[3] if len(parts) >= 4 else None
                print(client.login(username, password, alias=alias))
                continue

            # 2) LOGOUT：推荐用本地 :logout，但也兼容直接发 server LOGOUT
            if upper == "LOGOUT":
                print(client.send_raw(line))
                continue

            # 3) 其他命令：默认自动注入 token
            resp = client.send(line)
            print(resp)
        except EOFError:
            # 当脚本在没有交互输入的环境下运行（stdin 关闭）会抛 EOFError。
            # 这里做“友好退出”，避免返回码为 1。
            print("\n检测到输入流结束（EOF），已退出。")
            return 0
        except KeyboardInterrupt:
            print("\n检测到 Ctrl+C，已退出。")
            return 0


def _print_exchange(command: str, response: str) -> None:
    print("\n" + "-" * 60)
    max_cmd_len = 240
    if len(command) > max_cmd_len:
        print(f">>> {command[:max_cmd_len]} ...（已截断，原始长度 {len(command)} 字符）")
    else:
        print(f">>> {command}")
    print("--- 响应 ---")
    if response is None:
        print("(无响应)")
        return
    resp = response.rstrip("\n")
    # 避免某些命令输出过长导致终端难以阅读
    max_len = 6000
    if len(resp) > max_len:
        print(resp[:max_len])
        print(f"\n...（已截断，原始长度 {len(resp)} 字符）")
    else:
        print(resp)


def run_smoke_demo(client: ServerClient) -> int:
    """自动化演示/冒烟测试：依次执行一批常用命令并打印输出。

    目标：
      - 覆盖 PWD / LS / TREE 指令与输出展示
      - 额外跑一遍“主流命令”（文件读写 + 论文流程）以便快速人工验收

    注意：
      - 本模式不会解析/断言复杂业务语义，只负责“把命令跑通并展示响应”。
      - 依赖默认账号：admin/admin123, author/author123, editor/editor123, reviewer/reviewer123
    """

    print("=" * 60)
    print("自动化演示模式（smoke/demo）")
    print(f"服务器：{client.cfg.host}:{client.cfg.port}")
    print("=" * 60)

    # 1) ADMIN：系统/文件相关
    _print_exchange("LOGIN admin admin123", client.login("admin", "admin123", alias="admin"))
    _print_exchange("PWD", client.send("PWD"))

    demo_root = f"/demo_test_client_{int(time.time())}"
    _print_exchange(f"MKDIR {demo_root}", client.send(f"MKDIR {demo_root}"))
    _print_exchange(f"WRITE {demo_root}/hello.txt hello_from_test_client", client.send(f"WRITE {demo_root}/hello.txt hello_from_test_client"))
    _print_exchange(f"READ {demo_root}/hello.txt", client.send(f"READ {demo_root}/hello.txt"))

    # 重点：PWD/LS/TREE
    _print_exchange("LS /", client.send("LS /"))
    _print_exchange(f"LS {demo_root}", client.send(f"LS {demo_root}"))
    _print_exchange("TREE /", client.send("TREE /"))

    # 常用管理员命令（输出较长，但对验收很有用）
    _print_exchange("USER_LIST", client.send("USER_LIST"))
    _print_exchange("SYSTEM_STATUS", client.send("SYSTEM_STATUS"))

    # 2) AUTHOR：论文上传
    paper_id = f"demo_paper_{int(time.time())}"
    _print_exchange("LOGIN author author123", client.login("author", "author123", alias="author"))
    _print_exchange(f"PAPER_UPLOAD {paper_id} <content>", client.send(f"PAPER_UPLOAD {paper_id} This_is_a_demo_paper_content"))
    _print_exchange(f"STATUS {paper_id}", client.send(f"STATUS {paper_id}"))

    # 2.1) AUTHOR：二进制论文文件上传（base64）
    # 说明：
    #   - 优先用 server/test/pdf_example 下的真实 PDF
    #   - 若不存在，则用最小 PDF 头部做兜底
    file_paper_id_pdf = f"demo_file_paper_pdf_{int(time.time())}"
    file_paper_id_docx = f"demo_file_paper_docx_{int(time.time())}"
    file_paper_id_rtf = f"demo_file_paper_rtf_{int(time.time())}"
    pdf_dir = Path(__file__).resolve().parent / "pdf_example"
    sample_pdf = None
    if pdf_dir.exists():
        for p in sorted(pdf_dir.glob("*.pdf")):
            if p.is_file():
                sample_pdf = p
                break
    if sample_pdf:
        pdf_bytes = sample_pdf.read_bytes()
        print(f"\n[i] 使用示例PDF：{sample_pdf.name}（{len(pdf_bytes)} bytes）")
    else:
        pdf_bytes = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n1 0 obj\n<<>>\nendobj\ntrailer\n<<>>\n%%EOF\n"
        print("\n[i] 未找到示例PDF，使用最小PDF内容（演示用）")

    pdf_b64 = base64.b64encode(pdf_bytes).decode("ascii")
    _print_exchange(
        f"PAPER_UPLOAD_FILE_B64 {file_paper_id_pdf} pdf <base64...>",
        client.send(f"PAPER_UPLOAD_FILE_B64 {file_paper_id_pdf} pdf {pdf_b64}"),
    )

    # 用 admin 读取，展示头部（便于人工确认）
    _print_exchange("(切回 admin 会话)", client.use("admin"))
    _print_exchange(
        f"READ /papers/{file_paper_id_pdf}/current.pdf",
        "(二进制内容不直接打印，改为下载并保存到本地文件)",
    )

    # 二进制可读性证明：把 PDF 原样保存到本地，然后打印头部/大小/SHA256。
    out_dir = Path(__file__).resolve().parent / "out"
    out_dir.mkdir(parents=True, exist_ok=True)
    local_pdf = out_dir / f"{file_paper_id_pdf}.current.pdf"
    raw = client.send_bytes(f"READ /papers/{file_paper_id_pdf}/current.pdf")
    if raw.startswith(b"OK: "):
        pdf_payload = raw[len(b"OK: ") :]
        local_pdf.write_bytes(pdf_payload)
        header = pdf_payload[:16]
        sha256 = hashlib.sha256(pdf_payload).hexdigest()
        print("\n[i] PDF 下载验证：")
        print(f"    - 已保存：{local_pdf}")
        print(f"    - 大小：{len(pdf_payload)} bytes")
        try:
            print(f"    - 头部（ASCII）：{header.decode('ascii', errors='replace')}")
        except Exception:
            print(f"    - 头部（raw bytes）：{header!r}")
        print(f"    - SHA-256：{sha256}")
        if pdf_payload.startswith(b"%PDF-"):
            print("    - 结论：内容以 %PDF- 开头（是有效 PDF）")
        else:
            print("    - 结论：未检测到 %PDF- 头（可能不是 PDF 或被破坏）")
    else:
        print("\n[!] PDF 下载失败（原始响应前 200 bytes）：")
        print(raw[:200].decode("utf-8", errors="replace"))

    # 同时演示 docx/rtf：只做“可上传 + 可读取”验证
    docx_b64 = base64.b64encode(b"PK\x03\x04" + b"demo_docx_payload").decode("ascii")
    rtf_b64 = base64.b64encode(b"{\\rtf1\\ansi\nHello}" ).decode("ascii")
    _print_exchange("(切回 author 会话)", client.use("author"))
    _print_exchange(
        f"PAPER_UPLOAD_FILE_B64 {file_paper_id_docx} docx <base64...>",
        client.send(f"PAPER_UPLOAD_FILE_B64 {file_paper_id_docx} docx {docx_b64}"),
    )
    _print_exchange(
        f"PAPER_UPLOAD_FILE_B64 {file_paper_id_rtf} rtf <base64...>",
        client.send(f"PAPER_UPLOAD_FILE_B64 {file_paper_id_rtf} rtf {rtf_b64}"),
    )

    _print_exchange("(切回 admin 会话)", client.use("admin"))
    _print_exchange(
        f"READ /papers/{file_paper_id_docx}/current.docx",
        client.send(f"READ /papers/{file_paper_id_docx}/current.docx"),
    )
    _print_exchange(
        f"READ /papers/{file_paper_id_rtf}/current.rtf",
        client.send(f"READ /papers/{file_paper_id_rtf}/current.rtf"),
    )

    # 3) EDITOR：分配审稿人
    _print_exchange("LOGIN editor editor123", client.login("editor", "editor123", alias="editor"))
    _print_exchange(f"ASSIGN_REVIEWER {paper_id} reviewer", client.send(f"ASSIGN_REVIEWER {paper_id} reviewer"))

    # 4) REVIEWER：下载论文并提交审稿
    _print_exchange("LOGIN reviewer reviewer123", client.login("reviewer", "reviewer123", alias="reviewer"))
    _print_exchange(f"PAPER_DOWNLOAD {paper_id}", client.send(f"PAPER_DOWNLOAD {paper_id}"))
    _print_exchange(f"REVIEW_SUBMIT {paper_id} <review>", client.send(f"REVIEW_SUBMIT {paper_id} looks_good"))
    _print_exchange(f"STATUS {paper_id}", client.send(f"STATUS {paper_id}"))

    # 5) EDITOR：最终决定
    _print_exchange("(切回 editor 会话)", client.use("editor"))
    _print_exchange(f"DECIDE {paper_id} ACCEPT", client.send(f"DECIDE {paper_id} ACCEPT"))
    _print_exchange(f"STATUS {paper_id}", client.send(f"STATUS {paper_id}"))

    # 6) AUTHOR：下载审稿意见
    _print_exchange("(切回 author 会话)", client.use("author"))
    _print_exchange(f"REVIEWS_DOWNLOAD {paper_id}", client.send(f"REVIEWS_DOWNLOAD {paper_id}"))

    print("\n" + "=" * 60)
    print("演示完成：以上已展示 PWD/LS/TREE 以及主流命令输出。")
    print("=" * 60)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="server 手动测试客户端（全中文）")
    parser.add_argument("--host", default="localhost", help="server 地址")
    parser.add_argument("--port", type=int, default=8080, help="server 端口")
    parser.add_argument("--timeout", type=float, default=5.0, help="socket 超时（秒）")
    parser.add_argument("--retries", type=int, default=3, help="连接失败重试次数")
    parser.add_argument(
        "--smoke",
        action="store_true",
        help="自动化演示/冒烟测试：跑 PWD/LS/TREE 等主流命令并展示输出（不进入交互模式）",
    )
    args = parser.parse_args()

    cfg = ClientConfig(host=args.host, port=args.port, timeout_sec=args.timeout, retries=args.retries)
    client = ServerClient(cfg)
    if args.smoke:
        return run_smoke_demo(client)
    return run_interactive_shell(client)


if __name__ == "__main__":
    raise SystemExit(main())