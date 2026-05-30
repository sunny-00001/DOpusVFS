#pragma once
#include <windows.h>
#include <string>

struct RcloneVFSConfig {
    std::wstring rclonePath;
    std::wstring configPath;
    bool autoStartDaemon;

    std::wstring rcAddr;
    DWORD rcPort;
    std::wstring rcUser;
    std::wstring rcPass;
    DWORD connTimeout;

    bool cacheEnabled;
    std::wstring vfsCacheMode;
    DWORD cacheSize;
    DWORD cacheMaxAge;
    DWORD cacheTTL;
    DWORD dirCacheTime;
    DWORD pollInterval;
    std::wstring cacheDir;

    DWORD maxConnections;
    DWORD transfers;
    DWORD checkers;
    std::wstring bandwidthLimit;
    DWORD bufferSize;
    DWORD readChunkSize;
    DWORD readAhead;
    DWORD lowLevelRetries;

    bool autoSync;
    DWORD syncInterval;
    DWORD writeBackDelay;
    DWORD maxIdle;
    DWORD statsInterval;
    bool cacheWriteBack;

    bool logEnabled;
    std::wstring logPath;
    std::wstring logLevel;
    DWORD logMaxSize;
    DWORD logMaxFiles;

    bool verboseLogging;
    bool confirmDelete;
    bool showHidden;
    bool caseSensitive;
    bool noModtime;
    bool noChecksum;
    bool fastList;
    bool useMmap;
    bool useRcloneCopy;

    RcloneVFSConfig() {
        rclonePath = L"rclone";
        configPath = L"";
        autoStartDaemon = true;

        rcAddr = L"127.0.0.57";
        rcPort = 8657;
        rcUser = L"opus";
        rcPass = L"";
        connTimeout = 30;

        cacheEnabled = false;
        vfsCacheMode = L"off";
        cacheSize = 1024;
        cacheMaxAge = 3600;
        cacheTTL = 300;
        dirCacheTime = 300;
        pollInterval = 60;
        cacheDir = L"";

        maxConnections = 4;
        transfers = 4;
        checkers = 8;
        bandwidthLimit = L"0";
        bufferSize = 16384;
        readChunkSize = 131072;
        readAhead = 524288;
        lowLevelRetries = 10;

        autoSync = false;
        syncInterval = 60;
        writeBackDelay = 5;
        maxIdle = 300;
        statsInterval = 60;
        cacheWriteBack = false;

        logEnabled = false;
        logPath = L"";
        logLevel = L"NOTICE";
        logMaxSize = 10;
        logMaxFiles = 5;

        verboseLogging = false;
        confirmDelete = false;
        showHidden = false;
        caseSensitive = false;
        noModtime = false;
        noChecksum = false;
        fastList = false;
        useMmap = false;
        useRcloneCopy = false;
    }
};

extern RcloneVFSConfig g_config;

void ConfigLoad();
void ConfigSave();
