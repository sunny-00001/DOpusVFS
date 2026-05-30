#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <winhttp.h>
#include <unordered_map>
#include <mutex>

struct RcloneFileInfo {
    std::wstring name;
    bool isDir;
    uint64_t size;
    FILETIME modTime;
};

struct CacheEntry {
    RcloneFileInfo info;
    ULONGLONG timestamp;
};

class RcloneClient {
public:
    static std::wstring GetRcloneExePath();
    static bool StartDaemon();
    static void StopDaemon();
    
    static std::vector<std::wstring> ListRemotes();
    static std::vector<RcloneFileInfo> ListDirectory(const std::wstring& fs, const std::wstring& remote);

    static bool Stat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& outInfo);
    static bool CopyFileToLocal(const std::wstring& fs, const std::wstring& remote, const std::wstring& localDestFile);
    static bool CopyLocalToRemote(const std::wstring& localSrcFile, const std::wstring& fs, const std::wstring& remote);
    static bool DeleteFile(const std::wstring& fs, const std::wstring& remote);
    static bool RemoveDir(const std::wstring& fs, const std::wstring& remote);
    static bool MakeDir(const std::wstring& fs, const std::wstring& remote);
    static bool Move(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);

    static HINTERNET s_hConnect;
    static void InvalidateCache();

    static std::string s_rcPass;
    static std::string Base64Encode(const std::string& input);

    static bool EnsureDaemonStarted();
    static bool IsDaemonRunning();

private:
    static PROCESS_INFORMATION s_processInfo;
    static HINTERNET s_hSession;
    
    static HANDLE s_hJob;

    static std::atomic<bool> s_isDaemonRunning;
    static std::mutex s_startupMutex;
    
    static std::string s_rcUser;
    static std::wstring s_authHeader; 
    static std::string GenerateEnvironmentPassword();
    
    static std::unordered_map<std::wstring, CacheEntry> s_statCache;
    static std::mutex s_cacheMutex;
    
    static std::string SendHttpPost(const std::string& path, const std::string& jsonPayload);
};
