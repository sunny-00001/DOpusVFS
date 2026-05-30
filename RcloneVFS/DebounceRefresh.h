#pragma once
#include <windows.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

class DebounceRefresh {
public:
    static void RequestRefresh();
    static void Shutdown();
    
private:
    static std::mutex s_mutex;
    static std::atomic<bool> s_pendingRefresh;
    static std::atomic<DWORD> s_lastRequestTime;
    static std::atomic<bool> s_running;
    static HANDLE s_timerThread;
    
    static DWORD WINAPI TimerThread(LPVOID param);
    static void DoRefresh();
};
