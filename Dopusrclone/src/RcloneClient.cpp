#include "RcloneClient.h"
#include "Utils.h"
#include "json.hpp"
#include <wincrypt.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Crypt32.lib")

using json = nlohmann::json;

PROCESS_INFORMATION RcloneClient::s_processInfo = { 0 };
HINTERNET RcloneClient::s_hSession = NULL;
HINTERNET RcloneClient::s_hConnect = NULL;
std::unordered_map<std::wstring, CacheEntry> RcloneClient::s_statCache;
std::mutex RcloneClient::s_cacheMutex;

HANDLE RcloneClient::s_hJob = NULL;
std::string RcloneClient::s_rcUser = "opus";
std::string RcloneClient::s_rcPass = "";
std::wstring RcloneClient::s_authHeader = L"";

std::atomic<bool> RcloneClient::s_isDaemonRunning(false);
std::mutex RcloneClient::s_startupMutex;

bool RcloneClient::IsDaemonRunning() {
    return s_isDaemonRunning.load();
}

bool RcloneClient::EnsureDaemonStarted() {
    if (s_isDaemonRunning.load()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(s_startupMutex);
    if (s_isDaemonRunning.load()) {
        return true;
    }

    if (StartDaemon()) {
        s_isDaemonRunning.store(true);
        return true;
    }
    return false;
}

void RcloneClient::InvalidateCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_statCache.clear();
}

std::string RcloneClient::GenerateEnvironmentPassword() {
    WCHAR compName[256] = {0};
    DWORD compLen = 256;
    GetComputerNameW(compName, &compLen);
    
    WCHAR userName[256] = {0};
    DWORD userLen = 256;
    GetUserNameW(userName, &userLen);
    
    std::wstring envData = std::wstring(compName) + L"_" + userName + L"_DOpusRclone_V1_Salt";
    
    unsigned long hash = 5381;
    for (wchar_t c : envData) {
        hash = ((hash << 5) + hash) + c; 
    }
    
    char hexBuf[32];
    sprintf_s(hexBuf, "%08X%08X", hash, hash ^ 0xDEADBEEF);
    return std::string(hexBuf);
}

std::string RcloneClient::Base64Encode(const std::string& input) {
    DWORD len = 0;
    CryptBinaryToStringA((const BYTE*)input.data(), (DWORD)input.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &len);
    std::string result(len, '\0');
    CryptBinaryToStringA((const BYTE*)input.data(), (DWORD)input.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &result[0], &len);
    if (!result.empty() && result.back() == '\0') result.pop_back(); 
    return result;
}

std::wstring RcloneClient::GetRcloneExePath() {
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&RcloneClient::StartDaemon, &hModule);
    WCHAR dllPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);
    std::wstring exePath = dllPath;
    size_t lastSlash = exePath.find_last_of(L"\\/");
    
    if (lastSlash != std::wstring::npos) {
        std::wstring localRclone = exePath.substr(0, lastSlash) + L"\\rclone.exe";
        DWORD attr = GetFileAttributesW(localRclone.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return localRclone;
        }
    }
    return L"rclone.exe";
}

bool RcloneClient::StartDaemon() {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    s_rcPass = GenerateEnvironmentPassword();
    
    std::string credentials = s_rcUser + ":" + s_rcPass;
    std::string base64Creds = Base64Encode(credentials);
    s_authHeader = L"Authorization: Basic " + Utf8ToWide(base64Creds) + L"\r\nContent-Type: application/json\r\n";

    s_hJob = CreateJobObjectW(NULL, NULL);
    if (s_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(s_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    std::wstring exePath = GetRcloneExePath();
    std::wstring cmd = L"\"" + exePath + L"\" rcd --rc-addr 127.0.0.57:8657 --rc-user " + Utf8ToWide(s_rcUser) + L" --rc-pass " + Utf8ToWide(s_rcPass);
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, 
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED, 
                                  NULL, NULL, &si, &s_processInfo);

    if (success) {
        if (s_hJob) {
            AssignProcessToJobObject(s_hJob, s_processInfo.hProcess);
        }
        ResumeThread(s_processInfo.hThread);
        
        s_hSession = WinHttpOpen(L"DOpusRclone/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (s_hSession) {
            WinHttpSetTimeouts(s_hSession, 60000, 60000, 120000, 3600000);
            DWORD maxConns = 1024;
            WinHttpSetOption(s_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
            s_hConnect = WinHttpConnect(s_hSession, L"127.0.0.57", 8657, 0);
        }
    }
    
    return success != 0;
}

void RcloneClient::StopDaemon() {
    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = NULL; }
    
    if (s_hJob) {
        CloseHandle(s_hJob);
        s_hJob = NULL;
    }

    if (s_processInfo.hProcess) {
        TerminateProcess(s_processInfo.hProcess, 0);
        CloseHandle(s_processInfo.hProcess);
        CloseHandle(s_processInfo.hThread);
        s_processInfo.hProcess = NULL;
    }
    s_isDaemonRunning.store(false);
}

std::string RcloneClient::SendHttpPost(const std::string& path, const std::string& jsonPayload) {
    if (!s_hConnect) return "";

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(s_hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) return "";

    BOOL bResults = WinHttpSendRequest(hRequest, s_authHeader.c_str(), (DWORD)-1, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);
    
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

bool RcloneClient::Stat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& outInfo) {
    std::wstring cacheKey = fs + L"|" + remote;
    ULONGLONG now = GetTickCount64();
    
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_statCache.find(cacheKey);
        if (it != s_statCache.end() && (now - it->second.timestamp < 3000)) {
            outInfo = it->second.info;
            return true;
        }
    }

    json req; req["fs"] = WideToUtf8(fs); req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/stat", req.dump());
    if (res.empty() || res.find("\"item\"") == std::string::npos) return false;

    try {
        auto j = json::parse(res);
        auto item = j["item"];
        outInfo.name = Utf8ToWide(item["Name"].get<std::string>());
        outInfo.isDir = item["IsDir"].get<bool>();
        outInfo.size = item["Size"].get<uint64_t>();
        outInfo.modTime = ParseRcloneTime(item["ModTime"].get<std::string>());

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_statCache[cacheKey] = { outInfo, now };
        return true;
    } catch (...) { return false; }
}

std::vector<RcloneFileInfo> RcloneClient::ListDirectory(const std::wstring& fs, const std::wstring& remote) {
    std::vector<RcloneFileInfo> result;
    json req; req["fs"] = WideToUtf8(fs); req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/list", req.dump());
    if (res.empty()) return result;

    ULONGLONG now = GetTickCount64();
    std::lock_guard<std::mutex> lock(s_cacheMutex);

    try {
        auto j = json::parse(res);
        if (j.contains("list")) {
            for (auto& item : j["list"]) {
                RcloneFileInfo info;
                info.name = Utf8ToWide(item["Name"].get<std::string>());
                info.isDir = item["IsDir"].get<bool>();
                info.size = item["Size"].get<uint64_t>();
                info.modTime = ParseRcloneTime(item["ModTime"].get<std::string>());
                result.push_back(info);

                std::wstring itemRemote = remote.empty() ? info.name : (remote + L"/" + info.name);
                s_statCache[fs + L"|" + itemRemote] = { info, now };
            }
        }
    } catch (...) {}
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
    if (!res.empty()) { InvalidateCache(); return true; }
    return false;
}

std::vector<std::wstring> RcloneClient::ListRemotes() {
    std::vector<std::wstring> remotes;
    std::string response = SendHttpPost("/config/listremotes", "{}");
    if (response.empty()) return remotes;

    try {
        auto j = json::parse(response);
        if (j.contains("remotes") && j["remotes"].is_array()) {
            for (const auto& item : j["remotes"]) {
                std::string r = item.get<std::string>();
                if (!r.empty() && r.back() == ':') r.pop_back();
                remotes.push_back(Utf8ToWide(r));
            }
        }
    } catch (...) {}
    return remotes;
}

bool RcloneClient::DeleteFile(const std::wstring& fs, const std::wstring& remote) {
    json req; req["fs"] = WideToUtf8(fs); req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/deletefile", req.dump());
    if (!res.empty()) { InvalidateCache(); return true; }
    return false;
}

bool RcloneClient::RemoveDir(const std::wstring& fs, const std::wstring& remote) {
    json req; req["fs"] = WideToUtf8(fs); req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/rmdir", req.dump());
    if (!res.empty()) { InvalidateCache(); return true; }
    return false;
}

bool RcloneClient::MakeDir(const std::wstring& fs, const std::wstring& remote) {
    json req; req["fs"] = WideToUtf8(fs); req["remote"] = WideToUtf8(remote);
    std::string res = SendHttpPost("/operations/mkdir", req.dump());
    if (!res.empty()) { InvalidateCache(); return true; }
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
        if (!res.empty()) { InvalidateCache(); return true; }
        return false;
    } else {
        json req;
        req["srcFs"] = WideToUtf8(srcFs);
        req["srcRemote"] = WideToUtf8(srcRemote);
        req["dstFs"] = WideToUtf8(dstFs);
        req["dstRemote"] = WideToUtf8(dstRemote);
        
        std::string res = SendHttpPost("/operations/movefile", req.dump());
        if (!res.empty()) { InvalidateCache(); return true; }
        return false;
    }
}
