#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <winhttp.h>
#include <unordered_map>
#include <mutex>
#include <atomic>

struct IPFSFileInfo {
    std::wstring name;
    std::wstring cid;
    bool isDir;
    uint64_t size;
    FILETIME modTime;
    uint64_t blocks;
    std::wstring type;
};

struct CacheEntry {
    IPFSFileInfo info;
    ULONGLONG timestamp;
};

struct IPFSFileStreamContext {
    bool isWrite;
    std::wstring cid;
    std::wstring path;
    uint64_t fileSize;
    uint64_t seekPos;
    uint64_t streamPos;

    HINTERNET hConnect;
    HINTERNET hRequest;

    HANDLE hReadPipe;
    HANDLE hProcess;

    IPFSFileStreamContext() : isWrite(false), fileSize(0), seekPos(0), streamPos(0),
        hConnect(NULL), hRequest(NULL), hReadPipe(NULL), hProcess(NULL) {}

    ~IPFSFileStreamContext() {
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hReadPipe) CloseHandle(hReadPipe);
        if (hProcess) { TerminateProcess(hProcess, 0); CloseHandle(hProcess); }
    }
};

class IPFSClient {
public:
    static bool Init();
    static void Cleanup();

    static bool IsDaemonRunning();
    static bool EnsureDaemonStarted();

    static std::vector<IPFSFileInfo> ListDirectory(const std::wstring& cid, const std::wstring& path);
    static bool Stat(const std::wstring& cid, const std::wstring& path, IPFSFileInfo& outInfo);
    static bool GetFileSize(const std::wstring& cid, uint64_t& size);

    static bool PinAdd(const std::wstring& cid);
    static bool PinRm(const std::wstring& cid);
    static bool PinLs(const std::wstring& cid, bool& isPinned);

    static std::wstring GetDaemonAddress();
    static void SetDaemonAddress(const std::wstring& addr);
    static int GetDaemonPort();
    static void SetDaemonPort(int port);

    static void InvalidateCache();

    static std::string SendHttpGet(const std::string& path);
    static std::string SendHttpPost(const std::string& path, const std::string& contentType = "", const std::string& body = "");

    static HINTERNET s_hSession;
    static HINTERNET s_hConnect;

private:
    static std::wstring s_daemonHost;
    static int s_daemonPort;
    static std::mutex s_startupMutex;
    static std::atomic<bool> s_isDaemonRunning;

    static std::unordered_map<std::wstring, CacheEntry> s_statCache;
    static std::mutex s_cacheMutex;
};
