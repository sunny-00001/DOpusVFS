#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads) 
    : m_stop(false)
    , m_activeTasks(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_condition.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            
            if (m_stop && m_tasks.empty()) {
                return;
            }
            
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        
        m_activeTasks++;
        task();
        m_activeTasks--;
        
        m_finished.notify_all();
    }
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_finished.wait(lock, [this] { 
        return m_tasks.empty() && m_activeTasks == 0; 
    });
}

size_t ThreadPool::getActiveTaskCount() const {
    return m_activeTasks;
}
