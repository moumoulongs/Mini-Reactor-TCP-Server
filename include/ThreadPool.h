#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    void submit(std::function<void()> task);

private:
    std::vector<std::thread> workers_;          // 保存所有worker线程
    std::queue<std::function<void()>> tasks_;   // 保存待执行任务
    std::mutex mutex_;                          // 保护任务队列和stop_
    std::condition_variable cv_;                // 唤醒worker
    bool stop_;                                 // 表示线程池是否准备停止
};