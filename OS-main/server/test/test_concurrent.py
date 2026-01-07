#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
并发测试脚本 - 论文审稿系统服务器

使用方法：
    python3 test_concurrent.py --clients 10
    python3 test_concurrent.py --clients 20 --scenario login
"""

import socket
import threading
import time
import argparse
import sys

# 服务器配置
SERVER_HOST = 'localhost'
SERVER_PORT = 8080

# 测试用户
TEST_USERS = [
    ('author', 'author123'),
    ('editor', 'editor123'),
    ('reviewer', 'reviewer123'),
]

class Stats:
    def __init__(self):
        self.lock = threading.Lock()
        self.success = 0
        self.failure = 0
        self.errors = []
        self.start_time = None
        self.end_time = None
    
    def record_success(self):
        with self.lock:
            self.success += 1
    
    def record_failure(self, error):
        with self.lock:
            self.failure += 1
            if len(self.errors) < 20:
                self.errors.append(error)
    
    def start(self):
        self.start_time = time.time()
    
    def end(self):
        self.end_time = time.time()
    
    def report(self):
        duration = self.end_time - self.start_time if self.end_time else 0
        total = self.success + self.failure
        success_rate = (self.success / total * 100) if total > 0 else 0
        
        print("\n" + "="*60)
        print("测试结果统计")
        print("="*60)
        print(f"总请求数: {total}")
        print(f"成功: {self.success}")
        print(f"失败: {self.failure}")
        print(f"成功率: {success_rate:.2f}%")
        print(f"耗时: {duration:.2f} 秒")
        if total > 0 and duration > 0:
            print(f"吞吐量: {total/duration:.2f} 请求/秒")
        
        if self.errors:
            print(f"\n错误列表:")
            for i, error in enumerate(self.errors):
                print(f"  {i+1}. {error}")
        print("="*60)

stats = Stats()

def send_command(command):
    """发送命令到服务器并接收响应"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        sock.sendall(command.encode('utf-8'))
        sock.shutdown(socket.SHUT_WR)
        
        response = b''
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
        
        sock.close()
        return response.decode('utf-8', errors='ignore')
    
    except Exception as e:
        return f"ERROR|{str(e)}"

def test_login(client_id, username, password):
    """测试登录"""
    command = f"LOGIN {username} {password}"
    
    print(f"[客户端 {client_id}] 登录: {username}")
    response = send_command(command)
    
    if response.startswith("OK"):
        stats.record_success()
        print(f"[客户端 {client_id}] ✓ 登录成功")
        return True
    else:
        stats.record_failure(f"登录失败 ({username}): {response[:50]}")
        print(f"[客户端 {client_id}] ✗ 登录失败")
        return False

def test_upload(client_id):
    """测试上传论文"""
    username, password = TEST_USERS[0]
    
    # 登录
    login_cmd = f"LOGIN {username} {password}"
    response = send_command(login_cmd)
    
    if not response.startswith("OK"):
        stats.record_failure(f"登录失败: {response[:50]}")
        return
    
    try:
        # 解析响应: "OK: <sessionId> ROLE=<role>"
        parts = response.split()
        session_id = parts[1]  # 第二部分是 sessionId
    except:
        stats.record_failure(f"无法解析 session_id: {response[:50]}")
        return
    
    # 上传论文
    paper_id = f"paper_{client_id}_{int(time.time())}"
    content = f"This is test paper {paper_id}.\nUploaded by client {client_id}."
    upload_cmd = f"PAPER_UPLOAD {session_id} {paper_id}\n{content}"
    
    print(f"[客户端 {client_id}] 上传论文: {paper_id}")
    response = send_command(upload_cmd)
    
    if response.startswith("OK"):
        stats.record_success()
        print(f"[客户端 {client_id}] ✓ 上传成功")
    else:
        stats.record_failure(f"上传失败: {response[:50]}")
        print(f"[客户端 {client_id}] ✗ 上传失败")

def test_mixed(client_id):
    """混合测试"""
    import random
    username, password = random.choice(TEST_USERS)
    
    # 登录
    if not test_login(client_id, username, password):
        return
    
    # 随机延迟
    time.sleep(random.uniform(0.1, 0.5))
    
    # 如果是 author，尝试上传
    if username == 'author':
        test_upload(client_id)

def run_concurrent_test(num_clients, scenario='login'):
    """运行并发测试"""
    print(f"\n{'='*60}")
    print(f"测试场景: {scenario}")
    print(f"并发客户端数: {num_clients}")
    print(f"{'='*60}\n")
    
    threads = []
    stats.start()
    
    for i in range(num_clients):
        if scenario == 'login':
            username, password = TEST_USERS[i % len(TEST_USERS)]
            t = threading.Thread(target=test_login, args=(i, username, password))
        elif scenario == 'upload':
            t = threading.Thread(target=test_upload, args=(i,))
        elif scenario == 'mixed':
            t = threading.Thread(target=test_mixed, args=(i,))
        else:
            print(f"未知场景: {scenario}")
            return
        
        threads.append(t)
        t.start()
        
        # 稍微错开启动时间
        time.sleep(0.05)
    
    # 等待所有线程完成
    for t in threads:
        t.join()
    
    stats.end()
    stats.report()

def main():
    parser = argparse.ArgumentParser(description='论文审稿系统并发测试')
    parser.add_argument('--clients', type=int, default=10, help='并发客户端数量')
    parser.add_argument('--scenario', choices=['login', 'upload', 'mixed'], 
                       default='login', help='测试场景')
    parser.add_argument('--host', default='localhost', help='服务器地址')
    parser.add_argument('--port', type=int, default=8080, help='服务器端口')
    
    args = parser.parse_args()
    
    global SERVER_HOST, SERVER_PORT
    SERVER_HOST = args.host
    SERVER_PORT = args.port
    
    print("="*60)
    print("论文审稿系统 - 并发测试工具")
    print("="*60)
    print(f"服务器: {SERVER_HOST}:{SERVER_PORT}")
    print(f"测试场景: {args.scenario}")
    print(f"并发数: {args.clients}")
    print("="*60)
    
    # 测试连接
    print("\n检查服务器连接...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((SERVER_HOST, SERVER_PORT))
        sock.close()
        print("✓ 服务器连接正常\n")
    except Exception as e:
        print(f"✗ 无法连接到服务器: {e}")
        print("请确保服务器正在运行")
        sys.exit(1)
    
    # 运行测试
    run_concurrent_test(args.clients, args.scenario)

if __name__ == '__main__':
    main()

