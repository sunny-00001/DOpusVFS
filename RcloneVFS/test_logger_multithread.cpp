#include <windows.h>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "../RcloneVFS/Logger.h"

void ThreadFunction(int threadId, int iterations) {
    for (int i = 0; i < iterations; i++) {
        LogError(L"Thread %d - Error message %d", threadId, i);
        LogWarn(L"Thread %d - Warning message %d", threadId, i);
        LogInfo(L"Thread %d - Info message %d", threadId, i);
        LogDebug(L"Thread %d - Debug message %d", threadId, i);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Logger Multi-threading Test" << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << std::endl;

    InitLogger(LOG_DEBUG);

    const int numThreads = 10;
    const int iterationsPerThread = 100;

    std::wcout << L"Starting " << numThreads << L" threads..." << std::endl;
    std::wcout << L"Each thread will write " << iterationsPerThread * 4 << L" log entries" << std::endl;
    std::wcout << L"Total log entries: " << numThreads * iterationsPerThread * 4 << std::endl;
    std::wcout << std::endl;

    std::vector<std::thread> threads;
    
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(ThreadFunction, i, iterationsPerThread);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Multi-threading test completed" << std::endl;
    std::wcout << L"Time taken: " << duration.count() << L" ms" << std::endl;
    std::wcout << L"Please check the log file for output" << std::endl;
    std::wcout << L"========================================" << std::endl;

    return 0;
}
