#include "IPFSClient.h"
#include "Utils.h"
#include <sstream>

#pragma comment(lib, "winhttp.lib")

std::wstring IPFSClient::s_daemonHost = L"127.0.0.1";
int IPFSClient::s_daemonPort = 5001;
HINTERNET IPFSClient::s_hSession = NULL;
HINTERNET IPFSClient::s_hConnect = NULL;
std::atomic<bool> IPFSClient::s_isDaemonRunning(false);
std::mutex IPFSClient::s_startupMutex;
std::unordered_map<std::wstring, CacheEntry> IPFSClient::s_statCache;
std::mutex IPFSClient::s_cacheMutex;

bool IPFSClient::Init() {
    s_hSession = WinHttpOpen(L"IPFSVFS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (s_hSession) {
        WinHttpSetTimeouts(s_hSession, 60000, 60000, 120000, 3600000);
        DWORD maxConns = 1024;
        WinHttpSetOption(s_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
        s_hConnect = WinHttpConnect(s_hSession, s_daemonHost.c_str(), (INTERNET_PORT)s_daemonPort, 0);
    }
    return s_hSession != NULL && s_hConnect != NULL;
}

void IPFSClient::Cleanup() {
    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = NULL; }
    s_isDaemonRunning.store(false);
}

bool IPFSClient::IsDaemonRunning() {
    return s_isDaemonRunning.load();
}

bool IPFSClient::EnsureDaemonStarted() {
    if (s_isDaemonRunning.load()) return true;

    std::lock_guard<std::mutex> lock(s_startupMutex);
    if (s_isDaemonRunning.load()) return true;

    if (!s_hSession) {
        if (!Init()) return false;
    }

    std::string res = SendHttpGet("/api/v0/version");
    if (!res.empty() && res.find("\"Version\"") != std::string::npos) {
        s_isDaemonRunning.store(true);
        return true;
    }

    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = NULL; }
    s_hConnect = WinHttpConnect(s_hSession, s_daemonHost.c_str(), (INTERNET_PORT)s_daemonPort, 0);
    if (!s_hConnect) return false;

    res = SendHttpGet("/api/v0/version");
    if (!res.empty() && res.find("\"Version\"") != std::string::npos) {
        s_isDaemonRunning.store(true);
        return true;
    }

    return false;
}

std::wstring IPFSClient::GetDaemonAddress() { return s_daemonHost; }
void IPFSClient::SetDaemonAddress(const std::wstring& addr) { s_daemonHost = addr; }
int IPFSClient::GetDaemonPort() { return s_daemonPort; }
void IPFSClient::SetDaemonPort(int port) { s_daemonPort = port; }

void IPFSClient::InvalidateCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_statCache.clear();
}

std::string IPFSClient::SendHttpGet(const std::string& path) {
    if (!s_hConnect) return "";

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(s_hConnect, L"POST", wPath.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) return "";

    BOOL bResults = WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n",
        (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string responseStr;
    if (bResults && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
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

std::string IPFSClient::SendHttpPost(const std::string& path, const std::string& contentType, const std::string& body) {
    if (!s_hConnect) return "";

    std::wstring wPath = Utf8ToWide(path);
    HINTERNET hRequest = WinHttpOpenRequest(s_hConnect, L"POST", wPath.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) return "";

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!contentType.empty()) {
        headers = Utf8ToWide("Content-Type: " + contentType + "\r\n");
    }

    BOOL bResults;
    if (!body.empty()) {
        bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
            (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);
    } else {
        bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    std::string responseStr;
    if (bResults && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
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

static bool ParseJsonObject(const std::string& json, const std::string& key, std::string& value) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return false;

    size_t valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valueStart == std::string::npos) return false;

    if (json[valueStart] == '"') {
        size_t valueEnd = json.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) return false;
        value = json.substr(valueStart + 1, valueEnd - valueStart - 1);
        return true;
    }

    size_t valueEnd = json.find_first_of(",}]\n\r \t", valueStart);
    if (valueEnd == std::string::npos) valueEnd = json.length();
    value = json.substr(valueStart, valueEnd - valueStart);
    return true;
}

static bool ParseJsonBool(const std::string& json, const std::string& key, bool& value) {
    std::string strVal;
    if (!ParseJsonObject(json, key, strVal)) return false;
    value = (strVal == "true");
    return true;
}

static bool ParseJsonNumber(const std::string& json, const std::string& key, uint64_t& value) {
    std::string strVal;
    if (!ParseJsonObject(json, key, strVal)) return false;
    try { value = std::stoull(strVal); return true; }
    catch (...) { return false; }
}

static std::vector<std::string> ExtractJsonObjects(const std::string& json) {
    std::vector<std::string> objects;
    size_t pos = 0;
    while (pos < json.length()) {
        size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos) break;

        int depth = 0;
        size_t objEnd = objStart;
        for (size_t i = objStart; i < json.length(); i++) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0) { objEnd = i; break; }
            }
        }
        if (objEnd > objStart) {
            objects.push_back(json.substr(objStart, objEnd - objStart + 1));
            pos = objEnd + 1;
        } else {
            break;
        }
    }
    return objects;
}

bool IPFSClient::Stat(const std::wstring& cid, const std::wstring& path, IPFSFileInfo& outInfo) {
    std::wstring cacheKey = cid + L"|" + path;
    ULONGLONG now = GetTickCount64();

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_statCache.find(cacheKey);
        if (it != s_statCache.end() && (now - it->second.timestamp < 5000)) {
            outInfo = it->second.info;
            return true;
        }
    }

    std::string ipfsPath = "/ipfs/" + WideToUtf8(cid);
    if (!path.empty()) ipfsPath += "/" + WideToUtf8(path);

    std::string urlPath = "/api/v0/files/stat?arg=" + UrlEncode(ipfsPath);
    std::string res = SendHttpGet(urlPath);
    if (res.empty()) return false;

    std::string name, hash, type;
    uint64_t size = 0, blocks = 0;

    if (!ParseJsonObject(res, "Hash", hash)) return false;
    ParseJsonObject(res, "Name", name);
    ParseJsonObject(res, "Type", type);
    ParseJsonNumber(res, "Size", size);
    ParseJsonNumber(res, "Blocks", blocks);

    outInfo.cid = Utf8ToWide(hash);
    if (name.empty()) {
        std::wstring wpath = path;
        size_t lastSlash = wpath.find_last_of(L"/\\");
        outInfo.name = (lastSlash != std::wstring::npos) ? wpath.substr(lastSlash + 1) : wpath;
        if (outInfo.name.empty()) outInfo.name = outInfo.cid;
    } else {
        outInfo.name = Utf8ToWide(name);
    }
    outInfo.isDir = (type == "directory" || type == "1");
    outInfo.size = size;
    outInfo.blocks = blocks;
    outInfo.type = (type == "directory" || type == "1") ? L"目录" : L"文件";
    GetSystemTimeAsFileTime(&outInfo.modTime);

    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_statCache[cacheKey] = { outInfo, now };
    return true;
}

std::vector<IPFSFileInfo> IPFSClient::ListDirectory(const std::wstring& cid, const std::wstring& path) {
    std::vector<IPFSFileInfo> result;

    std::string ipfsPath = "/ipfs/" + WideToUtf8(cid);
    if (!path.empty()) ipfsPath += "/" + WideToUtf8(path);

    std::string urlPath = "/api/v0/ls?arg=" + UrlEncode(ipfsPath);
    std::string res = SendHttpGet(urlPath);
    if (res.empty()) return result;

    std::string objectsStr;
    size_t objectsPos = res.find("\"Objects\"");
    if (objectsPos == std::string::npos) return result;

    std::vector<std::string> topObjects = ExtractJsonObjects(res.substr(objectsPos));
    if (topObjects.empty()) return result;

    std::string linksStr;
    size_t linksPos = topObjects[0].find("\"Links\"");
    if (linksPos == std::string::npos) return result;

    std::vector<std::string> linkObjects = ExtractJsonObjects(topObjects[0].substr(linksPos));

    ULONGLONG now = GetTickCount64();
    std::lock_guard<std::mutex> lock(s_cacheMutex);

    for (const auto& linkObj : linkObjects) {
        IPFSFileInfo info;
        std::string name, hash, type;
        uint64_t size = 0;

        ParseJsonObject(linkObj, "Name", name);
        ParseJsonObject(linkObj, "Hash", hash);
        ParseJsonObject(linkObj, "Type", type);
        ParseJsonNumber(linkObj, "Size", size);

        info.name = Utf8ToWide(name);
        info.cid = Utf8ToWide(hash);
        info.isDir = (type == "1" || type == "directory");
        info.size = size;
        info.type = info.isDir ? L"目录" : L"文件";
        info.blocks = 0;
        GetSystemTimeAsFileTime(&info.modTime);

        result.push_back(info);

        std::wstring itemPath = path.empty() ? info.name : (path + L"/" + info.name);
        s_statCache[cid + L"|" + itemPath] = { info, now };
    }

    return result;
}

bool IPFSClient::GetFileSize(const std::wstring& cid, uint64_t& size) {
    std::string urlPath = "/api/v0/object/stat?arg=" + UrlEncode(WideToUtf8(cid));
    std::string res = SendHttpGet(urlPath);
    if (res.empty()) return false;
    return ParseJsonNumber(res, "DataSize", size) || ParseJsonNumber(res, "CumulativeSize", size);
}

bool IPFSClient::PinAdd(const std::wstring& cid) {
    std::string urlPath = "/api/v0/pin/add?arg=" + UrlEncode(WideToUtf8(cid));
    std::string res = SendHttpGet(urlPath);
    return !res.empty();
}

bool IPFSClient::PinRm(const std::wstring& cid) {
    std::string urlPath = "/api/v0/pin/rm?arg=" + UrlEncode(WideToUtf8(cid));
    std::string res = SendHttpGet(urlPath);
    return !res.empty();
}

bool IPFSClient::PinLs(const std::wstring& cid, bool& isPinned) {
    std::string urlPath = "/api/v0/pin/ls?arg=" + UrlEncode(WideToUtf8(cid));
    std::string res = SendHttpGet(urlPath);
    if (res.empty()) { isPinned = false; return true; }
    isPinned = (res.find("\"Pinned\"") != std::string::npos || res.find("\"recursive\"") != std::string::npos);
    return true;
}
