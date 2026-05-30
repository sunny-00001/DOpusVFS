#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct RclonePathInfo {
    std::wstring remote;
    std::wstring remotePath;
    std::wstring fs;
    bool isRoot;
    bool isRemoteRoot;
};

struct RcloneFileInfo {
    std::wstring name;
    bool isDir;
    uint64_t size;
    FILETIME modTime;
    std::wstring mimeType;
    std::wstring id;
    std::wstring hash;
    std::string hashType;
};

struct RcloneRemoteInfo {
    std::wstring name;
    std::wstring type;
};

struct RcloneAboutInfo {
    uint64_t total = 0;
    uint64_t used = 0;
    uint64_t free = 0;
    uint64_t trashed = 0;
    uint64_t other = 0;
    uint64_t objects = 0;
    bool hasTotal = false;
    bool hasUsed = false;
    bool hasFree = false;
    bool hasTrashed = false;
    bool hasObjects = false;
    std::string source;  // "about_api" or "recursive_calc"
};

struct RcloneBackendFeatures {
    bool supportsAbout = false;
    bool supportsPublicLink = false;
    bool supportsTrash = false;
    bool supportsVersions = false;
    bool supportsHash = false;
    std::vector<std::string> hashTypes;
    std::wstring remoteType;
};

struct RcloneVersionInfo {
    std::wstring id;
    std::wstring modTime;
    uint64_t size = 0;
    bool isCurrent = false;
};

enum {
    COL_RC_REMOTE = 1,
    COL_RC_STYPE,
    COL_RC_QUOTA_TOTAL,
    COL_RC_QUOTA_USED,
    COL_RC_QUOTA_FREE,
    COL_RC_QUOTA_USAGE,
    COL_RC_TRASHED_SIZE,
    COL_RC_STATUS,

    COL_RC_REMOTE2 = 101,
    COL_RC_ITYPE,
    COL_RC_FSIZE,
    COL_RC_MTIME,
    COL_RC_MIME,
    COL_RC_FID,
    COL_RC_RPATH,
    COL_RC_HASH,
    COL_RC_SHARED,
};

#define NUM_ROOT_COLUMNS 8
#define NUM_FILE_COLUMNS 9
#define NUM_ALL_COLUMNS (NUM_ROOT_COLUMNS + NUM_FILE_COLUMNS)
