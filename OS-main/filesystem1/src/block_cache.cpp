// block_cache.cpp - LRU 块缓存实现
#include "../include/block_cache.h"
#include <iostream>
#include <iomanip>

// 全局缓存实例
static BlockCache* g_block_cache = nullptr;

// ==================== BlockCache 类实现 ====================

BlockCache::BlockCache(size_t capacity) 
    : m_capacity(capacity), m_hits(0), m_misses(0), m_replacements(0) {
    if (capacity > 0) {
        std::cout << "✅ Block cache initialized with capacity: " << capacity << " blocks" << std::endl;
    }
}

BlockCache::~BlockCache() {
    // 析构时打印统计信息
    if (m_capacity > 0) {
        print_stats();
    }
}

void BlockCache::touch(typename std::list<CacheBlock>::iterator it) {
    // 将块移动到链表头部
    m_items.splice(m_items.begin(), m_items, it);
}

bool BlockCache::evict_lru(int fd) {
    if (m_items.empty()) {
        return true;
    }
    
    // 获取最久未使用的块（链表尾部）
    auto& lru_block = m_items.back();
    
    // 如果是脏块，先写回磁盘
    if (lru_block.dirty) {
        read_block(fd, lru_block.block_id, nullptr);  // 确保块存在
        write_block(fd, lru_block.block_id, lru_block.data);
    }
    
    // 从查找表中删除
    m_lookup.erase(lru_block.block_id);
    
    // 从链表中删除
    m_items.pop_back();
    
    m_replacements++;
    return true;
}

bool BlockCache::read_block_cached(int fd, int block_id, void* buf) {
    if (m_capacity == 0) {
        // 缓存被禁用，直接读取
        read_block(fd, block_id, buf);
        return true;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 查找缓存
    auto it = m_lookup.find(block_id);
    
    if (it != m_lookup.end()) {
        // 缓存命中
        m_hits++;
        
        // 将块移动到链表头部
        touch(it->second);
        
        // 复制数据
        memcpy(buf, it->second->data, BLOCK_SIZE);
        return true;
    }
    
    // 缓存未命中
    m_misses++;
    
    // 检查缓存是否已满
    if (m_items.size() >= m_capacity) {
        // 淘汰最久未使用的块
        if (!evict_lru(fd)) {
            return false;
        }
    }
    
    // 从磁盘读取块
    char temp_buf[BLOCK_SIZE];
    read_block(fd, block_id, temp_buf);
    
    // 添加到缓存（链表头部）
    m_items.emplace_front();
    auto& new_block = m_items.front();
    new_block.block_id = block_id;
    memcpy(new_block.data, temp_buf, BLOCK_SIZE);
    new_block.dirty = false;
    
    // 更新查找表
    m_lookup[block_id] = m_items.begin();
    
    // 复制数据到输出缓冲区
    memcpy(buf, temp_buf, BLOCK_SIZE);
    
    return true;
}

bool BlockCache::write_block_cached(int fd, int block_id, const void* buf) {
    if (m_capacity == 0) {
        // 缓存被禁用，直接写入磁盘
        write_block(fd, block_id, buf);
        return true;
    }
    
    // 必须先获取锁再写入，确保缓存和磁盘的一致性
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 写穿策略：同时更新缓存和磁盘
    write_block(fd, block_id, buf);
    
    // 查找缓存
    auto it = m_lookup.find(block_id);
    
    if (it != m_lookup.end()) {
        // 缓存命中，更新缓存中的数据
        m_hits++;
        
        // 将块移动到链表头部
        touch(it->second);
        
        // 更新缓存数据（已写入磁盘，不需要标记为脏）
        memcpy(it->second->data, buf, BLOCK_SIZE);
        it->second->dirty = false;
        
        return true;
    }
    
    // 缓存未命中
    m_misses++;
    
    // 检查缓存是否已满
    if (m_items.size() >= m_capacity) {
        // 淘汰最久未使用的块
        if (!evict_lru(fd)) {
            return false;
        }
    }
    
    // 添加到缓存（链表头部）
    m_items.emplace_front();
    auto& new_block = m_items.front();
    new_block.block_id = block_id;
    memcpy(new_block.data, buf, BLOCK_SIZE);
    new_block.dirty = false;  // 已写入磁盘，不是脏块
    
    // 更新查找表
    m_lookup[block_id] = m_items.begin();
    
    return true;
}

void BlockCache::invalidate(int block_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_lookup.find(block_id);
    if (it == m_lookup.end()) {
        return;
    }
    
    // 从链表和查找表中删除
    m_items.erase(it->second);
    m_lookup.erase(it);
}

void BlockCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_items.clear();
    m_lookup.clear();
}

bool BlockCache::flush_all(int fd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& block : m_items) {
        if (block.dirty) {
            write_block(fd, block.block_id, block.data);
            block.dirty = false;
        }
    }
    return true;
}

void BlockCache::print_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_capacity == 0) {
        std::cout << "📊 Block Cache: DISABLED" << std::endl;
        return;
    }
    
    size_t total_accesses = m_hits + m_misses;
    double hit_rate = (total_accesses > 0) ? (100.0 * m_hits / total_accesses) : 0.0;
    
    std::cout << "\n📊 Block Cache Statistics:" << std::endl;
    std::cout << "   Capacity:     " << m_capacity << " blocks" << std::endl;
    std::cout << "   Current Size: " << m_items.size() << " blocks" << std::endl;
    std::cout << "   Hits:         " << m_hits << std::endl;
    std::cout << "   Misses:       " << m_misses << std::endl;
    std::cout << "   Hit Rate:     " << std::fixed << std::setprecision(2) << hit_rate << "%" << std::endl;
    std::cout << "   Replacements: " << m_replacements << std::endl;
}

// ==================== C 接口实现 ====================

void block_cache_init(size_t capacity) {
    if (g_block_cache != nullptr) {
        delete g_block_cache;
    }
    g_block_cache = new BlockCache(capacity);
}

void block_cache_destroy() {
    if (g_block_cache != nullptr) {
        delete g_block_cache;
        g_block_cache = nullptr;
    }
}

void read_block_cached(int fd, int block_id, void* buf) {
    if (g_block_cache != nullptr) {
        g_block_cache->read_block_cached(fd, block_id, buf);
    } else {
        // 如果缓存未初始化，直接读取
        read_block(fd, block_id, buf);
    }
}

void write_block_cached(int fd, int block_id, const void* buf) {
    if (g_block_cache != nullptr) {
        g_block_cache->write_block_cached(fd, block_id, buf);
    } else {
        // 如果缓存未初始化，直接写入
        write_block(fd, block_id, buf);
    }
}

void block_cache_flush(int fd) {
    if (g_block_cache != nullptr) {
        g_block_cache->flush_all(fd);
    }
}

void block_cache_clear() {
    if (g_block_cache != nullptr) {
        g_block_cache->clear();
    }
}

void block_cache_get_stats(size_t* hits, size_t* misses, size_t* size, size_t* capacity) {
    if (g_block_cache != nullptr) {
        if (hits) *hits = g_block_cache->get_hits();
        if (misses) *misses = g_block_cache->get_misses();
        if (size) *size = g_block_cache->get_size();
        if (capacity) *capacity = g_block_cache->get_capacity();
    } else {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (size) *size = 0;
        if (capacity) *capacity = 0;
    }
}

void block_cache_print_stats() {
    if (g_block_cache != nullptr) {
        g_block_cache->print_stats();
    }
}

