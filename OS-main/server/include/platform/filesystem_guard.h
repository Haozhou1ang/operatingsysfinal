#pragma once

/**
 * 文件系统访问保护
 * 
 * 本头文件用于防止服务器代码直接调用底层文件系统 C API。
 * 
 * ⚠️ 重要安全规则：
 * 
 * 1. 服务器代码必须通过 RealFileSystemAdapter 访问文件系统
 * 2. 禁止直接包含 filesystem/include/disk.h 或 inode.h
 * 3. 禁止直接调用 disk_open, read_block, write_block 等函数
 * 
 * 原因：
 * - 底层 C API 不是线程安全的
 * - RealFileSystemAdapter 提供了必要的互斥锁保护
 * - 直接调用会导致数据竞争和文件系统损坏
 * 
 * 正确用法：
 * 
 *   #include "protocol/RealFileSystemAdapter.h"
 *   
 *   RealFileSystemAdapter fs("disk.img");
 *   std::string content, error;
 *   fs.readFile("/path", content, error);
 * 
 * 错误用法：
 * 
 *   #include "../../filesystem/include/disk.h"  // ❌ 禁止
 *   
 *   int fd = disk_open("disk.img");             // ❌ 禁止
 *   read_block(fd, 0, buffer);                  // ❌ 禁止
 */

// 如果检测到包含了底层文件系统头文件，产生编译错误
#ifdef FS_DISK_H
    #error "禁止在服务器代码中直接包含 filesystem/include/disk.h！请使用 RealFileSystemAdapter。"
#endif

#ifdef FS_INODE_H
    #error "禁止在服务器代码中直接包含 filesystem/include/inode.h！请使用 RealFileSystemAdapter。"
#endif

// 提供编译时检查宏
#define FILESYSTEM_ACCESS_GUARD_ENABLED 1

// 使用说明
namespace filesystem_guard {
    /**
     * 获取文件系统访问的正确方式
     * 
     * 示例代码：
     * 
     * ```cpp
     * #include "protocol/RealFileSystemAdapter.h"
     * 
     * void myFunction() {
     *     // 通过服务容器获取文件系统实例
     *     auto& services = AppServices::instance();
     *     FSProtocol* fs = services.getFSProtocol();
     *     
     *     // 使用文件系统接口
     *     std::string content, error;
     *     if (fs->readFile("/papers/paper1/current.txt", content, error)) {
     *         // 处理内容
     *     } else {
     *         // 处理错误
     *     }
     * }
     * ```
     */
    inline void usage_example() {
        // 这个函数仅用于文档目的，不应被调用
    }
}

// 编译时断言：确保在服务器环境中
#ifndef SERVER_BUILD
    // 如果需要，可以在 CMakeLists.txt 中定义 SERVER_BUILD
    // 这样可以在编译时区分服务器代码和其他代码
#endif

