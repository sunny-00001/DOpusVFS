#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include "DataStructs.h"

class RcloneCache {
public:
    static bool GetStat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& out);
    static void SetStat(const std::wstring& fs, const std::wstring& remote, const RcloneFileInfo& info);
    static void InvalidateStat(const std::wstring& fs, const std::wstring& remote);

    static bool GetDir(const std::wstring& fs, const std::wstring& remote, std::vector<RcloneFileInfo>& out);
    static void SetDir(const std::wstring& fs, const std::wstring& remote, const std::vector<RcloneFileInfo>& entries);
    static void InvalidateDir(const std::wstring& fs, const std::wstring& remote);

    static bool GetAbout(const std::wstring& fs, RcloneAboutInfo& out);
    static void SetAbout(const std::wstring& fs, const RcloneAboutInfo& info);
    static void InvalidateAbout(const std::wstring& fs);

    static bool GetFeatures(const std::wstring& fs, RcloneBackendFeatures& out);
    static void SetFeatures(const std::wstring& fs, const RcloneBackendFeatures& features);
    static void InvalidateFeatures(const std::wstring& fs);

    static bool GetRemotes(std::vector<RcloneRemoteInfo>& out);
    static void SetRemotes(const std::vector<RcloneRemoteInfo>& remotes);
    static void InvalidateRemotes();

    static void InvalidateAll();
    static void InvalidateForWrite(const std::wstring& fs, const std::wstring& remote);

    static void CleanupOldCacheFiles();
    
    static bool IsAboutRequestPending(const std::wstring& fs);
    static void MarkAboutRequestPending(const std::wstring& fs);
    static void ClearAboutRequestPending(const std::wstring& fs);
    static bool WaitForAboutRequest(const std::wstring& fs, RcloneAboutInfo& out, DWORD timeoutMs = 5000);

private:
    struct StatEntry {
        RcloneFileInfo info;
        ULONGLONG timestamp;
    };

    struct DirEntry {
        std::vector<RcloneFileInfo> entries;
        ULONGLONG timestamp;
    };

    struct AboutEntry {
        RcloneAboutInfo info;
        ULONGLONG timestamp;
        int accessCount;
        
        AboutEntry() : timestamp(0), accessCount(0) {}
    };

    struct FeatureEntry {
        RcloneBackendFeatures features;
        ULONGLONG timestamp;
    };

    struct RemoteEntry {
        std::vector<RcloneRemoteInfo> remotes;
        ULONGLONG timestamp;
    };

    struct RemoteCacheData {
        std::unordered_map<std::wstring, StatEntry> stats;
        std::unordered_map<std::wstring, DirEntry> dirs;
        AboutEntry about;
        FeatureEntry features;
        ULONGLONG timestamp;
        bool dirty;
    };

    static std::wstring MakeKey(const std::wstring& a, const std::wstring& b);
    static std::wstring ExtractFsName(const std::wstring& fs);

    static std::wstring GetCacheDir();
    static void EnsureCacheDir();
    static std::wstring GetRemoteCachePath(const std::wstring& fs);
    static bool WriteJsonFile(const std::wstring& filePath, const std::string& jsonContent);
    static std::string ReadJsonFile(const std::wstring& filePath);

    static bool LoadRemoteCache(const std::wstring& fs, RemoteCacheData& data);
    static bool SaveRemoteCache(const std::wstring& fs, const RemoteCacheData& data);

    static std::string SerializeStatEntry(const StatEntry& entry);
    static bool DeserializeStatEntry(const std::string& json, StatEntry& entry);
    static std::string SerializeDirEntry(const DirEntry& entry);
    static bool DeserializeDirEntry(const std::string& json, DirEntry& entry);

    static std::unordered_map<std::wstring, StatEntry> s_statCache;
    static std::unordered_map<std::wstring, DirEntry> s_dirCache;
    static std::unordered_map<std::wstring, AboutEntry> s_aboutCache;
    static std::unordered_map<std::wstring, FeatureEntry> s_featureCache;
    static RemoteEntry s_remoteCache;
    static std::mutex s_cacheMutex;
    
    static std::unordered_set<std::wstring> s_pendingAboutRequests;
    static std::condition_variable s_aboutCV;

    static std::wstring s_cacheDir;

    static constexpr ULONGLONG STAT_TTL = 30000;
    static constexpr ULONGLONG DIR_TTL = 60000;
    static constexpr ULONGLONG ABOUT_TTL = 300000;
    static constexpr ULONGLONG RECURSIVE_CALC_TTL = 1800000;  // 30 min for recursive calculation results
    static constexpr ULONGLONG REMOTE_TTL = 120000;
    static constexpr ULONGLONG FEATURE_TTL = 0;
    
    static constexpr size_t MAX_STAT_CACHE_SIZE = 1000;
    static constexpr size_t MAX_DIR_CACHE_SIZE = 500;
    
    static void CleanupCacheIfNeeded();
};
