#include "RcloneCache.h"
#include "PathParser.h"
#include "Utils.h"
#include "json.hpp"
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")

using json = nlohmann::json;

std::unordered_map<std::wstring, RcloneCache::StatEntry> RcloneCache::s_statCache;
std::unordered_map<std::wstring, RcloneCache::DirEntry> RcloneCache::s_dirCache;
std::unordered_map<std::wstring, RcloneCache::AboutEntry> RcloneCache::s_aboutCache;
std::unordered_map<std::wstring, RcloneCache::FeatureEntry> RcloneCache::s_featureCache;
RcloneCache::RemoteEntry RcloneCache::s_remoteCache;
std::mutex RcloneCache::s_cacheMutex;
std::wstring RcloneCache::s_cacheDir;

std::unordered_set<std::wstring> RcloneCache::s_pendingAboutRequests;
std::condition_variable RcloneCache::s_aboutCV;

std::wstring RcloneCache::MakeKey(const std::wstring& a, const std::wstring& b) {
    return a + L"|" + b;
}

std::wstring RcloneCache::ExtractFsName(const std::wstring& fs) {
    std::wstring name = fs;
    if (!name.empty() && name.back() == L':') {
        name.pop_back();
    }
    for (auto& c : name) {
        if (c == L':' || c == L'/' || c == L'\\' || c == L'|' || c == L'?' || c == L'*' || c == L'<' || c == L'>' || c == L'"') {
            c = L'_';
        }
    }
    return name.empty() ? L"unknown" : name;
}

std::wstring RcloneCache::GetCacheDir() {
    if (!s_cacheDir.empty()) return s_cacheDir;

    WCHAR appDataPath[MAX_PATH] = { 0 };
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath) != S_OK) {
        WCHAR tempPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tempPath);
        s_cacheDir = std::wstring(tempPath) + L"RcloneVFS\\cache\\";
        return s_cacheDir;
    }

    s_cacheDir = std::wstring(appDataPath) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\RcloneVFS\\cache\\";
    return s_cacheDir;
}

void RcloneCache::EnsureCacheDir() {
    std::wstring dir = GetCacheDir();
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }
}

std::wstring RcloneCache::GetRemoteCachePath(const std::wstring& fs) {
    return GetCacheDir() + ExtractFsName(fs) + L".json";
}

bool RcloneCache::WriteJsonFile(const std::wstring& filePath, const std::string& jsonContent) {
    EnsureCacheDir();

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, jsonContent.c_str(), (DWORD)jsonContent.size(), &written, NULL);
    CloseHandle(hFile);
    return ok && (written == jsonContent.size());
}

std::string RcloneCache::ReadJsonFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 10 * 1024 * 1024) {
        CloseHandle(hFile);
        return "";
    }

    std::string content((size_t)fileSize.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &content[0], (DWORD)fileSize.QuadPart, &read, NULL);
    CloseHandle(hFile);

    if (!ok || read != (DWORD)fileSize.QuadPart) return "";
    return content;
}

bool RcloneCache::LoadRemoteCache(const std::wstring& fs, RemoteCacheData& data) {
    std::wstring filePath = GetRemoteCachePath(fs);
    std::string content = ReadJsonFile(filePath);
    if (content.empty()) return false;

    try {
        auto j = json::parse(content);
        data.timestamp = j.value("timestamp", 0ULL);
        data.dirty = false;

        if (j.contains("stats") && j["stats"].is_object()) {
            for (auto it = j["stats"].begin(); it != j["stats"].end(); ++it) {
                StatEntry entry;
                std::wstring key = Utf8ToWide(it.key());
                auto& val = it.value();
                entry.timestamp = val.value("timestamp", 0ULL);
                if (val.contains("info")) {
                    auto& infoJson = val["info"];
                    entry.info.name = Utf8ToWide(infoJson.value("name", ""));
                    entry.info.isDir = infoJson.value("isDir", false);
                    entry.info.size = infoJson.value("size", 0ULL);
                    entry.info.mimeType = Utf8ToWide(infoJson.value("mimeType", ""));
                    entry.info.id = Utf8ToWide(infoJson.value("id", ""));
                    entry.info.hash = Utf8ToWide(infoJson.value("hash", ""));
                    entry.info.hashType = infoJson.value("hashType", "");
                    if (infoJson.contains("modTime") && infoJson["modTime"].is_string()) {
                        entry.info.modTime = ParseRcloneTime(infoJson["modTime"].get<std::string>());
                    }
                }
                data.stats[key] = entry;
            }
        }

        if (j.contains("dirs") && j["dirs"].is_object()) {
            for (auto it = j["dirs"].begin(); it != j["dirs"].end(); ++it) {
                DirEntry entry;
                std::wstring key = Utf8ToWide(it.key());
                auto& val = it.value();
                entry.timestamp = val.value("timestamp", 0ULL);
                if (val.contains("entries") && val["entries"].is_array()) {
                    for (const auto& item : val["entries"]) {
                        RcloneFileInfo info;
                        info.name = Utf8ToWide(item.value("name", ""));
                        info.isDir = item.value("isDir", false);
                        info.size = item.value("size", 0ULL);
                        info.mimeType = Utf8ToWide(item.value("mimeType", ""));
                        info.id = Utf8ToWide(item.value("id", ""));
                        info.hash = Utf8ToWide(item.value("hash", ""));
                        info.hashType = item.value("hashType", "");
                        if (item.contains("modTime") && item["modTime"].is_string()) {
                            info.modTime = ParseRcloneTime(item["modTime"].get<std::string>());
                        }
                        entry.entries.push_back(info);
                    }
                }
                data.dirs[key] = entry;
            }
        }

        if (j.contains("about")) {
            auto& aboutJson = j["about"];
            data.about.timestamp = aboutJson.value("timestamp", 0ULL);
            data.about.accessCount = aboutJson.value("accessCount", 0);
            if (aboutJson.contains("info")) {
                auto& infoJson = aboutJson["info"];
                data.about.info = {};
                if (infoJson.contains("total")) { data.about.info.total = infoJson["total"].get<uint64_t>(); data.about.info.hasTotal = true; }
                if (infoJson.contains("used")) { data.about.info.used = infoJson["used"].get<uint64_t>(); data.about.info.hasUsed = true; }
                if (infoJson.contains("free")) { data.about.info.free = infoJson["free"].get<uint64_t>(); data.about.info.hasFree = true; }
                if (infoJson.contains("trashed")) { data.about.info.trashed = infoJson["trashed"].get<uint64_t>(); data.about.info.hasTrashed = true; }
                if (infoJson.contains("source") && infoJson["source"].is_string()) { data.about.info.source = infoJson["source"].get<std::string>(); }
                if (infoJson.contains("objects")) { data.about.info.objects = infoJson["objects"].get<uint64_t>(); data.about.info.hasObjects = true; }
            }
        }

        if (j.contains("features")) {
            auto& featJson = j["features"];
            data.features.timestamp = featJson.value("timestamp", 0ULL);
            if (featJson.contains("info")) {
                auto& infoJson = featJson["info"];
                data.features.features = {};
                data.features.features.supportsAbout = infoJson.value("supportsAbout", false);
                data.features.features.supportsPublicLink = infoJson.value("supportsPublicLink", false);
                data.features.features.supportsTrash = infoJson.value("supportsTrash", false);
                data.features.features.supportsVersions = infoJson.value("supportsVersions", false);
                data.features.features.supportsHash = infoJson.value("supportsHash", false);
                data.features.features.remoteType = Utf8ToWide(infoJson.value("remoteType", ""));
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool RcloneCache::SaveRemoteCache(const std::wstring& fs, const RemoteCacheData& data) {
    json j;
    j["timestamp"] = data.timestamp;

    json statsJson = json::object();
    for (const auto& pair : data.stats) {
        json entryJson;
        entryJson["timestamp"] = pair.second.timestamp;
        json infoJson;
        infoJson["name"] = WideToUtf8(pair.second.info.name);
        infoJson["isDir"] = pair.second.info.isDir;
        infoJson["size"] = pair.second.info.size;
        infoJson["mimeType"] = WideToUtf8(pair.second.info.mimeType);
        infoJson["id"] = WideToUtf8(pair.second.info.id);
        infoJson["hash"] = WideToUtf8(pair.second.info.hash);
        infoJson["hashType"] = pair.second.info.hashType;
        SYSTEMTIME st;
        if (FileTimeToSystemTime(&pair.second.info.modTime, &st)) {
            char timeBuf[64];
            sprintf_s(timeBuf, "%04d-%02d-%02dT%02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            infoJson["modTime"] = timeBuf;
        }
        entryJson["info"] = infoJson;
        statsJson[WideToUtf8(pair.first)] = entryJson;
    }
    j["stats"] = statsJson;

    json dirsJson = json::object();
    for (const auto& pair : data.dirs) {
        json entryJson;
        entryJson["timestamp"] = pair.second.timestamp;
        json entriesJson = json::array();
        for (const auto& info : pair.second.entries) {
            json item;
            item["name"] = WideToUtf8(info.name);
            item["isDir"] = info.isDir;
            item["size"] = info.size;
            item["mimeType"] = WideToUtf8(info.mimeType);
            item["id"] = WideToUtf8(info.id);
            item["hash"] = WideToUtf8(info.hash);
            item["hashType"] = info.hashType;
            SYSTEMTIME st;
            if (FileTimeToSystemTime(&info.modTime, &st)) {
                char timeBuf[64];
                sprintf_s(timeBuf, "%04d-%02d-%02dT%02d:%02d:%02d",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                item["modTime"] = timeBuf;
            }
            entriesJson.push_back(item);
        }
        entryJson["entries"] = entriesJson;
        dirsJson[WideToUtf8(pair.first)] = entryJson;
    }
    j["dirs"] = dirsJson;

    if (data.about.timestamp > 0) {
        json aboutJson;
        aboutJson["timestamp"] = data.about.timestamp;
        aboutJson["accessCount"] = data.about.accessCount;
        json infoJson = json::object();
        if (data.about.info.hasTotal) infoJson["total"] = data.about.info.total;
        if (data.about.info.hasUsed) infoJson["used"] = data.about.info.used;
        if (data.about.info.hasFree) infoJson["free"] = data.about.info.free;
        if (data.about.info.hasTrashed) infoJson["trashed"] = data.about.info.trashed;
        if (!data.about.info.source.empty()) infoJson["source"] = data.about.info.source;
        if (data.about.info.hasObjects) infoJson["objects"] = data.about.info.objects;
        aboutJson["info"] = infoJson;
        j["about"] = aboutJson;
    }

    if (data.features.timestamp > 0) {
        json featJson;
        featJson["timestamp"] = data.features.timestamp;
        json infoJson = json::object();
        infoJson["supportsAbout"] = data.features.features.supportsAbout;
        infoJson["supportsPublicLink"] = data.features.features.supportsPublicLink;
        infoJson["supportsTrash"] = data.features.features.supportsTrash;
        infoJson["supportsVersions"] = data.features.features.supportsVersions;
        infoJson["supportsHash"] = data.features.features.supportsHash;
        infoJson["remoteType"] = WideToUtf8(data.features.features.remoteType);
        featJson["info"] = infoJson;
        j["features"] = featJson;
    }

    std::wstring filePath = GetRemoteCachePath(fs);
    return WriteJsonFile(filePath, j.dump());
}

bool RcloneCache::GetStat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& out) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    auto it = s_statCache.find(key);
    if (it != s_statCache.end() && (GetTickCount64() - it->second.timestamp < STAT_TTL)) {
        out = it->second.info;
        return true;
    }

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        auto it2 = data.stats.find(key);
        if (it2 != data.stats.end() && (GetTickCount64() - it2->second.timestamp < STAT_TTL)) {
            out = it2->second.info;
            s_statCache[key] = it2->second;
            return true;
        }
    }

    return false;
}

void RcloneCache::SetStat(const std::wstring& fs, const std::wstring& remote, const RcloneFileInfo& info) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    ULONGLONG now = GetTickCount64();
    s_statCache[key] = { info, now };

    RemoteCacheData data;
    LoadRemoteCache(fs, data);
    data.stats[key] = { info, now };
    data.timestamp = now;
    SaveRemoteCache(fs, data);
}

void RcloneCache::InvalidateStat(const std::wstring& fs, const std::wstring& remote) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    s_statCache.erase(key);

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        data.stats.erase(key);
        SaveRemoteCache(fs, data);
    }
}

bool RcloneCache::GetDir(const std::wstring& fs, const std::wstring& remote, std::vector<RcloneFileInfo>& out) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    auto it = s_dirCache.find(key);
    if (it != s_dirCache.end() && (GetTickCount64() - it->second.timestamp < DIR_TTL)) {
        out = it->second.entries;
        return true;
    }

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        auto it2 = data.dirs.find(key);
        if (it2 != data.dirs.end() && (GetTickCount64() - it2->second.timestamp < DIR_TTL)) {
            out = it2->second.entries;
            s_dirCache[key] = it2->second;
            return true;
        }
    }

    return false;
}

void RcloneCache::SetDir(const std::wstring& fs, const std::wstring& remote, const std::vector<RcloneFileInfo>& entries) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    ULONGLONG now = GetTickCount64();
    s_dirCache[key] = { entries, now };

    for (const auto& entry : entries) {
        std::wstring itemRemote = remote.empty() ? entry.name : (remote + L"/" + entry.name);
        s_statCache[MakeKey(fs, itemRemote)] = { entry, now };
    }

    RemoteCacheData data;
    LoadRemoteCache(fs, data);
    data.dirs[key] = { entries, now };
    for (const auto& entry : entries) {
        std::wstring itemRemote = remote.empty() ? entry.name : (remote + L"/" + entry.name);
        data.stats[MakeKey(fs, itemRemote)] = { entry, now };
    }
    data.timestamp = now;
    SaveRemoteCache(fs, data);
}

void RcloneCache::InvalidateDir(const std::wstring& fs, const std::wstring& remote) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto key = MakeKey(fs, remote);
    s_dirCache.erase(key);

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        data.dirs.erase(key);
        SaveRemoteCache(fs, data);
    }
}

bool RcloneCache::GetAbout(const std::wstring& fs, RcloneAboutInfo& out) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_aboutCache.find(fs);
    
    if (it != s_aboutCache.end()) {
        ULONGLONG age = GetTickCount64() - it->second.timestamp;
        ULONGLONG dynamicTTL = ABOUT_TTL;
        
        // Source-based TTL: recursive_calc costs more, cache longer
        if (it->second.info.source == "recursive_calc") {
            dynamicTTL = RECURSIVE_CALC_TTL;
        }
        
        if (it->second.accessCount > 10) {
            dynamicTTL = dynamicTTL / 2;
        } else if (it->second.accessCount > 5) {
            dynamicTTL = dynamicTTL * 3 / 4;
        }
        
        if (age < dynamicTTL) {
            out = it->second.info;
            it->second.accessCount++;
            return true;
        }
    }

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        if (data.about.timestamp > 0) {
            ULONGLONG age = GetTickCount64() - data.about.timestamp;
            ULONGLONG dynamicTTL = ABOUT_TTL;
            
            if (data.about.info.source == "recursive_calc") {
                dynamicTTL = RECURSIVE_CALC_TTL;
            }
            
            if (data.about.accessCount > 10) {
                dynamicTTL = dynamicTTL / 2;
            } else if (data.about.accessCount > 5) {
                dynamicTTL = dynamicTTL * 3 / 4;
            }
            
            if (age < dynamicTTL) {
                out = data.about.info;
                data.about.accessCount++;
                s_aboutCache[fs] = data.about;
                return true;
            }
        }
    }

    return false;
}

void RcloneCache::SetAbout(const std::wstring& fs, const RcloneAboutInfo& info) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    ULONGLONG now = GetTickCount64();
    
    AboutEntry entry;
    entry.info = info;
    entry.timestamp = now;
    entry.accessCount = 0;
    
    auto it = s_aboutCache.find(fs);
    if (it != s_aboutCache.end()) {
        entry.accessCount = it->second.accessCount;
    }
    
    s_aboutCache[fs] = entry;

    RemoteCacheData data;
    LoadRemoteCache(fs, data);
    data.about = entry;
    data.timestamp = now;
    SaveRemoteCache(fs, data);
}

void RcloneCache::InvalidateAbout(const std::wstring& fs) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_aboutCache.erase(fs);

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        data.about = {};
        SaveRemoteCache(fs, data);
    }
}

bool RcloneCache::GetFeatures(const std::wstring& fs, RcloneBackendFeatures& out) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_featureCache.find(fs);
    if (it != s_featureCache.end()) {
        out = it->second.features;
        return true;
    }

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        if (data.features.timestamp > 0) {
            out = data.features.features;
            s_featureCache[fs] = data.features;
            return true;
        }
    }

    return false;
}

void RcloneCache::SetFeatures(const std::wstring& fs, const RcloneBackendFeatures& features) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    ULONGLONG now = GetTickCount64();
    s_featureCache[fs] = { features, now };

    RemoteCacheData data;
    LoadRemoteCache(fs, data);
    data.features = { features, now };
    data.timestamp = now;
    SaveRemoteCache(fs, data);
}

void RcloneCache::InvalidateFeatures(const std::wstring& fs) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_featureCache.erase(fs);

    RemoteCacheData data;
    if (LoadRemoteCache(fs, data)) {
        data.features = {};
        SaveRemoteCache(fs, data);
    }
}

bool RcloneCache::GetRemotes(std::vector<RcloneRemoteInfo>& out) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    if (!s_remoteCache.remotes.empty() && (GetTickCount64() - s_remoteCache.timestamp < REMOTE_TTL)) {
        out = s_remoteCache.remotes;
        return true;
    }

    std::wstring filePath = GetCacheDir() + L"remotes.json";
    std::string content = ReadJsonFile(filePath);
    if (!content.empty()) {
        try {
            auto j = json::parse(content);
            ULONGLONG timestamp = j.value("timestamp", 0ULL);
            if (GetTickCount64() - timestamp < REMOTE_TTL) {
                out.clear();
                for (const auto& item : j["remotes"]) {
                    RcloneRemoteInfo info;
                    info.name = Utf8ToWide(item["name"].get<std::string>());
                    info.type = Utf8ToWide(item.value("type", ""));
                    out.push_back(info);
                }
                s_remoteCache = { out, timestamp };
                return true;
            }
        } catch (...) {}
    }

    return false;
}

void RcloneCache::SetRemotes(const std::vector<RcloneRemoteInfo>& remotes) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    ULONGLONG now = GetTickCount64();
    s_remoteCache = { remotes, now };

    json j;
    j["timestamp"] = now;
    auto& arr = j["remotes"] = json::array();
    for (const auto& r : remotes) {
        json item;
        item["name"] = WideToUtf8(r.name);
        item["type"] = WideToUtf8(r.type);
        arr.push_back(item);
    }

    std::wstring filePath = GetCacheDir() + L"remotes.json";
    WriteJsonFile(filePath, j.dump());
}

void RcloneCache::InvalidateRemotes() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_remoteCache.remotes.clear();

    std::wstring filePath = GetCacheDir() + L"remotes.json";
    DeleteFileW(filePath.c_str());
}

void RcloneCache::InvalidateAll() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_statCache.clear();
    s_dirCache.clear();
    s_aboutCache.clear();
    s_featureCache.clear();
    s_remoteCache.remotes.clear();

    std::wstring dir = GetCacheDir();
    std::wstring pattern = dir + L"*.json";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((dir + findData.cFileName).c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
}

void RcloneCache::InvalidateForWrite(const std::wstring& fs, const std::wstring& remote) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);

    std::wstring prefix = MakeKey(fs, remote);
    for (auto it = s_statCache.begin(); it != s_statCache.end(); ) {
        if (it->first.find(prefix) == 0 || it->first.find(fs + L"|") == 0) {
            it = s_statCache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = s_dirCache.begin(); it != s_dirCache.end(); ) {
        if (it->first.find(prefix) == 0 || it->first.find(fs + L"|") == 0) {
            it = s_dirCache.erase(it);
        } else {
            ++it;
        }
    }

    s_aboutCache.erase(fs);

    std::wstring filePath = GetRemoteCachePath(fs);
    DeleteFileW(filePath.c_str());
}

void RcloneCache::CleanupOldCacheFiles() {
    std::wstring dir = GetCacheDir();
    std::wstring pattern = dir + L"stat_*.json";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((dir + findData.cFileName).c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    pattern = dir + L"dir_*.json";
    hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((dir + findData.cFileName).c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    pattern = dir + L"about_*.json";
    hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((dir + findData.cFileName).c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    pattern = dir + L"features_*.json";
    hFind = FindFirstFileW(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((dir + findData.cFileName).c_str());
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
}

bool RcloneCache::IsAboutRequestPending(const std::wstring& fs) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    return s_pendingAboutRequests.find(fs) != s_pendingAboutRequests.end();
}

void RcloneCache::MarkAboutRequestPending(const std::wstring& fs) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_pendingAboutRequests.insert(fs);
}

void RcloneCache::ClearAboutRequestPending(const std::wstring& fs) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_pendingAboutRequests.erase(fs);
    s_aboutCV.notify_all();
}

bool RcloneCache::WaitForAboutRequest(const std::wstring& fs, RcloneAboutInfo& out, DWORD timeoutMs) {
    std::unique_lock<std::mutex> lock(s_cacheMutex);
    
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    
    while (s_pendingAboutRequests.find(fs) != s_pendingAboutRequests.end()) {
        if (s_aboutCV.wait_until(lock, deadline) == std::cv_status::timeout) {
            return false;
        }
    }
    
    auto it = s_aboutCache.find(fs);
    if (it != s_aboutCache.end() && (GetTickCount64() - it->second.timestamp < ABOUT_TTL)) {
        out = it->second.info;
        return true;
    }
    
    return false;
}
