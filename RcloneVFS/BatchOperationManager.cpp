#include "BatchOperationManager.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "PathParser.h"
#include "Logger.h"
#include "Utils.h"
#include "json.hpp"
#include <regex>
#include <sstream>
#include <process.h>

using json = nlohmann::json;

PathType BatchOperationManager::ParsePathType(const std::wstring& path) {
    if (path.empty()) return PathType::UNKNOWN;
    
    if (path.compare(0, 9, L"rclone://") == 0) {
        return PathType::RCLONE_VIRTUAL;
    }
    
    if (path.size() >= 2 && (path[1] == L':' || path[0] == L'\\')) {
        return PathType::LOCAL_FILESYSTEM;
    }
    
    return PathType::UNKNOWN;
}

ParsedPath BatchOperationManager::ParsePath(const std::wstring& path) {
    ParsedPath result;
    result.fullPath = path;
    result.type = ParsePathType(path);
    
    if (result.type == PathType::RCLONE_VIRTUAL) {
        std::wstring rest = path.substr(9);
        size_t colonPos = rest.find(L':');
        if (colonPos != std::wstring::npos) {
            result.remote = rest.substr(0, colonPos);
            result.path = rest.substr(colonPos + 1);
            while (!result.path.empty() && result.path[0] == L'/') {
                result.path = result.path.substr(1);
            }
        }
    } else if (result.type == PathType::LOCAL_FILESYSTEM) {
        result.path = path;
    }
    
    return result;
}

std::wstring BatchOperationManager::BuildRclonePath(const std::wstring& remote, const std::wstring& path) {
    if (remote.empty()) return path;
    return remote + L":" + path;
}

bool BatchOperationManager::ParseProgressLine(const std::string& line, RcloneProgressInfo& info) {
    static std::regex transferredRegex(R"(Transferred:\s*([\d.]+\s*[KMGT]?i?B)\s*/\s*([\d.]+\s*[KMGT]?i?B),\s*(\d+)%?)");
    static std::regex speedRegex(R"(([\d.]+\s*[KMGT]?i?B/s))");
    static std::regex etaRegex(R"(ETA\s*([\dhms]+))");
    static std::regex errorRegex(R"(ERROR\s*:\s*(.+))");
    
    std::smatch match;
    
    if (std::regex_search(line, match, errorRegex)) {
        info.error = Utf8ToWide(match[1].str());
        return true;
    }
    
    if (std::regex_search(line, match, transferredRegex)) {
        std::string transferredStr = match[1].str();
        std::string totalStr = match[2].str();
        std::string percentStr = match[3].str();
        
        auto parseSize = [](const std::string& s) -> uint64_t {
            double value = 0;
            char unit[16] = {0};
            if (sscanf_s(s.c_str(), "%lf%15s", &value, unit, (unsigned int)sizeof(unit)) >= 1) {
                std::string u = unit;
                if (u == "KiB" || u == "KB") return (uint64_t)(value * 1024);
                if (u == "MiB" || u == "MB") return (uint64_t)(value * 1024 * 1024);
                if (u == "GiB" || u == "GB") return (uint64_t)(value * 1024 * 1024 * 1024);
                if (u == "TiB" || u == "TB") return (uint64_t)(value * 1024 * 1024 * 1024 * 1024);
                return (uint64_t)value;
            }
            return 0;
        };
        
        info.bytesTransferred = parseSize(transferredStr);
        info.totalBytes = parseSize(totalStr);
        info.percentage = std::stod(percentStr);
        
        if (std::regex_search(line, match, speedRegex)) {
            std::string speedStr = match[1].str();
            double speedValue = 0;
            char speedUnit[16] = {0};
            if (sscanf_s(speedStr.c_str(), "%lf%15s", &speedValue, speedUnit, (unsigned int)sizeof(speedUnit)) >= 1) {
                std::string u = speedUnit;
                if (u.find('K') != std::string::npos) info.speed = speedValue * 1024;
                else if (u.find('M') != std::string::npos) info.speed = speedValue * 1024 * 1024;
                else if (u.find('G') != std::string::npos) info.speed = speedValue * 1024 * 1024 * 1024;
                else info.speed = speedValue;
            }
        }
        
        if (std::regex_search(line, match, etaRegex)) {
            info.eta = Utf8ToWide(match[1].str());
        }
        
        return true;
    }
    
    return false;
}

bool BatchOperationManager::ExecuteRemoteToRemote(
    const std::wstring& srcFs,
    const std::wstring& srcPath,
    const std::wstring& dstFs,
    const std::wstring& dstPath,
    bool move,
    HANDLE hAbortEvent,
    std::function<void(const RcloneProgressInfo&)> progressCallback,
    std::wstring& outError
) {
    std::wstring srcRclone = BuildRclonePath(srcFs, srcPath);
    std::wstring dstRclone = BuildRclonePath(dstFs, dstPath);
    
    json req;
    req["srcFs"] = WideToUtf8(srcRclone);
    req["dstFs"] = WideToUtf8(dstRclone);
    
    std::string endpoint = move ? "/sync/move" : "/sync/copy";
    
    if (move) {
        req["deleteEmptySrcDirs"] = true;
    }
    
    std::string result = RcloneClient::SendHttpPost(endpoint, req.dump());
    if (result.empty()) {
        outError = L"rclone operation failed";
        return false;
    }
    
    try {
        auto j = json::parse(result);
        if (j.contains("error") && !j["error"].is_null()) {
            outError = Utf8ToWide(j["error"].get<std::string>());
            return false;
        }
    } catch (...) {}
    
    RcloneCache::InvalidateForWrite(srcFs, srcPath);
    RcloneCache::InvalidateForWrite(dstFs, dstPath);
    
    return true;
}

bool BatchOperationManager::ExecuteLocalToRemote(
    const std::wstring& localPath,
    const std::wstring& dstFs,
    const std::wstring& dstPath,
    bool move,
    HANDLE hAbortEvent,
    std::function<void(const RcloneProgressInfo&)> progressCallback,
    std::wstring& outError
) {
    size_t lastSlash = localPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) {
        outError = L"Invalid local path";
        return false;
    }
    
    std::wstring localDir = localPath.substr(0, lastSlash);
    std::wstring fileName = localPath.substr(lastSlash + 1);
    
    json req;
    req["srcFs"] = WideToUtf8(localDir);
    req["srcRemote"] = WideToUtf8(fileName);
    req["dstFs"] = WideToUtf8(dstFs);
    req["dstRemote"] = WideToUtf8(dstPath);
    
    std::string endpoint = move ? "/operations/movefile" : "/operations/copyfile";
    
    std::string result = RcloneClient::SendHttpPost(endpoint, req.dump());
    if (result.empty()) {
        outError = L"rclone operation failed";
        return false;
    }
    
    try {
        auto j = json::parse(result);
        if (j.contains("error") && !j["error"].is_null()) {
            outError = Utf8ToWide(j["error"].get<std::string>());
            return false;
        }
    } catch (...) {}
    
    RcloneCache::InvalidateForWrite(dstFs, dstPath);
    
    return true;
}

bool BatchOperationManager::ExecuteRemoteToLocal(
    const std::wstring& srcFs,
    const std::wstring& srcPath,
    const std::wstring& localPath,
    bool move,
    HANDLE hAbortEvent,
    std::function<void(const RcloneProgressInfo&)> progressCallback,
    std::wstring& outError
) {
    size_t lastSlash = localPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) {
        outError = L"Invalid local path";
        return false;
    }
    
    std::wstring localDir = localPath.substr(0, lastSlash);
    std::wstring fileName = localPath.substr(lastSlash + 1);
    
    json req;
    req["srcFs"] = WideToUtf8(srcFs);
    req["srcRemote"] = WideToUtf8(srcPath);
    req["dstFs"] = WideToUtf8(localDir);
    req["dstRemote"] = WideToUtf8(fileName);
    
    std::string endpoint = move ? "/operations/movefile" : "/operations/copyfile";
    
    std::string result = RcloneClient::SendHttpPost(endpoint, req.dump());
    if (result.empty()) {
        outError = L"rclone operation failed";
        return false;
    }
    
    try {
        auto j = json::parse(result);
        if (j.contains("error") && !j["error"].is_null()) {
            outError = Utf8ToWide(j["error"].get<std::string>());
            return false;
        }
    } catch (...) {}
    
    if (move) {
        RcloneCache::InvalidateForWrite(srcFs, srcPath);
    }
    
    return true;
}

bool BatchOperationManager::ExecuteCopy(
    const ParsedPath& src,
    const ParsedPath& dst,
    bool move,
    HANDLE hAbortEvent,
    std::function<void(const RcloneProgressInfo&)> progressCallback,
    std::wstring& outError
) {
    if (src.type == PathType::RCLONE_VIRTUAL && dst.type == PathType::RCLONE_VIRTUAL) {
        return ExecuteRemoteToRemote(
            src.remote, src.path,
            dst.remote, dst.path,
            move, hAbortEvent, progressCallback, outError
        );
    }
    else if (src.type == PathType::LOCAL_FILESYSTEM && dst.type == PathType::RCLONE_VIRTUAL) {
        return ExecuteLocalToRemote(
            src.path,
            dst.remote, dst.path,
            move, hAbortEvent, progressCallback, outError
        );
    }
    else if (src.type == PathType::RCLONE_VIRTUAL && dst.type == PathType::LOCAL_FILESYSTEM) {
        return ExecuteRemoteToLocal(
            src.remote, src.path,
            dst.path,
            move, hAbortEvent, progressCallback, outError
        );
    }
    
    outError = L"Unsupported path combination";
    return false;
}

UINT BatchOperationManager::ProcessBatchOperation(LPVFSBATCHDATAW lpBatchData, const std::wstring& currentPath) {
    if (!lpBatchData) return VFSBATCHRES_DODEFAULT;
    
    UINT operation = lpBatchData->uiOperation;
    int numFiles = lpBatchData->iNumFiles;
    LPWSTR pszFiles = lpBatchData->pszFiles;
    LPWSTR pszDestPath = lpBatchData->pszDestPath;
    int* piResults = lpBatchData->piResults;
    HANDLE hAbortEvent = lpBatchData->hAbortEvent;
    DWORD flags = lpBatchData->dwFlags;
    bool isMove = (flags & BATCHF_COPY_DELETE_ORIGINAL) != 0;
    
    if (operation != VFSBATCHOP_EXTRACT && operation != VFSBATCHOP_ADD) {
        return VFSBATCHRES_DODEFAULT;
    }
    
    ParsedPath destParsed = ParsePath(pszDestPath ? pszDestPath : L"");
    
    if (destParsed.type == PathType::UNKNOWN) {
        return VFSBATCHRES_DODEFAULT;
    }
    
    if (destParsed.type == PathType::LOCAL_FILESYSTEM && operation == VFSBATCHOP_ADD) {
        return VFSBATCHRES_DODEFAULT;
    }
    
    std::vector<std::wstring> srcFiles;
    LPWSTR p = pszFiles;
    while (p && *p) {
        srcFiles.push_back(p);
        p += wcslen(p) + 1;
    }
    
    if (srcFiles.empty()) {
        return VFSBATCHRES_DODEFAULT;
    }
    
    bool allHandled = true;
    bool anyHandled = false;
    
    for (int i = 0; i < numFiles && i < (int)srcFiles.size(); i++) {
        if (hAbortEvent && WaitForSingleObject(hAbortEvent, 0) == WAIT_OBJECT_0) {
            if (piResults) piResults[i] = ERROR_CANCELLED;
            return VFSBATCHRES_ABORT;
        }
        
        ParsedPath srcParsed = ParsePath(srcFiles[i]);
        
        if (srcParsed.type == PathType::LOCAL_FILESYSTEM && destParsed.type == PathType::LOCAL_FILESYSTEM) {
            if (piResults) piResults[i] = 0;
            allHandled = false;
            continue;
        }
        
        std::wstring error;
        bool success = ExecuteCopy(
            srcParsed, destParsed,
            isMove,
            hAbortEvent,
            nullptr,
            error
        );
        
        if (piResults) {
            piResults[i] = success ? 0 : ERROR_GEN_FAILURE;
        }
        
        if (success) {
            anyHandled = true;
        } else {
            allHandled = false;
            LogError(L"BatchOperation failed: %s -> %s : %s", srcFiles[i].c_str(), pszDestPath ? pszDestPath : L"", error.c_str());
        }
    }
    
    if (allHandled) {
        return VFSBATCHRES_COMPLETE;
    } else if (anyHandled) {
        return VFSBATCHRES_HANDLED | VFSBATCHRES_CALLFOREACH;
    } else {
        return VFSBATCHRES_DODEFAULT;
    }
}
