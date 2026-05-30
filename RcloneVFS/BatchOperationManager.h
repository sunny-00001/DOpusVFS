#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "headers/vfs plugins.h"

enum class PathType {
    RCLONE_VIRTUAL,
    LOCAL_FILESYSTEM,
    UNKNOWN
};

struct ParsedPath {
    PathType type = PathType::UNKNOWN;
    std::wstring remote;
    std::wstring path;
    std::wstring fullPath;
};

struct RcloneProgressInfo {
    uint64_t bytesTransferred = 0;
    uint64_t totalBytes = 0;
    double percentage = 0.0;
    double speed = 0.0;
    std::wstring eta;
    std::wstring currentFile;
    std::wstring error;
};

class BatchOperationManager {
public:
    static UINT ProcessBatchOperation(LPVFSBATCHDATAW lpBatchData, const std::wstring& currentPath);

private:
    static PathType ParsePathType(const std::wstring& path);
    static ParsedPath ParsePath(const std::wstring& path);
    
    static bool ExecuteCopy(
        const ParsedPath& src,
        const ParsedPath& dst,
        bool move,
        HANDLE hAbortEvent,
        std::function<void(const RcloneProgressInfo&)> progressCallback,
        std::wstring& outError
    );
    
    static bool ExecuteRemoteToRemote(
        const std::wstring& srcFs,
        const std::wstring& srcPath,
        const std::wstring& dstFs,
        const std::wstring& dstPath,
        bool move,
        HANDLE hAbortEvent,
        std::function<void(const RcloneProgressInfo&)> progressCallback,
        std::wstring& outError
    );
    
    static bool ExecuteLocalToRemote(
        const std::wstring& localPath,
        const std::wstring& dstFs,
        const std::wstring& dstPath,
        bool move,
        HANDLE hAbortEvent,
        std::function<void(const RcloneProgressInfo&)> progressCallback,
        std::wstring& outError
    );
    
    static bool ExecuteRemoteToLocal(
        const std::wstring& srcFs,
        const std::wstring& srcPath,
        const std::wstring& localPath,
        bool move,
        HANDLE hAbortEvent,
        std::function<void(const RcloneProgressInfo&)> progressCallback,
        std::wstring& outError
    );
    
    static bool ParseProgressLine(const std::string& line, RcloneProgressInfo& info);
    static std::wstring BuildRclonePath(const std::wstring& remote, const std::wstring& path);
};
