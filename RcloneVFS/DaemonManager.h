#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

class DaemonManager {
public:
    static bool EnsureRunning();
    static void Shutdown();
    static void StartHeartbeat();
    static void StopHeartbeat();

    static bool Reconnect();
    static bool IsRestartFailed();
    static void ResetRestartState();

    static std::wstring GetRcloneExePath();
    static std::string GetRcPass();
    static std::string GetRcUser();
    static std::wstring GetAuthHeader();
    static HINTERNET GetConnect();
    static bool IsRunning();

private:
    static bool StartDaemon();
    static void DoShutdown();
    static bool RebuildConnect();
    static std::string GeneratePassword();
    static std::string Base64Encode(const std::string& input);
    static void HeartbeatLoop();

    static PROCESS_INFORMATION s_processInfo;
    static HINTERNET s_hSession;
    static HINTERNET s_hConnect;
    static HANDLE s_hJob;

    static std::string s_rcUser;
    static std::string s_rcPass;
    static std::wstring s_authHeader;

    static std::atomic<bool> s_isRunning;
    static std::mutex s_startupMutex;

    static std::thread s_heartbeatThread;
    static std::atomic<bool> s_heartbeatRunning;

    static std::atomic<int> s_consecutiveFailures;
    static std::atomic<bool> s_restartFailed;

    static constexpr int HEARTBEAT_INTERVAL = 30;
    static constexpr int MAX_RESTART_ATTEMPTS = 3;
};
