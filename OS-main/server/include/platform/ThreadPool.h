#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <iostream>
#include <sstream>

/**
 * ThreadPool - 线程池实现
 * 
 * 功能：
 * - 固定大小的工作线程池
 * - 任务队列管理
 * - 优雅关闭
 * - 线程安全
 * 
 * 使用场景：
 * - 限制服务器并发连接数
 * - 避免线程创建/销毁开销
 * - 防止资源耗尽
 */
class ThreadPool {
public:
    /**
     * 构造函数
     * @param numThreads 线程池大小（默认：硬件并发数）
     * @param maxQueueSize 最大任务队列大小（0表示无限制）
     */
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency(), 
                       size_t maxQueueSize = 0)
        : m_maxQueueSize(maxQueueSize), m_stop(false) {
        
        if (numThreads == 0) {
            numThreads = 1;  // 至少一个线程
        }
        
        {
            std::ostringstream oss;
            oss << "ThreadPool: 初始化 " << numThreads << " 个工作线程";
            if (maxQueueSize > 0) {
                oss << "，最大队列大小: " << maxQueueSize;
            }
            logLine(std::cout, oss.str());
        }
        
        // 创建工作线程
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this, i] {
                this->workerThread(i);
            });
        }
    }

    /**
     * 析构函数 - 等待所有任务完成并关闭线程池
     */
    ~ThreadPool() {
        shutdown();
    }

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * 提交任务到线程池
     * @param task 要执行的任务
     * @return true 如果任务成功加入队列，false 如果队列已满
     */
    bool enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // 检查是否已停止
            if (m_stop) {
                return false;
            }
            
            // 检查队列是否已满
            if (m_maxQueueSize > 0 && m_tasks.size() >= m_maxQueueSize) {
                std::ostringstream oss;
                oss << "ThreadPool: 任务队列已满 (" << m_tasks.size()
                    << "/" << m_maxQueueSize << ")，拒绝新任务";
                logLine(std::cerr, oss.str());
                return false;
            }
            
            m_tasks.push(std::move(task));
        }
        
        // 通知一个工作线程
        m_condition.notify_one();
        return true;
    }

    /**
     * 优雅关闭线程池
     * - 停止接受新任务
     * - 等待所有已提交的任务完成
     * - 关闭所有工作线程
     */
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop) {
                return;  // 已经关闭
            }
            m_stop = true;
        }
        
        // 唤醒所有工作线程
        m_condition.notify_all();
        
        // 等待所有线程完成
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        logLine(std::cout, "ThreadPool: 已关闭");
    }

    /**
     * 获取当前队列中的任务数量
     */
    size_t queueSize() const {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        return m_tasks.size();
    }

    /**
     * 获取活跃线程数
     */
    size_t activeThreads() const {
        return m_activeThreads.load();
    }

    /**
     * 获取线程池大小
     */
    size_t poolSize() const {
        return m_workers.size();
    }

private:
    inline static std::mutex s_logMutex;

    static void logLine(std::ostream& os, const std::string& line) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        os << line << std::endl;
    }

    /**
     * 工作线程函数
     */
    void workerThread(size_t threadId) {
        {
            std::ostringstream oss;
            oss << "ThreadPool: 工作线程 " << threadId << " 已启动";
            logLine(std::cout, oss.str());
        }
        
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                
                // 等待任务或停止信号
                m_condition.wait(lock, [this] {
                    return m_stop || !m_tasks.empty();
                });
                
                // 如果停止且队列为空，退出
                if (m_stop && m_tasks.empty()) {
                    break;
                }
                
                // 取出任务
                if (!m_tasks.empty()) {
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
            }
            
            // 执行任务
            if (task) {
                m_activeThreads++;
                try {
                    task();
                } catch (const std::exception& e) {
                    std::ostringstream oss;
                    oss << "ThreadPool: 任务执行异常: " << e.what();
                    logLine(std::cerr, oss.str());
                } catch (...) {
                    logLine(std::cerr, "ThreadPool: 任务执行未知异常");
                }
                m_activeThreads--;
            }
        }

        {
            std::ostringstream oss;
            oss << "ThreadPool: 工作线程 " << threadId << " 已退出";
            logLine(std::cout, oss.str());
        }
    }

    std::vector<std::thread> m_workers;           // 工作线程
    std::queue<std::function<void()>> m_tasks;    // 任务队列
    
    mutable std::mutex m_queueMutex;              // 保护任务队列
    std::condition_variable m_condition;          // 条件变量
    
    size_t m_maxQueueSize;                        // 最大队列大小
    std::atomic<bool> m_stop;                     // 停止标志
    std::atomic<size_t> m_activeThreads{0};       // 活跃线程计数
};

