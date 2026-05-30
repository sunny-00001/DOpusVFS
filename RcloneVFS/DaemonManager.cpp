#include "DaemonManager.h"
#include "RcloneClient.h"
#include "ConfigManager.h"
#include "Utils.h"
#include <wincrypt.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Crypt32.lib")

PROCESS_INFORMATION DaemonManager::s_processInfo = { 0 };
HINTERNET DaemonManager::s_hSession = NULL;
HINTERNET DaemonManager::s_hConnect = NULL;
HANDLE DaemonManager::s_hJob = NULL;

std::string DaemonManager::s_rcUser = "opus";
std::string DaemonManager::s_rcPass = "";
std::wstring DaemonManager::s_authHeader = L"";

std::atomic<bool> DaemonManager::s_isRunning(false);
std::mutex DaemonManager::s_startupMutex;

std::thread DaemonManager::s_heartbeatThread;
std::atomic<bool> DaemonManager::s_heartbeatRunning(false);

std::atomic<int> DaemonManager::s_consecutiveFailures(0);
std::atomic<bool> DaemonManager::s_restartFailed(false);

bool DaemonManager::IsRunning() {
    return s_isRunning.load();
}

bool DaemonManager::IsRestartFailed() {
    return s_restartFailed.load();
}

void DaemonManager::ResetRestartState() {
    s_consecutiveFailures.store(0);
    s_restartFailed.store(false);
}

bool DaemonManager::EnsureRunning() {
    if (s_isRunning.load()) {
        return true;
    }

    if (s_restartFailed.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(s_startupMutex);
    if (s_isRunning.load()) {
        return true;
    }

    if (StartDaemon()) {
        s_isRunning.store(true);
        s_consecutiveFailures.store(0);
        s_restartFailed.store(false);
        StartHeartbeat();
        return true;
    }

    int failures = s_consecutiveFailures.fetch_add(1) + 1;
    if (failures >= MAX_RESTART_ATTEMPTS) {
        s_restartFailed.store(true);
    }
    return false;
}

bool DaemonManager::Reconnect() {
    if (s_restartFailed.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(s_startupMutex);

    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }

    if (!s_isRunning.load()) {
        if (StartDaemon()) {
            s_isRunning.store(true);
            s_consecutiveFailures.store(0);
            s_restartFailed.store(false);
            StartHeartbeat();
            return true;
        }

        int failures = s_consecutiveFailures.fetch_add(1) + 1;
        if (failures >= MAX_RESTART_ATTEMPTS) {
            s_restartFailed.store(true);
        }
        return false;
    }

    return RebuildConnect();
}

bool DaemonManager::RebuildConnect() {
    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = NULL; }

    s_hSession = WinHttpOpen(L"RcloneVFS/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (s_hSession) {
        WinHttpSetTimeouts(s_hSession, 15000, 15000, 15000, 300000);
        DWORD maxConns = 1024;
        WinHttpSetOption(s_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
        s_hConnect = WinHttpConnect(s_hSession, g_config.rcAddr.c_str(), (INTERNET_PORT)g_config.rcPort, 0);
        if (s_hConnect) return true;
    }
    return false;
}

bool DaemonManager::StartDaemon() {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (g_config.rcPass.empty()) {
        s_rcPass = GeneratePassword();
    } else {
        s_rcPass = WideToUtf8(g_config.rcPass);
    }
    s_rcUser = WideToUtf8(g_config.rcUser);

    std::string credentials = s_rcUser + ":" + s_rcPass;
    std::string base64Creds = Base64Encode(credentials);
    s_authHeader = L"Authorization: Basic " + Utf8ToWide(base64Creds) + L"\r\nContent-Type: application/json\r\n";

    s_hJob = CreateJobObjectW(NULL, NULL);
    if (s_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(s_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    std::wstring exePath = GetRcloneExePath();
    std::wstring cmd = L"\"" + exePath + L"\" rcd --rc-addr " + g_config.rcAddr + L":" + std::to_wstring(g_config.rcPort) + L" --rc-user " + g_config.rcUser + L" --rc-pass " + Utf8ToWide(s_rcPass);

    if (!g_config.configPath.empty()) {
        cmd += L" --config \"" + g_config.configPath + L"\"";
    }
    if (g_config.verboseLogging) {
        cmd += L" -vv";
    }
    if (g_config.useMmap) {
        cmd += L" --use-mmap";
    }
    if (g_config.noChecksum) {
        cmd += L" --no-checksum";
    }
    if (g_config.noModtime) {
        cmd += L" --no-modtime";
    }
    if (g_config.fastList) {
        cmd += L" --fast-list";
    }
    if (g_config.transfers != 4) {
        cmd += L" --transfers " + std::to_wstring(g_config.transfers);
    }
    if (g_config.checkers != 8) {
        cmd += L" --checkers " + std::to_wstring(g_config.checkers);
    }
    if (g_config.bandwidthLimit != L"0" && !g_config.bandwidthLimit.empty()) {
        cmd += L" --bwlimit " + g_config.bandwidthLimit + L"M";
    }
    if (g_config.bufferSize != 16384) {
        cmd += L" --buffer-size " + std::to_wstring(g_config.bufferSize) + L"k";
    }
    if (g_config.lowLevelRetries != 10) {
        cmd += L" --low-level-retries " + std::to_wstring(g_config.lowLevelRetries);
    }
    if (g_config.cacheEnabled) {
        cmd += L" --vfs-cache-mode " + g_config.vfsCacheMode;
        if (g_config.cacheSize != 1024) {
            cmd += L" --vfs-cache-max-size " + std::to_wstring(g_config.cacheSize) + L"M";
        }
        if (g_config.cacheMaxAge != 3600) {
            cmd += L" --vfs-cache-max-age " + std::to_wstring(g_config.cacheMaxAge) + L"s";
        }
        if (!g_config.cacheDir.empty()) {
            cmd += L" --cache-dir \"" + g_config.cacheDir + L"\"";
        }
    }
    if (g_config.dirCacheTime != 300) {
        cmd += L" --dir-cache-time " + std::to_wstring(g_config.dirCacheTime) + L"s";
    }
    if (g_config.pollInterval != 60) {
        cmd += L" --poll-interval " + std::to_wstring(g_config.pollInterval) + L"s";
    }
    if (g_config.logEnabled && !g_config.logPath.empty()) {
        cmd += L" --log-file \"" + g_config.logPath + L"\"";
        cmd += L" --log-level " + g_config.logLevel;
    }
    if (g_config.connTimeout != 30) {
        cmd += L" --timeout " + std::to_wstring(g_config.connTimeout) + L"s";
    }

    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                  NULL, NULL, &si, &s_processInfo);

    if (success) {
        if (s_hJob) {
            AssignProcessToJobObject(s_hJob, s_processInfo.hProcess);
        }
        ResumeThread(s_processInfo.hThread);

        if (s_hSession) WinHttpCloseHandle(s_hSession);
        if (s_hConnect) WinHttpCloseHandle(s_hConnect);

        s_hSession = WinHttpOpen(L"RcloneVFS/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (s_hSession) {
            WinHttpSetTimeouts(s_hSession, 15000, 15000, 15000, 300000);
            DWORD maxConns = 1024;
            WinHttpSetOption(s_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
            s_hConnect = WinHttpConnect(s_hSession, g_config.rcAddr.c_str(), (INTERNET_PORT)g_config.rcPort, 0);
        }

        for (int i = 0; i < 50; i++) {
            std::string res = RcloneClient::SendHttpPost("/rc/noop", "{}");
            if (!res.empty()) return true;
            Sleep(100);
        }

        return false;
    }

    return false;
}

void DaemonManager::DoShutdown() {
    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = NULL; }

    if (s_processInfo.hProcess) {
        RcloneClient::Shutdown();
        Sleep(500);
        TerminateProcess(s_processInfo.hProcess, 0);
        CloseHandle(s_processInfo.hProcess);
        CloseHandle(s_processInfo.hThread);
        s_processInfo.hProcess = NULL;
    }

    if (s_hJob) {
        CloseHandle(s_hJob);
        s_hJob = NULL;
    }

    s_isRunning.store(false);
}

void DaemonManager::Shutdown() {
    StopHeartbeat();
    DoShutdown();
}

void DaemonManager::StartHeartbeat() {
    if (s_heartbeatRunning.load()) return;
    s_heartbeatRunning.store(true);
    s_heartbeatThread = std::thread(HeartbeatLoop);
    s_heartbeatThread.detach();
}

void DaemonManager::StopHeartbeat() {
    s_heartbeatRunning.store(false);
}

void DaemonManager::HeartbeatLoop() {
    while (s_heartbeatRunning.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
        if (!s_heartbeatRunning.load()) break;

        if (s_isRunning.load()) {
            if (!RcloneClient::Noop()) {
                s_isRunning.store(false);

                if (s_restartFailed.load()) continue;

                std::lock_guard<std::mutex> lock(s_startupMutex);
                if (StartDaemon()) {
                    s_isRunning.store(true);
                    s_consecutiveFailures.store(0);
                } else {
                    int failures = s_consecutiveFailures.fetch_add(1) + 1;
                    if (failures >= MAX_RESTART_ATTEMPTS) {
                        s_restartFailed.store(true);
                    }
                }
            } else {
                s_consecutiveFailures.store(0);
            }
        }
    }
}

std::wstring DaemonManager::GetRcloneExePath() {
    if (!g_config.rclonePath.empty() && g_config.rclonePath != L"rclone") {
        DWORD attr = GetFileAttributesW(g_config.rclonePath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return g_config.rclonePath;
        }
    }

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&DaemonManager::StartDaemon, &hModule);
    WCHAR dllPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);
    std::wstring exePath = dllPath;
    size_t lastSlash = exePath.find_last_of(L"\\/");

    if (lastSlash != std::wstring::npos) {
        std::wstring localRclone = exePath.substr(0, lastSlash) + L"\\rclone.exe";
        DWORD attr = GetFileAttributesW(localRclone.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return localRclone;
        }
    }

    WCHAR searchPath[MAX_PATH] = {0};
    if (SearchPathW(NULL, L"rclone.exe", NULL, MAX_PATH, searchPath, NULL) > 0) {
        return searchPath;
    }

    return L"rclone.exe";
}

std::string DaemonManager::GetRcPass() { return s_rcPass; }
std::string DaemonManager::GetRcUser() { return s_rcUser; }
std::wstring DaemonManager::GetAuthHeader() { return s_authHeader; }
HINTERNET DaemonManager::GetConnect() { return s_hConnect; }

std::string DaemonManager::GeneratePassword() {
    BYTE randomBytes[16];
    HCRYPTPROV hCryptProv = 0;
    
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        WCHAR compName[256] = {0};
        DWORD compLen = 256;
        GetComputerNameW(compName, &compLen);

        WCHAR userName[256] = {0};
        DWORD userLen = 256;
        GetUserNameW(userName, &userLen);

        LARGE_INTEGER perfCounter;
        QueryPerformanceCounter(&perfCounter);
        
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        
        unsigned long hash = 5381;
        std::wstring envData = std::wstring(compName) + L"_" + userName + L"_" + 
                               std::to_wstring(perfCounter.QuadPart) + L"_" +
                               std::to_wstring(ft.dwLowDateTime) + L"_" + std::to_wstring(ft.dwHighDateTime);
        for (wchar_t c : envData) {
            hash = ((hash << 5) + hash) + c;
        }
        
        sprintf_s((char*)randomBytes, 16, "%08X%08X", hash, hash ^ 0xDEADBEEF);
    } else {
        if (!CryptGenRandom(hCryptProv, 16, randomBytes)) {
            CryptReleaseContext(hCryptProv, 0);
            for (int i = 0; i < 16; i++) {
                randomBytes[i] = (BYTE)(rand() % 256);
            }
        } else {
            CryptReleaseContext(hCryptProv, 0);
        }
    }
    
    char hexBuf[33];
    for (int i = 0; i < 16; i++) {
        sprintf_s(hexBuf + i*2, 3, "%02X", randomBytes[i]);
    }
    hexBuf[32] = '\0';
    return std::string(hexBuf);
}

std::string DaemonManager::Base64Encode(const std::string& input) {
    DWORD len = 0;
    CryptBinaryToStringA((const BYTE*)input.data(), (DWORD)input.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &len);
    std::string result(len, '\0');
    CryptBinaryToStringA((const BYTE*)input.data(), (DWORD)input.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &result[0], &len);
    if (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}
