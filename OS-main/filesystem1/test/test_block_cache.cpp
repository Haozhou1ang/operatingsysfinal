// test_block_cache.cpp - 块缓存测试
#include "../include/disk.h"
#include "../include/block_cache.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace std;

void test_basic_cache() {
    cout << "\n=== 测试基本缓存功能 ===" << endl;
    
    // 打开磁盘
    int fd = disk_open("disk.img");
    assert(fd >= 0);
    
    // 初始化缓存（容量为 10 个块）
    block_cache_init(10);
    
    // 测试写入
    char write_buf[BLOCK_SIZE];
    memset(write_buf, 'A', BLOCK_SIZE);
    write_block_cached(fd, 100, write_buf);
    cout << "✓ 写入块 100" << endl;
    
    // 测试读取（应该命中缓存）
    char read_buf[BLOCK_SIZE];
    read_block_cached(fd, 100, read_buf);
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
    cout << "✓ 读取块 100（应该命中缓存）" << endl;
    
    // 测试读取不同的块（应该未命中）
    memset(write_buf, 'B', BLOCK_SIZE);
    write_block_cached(fd, 101, write_buf);
    read_block_cached(fd, 101, read_buf);
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
    cout << "✓ 读取块 101（应该未命中）" << endl;
    
    // 打印统计信息
    block_cache_print_stats();
    
    // 清理
    block_cache_flush(fd);
    block_cache_destroy();
    disk_close(fd);
}

void test_lru_replacement() {
    cout << "\n=== 测试 LRU 替换策略 ===" << endl;
    
    int fd = disk_open("disk.img");
    assert(fd >= 0);
    
    // 初始化小容量缓存（容量为 3 个块）
    block_cache_init(3);
    
    char buf[BLOCK_SIZE];
    
    // 写入 3 个块，填满缓存
    for (int i = 0; i < 3; i++) {
        memset(buf, 'A' + i, BLOCK_SIZE);
        write_block_cached(fd, 200 + i, buf);
    }
    cout << "✓ 写入 3 个块（200-202），缓存已满" << endl;
    
    // 访问块 200，使其成为最近使用的
    read_block_cached(fd, 200, buf);
    cout << "✓ 访问块 200，使其成为最近使用的" << endl;
    
    // 写入新块 203，应该淘汰块 201（最久未使用）
    memset(buf, 'D', BLOCK_SIZE);
    write_block_cached(fd, 203, buf);
    cout << "✓ 写入块 203，应该淘汰块 201" << endl;
    
    // 验证块 200 和 202 还在缓存中（命中）
    size_t hits_before, misses_before, size, capacity;
    block_cache_get_stats(&hits_before, &misses_before, &size, &capacity);
    
    read_block_cached(fd, 200, buf);
    read_block_cached(fd, 202, buf);
    
    size_t hits_after, misses_after;
    block_cache_get_stats(&hits_after, &misses_after, &size, &capacity);
    
    assert(hits_after == hits_before + 2);
    cout << "✓ 块 200 和 202 仍在缓存中" << endl;
    
    // 验证块 201 不在缓存中（未命中）
    read_block_cached(fd, 201, buf);
    
    size_t hits_final, misses_final;
    block_cache_get_stats(&hits_final, &misses_final, &size, &capacity);
    
    assert(misses_final == misses_after + 1);
    cout << "✓ 块 201 已被淘汰（未命中）" << endl;
    
    // 打印统计信息
    block_cache_print_stats();
    
    // 清理
    block_cache_flush(fd);
    block_cache_destroy();
    disk_close(fd);
}

void test_dirty_block_flush() {
    cout << "\n=== 测试脏块刷新 ===" << endl;
    
    int fd = disk_open("disk.img");
    assert(fd >= 0);
    
    block_cache_init(5);
    
    // 写入一些块
    char write_buf[BLOCK_SIZE];
    memset(write_buf, 'X', BLOCK_SIZE);
    write_block_cached(fd, 300, write_buf);
    write_block_cached(fd, 301, write_buf);
    cout << "✓ 写入块 300 和 301" << endl;
    
    // 刷新缓存
    block_cache_flush(fd);
    cout << "✓ 刷新缓存到磁盘" << endl;
    
    // 清空缓存
    block_cache_clear();
    cout << "✓ 清空缓存" << endl;
    
    // 重新读取，验证数据已写入磁盘
    char read_buf[BLOCK_SIZE];
    read_block_cached(fd, 300, read_buf);
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
    cout << "✓ 数据已正确写入磁盘" << endl;
    
    // 打印统计信息
    block_cache_print_stats();
    
    // 清理
    block_cache_destroy();
    disk_close(fd);
}

void test_cache_disabled() {
    cout << "\n=== 测试禁用缓存 ===" << endl;
    
    int fd = disk_open("disk.img");
    assert(fd >= 0);
    
    // 初始化容量为 0 的缓存（禁用）
    block_cache_init(0);
    
    char buf[BLOCK_SIZE];
    memset(buf, 'Z', BLOCK_SIZE);
    
    // 写入和读取应该直接操作磁盘
    write_block_cached(fd, 400, buf);
    read_block_cached(fd, 400, buf);
    cout << "✓ 缓存已禁用，直接操作磁盘" << endl;
    
    // 统计信息应该显示缓存被禁用
    block_cache_print_stats();
    
    // 清理
    block_cache_destroy();
    disk_close(fd);
}

void test_performance() {
    cout << "\n=== 测试缓存性能 ===" << endl;
    
    int fd = disk_open("disk.img");
    assert(fd >= 0);
    
    // 初始化缓存
    block_cache_init(50);
    
    char buf[BLOCK_SIZE];
    
    // 模拟工作负载：重复访问一些块
    cout << "模拟工作负载..." << endl;
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 20; i++) {
            // 重复访问前 20 个块
            read_block_cached(fd, 500 + i, buf);
        }
    }
    
    // 打印统计信息
    block_cache_print_stats();
    
    // 清理
    block_cache_flush(fd);
    block_cache_destroy();
    disk_close(fd);
}

int main() {
    cout << "块缓存测试开始..." << endl;
    
    try {
        test_basic_cache();
        test_lru_replacement();
        test_dirty_block_flush();
        test_cache_disabled();
        test_performance();
        
        cout << "\n=== 所有测试通过! ===" << endl;
    } catch (const exception& e) {
        cerr << "❌ 测试失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}

