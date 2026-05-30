#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    
    template<class F>
    void enqueue(F&& f);
    
    void waitAll();
    size_t getActiveTaskCount() const;
    
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::condition_variable m_finished;
    std::atomic<bool> m_stop;
    std::atomic<size_t> m_activeTasks;
    
    void workerThread();
};

template<class F>
void ThreadPool::enqueue(F&& f) {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_tasks.emplace(std::forward<F>(f));
    }
    m_condition.notify_one();
}
