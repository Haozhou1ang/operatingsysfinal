// block_cache.h - LRU 块缓存实现
#ifndef FS_BLOCK_CACHE_H
#define FS_BLOCK_CACHE_H

// C 接口声明（不依赖 C++ 特性）
#ifdef __cplusplus
extern "C" {
#endif

/**
 * C 接口：初始化块缓存
 * @param capacity 缓存容量（块数）
 */
void block_cache_init(unsigned long capacity);

/**
 * C 接口：销毁块缓存
 */
void block_cache_destroy();

/**
 * C 接口：读取块（带缓存）
 */
void read_block_cached(int fd, int block_id, void* buf);

/**
 * C 接口：写入块（带缓存）
 */
void write_block_cached(int fd, int block_id, const void* buf);

/**
 * C 接口：刷新所有脏块
 */
void block_cache_flush(int fd);

/**
 * C 接口：清空缓存
 */
void block_cache_clear();

/**
 * C 接口：获取缓存统计信息
 */
void block_cache_get_stats(unsigned long* hits, unsigned long* misses, unsigned long* size, unsigned long* capacity);

/**
 * C 接口：打印缓存统计信息
 */
void block_cache_print_stats();

#ifdef __cplusplus
}
#endif

// C++ 类定义（仅在 C++ 编译时可用）
#ifdef __cplusplus

#include "disk.h"
#include <list>
#include <unordered_map>
#include <cstring>
#include <mutex>

/**
 * BlockCache - LRU 块缓存（线程安全）
 * 
 * 缓存磁盘块的读写操作，减少实际的磁盘 I/O
 * 使用 LRU (Least Recently Used) 策略进行缓存替换
 * 所有公共方法都是线程安全的
 */
class BlockCache {
public:
    /**
     * 构造函数
     * @param capacity 缓存容量（块数）
     */
    explicit BlockCache(size_t capacity);
    
    /**
     * 析构函数
     */
    ~BlockCache();
    
    // 禁止拷贝和移动
    BlockCache(const BlockCache&) = delete;
    BlockCache& operator=(const BlockCache&) = delete;
    
    /**
     * 读取块（带缓存）
     * @param fd 文件描述符
     * @param block_id 块 ID
     * @param buf 输出缓冲区
     * @return 是否成功
     */
    bool read_block_cached(int fd, int block_id, void* buf);
    
    /**
     * 写入块（带缓存）
     * @param fd 文件描述符
     * @param block_id 块 ID
     * @param buf 输入缓冲区
     * @return 是否成功
     */
    bool write_block_cached(int fd, int block_id, const void* buf);
    
    /**
     * 使缓存失效（删除指定块的缓存）
     * @param block_id 块 ID
     */
    void invalidate(int block_id);
    
    /**
     * 清空所有缓存
     */
    void clear();
    
    /**
     * 刷新所有脏块到磁盘
     * @param fd 文件描述符
     * @return 是否成功
     */
    bool flush_all(int fd);
    
    // 统计信息
    size_t get_hits() const { return m_hits; }
    size_t get_misses() const { return m_misses; }
    size_t get_size() const { return m_items.size(); }
    size_t get_capacity() const { return m_capacity; }
    size_t get_replacements() const { return m_replacements; }
    
    /**
     * 打印缓存统计信息
     */
    void print_stats() const;

private:
    // 缓存块结构
    struct CacheBlock {
        int block_id;
        char data[1024];  // BLOCK_SIZE
        bool dirty;  // 是否被修改过
        
        CacheBlock() : block_id(-1), dirty(false) {
            memset(data, 0, 1024);
        }
    };
    
    size_t m_capacity;  // 缓存容量
    
    // LRU 链表：头部是最近使用的，尾部是最久未使用的
    std::list<CacheBlock> m_items;
    
    // 快速查找表：block_id -> 链表迭代器
    std::unordered_map<int, typename std::list<CacheBlock>::iterator> m_lookup;
    
    // 统计信息
    size_t m_hits;          // 缓存命中次数
    size_t m_misses;        // 缓存未命中次数
    size_t m_replacements;  // 缓存替换次数
    
    // 线程安全锁
    mutable std::mutex m_mutex;
    
    /**
     * 将块移动到链表头部（标记为最近使用）
     * 注意：调用者必须持有 m_mutex
     */
    void touch(typename std::list<CacheBlock>::iterator it);
    
    /**
     * 淘汰最久未使用的块
     * 注意：调用者必须持有 m_mutex
     * @param fd 文件描述符
     * @return 是否成功
     */
    bool evict_lru(int fd);
};

#endif // __cplusplus

#endif // FS_BLOCK_CACHE_H
