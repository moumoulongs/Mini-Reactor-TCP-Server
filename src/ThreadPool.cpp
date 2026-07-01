#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
    : stop_(false) {
    for(size_t i = 0; i < threadCount; i++) {
        workers_.emplace_back([this]() {
            while(true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    
                    cv_.wait(lock, [this](){
                        return stop_ || !tasks_.empty();
                    });

                    if(stop_ && tasks_.empty()) {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                task();
            }
        });
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stop_) {
            return;
        }

        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }

    cv_.notify_all();

    for(std::thread &worker : workers_) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}