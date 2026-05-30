#include "RcloneClient.h"
#include "RcloneCache.h"
#include "DaemonManager.h"
#include "Utils.h"
#include "PathParser.h"
#include "Logger.h"
#include "json.hpp"
#include "resource.h"
#include <commctrl.h>
#include <shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Comctl32.lib")

using json = nlohmann::json;

bool RcloneClient::IsDaemonRunning() {
    return DaemonManager::IsRunning();
}

bool RcloneClient::EnsureDaemonStarted() {
    return DaemonManager::EnsureRunning();
}

std::string RcloneClient::SendHttpPost(const std::string& path, const std::string& jsonPayload) {
    HINTERNET hConnect = DaemonManager::GetConnect();
    if (!hConnect) return "";

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) return "";

    std::wstring authHeader = DaemonManager::GetAuthHeader();
    BOOL bResults = WinHttpSendRequest(hRequest, authHeader.c_str(), (DWORD)-1, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);

    std::string responseStr = "";
    if (bResults && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        if (dwStatusCode != 200) {
            WinHttpCloseHandle(hRequest);
            return "";
        }
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize > 0) {
                std::vector<char> buffer(dwSize);
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    responseStr.append(buffer.data(), dwDownloaded);
                }
            }
        } while (dwSize > 0);
    }
    WinHttpCloseHandle(hRequest);
    return responseStr;
}

std::string RcloneClient::SendHttpPostFull(const std::string& path, const std::string& jsonPayload) {
    HINTERNET hConnect = DaemonManager::GetConnect();
    if (!hConnect) return "";

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) return "";

    std::wstring authHeader = DaemonManager::GetAuthHeader();
    BOOL bResults = WinHttpSendRequest(hRequest, authHeader.c_str(), (DWORD)-1, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);

    std::string responseStr = "";
    if (bResults && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize > 0) {
                std::vector<char> buffer(dwSize);
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    responseStr.append(buffer.data(), dwDownloaded);
                }
            }
        } while (dwSize > 0);
    }
    WinHttpCloseHandle(hRequest);
    return responseStr;
}

bool RcloneClient::Stat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& outInfo) {
    if (RcloneCache::GetStat(fs, remote, outInfo)) {
        return true;
    }

    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/stat", req.dump());
    if (res.empty() || res.find("\"item\"") == std::string::npos) return false;

    try {
        auto j = json::parse(res);
        auto item = j["item"];
        outInfo.name = Utf8ToWide(item["Name"].get<std::string>());
        outInfo.isDir = item["IsDir"].get<bool>();
        outInfo.size = item["Size"].get<uint64_t>();
        outInfo.modTime = ParseRcloneTime(item["ModTime"].get<std::string>());
        if (item.contains("MimeType") && item["MimeType"].is_string()) {
            outInfo.mimeType = Utf8ToWide(item["MimeType"].get<std::string>());
        }
        if (item.contains("ID") && item["ID"].is_string()) {
            outInfo.id = Utf8ToWide(item["ID"].get<std::string>());
        }
        if (item.contains("Hashes") && item["Hashes"].is_object()) {
            for (auto& h : item["Hashes"].items()) {
                outInfo.hashType = h.key();
                outInfo.hash = Utf8ToWide(h.value().get<std::string>());
                break;
            }
        }

        RcloneCache::SetStat(fs, remote, outInfo);
        return true;
    } catch (...) { return false; }
}

bool RcloneClient::About(const std::wstring& fs, RcloneAboutInfo& outInfo) {
    if (RcloneCache::GetAbout(fs, outInfo)) {
        return true;
    }

    json req;
    req["fs"] = WideToUtf8(fs);
    std::string res = SendHttpPost("/operations/about", req.dump());
    if (res.empty()) return false;

    try {
        auto j = json::parse(res);
        if (j.contains("total") && j["total"].is_number()) {
            outInfo.total = j["total"].get<uint64_t>();
            outInfo.hasTotal = true;
        }
        if (j.contains("used") && j["used"].is_number()) {
            outInfo.used = j["used"].get<uint64_t>();
            outInfo.hasUsed = true;
        }
        if (j.contains("free") && j["free"].is_number()) {
            outInfo.free = j["free"].get<uint64_t>();
            outInfo.hasFree = true;
        }
        if (j.contains("trashed") && j["trashed"].is_number()) {
            outInfo.trashed = j["trashed"].get<uint64_t>();
            outInfo.hasTrashed = true;
        }
        if (j.contains("objects") && j["objects"].is_number()) {
            outInfo.objects = j["objects"].get<uint64_t>();
            outInfo.hasObjects = true;
        }

        outInfo.source = "about_api";
        RcloneCache::SetAbout(fs, outInfo);
        return true;
    } catch (const json::exception& e) {
        LogError(L"About JSON parse error for %s: %s", fs.c_str(), Utf8ToWide(e.what()).c_str());
        return false;
    } catch (const std::exception& e) {
        LogError(L"About error for %s: %s", fs.c_str(), Utf8ToWide(e.what()).c_str());
        return false;
    } catch (...) {
        LogError(L"About unknown error for %s", fs.c_str());
        return false;
    }
}

uint64_t RcloneClient::CalculateUsedSpaceRecursive(const std::wstring& fs, const std::wstring& path) {
    uint64_t totalSize = 0;
    std::vector<RcloneFileInfo> files = ListDirectory(fs, path);
    
    for (size_t i = 0; i < files.size(); i++) {
        const RcloneFileInfo& file = files[i];
        if (file.isDir) {
            std::wstring subPath = path.empty() ? file.name : path + L"/" + file.name;
            totalSize += CalculateUsedSpaceRecursive(fs, subPath);
        } else {
            totalSize += file.size;
        }
    }
    return totalSize;
}

bool RcloneClient::CalcAboutRecursive(const std::wstring& fs, RcloneAboutInfo& outInfo) {
    uint64_t used = CalculateUsedSpaceRecursive(fs, L"");
    outInfo = {};
    outInfo.used = used;
    outInfo.hasUsed = true;
    outInfo.source = "recursive_calc";
    RcloneCache::SetAbout(fs, outInfo);
    return true;
}

RcloneResult RcloneClient::AboutWithRetry(const std::wstring& fs, RcloneAboutInfo& outInfo, int maxRetries) {
    RcloneResult result;
    
    for (int i = 0; i < maxRetries; ++i) {
        result.retryCount = i;
        
        if (RcloneCache::GetAbout(fs, outInfo)) {
            result.success = true;
            result.error = RcloneError::None;
            return result;
        }
        
        json req;
        req["fs"] = WideToUtf8(fs);
        std::string res = SendHttpPost("/operations/about", req.dump());
        
        if (res.empty()) {
            result.error = RcloneError::NetworkError;
            result.errorMessage = L"Empty response from rclone daemon";
            LogError(L"About request failed for %s: %s (attempt %d)", fs.c_str(), result.errorMessage.c_str(), i + 1);
            Sleep(1000 * (i + 1));
            continue;
        }
        
        try {
            auto j = json::parse(res);
            
            if (j.contains("error")) {
                std::string error = j["error"];
                if (error.find("auth") != std::string::npos) {
                    result.error = RcloneError::AuthError;
                    result.errorMessage = L"Authentication failed";
                    LogError(L"About request failed for %s: %s", fs.c_str(), result.errorMessage.c_str());
                    break;
                } else if (error.find("not found") != std::string::npos) {
                    result.error = RcloneError::NotFound;
                    result.errorMessage = L"Remote not found";
                    LogError(L"About request failed for %s: %s", fs.c_str(), result.errorMessage.c_str());
                    break;
                } else {
                    result.error = RcloneError::ServerError;
                    result.errorMessage = Utf8ToWide(error);
                    LogError(L"About request failed for %s: %s (attempt %d)", fs.c_str(), result.errorMessage.c_str(), i + 1);
                    Sleep(1000 * (i + 1));
                    continue;
                }
            }
            
            if (j.contains("total") && j["total"].is_number()) {
                outInfo.total = j["total"].get<uint64_t>();
                outInfo.hasTotal = true;
            }
            if (j.contains("used") && j["used"].is_number()) {
                outInfo.used = j["used"].get<uint64_t>();
                outInfo.hasUsed = true;
            }
            if (j.contains("free") && j["free"].is_number()) {
                outInfo.free = j["free"].get<uint64_t>();
                outInfo.hasFree = true;
            }
            if (j.contains("trashed") && j["trashed"].is_number()) {
                outInfo.trashed = j["trashed"].get<uint64_t>();
                outInfo.hasTrashed = true;
            }
            if (j.contains("objects") && j["objects"].is_number()) {
                outInfo.objects = j["objects"].get<uint64_t>();
                outInfo.hasObjects = true;
            }
            
            RcloneCache::SetAbout(fs, outInfo);
            result.success = true;
            result.error = RcloneError::None;
            return result;
            
        } catch (const std::exception& e) {
            result.error = RcloneError::InvalidResponse;
            result.errorMessage = Utf8ToWide(e.what());
            LogError(L"About request failed for %s: %s (attempt %d)", fs.c_str(), result.errorMessage.c_str(), i + 1);
            Sleep(1000 * (i + 1));
        }
    }
    
    LogError(L"About request failed for %s after %d attempts", fs.c_str(), maxRetries);
    return result;
}

std::vector<RcloneFileInfo> RcloneClient::ListDirectory(const std::wstring& fs, const std::wstring& remote) {
    std::vector<RcloneFileInfo> cached;
    if (RcloneCache::GetDir(fs, remote, cached)) {
        return cached;
    }

    std::vector<RcloneFileInfo> result;
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/list", req.dump());
    if (res.empty()) return result;

    try {
        auto j = json::parse(res);
        if (j.contains("list")) {
            for (auto& item : j["list"]) {
                RcloneFileInfo info;
                info.name = Utf8ToWide(item["Name"].get<std::string>());
                info.isDir = item["IsDir"].get<bool>();
                info.size = item["Size"].get<uint64_t>();
                info.modTime = ParseRcloneTime(item["ModTime"].get<std::string>());
                if (item.contains("MimeType") && item["MimeType"].is_string()) {
                    info.mimeType = Utf8ToWide(item["MimeType"].get<std::string>());
                }
                if (item.contains("ID") && item["ID"].is_string()) {
                    info.id = Utf8ToWide(item["ID"].get<std::string>());
                }
                if (item.contains("Hashes") && item["Hashes"].is_object()) {
                    for (auto& h : item["Hashes"].items()) {
                        info.hashType = h.key();
                        info.hash = Utf8ToWide(h.value().get<std::string>());
                        break;
                    }
                }
                result.push_back(info);
            }
        }
    } catch (...) {}

    RcloneCache::SetDir(fs, remote, result);
    return result;
}

bool RcloneClient::CopyFileToLocal(const std::wstring& fs, const std::wstring& remote, const std::wstring& localDestFile) {
    json requestJson;
    requestJson["srcFs"] = WideToUtf8(fs);
    requestJson["srcRemote"] = WideToUtf8(remote);

    std::wstring localPath = localDestFile;
    size_t lastSlash = localPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) return false;

    requestJson["dstFs"] = WideToUtf8(localPath.substr(0, lastSlash));
    requestJson["dstRemote"] = WideToUtf8(localPath.substr(lastSlash + 1));

    std::string response = SendHttpPost("/operations/copyfile", requestJson.dump());
    return !response.empty();
}

bool RcloneClient::CopyLocalToRemote(const std::wstring& localSrcFile, const std::wstring& fs, const std::wstring& remote) {
    json requestJson;
    requestJson["dstFs"] = WideToUtf8(fs);
    requestJson["dstRemote"] = WideToUtf8(remote);

    std::wstring localPath = localSrcFile;
    size_t lastSlash = localPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) return false;

    requestJson["srcFs"] = WideToUtf8(localPath.substr(0, lastSlash));
    requestJson["srcRemote"] = WideToUtf8(localPath.substr(lastSlash + 1));

    std::string res = SendHttpPost("/operations/copyfile", requestJson.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(fs, remote); return true; }
    return false;
}

std::vector<RcloneRemoteInfo> RcloneClient::ListRemotesWithType() {
    std::vector<RcloneRemoteInfo> cached;
    if (RcloneCache::GetRemotes(cached)) {
        return cached;
    }

    std::vector<RcloneRemoteInfo> remotes;
    std::string response = SendHttpPost("/config/dump", "{}");
    if (response.empty()) return remotes;

    try {
        auto j = json::parse(response);
        for (auto it = j.begin(); it != j.end(); ++it) {
            RcloneRemoteInfo info;
            std::string name = it.key();
            if (!name.empty() && name.back() == ':') name.pop_back();
            info.name = Utf8ToWide(name);
            if (it.value().contains("type") && it.value()["type"].is_string()) {
                info.type = Utf8ToWide(it.value()["type"].get<std::string>());
            }
            remotes.push_back(info);
        }
    } catch (...) {}

    RcloneCache::SetRemotes(remotes);
    return remotes;
}

bool RcloneClient::DeleteFile(const std::wstring& fs, const std::wstring& remote) {
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/deletefile", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(fs, remote); return true; }
    return false;
}

bool RcloneClient::RemoveDir(const std::wstring& fs, const std::wstring& remote) {
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/rmdir", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(fs, remote); return true; }
    return false;
}

bool RcloneClient::Purge(const std::wstring& fs, const std::wstring& remote) {
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/purge", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(fs, remote); return true; }
    return false;
}

bool RcloneClient::MakeDir(const std::wstring& fs, const std::wstring& remote) {
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/mkdir", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(fs, remote); return true; }
    return false;
}

bool RcloneClient::Move(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote) {
    RcloneFileInfo info;
    if (!Stat(srcFs, srcRemote, info)) return false;

    if (info.isDir) {
        json req;
        req["srcFs"] = WideToUtf8(srcFs + srcRemote);
        req["dstFs"] = WideToUtf8(dstFs + dstRemote);
        req["deleteEmptySrcDirs"] = true;

        std::string res = SendHttpPost("/sync/move", req.dump());
        if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
        return false;
    } else {
        json req;
        req["srcFs"] = WideToUtf8(srcFs);
        req["srcRemote"] = WideToUtf8(srcRemote);
        req["dstFs"] = WideToUtf8(dstFs);
        req["dstRemote"] = WideToUtf8(dstRemote);

        std::string res = SendHttpPost("/operations/movefile", req.dump());
        if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
        return false;
    }
}

bool RcloneClient::CopyFileRemote(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs);
    req["srcRemote"] = WideToUtf8(srcRemote);
    req["dstFs"] = WideToUtf8(dstFs);
    req["dstRemote"] = WideToUtf8(dstRemote);
    std::string res = SendHttpPost("/operations/copyfile", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
    return false;
}

bool RcloneClient::CopyDir(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs + srcRemote);
    req["dstFs"] = WideToUtf8(dstFs + dstRemote);
    std::string res = SendHttpPost("/sync/copy", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
    return false;
}

bool RcloneClient::MoveFileRemote(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs);
    req["srcRemote"] = WideToUtf8(srcRemote);
    req["dstFs"] = WideToUtf8(dstFs);
    req["dstRemote"] = WideToUtf8(dstRemote);
    std::string res = SendHttpPost("/operations/movefile", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
    return false;
}

bool RcloneClient::MoveDir(const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs + srcRemote);
    req["dstFs"] = WideToUtf8(dstFs + dstRemote);
    req["deleteEmptySrcDirs"] = true;
    std::string res = SendHttpPost("/sync/move", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
    return false;
}

bool RcloneClient::PublicLink(const std::wstring& fs, const std::wstring& remote, std::wstring& outUrl,
                               const std::wstring& expire) {
    json req;
    req["fs"] = WideToUtf8(fs);
    req["remote"] = WideToUtf8(remote);
    if (!expire.empty()) {
        req["expire"] = WideToUtf8(expire);
    }
    std::string res = SendHttpPostFull("/operations/publiclink", req.dump());
    if (res.empty()) return false;

    try {
        auto j = json::parse(res);
        if (j.contains("url") && j["url"].is_string()) {
            outUrl = Utf8ToWide(j["url"].get<std::string>());
            return true;
        }
        if (j.contains("error") && j["error"].is_string()) {
            outUrl = Utf8ToWide(j["error"].get<std::string>());
            return false;
        }
    } catch (...) {}
    return false;
}

bool RcloneClient::Cleanup(const std::wstring& fs) {
    json req;
    req["fs"] = WideToUtf8(fs);
    std::string res = SendHttpPost("/operations/cleanup", req.dump());
    if (!res.empty()) {
        RcloneCache::InvalidateAbout(fs);
        return true;
    }
    return false;
}

std::string RcloneClient::SendHttpPostWithRetry(const std::string& path, const std::string& jsonPayload) {
    std::string result = SendHttpPost(path, jsonPayload);
    if (!result.empty()) return result;

    if (DaemonManager::Reconnect()) {
        result = SendHttpPost(path, jsonPayload);
    }
    return result;
}

bool RcloneClient::Noop() {
    std::string res = SendHttpPost("/rc/noop", "{}");
    return !res.empty();
}

bool RcloneClient::Shutdown() {
    std::string res = SendHttpPost("/core/shutdown", "{}");
    return !res.empty();
}

bool RcloneClient::Check(const std::wstring& srcFs, const std::wstring& srcRemote,
                         const std::wstring& dstFs, const std::wstring& dstRemote,
                         bool oneWay, std::string& outReport) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs + srcRemote);
    req["dstFs"] = WideToUtf8(dstFs + dstRemote);
    if (oneWay) req["oneWay"] = true;
    std::string res = SendHttpPostFull("/operations/check", req.dump());
    if (res.empty()) return false;
    outReport = res;
    return true;
}

bool RcloneClient::SyncCopy(const std::wstring& srcFs, const std::wstring& srcRemote,
                             const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs + srcRemote);
    req["dstFs"] = WideToUtf8(dstFs + dstRemote);
    req["createEmptySrcDirs"] = true;
    std::string res = SendHttpPost("/sync/copy", req.dump());
    return !res.empty();
}

bool RcloneClient::SyncMove(const std::wstring& srcFs, const std::wstring& srcRemote,
                             const std::wstring& dstFs, const std::wstring& dstRemote) {
    json req;
    req["srcFs"] = WideToUtf8(srcFs + srcRemote);
    req["dstFs"] = WideToUtf8(dstFs + dstRemote);
    req["deleteEmptySrcDirs"] = true;
    std::string res = SendHttpPost("/sync/move", req.dump());
    if (!res.empty()) { RcloneCache::InvalidateForWrite(srcFs, srcRemote); return true; }
    return false;
}

bool RcloneClient::BackendCommand(const std::wstring& fs, const std::string& command,
                                   const std::string& jsonArgs, std::string& outResult) {
    std::string fsStr = WideToUtf8(fs);
    std::string path = "/backend/" + command;

    json req;
    req["fs"] = fsStr;
    if (!jsonArgs.empty()) {
        try {
            auto args = json::parse(jsonArgs);
            req["arg"] = args;
        } catch (...) {
            req["arg"] = jsonArgs;
        }
    }
    std::string res = SendHttpPostFull(path, req.dump());
    if (res.empty()) return false;
    outResult = res;
    return true;
}

bool RcloneClient::GetVersions(const std::wstring& fs, const std::wstring& remote,
                                std::vector<RcloneVersionInfo>& outVersions) {
    outVersions.clear();

    std::string result;
    std::string reqJson = "{\"remote\":\"" + WideToUtf8(remote) + "\"}";
    if (!BackendCommand(fs, "versions", reqJson, result)) {
        return false;
    }

    try {
        auto j = json::parse(result);
        if (j.contains("versions") && j["versions"].is_array()) {
            for (const auto& v : j["versions"]) {
                RcloneVersionInfo vi;
                vi.id = Utf8ToWide(v.value("ID", ""));
                vi.modTime = Utf8ToWide(v.value("ModTime", ""));
                vi.size = v.value("Size", 0ULL);
                vi.isCurrent = v.value("IsCurrent", false);
                outVersions.push_back(vi);
            }
        }
        return !outVersions.empty();
    } catch (...) {
        return false;
    }
}

bool RcloneClient::RestoreVersion(const std::wstring& fs, const std::wstring& remote,
                                   const std::wstring& versionId) {
    std::string result;
    std::string reqJson = "{\"remote\":\"" + WideToUtf8(remote) +
                          "\",\"versionId\":\"" + WideToUtf8(versionId) + "\"}";
    if (!BackendCommand(fs, "restore", reqJson, result)) {
        return false;
    }

    RcloneCache::InvalidateForWrite(fs, remote);
    return true;
}

bool RcloneClient::DeleteVersion(const std::wstring& fs, const std::wstring& remote,
                                  const std::wstring& versionId) {
    std::string result;
    std::string reqJson = "{\"remote\":\"" + WideToUtf8(remote) +
                          "\",\"versionId\":\"" + WideToUtf8(versionId) + "\"}";
    if (!BackendCommand(fs, "deleteversion", reqJson, result)) {
        return false;
    }

    RcloneCache::InvalidateStat(fs, remote);
    return true;
}

// ==================== Phase 3 APIs ====================

// Helper: parse rclone error from response. Returns true if response contains an error.
static bool ParseRcloneError(const std::string& res, std::string* outError) {
    if (res.empty()) {
        if (outError) *outError = "No response from rclone";
        return true;  // empty = failure
    }
    try {
        auto j = json::parse(res);
        if (j.contains("error") && j["error"].is_string()) {
            const auto& err = j["error"].get<std::string>();
            if (!err.empty()) {
                if (outError) *outError = err;
                return true;
            }
        }
    } catch (...) {}
    return false;
}

bool RcloneClient::ConfigCreate(const std::wstring& name, const std::string& type,
                                 const std::map<std::string, std::string>& parameters,
                                 HWND hwndParent, std::string* outError) {
    json req;
    req["name"] = WideToUtf8(name);
    req["type"] = type;
    req["_async"] = true;  // Required for OAuth authorization flow
    if (!parameters.empty()) {
        json params = json::object();
        for (const auto& kv : parameters) {
            params[kv.first] = kv.second;
        }
        req["parameters"] = params;
    }
    std::string res = SendHttpPostFull("/config/create", req.dump());
    if (ParseRcloneError(res, outError)) return false;

    // Async response: {"jobid":N} — wait for authorization to complete
    try {
        auto j = json::parse(res);
        if (j.contains("jobid") && j["jobid"].is_number_integer()) {
            int jobid = j["jobid"].get<int>();
            if (hwndParent) {
                // WaitForJob shows progress dialog; user authorizes in browser
                bool ok = WaitForJob(jobid, hwndParent);
                if (!ok) {
                    if (outError) *outError = "Authorization failed or was cancelled";
                    return false;
                }
            } else {
                // No parent window: poll synchronously
                for (int i = 0; i < 300; i++) {  // max 5 min
                    RcloneJobStatus st;
                    if (GetJobStatus(jobid, st) && st.finished) {
                        if (!st.success) {
                            if (outError) *outError = st.error.empty() ? "Config creation failed" : st.error;
                            return false;
                        }
                        break;
                    }
                    Sleep(1000);
                }
            }
        }
    } catch (...) {}

    RcloneCache::InvalidateAll();
    return true;
}

bool RcloneClient::ConfigDelete(const std::wstring& name, std::string* outError) {
    json req;
    req["name"] = WideToUtf8(name);
    std::string res = SendHttpPostFull("/config/delete", req.dump());
    if (ParseRcloneError(res, outError)) return false;
    RcloneCache::InvalidateAll();
    return true;
}

bool RcloneClient::ConfigUpdate(const std::wstring& name,
                                 const std::map<std::string, std::string>& parameters,
                                 std::string* outError) {
    json req;
    req["name"] = WideToUtf8(name);
    if (!parameters.empty()) {
        json params = json::object();
        for (const auto& kv : parameters) {
            params[kv.first] = kv.second;
        }
        req["parameters"] = params;
    }
    std::string res = SendHttpPostFull("/config/update", req.dump());
    if (ParseRcloneError(res, outError)) return false;
    RcloneCache::InvalidateAll();
    return true;
}

bool RcloneClient::ConfigProviders(std::vector<std::string>& outProviders) {
    std::string res = SendHttpPostFull("/config/providers", "{}");
    if (res.empty()) return false;
    try {
        auto j = json::parse(res);
        if (j.contains("providers") && j["providers"].is_array()) {
            for (const auto& p : j["providers"]) {
                if (p.contains("Name") && p["Name"].is_string()) {
                    outProviders.push_back(p["Name"].get<std::string>());
                }
            }
        }
        return true;
    } catch (...) {}
    return false;
}

bool RcloneClient::SetBwlimit(const std::string& limit, std::string* outError) {
    json req;
    req["rate"] = limit;
    std::string res = SendHttpPostFull("/core/bwlimit", req.dump());
    if (ParseRcloneError(res, outError)) return false;
    return true;
}

bool RcloneClient::GetBwlimit(std::string& outLimit) {
    std::string res = SendHttpPostFull("/core/bwlimit", "{}");
    if (res.empty()) return false;
    try {
        auto j = json::parse(res);
        if (j.contains("rate") && j["rate"].is_string()) {
            outLimit = j["rate"].get<std::string>();
            return true;
        }
    } catch (...) {}
    return false;
}

bool RcloneClient::StopJob(int jobid, std::string* outError) {
    json req;
    req["jobid"] = jobid;
    std::string res = SendHttpPostFull("/job/stop", req.dump());
    if (ParseRcloneError(res, outError)) return false;
    return true;
}

bool RcloneClient::CoreCommand(const std::string& command, const std::string& jsonArgs, std::string& outResult) {
    json req;
    req["command"] = command;
    if (!jsonArgs.empty()) {
        try {
            auto args = json::parse(jsonArgs);
            req["arg"] = args;
        } catch (...) {
            req["arg"] = jsonArgs;
        }
    }
    std::string res = SendHttpPostFull("/core/command", req.dump());
    if (res.empty()) return false;
    outResult = res;
    return true;
}

bool RcloneClient::CacheExpire(const std::wstring& fs, const std::wstring& remote) {
    json req;
    req["fs"] = WideToUtf8(fs);
    if (!remote.empty()) {
        req["remote"] = WideToUtf8(remote);
    }
    std::string res = SendHttpPostFull("/cache/expire", req.dump());
    if (res.empty()) return false;
    // Also invalidate local L2 cache
    if (remote.empty()) {
        RcloneCache::InvalidateAll();
    } else {
        RcloneCache::InvalidateForWrite(fs, remote);
    }
    return true;
}

bool RcloneClient::ListJobs(std::vector<std::pair<int, RcloneJobStatus>>& outJobs) {
    std::string res = SendHttpPostFull("/job/list", "{}");
    if (res.empty()) return false;
    try {
        auto j = json::parse(res);
        if (j.contains("jobids") && j["jobids"].is_array()) {
            for (const auto& id : j["jobids"]) {
                if (id.is_number_integer()) {
                    int jobid = id.get<int>();
                    RcloneJobStatus status;
                    if (GetJobStatus(jobid, status)) {
                        outJobs.push_back({ jobid, status });
                    }
                }
            }
        }
        return true;
    } catch (...) {}
    return false;
}

bool RcloneClient::GetJobStatus(int jobid, RcloneJobStatus& out) {
    json req;
    req["jobid"] = jobid;
    std::string res = SendHttpPostFull("/job/status", req.dump());
    if (res.empty()) return false;

    try {
        auto j = json::parse(res);
        out.finished = j.value("finished", false);
        out.success = j.value("success", false);
        out.error = j.value("error", "");

        if (j.contains("progress") && j["progress"].is_number()) {
            out.progress = j["progress"].get<double>();
        }

        if (j.contains("transferred") && j["transferred"].is_number()) {
            out.transferred = j["transferred"].get<int64_t>();
        }
        if (j.contains("total") && j["total"].is_number()) {
            out.total = j["total"].get<int64_t>();
        }
        if (j.contains("speed") && j["speed"].is_number()) {
            out.speed = j["speed"].get<int64_t>();
        }

        return true;
    } catch (...) { return false; }
}

struct ProgressDlgData {
    int jobid;
    volatile bool cancelled;
    HWND hDlg;
    HWND hProgress;
    HWND hText;
    HWND hPercent;
};

static INT_PTR CALLBACK ProgressDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProgressDlgData* data = (ProgressDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (ProgressDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        data->hDlg = hDlg;
        data->hProgress = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
        data->hText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
        data->hPercent = GetDlgItem(hDlg, IDC_PROGRESS_PERCENT);
        SendMessageW(data->hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(data->hProgress, PBM_SETPOS, 0, 0);
        SetTimer(hDlg, 1, 1000, NULL);
        return TRUE;
    }
    case WM_TIMER: {
        if (wParam == 1 && data) {
            RcloneJobStatus status;
            if (RcloneClient::GetJobStatus(data->jobid, status)) {
                if (status.finished) {
                    KillTimer(hDlg, 1);
                    EndDialog(hDlg, status.success ? IDOK : IDCANCEL);
                    return TRUE;
                }
                int pct = (int)(status.progress * 100.0);
                SendMessageW(data->hProgress, PBM_SETPOS, pct, 0);

                WCHAR text[256];
                if (status.total > 0) {
                    StringCchPrintfW(text, 256, L"%s / %s",
                        PathParser::FormatSize(status.transferred).c_str(),
                        PathParser::FormatSize(status.total).c_str());
                } else {
                    StringCchPrintfW(text, 256, L"%s",
                        PathParser::FormatSize(status.transferred).c_str());
                }
                SetWindowTextW(data->hText, text);

                WCHAR pctText[32];
                StringCchPrintfW(pctText, 32, L"%d%%", pct);
                SetWindowTextW(data->hPercent, pctText);
            }
        }
        return TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDCANCEL && data) {
            data->cancelled = true;
            json req;
            req["jobid"] = data->jobid;
            RcloneClient::SendHttpPost("/job/stop", req.dump());
            KillTimer(hDlg, 1);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE: {
        if (data) {
            data->cancelled = true;
            json req;
            req["jobid"] = data->jobid;
            RcloneClient::SendHttpPost("/job/stop", req.dump());
        }
        KillTimer(hDlg, 1);
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}

static DWORD WINAPI ProgressThreadProc(LPVOID param) {
    ProgressDlgData* data = (ProgressDlgData*)param;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&ProgressDlgProc, &hModule);

    DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_JOB_PROGRESS), NULL, ProgressDlgProc, (LPARAM)data);
    return 0;
}

bool RcloneClient::WaitForJob(int jobid, HWND hwndParent) {
    ProgressDlgData data = {};
    data.jobid = jobid;
    data.cancelled = false;

    HANDLE hThread = CreateThread(NULL, 0, ProgressThreadProc, &data, 0, NULL);
    if (!hThread) {
        while (true) {
            RcloneJobStatus status;
            if (!GetJobStatus(jobid, status)) return false;
            if (status.finished) return status.success;
            Sleep(1000);
        }
    }

    while (WaitForSingleObject(hThread, 100) == WAIT_TIMEOUT) {
        RcloneJobStatus status;
        if (GetJobStatus(jobid, status) && status.finished) {
            if (data.hDlg) {
                PostMessageW(data.hDlg, WM_CLOSE, 0, 0);
            }
            break;
        }
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);

    RcloneJobStatus finalStatus;
    if (GetJobStatus(jobid, finalStatus)) {
        return finalStatus.success;
    }
    return false;
}
