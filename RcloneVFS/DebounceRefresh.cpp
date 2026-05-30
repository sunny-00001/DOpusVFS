#include "DebounceRefresh.h"
#include <shellapi.h>

std::mutex DebounceRefresh::s_mutex;
std::atomic<bool> DebounceRefresh::s_pendingRefresh(false);
std::atomic<DWORD> DebounceRefresh::s_lastRequestTime(0);
std::atomic<bool> DebounceRefresh::s_running(true);
HANDLE DebounceRefresh::s_timerThread = NULL;

DWORD WINAPI DebounceRefresh::TimerThread(LPVOID param) {
    const DWORD DEBOUNCE_DELAY = 1000;  // 1秒防抖延迟
    
    while (s_running) {
        if (s_pendingRefresh) {
            DWORD now = GetTickCount();
            DWORD lastRequest = s_lastRequestTime;
            
            if (now - lastRequest >= DEBOUNCE_DELAY) {
                s_pendingRefresh = false;
                DoRefresh();
            }
        }
        
        Sleep(100);  // 每100ms检查一次
    }
    
    return 0;
}

void DebounceRefresh::RequestRefresh() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    s_lastRequestTime = GetTickCount();
    s_pendingRefresh = true;
    
    if (!s_timerThread) {
        s_timerThread = CreateThread(NULL, 0, TimerThread, NULL, 0, NULL);
    }
}

void DebounceRefresh::Shutdown() {
    s_running = false;
    
    if (s_timerThread) {
        WaitForSingleObject(s_timerThread, 2000);
        CloseHandle(s_timerThread);
        s_timerThread = NULL;
    }
}

void DebounceRefresh::DoRefresh() {
    ShellExecuteW(NULL, L"open", L"dopusrt.exe", L"/cmd Go REFRESH", NULL, SW_HIDE);
}
