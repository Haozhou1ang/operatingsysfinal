#pragma once

#include <cstddef>

// 缓存统计信息（用于测试/观测，不参与业务逻辑）
struct CacheStats {
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t size = 0;
    std::size_t capacity = 0;
};

// 仅用于“可观测性/测试”的可选接口。
// 真实的 filesystem 适配层不需要实现它；只有当 FSProtocol 外层包了缓存装饰器时才可用。
class ICacheStatsProvider {
public:
    virtual ~ICacheStatsProvider() = default;

    virtual CacheStats cacheStats() const = 0;
    virtual void clearCache() = 0;
};
