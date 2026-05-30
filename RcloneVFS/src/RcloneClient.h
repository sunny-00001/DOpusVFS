#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <winhttp.h>
#include "DataStructs.h"

enum class RcloneError {
    None,
    NetworkError,
    AuthError,
    NotFound,
    Timeout,
    ServerError,
    InvalidResponse,
    Unknown
};

struct RcloneResult {
    bool success;
    RcloneError error;
    std::wstring errorMessage;
    int retryCount;
    
    RcloneResult() : success(false), error(RcloneError::Unknown), errorMessage(L""), retryCount(0) {}
};

struct RcloneJobStatus {
    bool finished = false;
    bool success = false;
    double progress = 0.0;
    int64_t transferred = 0;
    int64_t total = 0;
    int64_t speed = 0;
    std::string error;
};

class RcloneClient {
public:
    static bool EnsureDaemonStarted();
    static bool IsDaemonRunning();

    static std::vector<RcloneRemoteInfo> ListRemotesWithType();
    static std::vector<RcloneFileInfo> ListDirectory(const std::wstring& fs, const std::wstring& remote);

    static bool Stat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& outInfo);
    static bool About(const std::wstring& fs, RcloneAboutInfo& outInfo);
    static RcloneResult AboutWithRetry(const std::wstring& fs, RcloneAboutInfo& outInfo, int maxRetries = 3);
    static uint64_t CalculateUsedSpaceRecursive(const std::wstring& fs, const std::wstring& path);
    static bool CalcAboutRecursive(const std::wstring& fs, RcloneAboutInfo& outInfo);
    static bool CopyFileToLocal(const std::wstring& fs, const std::wstring& remote, const std::wstring& localDestFile);
    static bool CopyLocalToRemote(const std::wstring& localSrcFile, const std::wstring& fs, const std::wstring& remote);
    static bool DeleteFile(const std::wstring& fs, const std::wstring& remote);
    static bool RemoveDir(const std::wstring& fs, const std::wstring& remote);
    static bool Purge(const std::wstring& fs, const std::wstring& remote);
    static bool MakeDir(const std::wstring& fs, const std::wstring& remote);
    static bool Move(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool CopyFileRemote(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool CopyDir(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool MoveFileRemote(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool MoveDir(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);

    static bool PublicLink(const std::wstring& fs, const std::wstring& remote, std::wstring& outUrl,
                           const std::wstring& expire = L"");
    static bool Cleanup(const std::wstring& fs);

    // Phase 2: Check / Sync / BackendCommand
    static bool Check(const std::wstring& srcFs, const std::wstring& srcRemote,
                     const std::wstring& dstFs, const std::wstring& dstRemote,
                     bool oneWay, std::string& outReport);
    static bool SyncCopy(const std::wstring& srcFs, const std::wstring& srcRemote,
                         const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool SyncMove(const std::wstring& srcFs, const std::wstring& srcRemote,
                         const std::wstring& dstFs, const std::wstring& dstRemote);
    static bool BackendCommand(const std::wstring& fs, const std::string& command,
                               const std::string& jsonArgs, std::string& outResult);

    // Version management
    static bool GetVersions(const std::wstring& fs, const std::wstring& remote,
                            std::vector<RcloneVersionInfo>& outVersions);
    static bool RestoreVersion(const std::wstring& fs, const std::wstring& remote,
                               const std::wstring& versionId);
    static bool DeleteVersion(const std::wstring& fs, const std::wstring& remote,
                              const std::wstring& versionId);

    // Phase 3: Config / Bwlimit / Job
    struct RemoteConfig {
        std::wstring name;
        std::string type;
        std::map<std::string, std::string> fields;
    };
    static bool ConfigCreate(const std::wstring& name, const std::string& type,
                             const std::map<std::string, std::string>& parameters,
                             HWND hwndParent = NULL, std::string* outError = nullptr);
    static bool ConfigDelete(const std::wstring& name, std::string* outError = nullptr);
    static bool ConfigUpdate(const std::wstring& name,
                             const std::map<std::string, std::string>& parameters,
                             std::string* outError = nullptr);
    static bool ConfigProviders(std::vector<std::string>& outProviders);

    static bool SetBwlimit(const std::string& limit, std::string* outError = nullptr);  // e.g. "1M" or "off"
    static bool GetBwlimit(std::string& outLimit);

    static bool StopJob(int jobid, std::string* outError = nullptr);
    static bool ListJobs(std::vector<std::pair<int, RcloneJobStatus>>& outJobs);
    static bool CoreCommand(const std::string& command, const std::string& jsonArgs, std::string& outResult);
    static bool CacheExpire(const std::wstring& fs, const std::wstring& remote = L"");

    static bool Noop();
    static bool Shutdown();

    static bool GetJobStatus(int jobid, RcloneJobStatus& out);
    static bool WaitForJob(int jobid, HWND hwndParent);

    static std::string SendHttpPost(const std::string& path, const std::string& jsonPayload);
    static std::string SendHttpPostFull(const std::string& path, const std::string& jsonPayload);
    static std::string SendHttpPostWithRetry(const std::string& path, const std::string& jsonPayload);
};
