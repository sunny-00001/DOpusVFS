#include "ConfigManager.h"
#include "json.hpp"
#include "Utils.h"
#include <shlobj.h>
#include <strsafe.h>
#include <algorithm>

using json = nlohmann::json;

RcloneVFSConfig g_config;

static std::wstring GetConfigFilePath() {
    WCHAR appDataPath[MAX_PATH] = { 0 };
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath) != S_OK) {
        return L"";
    }
    std::wstring configDir = std::wstring(appDataPath) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\RcloneVFS\\config\\";
    
    // Create directory if not exists
    DWORD attr = GetFileAttributesW(configDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SHCreateDirectoryExW(NULL, configDir.c_str(), NULL);
    }
    
    return configDir + L"settings.json";
}

static bool ReadJsonFile(const std::wstring& filePath, json& out) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    std::string content((size_t)fileSize.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &content[0], (DWORD)fileSize.QuadPart, &read, NULL);
    CloseHandle(hFile);

    if (!ok || read != (DWORD)fileSize.QuadPart) return false;

    try {
        out = json::parse(content);
        return true;
    } catch (...) {
        return false;
    }
}

static bool WriteJsonFile(const std::wstring& filePath, const json& j) {
    std::string content = j.dump(4);
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(hFile);
    return ok && (written == content.size());
}

void ConfigLoad() {
    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty()) return;

    json j;
    if (!ReadJsonFile(filePath, j)) {
        // Try loading from registry for backward compatibility
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RcloneVFS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t buf[1024] = {};
            DWORD size = sizeof(buf);
            
            if (RegGetValueW(hKey, NULL, L"RclonePath", RRF_RT_REG_SZ, NULL, buf, &size) == ERROR_SUCCESS)
                g_config.rclonePath = buf;
            if (RegGetValueW(hKey, NULL, L"ConfigPath", RRF_RT_REG_SZ, NULL, buf, &size) == ERROR_SUCCESS)
                g_config.configPath = buf;
            
            DWORD val = 0;
            size = sizeof(DWORD);
            if (RegGetValueW(hKey, NULL, L"AutoStartDaemon", RRF_RT_DWORD, NULL, &val, &size) == ERROR_SUCCESS)
                g_config.autoStartDaemon = (val != 0);
            
            RegCloseKey(hKey);
        }
        return;
    }

    // Load from JSON
    if (j.contains("rclonePath")) g_config.rclonePath = Utf8ToWide(j["rclonePath"].get<std::string>());
    if (j.contains("configPath")) g_config.configPath = Utf8ToWide(j["configPath"].get<std::string>());
    if (j.contains("autoStartDaemon")) g_config.autoStartDaemon = j["autoStartDaemon"].get<bool>();

    if (j.contains("rcAddr")) g_config.rcAddr = Utf8ToWide(j["rcAddr"].get<std::string>());
    if (j.contains("rcPort")) g_config.rcPort = j["rcPort"].get<DWORD>();
    if (j.contains("rcUser")) g_config.rcUser = Utf8ToWide(j["rcUser"].get<std::string>());
    if (j.contains("rcPass")) g_config.rcPass = Utf8ToWide(j["rcPass"].get<std::string>());
    if (j.contains("connTimeout")) g_config.connTimeout = j["connTimeout"].get<DWORD>();

    if (j.contains("cacheEnabled")) g_config.cacheEnabled = j["cacheEnabled"].get<bool>();
    if (j.contains("vfsCacheMode")) g_config.vfsCacheMode = Utf8ToWide(j["vfsCacheMode"].get<std::string>());
    if (j.contains("cacheSize")) g_config.cacheSize = j["cacheSize"].get<DWORD>();
    if (j.contains("cacheMaxAge")) g_config.cacheMaxAge = j["cacheMaxAge"].get<DWORD>();
    if (j.contains("cacheTTL")) g_config.cacheTTL = j["cacheTTL"].get<DWORD>();
    if (j.contains("dirCacheTime")) g_config.dirCacheTime = j["dirCacheTime"].get<DWORD>();
    if (j.contains("pollInterval")) g_config.pollInterval = j["pollInterval"].get<DWORD>();
    if (j.contains("cacheDir")) g_config.cacheDir = Utf8ToWide(j["cacheDir"].get<std::string>());

    if (j.contains("maxConnections")) g_config.maxConnections = j["maxConnections"].get<DWORD>();
    if (j.contains("transfers")) g_config.transfers = j["transfers"].get<DWORD>();
    if (j.contains("checkers")) g_config.checkers = j["checkers"].get<DWORD>();
    if (j.contains("bandwidthLimit")) g_config.bandwidthLimit = Utf8ToWide(j["bandwidthLimit"].get<std::string>());
    if (j.contains("bufferSize")) g_config.bufferSize = j["bufferSize"].get<DWORD>();
    if (j.contains("readChunkSize")) g_config.readChunkSize = j["readChunkSize"].get<DWORD>();
    if (j.contains("readAhead")) g_config.readAhead = j["readAhead"].get<DWORD>();
    if (j.contains("lowLevelRetries")) g_config.lowLevelRetries = j["lowLevelRetries"].get<DWORD>();

    if (j.contains("autoSync")) g_config.autoSync = j["autoSync"].get<bool>();
    if (j.contains("syncInterval")) g_config.syncInterval = j["syncInterval"].get<DWORD>();
    if (j.contains("writeBackDelay")) g_config.writeBackDelay = j["writeBackDelay"].get<DWORD>();
    if (j.contains("maxIdle")) g_config.maxIdle = j["maxIdle"].get<DWORD>();
    if (j.contains("statsInterval")) g_config.statsInterval = j["statsInterval"].get<DWORD>();
    if (j.contains("cacheWriteBack")) g_config.cacheWriteBack = j["cacheWriteBack"].get<bool>();

    if (j.contains("logEnabled")) g_config.logEnabled = j["logEnabled"].get<bool>();
    if (j.contains("logPath")) g_config.logPath = Utf8ToWide(j["logPath"].get<std::string>());
    if (j.contains("logLevel")) g_config.logLevel = Utf8ToWide(j["logLevel"].get<std::string>());
    if (j.contains("logMaxSize")) g_config.logMaxSize = j["logMaxSize"].get<DWORD>();
    if (j.contains("logMaxFiles")) g_config.logMaxFiles = j["logMaxFiles"].get<DWORD>();

    if (j.contains("verboseLogging")) g_config.verboseLogging = j["verboseLogging"].get<bool>();
    if (j.contains("confirmDelete")) g_config.confirmDelete = j["confirmDelete"].get<bool>();
    if (j.contains("showHidden")) g_config.showHidden = j["showHidden"].get<bool>();
    if (j.contains("caseSensitive")) g_config.caseSensitive = j["caseSensitive"].get<bool>();
    if (j.contains("noModtime")) g_config.noModtime = j["noModtime"].get<bool>();
    if (j.contains("noChecksum")) g_config.noChecksum = j["noChecksum"].get<bool>();
    if (j.contains("fastList")) g_config.fastList = j["fastList"].get<bool>();
    if (j.contains("useMmap")) g_config.useMmap = j["useMmap"].get<bool>();
    if (j.contains("useRcloneCopy")) g_config.useRcloneCopy = j["useRcloneCopy"].get<bool>();
}

void ConfigSave() {
    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty()) return;

    json j;
    j["version"] = 1;
    j["rclonePath"] = WideToUtf8(g_config.rclonePath);
    j["configPath"] = WideToUtf8(g_config.configPath);
    j["autoStartDaemon"] = g_config.autoStartDaemon;

    j["rcAddr"] = WideToUtf8(g_config.rcAddr);
    j["rcPort"] = g_config.rcPort;
    j["rcUser"] = WideToUtf8(g_config.rcUser);
    j["rcPass"] = WideToUtf8(g_config.rcPass);
    j["connTimeout"] = g_config.connTimeout;

    j["cacheEnabled"] = g_config.cacheEnabled;
    j["vfsCacheMode"] = WideToUtf8(g_config.vfsCacheMode);
    j["cacheSize"] = g_config.cacheSize;
    j["cacheMaxAge"] = g_config.cacheMaxAge;
    j["cacheTTL"] = g_config.cacheTTL;
    j["dirCacheTime"] = g_config.dirCacheTime;
    j["pollInterval"] = g_config.pollInterval;
    j["cacheDir"] = WideToUtf8(g_config.cacheDir);

    j["maxConnections"] = g_config.maxConnections;
    j["transfers"] = g_config.transfers;
    j["checkers"] = g_config.checkers;
    j["bandwidthLimit"] = WideToUtf8(g_config.bandwidthLimit);
    j["bufferSize"] = g_config.bufferSize;
    j["readChunkSize"] = g_config.readChunkSize;
    j["readAhead"] = g_config.readAhead;
    j["lowLevelRetries"] = g_config.lowLevelRetries;

    j["autoSync"] = g_config.autoSync;
    j["syncInterval"] = g_config.syncInterval;
    j["writeBackDelay"] = g_config.writeBackDelay;
    j["maxIdle"] = g_config.maxIdle;
    j["statsInterval"] = g_config.statsInterval;
    j["cacheWriteBack"] = g_config.cacheWriteBack;

    j["logEnabled"] = g_config.logEnabled;
    j["logPath"] = WideToUtf8(g_config.logPath);
    j["logLevel"] = WideToUtf8(g_config.logLevel);
    j["logMaxSize"] = g_config.logMaxSize;
    j["logMaxFiles"] = g_config.logMaxFiles;

    j["verboseLogging"] = g_config.verboseLogging;
    j["confirmDelete"] = g_config.confirmDelete;
    j["showHidden"] = g_config.showHidden;
    j["caseSensitive"] = g_config.caseSensitive;
    j["noModtime"] = g_config.noModtime;
    j["noChecksum"] = g_config.noChecksum;
    j["fastList"] = g_config.fastList;
    j["useMmap"] = g_config.useMmap;
    j["useRcloneCopy"] = g_config.useRcloneCopy;

    WriteJsonFile(filePath, j);
}