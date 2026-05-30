#include "GitHubClient.h"
#include <sstream>
#include <algorithm>
#include <shlobj.h>
#include <cmath>

#pragma comment(lib, "winhttp.lib")

HINTERNET GitHubClient::s_hSession = nullptr;
HINTERNET GitHubClient::s_hConnect = nullptr;
GitHubConfig GitHubClient::s_config;
std::mutex GitHubClient::s_cacheMutex;
std::mutex GitHubClient::s_configMutex;
std::mutex GitHubClient::s_sessionMutex;
std::unordered_map<std::wstring, GitHubClient::DirCacheEntry> GitHubClient::s_dirCache;
std::unordered_map<std::wstring, GitHubClient::RepoCacheEntry> GitHubClient::s_repoCache;
std::unordered_map<std::wstring, GitHubClient::SearchCacheEntry> GitHubClient::s_searchCache;

void GitHubClient::Init() {
    std::lock_guard<std::mutex> lock(s_sessionMutex);
    if (s_hSession) return;
    
    s_hSession = WinHttpOpen(L"GitHubVFS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (s_hSession) {
        DWORD timeout = 30000;
        WinHttpSetOption(s_hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(s_hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    }
    LoadConfig();
}

void GitHubClient::Cleanup() {
    std::lock_guard<std::mutex> lock(s_sessionMutex);
    if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = nullptr; }
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = nullptr; }
    s_dirCache.clear(); s_repoCache.clear(); s_searchCache.clear();
}

std::wstring GitHubClient::GetConfigFilePath() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath)))
        // 使用与日志相同的 VFSPlugin 目录，保持配置和日志路径一致
        return std::wstring(appDataPath) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\GithubVFS\\config.json";
    return L"GitHubVFS_config.json";
}

bool GitHubClient::LoadConfig() {
    std::wstring configPath = GetConfigFilePath();
    HANDLE hFile = CreateFileW(configPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        s_config = GitHubConfig();
        s_config.cacheTimeout = 300; s_config.connTimeout = 30; s_config.itemsPerPage = 100;
        s_config.showForks = true; s_config.showArchived = true; s_config.showPrivate = true;
        s_config.showDescription = true; s_config.defaultBranchName = L"main";
        s_config.apiUrl = L"api.github.com"; s_config.autoRefresh = false; s_config.refreshInterval = 300;
        s_config.largeFileWarn = true; s_config.largeFileSizeMB = 10;
        s_config.pathMode = GITHUB_PATH_STRICT; s_config.metaPrefix = L".meta";
        s_config.refKeyword = L"@"; s_config.enableAliases = true; s_config.enableOldPaths = true;
        return false;
    }
    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::string content(fileSize, '\0');
    DWORD bytesRead; ReadFile(hFile, &content[0], fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    
    s_config.authMode = (GitHubAuthMode)ParseJsonInt(content, "authMode");
    s_config.token = ParseJsonString(content, "token");
    s_config.username = ParseJsonString(content, "username");
    s_config.password = ParseJsonString(content, "password");
    s_config.apiUrl = ParseJsonString(content, "apiUrl");
    if (s_config.apiUrl.empty()) s_config.apiUrl = L"api.github.com";
    s_config.cacheTimeout = ParseJsonInt(content, "cacheTimeout"); if (s_config.cacheTimeout == 0) s_config.cacheTimeout = 300;
    s_config.connTimeout = ParseJsonInt(content, "connTimeout"); if (s_config.connTimeout == 0) s_config.connTimeout = 30;
    s_config.itemsPerPage = ParseJsonInt(content, "itemsPerPage"); if (s_config.itemsPerPage == 0) s_config.itemsPerPage = 100;
    s_config.showForks = ParseJsonBool(content, "showForks");
    s_config.showArchived = ParseJsonBool(content, "showArchived");
    s_config.showPrivate = ParseJsonBool(content, "showPrivate");
    s_config.showDescription = ParseJsonBool(content, "showDescription");
    s_config.defaultBranchName = ParseJsonString(content, "defaultBranchName");
    if (s_config.defaultBranchName.empty()) s_config.defaultBranchName = L"main";
    s_config.proxyType = (GitHubProxyType)ParseJsonInt(content, "proxyType");
    s_config.proxyHost = ParseJsonString(content, "proxyHost");
    s_config.proxyPort = ParseJsonInt(content, "proxyPort");
    s_config.currentUser = ParseJsonString(content, "currentUser");
    s_config.repoSort = (GitHubRepoSort)ParseJsonInt(content, "repoSort");
    s_config.commitMessageTemplate = ParseJsonString(content, "commitMessageTemplate");
    s_config.autoRefresh = ParseJsonBool(content, "autoRefresh");
    s_config.refreshInterval = ParseJsonInt(content, "refreshInterval"); if (s_config.refreshInterval == 0) s_config.refreshInterval = 300;
    s_config.largeFileWarn = ParseJsonBool(content, "largeFileWarn");
    s_config.largeFileSizeMB = ParseJsonInt(content, "largeFileSizeMB"); if (s_config.largeFileSizeMB == 0) s_config.largeFileSizeMB = 10;
    s_config.pathMode = (GitHubPathMode)ParseJsonInt(content, "pathMode");
    s_config.metaPrefix = ParseJsonString(content, "metaPrefix"); if (s_config.metaPrefix.empty()) s_config.metaPrefix = L".meta";
    s_config.refKeyword = ParseJsonString(content, "refKeyword"); if (s_config.refKeyword.empty()) s_config.refKeyword = L"@";
    s_config.enableAliases = ParseJsonBool(content, "enableAliases");
    s_config.enableOldPaths = ParseJsonBool(content, "enableOldPaths");
    return true;
}

bool GitHubClient::SaveConfig() {
    std::wstring configPath = GetConfigFilePath();
    std::wstring dir = configPath.substr(0, configPath.rfind(L'\\'));
    // 递归创建多级目录（路径可能包含 GPSoftware\Directory Opus\... 等多级）
    std::wstring curDir;
    size_t pos = 0;
    while ((pos = dir.find(L'\\', pos)) != std::wstring::npos) {
        curDir = dir.substr(0, pos);
        CreateDirectoryW(curDir.c_str(), nullptr);
        pos++;
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    std::stringstream ss;
    ss << "{\n  \"authMode\": " << s_config.authMode << ",\n  \"token\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.token)) << "\",\n";
    ss << "  \"username\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.username)) << "\",\n  \"password\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.password)) << "\",\n";
    ss << "  \"apiUrl\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.apiUrl)) << "\",\n  \"cacheTimeout\": " << s_config.cacheTimeout << ",\n";
    ss << "  \"connTimeout\": " << s_config.connTimeout << ",\n  \"itemsPerPage\": " << s_config.itemsPerPage << ",\n";
    ss << "  \"showForks\": " << (s_config.showForks ? "true" : "false") << ",\n  \"showArchived\": " << (s_config.showArchived ? "true" : "false") << ",\n";
    ss << "  \"showPrivate\": " << (s_config.showPrivate ? "true" : "false") << ",\n  \"showDescription\": " << (s_config.showDescription ? "true" : "false") << ",\n";
    ss << "  \"defaultBranchName\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.defaultBranchName)) << "\",\n  \"proxyType\": " << s_config.proxyType << ",\n";
    ss << "  \"proxyHost\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.proxyHost)) << "\",\n  \"proxyPort\": " << s_config.proxyPort << ",\n";
    ss << "  \"currentUser\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.currentUser)) << "\",\n  \"repoSort\": " << s_config.repoSort << ",\n";
    ss << "  \"commitMessageTemplate\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.commitMessageTemplate)) << "\",\n";
    ss << "  \"autoRefresh\": " << (s_config.autoRefresh ? "true" : "false") << ",\n  \"refreshInterval\": " << s_config.refreshInterval << ",\n";
    ss << "  \"largeFileWarn\": " << (s_config.largeFileWarn ? "true" : "false") << ",\n  \"largeFileSizeMB\": " << s_config.largeFileSizeMB << ",\n";
    ss << "  \"pathMode\": " << s_config.pathMode << ",\n  \"metaPrefix\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.metaPrefix)) << "\",\n";
    ss << "  \"refKeyword\": \"" << GitHubClient::JsonEscape(WideToUtf8(s_config.refKeyword)) << "\",\n";
    ss << "  \"enableAliases\": " << (s_config.enableAliases ? "true" : "false") << ",\n  \"enableOldPaths\": " << (s_config.enableOldPaths ? "true" : "false") << "\n}\n";
    std::string content = ss.str();
    HANDLE hFile = CreateFileW(configPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD bytesWritten; BOOL result = WriteFile(hFile, content.c_str(), (DWORD)content.length(), &bytesWritten, nullptr);
    CloseHandle(hFile); return result != 0;
}

GitHubConfig& GitHubClient::GetConfig() { return s_config; }

std::string GitHubClient::WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::wstring GitHubClient::Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::wstring GitHubClient::UrlEncode(const std::wstring& value) { return Utf8ToWide(UrlEncodeA(WideToUtf8(value))); }

std::string GitHubClient::UrlEncodeA(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') result += c;
        else { char buf[4]; sprintf_s(buf, "%%%02X", (unsigned char)c); result += buf; }
    }
    return result;
}

std::wstring GitHubClient::UrlEncodePathSegments(const std::wstring& path) { return Utf8ToWide(UrlEncodePathSegmentsA(WideToUtf8(path))); }

std::string GitHubClient::UrlEncodePathSegmentsA(const std::string& path) {
    std::string result; size_t start = 0, pos = path.find('/');
    while (pos != std::string::npos) {
        if (pos > start) result += UrlEncodeA(path.substr(start, pos - start));
        result += '/'; start = pos + 1; pos = path.find('/', start);
    }
    if (start < path.length()) result += UrlEncodeA(path.substr(start));
    return result;
}

std::string GitHubClient::JsonEscape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break; case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break; case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break; default: result += c;
        }
    }
    return result;
}

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::wstring GitHubClient::Base64EncodeW(const std::vector<BYTE>& data) { return Utf8ToWide(Base64EncodeA(data)); }

std::string GitHubClient::Base64EncodeA(const std::vector<BYTE>& data) {
    std::string result; int i = 0; unsigned char a3[3], a4[4];
    size_t in_len = data.size(); const BYTE* b = data.data();
    while (in_len--) {
        a3[i++] = *(b++); if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2; a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6); a4[3] = a3[2] & 0x3f;
            for (i = 0; i < 4; i++) result += base64_chars[a4[i]]; i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++) a3[j] = '\0';
        a4[0] = (a3[0] & 0xfc) >> 2; a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++) result += base64_chars[a4[j]];
        while (i++ < 3) result += '=';
    }
    return result;
}

std::vector<BYTE> GitHubClient::Base64DecodeA(const std::string& encoded) {
    static const int t[256] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1 };
    std::vector<BYTE> result; size_t in_len = encoded.size(); int i = 0; unsigned char a4[4], a3[3];
    while (in_len-- && encoded[i] != '=') {
        int v = t[(unsigned char)encoded[i]]; if (v == -1) break;
        a4[i % 4] = v; i++;
        if (i % 4 == 0) {
            a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
            a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
            a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
            for (int j = 0; j < 3; j++) result.push_back(a3[j]);
        }
    }
    if (i % 4) {
        for (int j = i % 4; j < 4; j++) a4[j] = 0;
        a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
        a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
        for (int j = 0; j < (int)(i % 4) - 1; j++) result.push_back(a3[j]);
    }
    return result;
}

bool GitHubClient::EnsureSession() {
    if (!s_hSession) Init(); if (!s_hSession) return false;
    if (!s_hConnect) s_hConnect = WinHttpConnect(s_hSession, s_config.apiUrl.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    return s_hConnect != nullptr;
}

std::wstring GitHubClient::GetAuthHeader() {
    if (s_config.token.empty()) return L"";
    if (s_config.token.find(L"github_pat_") == 0) {
        return L"Authorization: Bearer " + s_config.token;
    }
    return L"Authorization: token " + s_config.token;
}

std::string GitHubClient::SendHttpRequest(const std::wstring& method, const std::wstring& path, const std::string& body, const HttpRequestOpts& opts) {
    if (!EnsureSession()) return "";
    HINTERNET hReq = WinHttpOpenRequest(s_hConnect, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) return "";
    std::wstring hdrs = L"User-Agent: GitHubVFS/1.0\r\n";
    if (opts.addAcceptV3) hdrs += L"Accept: application/vnd.github.v3+json\r\n";
    if (opts.addContentType) hdrs += L"Content-Type: application/json\r\n";
    std::wstring auth = GetAuthHeader(); if (!auth.empty()) hdrs += auth + L"\r\n";
    BOOL ok = WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.length(), opts.hasBody ? (LPVOID)body.c_str() : WINHTTP_NO_REQUEST_DATA, opts.hasBody ? (DWORD)body.length() : 0, opts.hasBody ? (DWORD)body.length() : 0, 0);
    std::string resp;
    if (ok && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        if ((code >= 200 && code < 300) || !opts.lenientErrors) {
            DWORD avail = 0, rd = 0;
            do { WinHttpQueryDataAvailable(hReq, &avail); if (!avail) break; std::vector<char> buf(avail); WinHttpReadData(hReq, buf.data(), avail, &rd); resp.append(buf.data(), rd); } while (avail > 0);
        }
    }
    WinHttpCloseHandle(hReq); return resp;
}

std::string GitHubClient::SendHttpGet(const std::wstring& path) { return SendHttpRequest(L"GET", path, "", HttpRequestOpts()); }
std::string GitHubClient::SendHttpGetImpl(const std::wstring& path) { return SendHttpGet(path); }
std::string GitHubClient::SendHttpDelete(const std::wstring& path) { return SendHttpRequest(L"DELETE", path, "", HttpRequestOpts()); }
std::string GitHubClient::SendHttpPut(const std::wstring& path, const std::string& body) { HttpRequestOpts o; o.hasBody = o.addContentType = true; return SendHttpRequest(L"PUT", path, body, o); }
std::string GitHubClient::SendHttpPatch(const std::wstring& path, const std::string& body) { HttpRequestOpts o; o.hasBody = o.addContentType = true; return SendHttpRequest(L"PATCH", path, body, o); }

std::vector<BYTE> GitHubClient::SendHttpGetBinary(const std::wstring& path) {
    if (!EnsureSession()) return {};
    HINTERNET hReq = WinHttpOpenRequest(s_hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) return {};
    std::wstring hdrs = L"User-Agent: GitHubVFS/1.0\r\n"; std::wstring auth = GetAuthHeader(); if (!auth.empty()) hdrs += auth + L"\r\n";
    std::vector<BYTE> result;
    if (WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        if (code >= 200 && code < 300) {
            DWORD avail = 0, rd = 0;
            do { WinHttpQueryDataAvailable(hReq, &avail); if (!avail) break; size_t off = result.size(); result.resize(off + avail); WinHttpReadData(hReq, result.data() + off, avail, &rd); if (rd < avail) result.resize(off + rd); } while (avail > 0);
        }
    }
    WinHttpCloseHandle(hReq); return result;
}

std::string GitHubClient::SendHttpPost(const std::wstring& host, const std::wstring& path, const std::string& body, const std::wstring& extraHeaders) {
    if (!s_hSession) Init(); if (!s_hSession) return "";
    HINTERNET hConn = WinHttpConnect(s_hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0); if (!hConn) return "";
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); return ""; }
    std::wstring hdrs = L"User-Agent: GitHubVFS/1.0\r\nContent-Type: application/json\r\n"; if (!extraHeaders.empty()) hdrs += extraHeaders + L"\r\n";
    std::string resp;
    if (WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.length(), (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0) && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD avail = 0, rd = 0;
        do { WinHttpQueryDataAvailable(hReq, &avail); if (!avail) break; std::vector<char> buf(avail); WinHttpReadData(hReq, buf.data(), avail, &rd); resp.append(buf.data(), rd); } while (avail > 0);
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); return resp;
}

FILETIME GitHubClient::Iso8601ToFileTime(const std::string& iso) {
    FILETIME ft = {0, 0}; if (iso.empty()) return ft;
    SYSTEMTIME st = {0}; int y, m, d, h, mn, s;
    if (sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &mn, &s) >= 6) {
        st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d; st.wHour = (WORD)h; st.wMinute = (WORD)mn; st.wSecond = (WORD)s;
        SystemTimeToFileTime(&st, &ft);
    }
    return ft;
}

size_t GitHubClient::FindJsonObjectEnd(const std::string& s, size_t start) {
    int d = 0; bool inStr = false;
    for (size_t i = start; i < s.length(); i++) {
        if (s[i] == '"' && (i == 0 || s[i-1] != '\\')) inStr = !inStr;
        else if (!inStr) { if (s[i] == '{') d++; else if (s[i] == '}') { d--; if (d == 0) return i + 1; } }
    }
    return std::string::npos;
}

std::wstring GitHubClient::ParseJsonString(const std::string& json, const std::string& key) {
    std::string sk = "\"" + key + "\""; size_t kp = json.find(sk); if (kp == std::string::npos) return L"";
    size_t cp = json.find(':', kp); if (cp == std::string::npos) return L"";
    size_t vs = json.find_first_not_of(" \t\n\r", cp + 1); if (vs == std::string::npos) return L"";
    if (json[vs] == '"') {
        vs++; std::string v;
        for (size_t i = vs; i < json.length(); i++) {
            if (json[i] == '\\' && i + 1 < json.length()) {
                char n = json[i + 1];
                switch (n) { case '"': v += '"'; break; case '\\': v += '\\'; break; case 'n': v += '\n'; break; case 'r': v += '\r'; break; case 't': v += '\t'; break; default: v += n; }
                i++;
            } else if (json[i] == '"') break; else v += json[i];
        }
        return Utf8ToWide(v);
    }
    return L"";
}

int GitHubClient::ParseJsonInt(const std::string& json, const std::string& key) {
    std::string sk = "\"" + key + "\""; size_t kp = json.find(sk); if (kp == std::string::npos) return 0;
    size_t cp = json.find(':', kp); if (cp == std::string::npos) return 0;
    size_t vs = json.find_first_not_of(" \t\n\r", cp + 1); if (vs == std::string::npos) return 0;
    try { return std::stoi(json.substr(vs)); } catch (...) { return 0; }
}

int64_t GitHubClient::ParseJsonInt64(const std::string& json, const std::string& key) {
    std::string sk = "\"" + key + "\""; size_t kp = json.find(sk); if (kp == std::string::npos) return 0;
    size_t cp = json.find(':', kp); if (cp == std::string::npos) return 0;
    size_t vs = json.find_first_not_of(" \t\n\r", cp + 1); if (vs == std::string::npos) return 0;
    try { return std::stoll(json.substr(vs)); } catch (...) { return 0; }
}

bool GitHubClient::ParseJsonBool(const std::string& json, const std::string& key) {
    std::string sk = "\"" + key + "\""; size_t kp = json.find(sk); if (kp == std::string::npos) return false;
    size_t cp = json.find(':', kp); if (cp == std::string::npos) return false;
    size_t vs = json.find_first_not_of(" \t\n\r", cp + 1); if (vs == std::string::npos) return false;
    return json.substr(vs, 4) == "true";
}

FILETIME GitHubClient::ParseJsonFileTime(const std::string& json, const std::string& key) { return Iso8601ToFileTime(WideToUtf8(ParseJsonString(json, key))); }

void GitHubClient::InvalidateCache() { std::lock_guard<std::mutex> lk(s_cacheMutex); s_dirCache.clear(); s_repoCache.clear(); s_searchCache.clear(); }
void GitHubClient::ResetSession() { std::lock_guard<std::mutex> lk(s_sessionMutex); if (s_hConnect) { WinHttpCloseHandle(s_hConnect); s_hConnect = nullptr; } s_config.currentUser.clear(); InvalidateCache(); }
bool GitHubClient::TestConnection(std::wstring& outUser) { std::string r = SendHttpGet(L"/user"); if (r.empty()) return false; outUser = ParseJsonString(r, "login"); return !outUser.empty(); }
bool GitHubClient::StartOAuthDeviceFlow(std::wstring& outDC, std::wstring& outUC, std::wstring& outVU) { return false; }
bool GitHubClient::PollOAuthToken(const std::wstring& dc, std::wstring& outTok) { return false; }

static GitHubRepoInfo ParseRepoInfo(const std::string& j) {
    GitHubRepoInfo i;
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.fullName = GitHubClient::ParseJsonString(j, "full_name");
    i.description = GitHubClient::ParseJsonString(j, "description");
    i.language = GitHubClient::ParseJsonString(j, "language");
    i.defaultBranch = GitHubClient::ParseJsonString(j, "default_branch");
    i.cloneUrl = GitHubClient::ParseJsonString(j, "clone_url");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    i.isPrivate = GitHubClient::ParseJsonBool(j, "private");
    i.isFork = GitHubClient::ParseJsonBool(j, "fork");
    i.isArchived = GitHubClient::ParseJsonBool(j, "archived");
    i.stargazersCount = GitHubClient::ParseJsonInt64(j, "stargazers_count");
    i.forksCount = GitHubClient::ParseJsonInt64(j, "forks_count");
    i.openIssuesCount = GitHubClient::ParseJsonInt64(j, "open_issues_count");
    i.size = GitHubClient::ParseJsonInt64(j, "size");
    i.updatedAt = GitHubClient::ParseJsonFileTime(j, "updated_at");
    i.createdAt = GitHubClient::ParseJsonFileTime(j, "created_at");
    i.pushedAt = GitHubClient::ParseJsonFileTime(j, "pushed_at");
    return i;
}

std::vector<GitHubRepoInfo> GitHubClient::ListUserRepos(int page) {
    std::wstring p = L"/user/repos?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page) + L"&sort=updated";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubRepoInfo>(r, ParseRepoInfo);
}

std::vector<GitHubRepoInfo> GitHubClient::ListUserRepos(const std::wstring& owner, int page) {
    std::wstring p = L"/users/" + UrlEncodePathSegments(owner) + L"/repos?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page) + L"&sort=updated";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubRepoInfo>(r, ParseRepoInfo);
}

std::vector<GitHubRepoInfo> GitHubClient::ListOrgRepos(const std::wstring& org, int page) {
    std::wstring p = L"/orgs/" + UrlEncodePathSegments(org) + L"/repos?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page) + L"&sort=updated";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubRepoInfo>(r, ParseRepoInfo);
}

bool GitHubClient::GetRepoInfo(const std::wstring& owner, const std::wstring& repo, GitHubRepoInfo& out) {
    std::wstring ck = L"repo:" + owner + L"/" + repo;
    { std::lock_guard<std::mutex> lk(s_cacheMutex); auto it = s_repoCache.find(ck); if (it != s_repoCache.end()) { ULONGLONG now = GetTickCount64(); if (now - it->second.timestamp < (ULONGLONG)s_config.cacheTimeout * 1000) { out = it->second.info; return true; } } }
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo);
    std::string r = SendHttpGet(p); if (r.empty()) return false;
    out = ParseRepoInfo(r);
    { std::lock_guard<std::mutex> lk(s_cacheMutex); RepoCacheEntry e; e.info = out; e.timestamp = GetTickCount64(); s_repoCache[ck] = e; }
    return true;
}

GitHubSearchResult GitHubClient::SearchRepos(const std::wstring& q, int page, const std::wstring& sort) {
    std::wstring p = L"/search/repositories?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page) + L"&sort=" + sort;
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.repos = ParseJsonArrayAfterKey<GitHubRepoInfo>(r, "items", ParseRepoInfo); return res;
}

GitHubSearchResult GitHubClient::SearchUserRepos(const std::wstring& user, int page) { return SearchRepos(L"user:" + user, page, L"updated"); }
GitHubSearchResult GitHubClient::SearchOrgRepos(const std::wstring& org, int page) { return SearchRepos(L"org:" + org, page, L"updated"); }

static GitHubCodeItem ParseCodeItem(const std::string& j) {
    GitHubCodeItem i;
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.path = GitHubClient::ParseJsonString(j, "path");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    std::string rj = j.substr(j.find("\"repository\":"));
    i.owner = GitHubClient::ParseJsonString(rj, "owner");
    i.repo = GitHubClient::ParseJsonString(rj, "name");
    i.repoFullName = GitHubClient::ParseJsonString(rj, "full_name");
    i.repoHtmlUrl = GitHubClient::ParseJsonString(rj, "html_url");
    i.repoLanguage = GitHubClient::ParseJsonString(rj, "language");
    i.repoStars = GitHubClient::ParseJsonInt(rj, "stargazers_count");
    return i;
}

GitHubCodeSearchResult GitHubClient::SearchCode(const std::wstring& q, int page) {
    std::wstring p = L"/search/code?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubCodeSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.items = ParseJsonArrayAfterKey<GitHubCodeItem>(r, "items", ParseCodeItem); return res;
}

static GitHubUserInfo ParseUserInfo(const std::string& j) {
    GitHubUserInfo i;
    i.login = GitHubClient::ParseJsonString(j, "login");
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.avatarUrl = GitHubClient::ParseJsonString(j, "avatar_url");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    i.bio = GitHubClient::ParseJsonString(j, "bio");
    i.company = GitHubClient::ParseJsonString(j, "company");
    i.location = GitHubClient::ParseJsonString(j, "location");
    i.email = GitHubClient::ParseJsonString(j, "email");
    i.blog = GitHubClient::ParseJsonString(j, "blog");
    i.publicRepos = GitHubClient::ParseJsonInt(j, "public_repos");
    i.followers = GitHubClient::ParseJsonInt(j, "followers");
    i.following = GitHubClient::ParseJsonInt(j, "following");
    return i;
}

GitHubUserSearchResult GitHubClient::SearchUsers(const std::wstring& q, int page) {
    std::wstring p = L"/search/users?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubUserSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.users = ParseJsonArrayAfterKey<GitHubUserInfo>(r, "items", ParseUserInfo); return res;
}

static GitHubIssueInfo ParseIssueInfo(const std::string& j) {
    GitHubIssueInfo i;
    i.number = GitHubClient::ParseJsonInt(j, "number");
    i.title = GitHubClient::ParseJsonString(j, "title");
    i.body = GitHubClient::ParseJsonString(j, "body");
    i.state = GitHubClient::ParseJsonString(j, "state");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    i.comments = GitHubClient::ParseJsonInt(j, "comments");
    i.createdAt = GitHubClient::ParseJsonFileTime(j, "created_at");
    i.updatedAt = GitHubClient::ParseJsonFileTime(j, "updated_at");
    i.closedAt = GitHubClient::ParseJsonFileTime(j, "closed_at");
    i.isPullRequest = j.find("\"pull_request\"") != std::string::npos;
    std::string uj = j.substr(j.find("\"user\":"));
    i.author = GitHubClient::ParseJsonString(uj, "login");
    return i;
}

GitHubIssueSearchResult GitHubClient::SearchIssues(const std::wstring& q, int page) {
    std::wstring p = L"/search/issues?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubIssueSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.items = ParseJsonArrayAfterKey<GitHubIssueInfo>(r, "items", ParseIssueInfo); return res;
}

static GitHubCommitSearchItem ParseCommitItem(const std::string& j) {
    GitHubCommitSearchItem i;
    i.sha = GitHubClient::ParseJsonString(j, "sha");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    std::string cj = j.substr(j.find("\"commit\":"));
    i.message = GitHubClient::ParseJsonString(cj, "message");
    std::string aj = cj.substr(cj.find("\"author\":"));
    i.author = GitHubClient::ParseJsonString(aj, "name");
    i.date = GitHubClient::ParseJsonString(aj, "date");
    std::string rj = j.substr(j.find("\"repository\":"));
    i.owner = GitHubClient::ParseJsonString(rj, "owner");
    i.repo = GitHubClient::ParseJsonString(rj, "name");
    return i;
}

GitHubCommitSearchResult GitHubClient::SearchCommits(const std::wstring& q, int page) {
    std::wstring p = L"/search/commits?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubCommitSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.items = ParseJsonArrayAfterKey<GitHubCommitSearchItem>(r, "items", ParseCommitItem); return res;
}

static GitHubTopicInfo ParseTopicInfo(const std::string& j) {
    GitHubTopicInfo i;
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.description = GitHubClient::ParseJsonString(j, "description");
    i.repoCount = GitHubClient::ParseJsonInt(j, "curated") ? GitHubClient::ParseJsonInt(j, "featured") : GitHubClient::ParseJsonInt(j, "score");
    return i;
}

GitHubTopicSearchResult GitHubClient::SearchTopics(const std::wstring& q, int page) {
    std::wstring p = L"/search/topics?q=" + UrlEncode(q) + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    GitHubTopicSearchResult res; res.totalCount = ParseJsonInt(r, "total_count"); res.incomplete = ParseJsonBool(r, "incomplete_results");
    res.items = ParseJsonArrayAfterKey<GitHubTopicInfo>(r, "items", ParseTopicInfo); return res;
}

std::vector<GitHubIssueInfo> GitHubClient::ListIssues(const std::wstring& owner, const std::wstring& repo, const std::wstring& state, int page) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/issues?state=" + state + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubIssueInfo>(r, ParseIssueInfo);
}

bool GitHubClient::GetIssue(const std::wstring& owner, const std::wstring& repo, int num, GitHubIssueInfo& out) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/issues/" + std::to_wstring(num);
    std::string r = SendHttpGet(p); if (r.empty()) return false;
    out = ParseIssueInfo(r); out.owner = owner; out.repo = repo; return true;
}

bool GitHubClient::CreateIssue(const std::wstring& owner, const std::wstring& repo, const std::wstring& title, const std::wstring& body, int& outNum) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/issues";
    std::string jb = R"({"title":")" + JsonEscape(WideToUtf8(title)) + R"(","body":")" + JsonEscape(WideToUtf8(body)) + R"("})";
    HttpRequestOpts o; o.hasBody = o.addContentType = true;
    std::string r = SendHttpRequest(L"POST", p, jb, o); if (r.empty()) return false;
    outNum = ParseJsonInt(r, "number"); return outNum > 0;
}

bool GitHubClient::UpdateIssue(const std::wstring& owner, const std::wstring& repo, int num, const std::wstring& title, const std::wstring& body, const std::wstring& state) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/issues/" + std::to_wstring(num);
    std::string jb = R"({"title":")" + JsonEscape(WideToUtf8(title)) + R"(","body":")" + JsonEscape(WideToUtf8(body)) + R"(","state":")" + WideToUtf8(state) + R"("})";
    std::string r = SendHttpPatch(p, jb); return !r.empty();
}

bool GitHubClient::CloseIssue(const std::wstring& owner, const std::wstring& repo, int num) { return UpdateIssue(owner, repo, num, L"", L"", L"closed"); }

std::vector<GitHubIssueInfo> GitHubClient::ListPullRequests(const std::wstring& owner, const std::wstring& repo, const std::wstring& state, int page) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/pulls?state=" + state + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    std::vector<GitHubIssueInfo> res; size_t pos = 0;
    while ((pos = r.find('{', pos)) != std::string::npos) {
        size_t end = FindJsonObjectEnd(r, pos); if (end == std::string::npos) break;
        GitHubIssueInfo i = ParseIssueInfo(r.substr(pos, end - pos)); i.isPullRequest = true; i.owner = owner; i.repo = repo;
        res.push_back(i); pos = end;
    }
    return res;
}

bool GitHubClient::GetPullRequest(const std::wstring& owner, const std::wstring& repo, int num, GitHubIssueInfo& out) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/pulls/" + std::to_wstring(num);
    std::string r = SendHttpGet(p); if (r.empty()) return false;
    out = ParseIssueInfo(r); out.isPullRequest = true; out.owner = owner; out.repo = repo; return true;
}

static GitHubBranchInfo ParseBranchInfo(const std::string& j) {
    GitHubBranchInfo i;
    i.name = GitHubClient::ParseJsonString(j, "name");
    std::string cj = j.substr(j.find("\"commit\":"));
    i.sha = GitHubClient::ParseJsonString(cj, "sha");
    i.isDefault = false;
    return i;
}

std::vector<GitHubBranchInfo> GitHubClient::ListBranches(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/branches?per_page=100";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    auto br = ParseJsonArray<GitHubBranchInfo>(r, ParseBranchInfo);
    GitHubRepoInfo ri; if (GetRepoInfo(owner, repo, ri)) { for (auto& b : br) if (b.name == ri.defaultBranch) { b.isDefault = true; break; } }
    return br;
}

std::vector<GitHubBranchInfo> GitHubClient::ListTags(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/tags?per_page=100";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    std::vector<GitHubBranchInfo> tags; size_t pos = 0;
    while ((pos = r.find('{', pos)) != std::string::npos) {
        size_t end = FindJsonObjectEnd(r, pos); if (end == std::string::npos) break;
        GitHubBranchInfo i; std::string obj = r.substr(pos, end - pos);
        i.name = ParseJsonString(obj, "name"); i.sha = ParseJsonString(obj, "commit"); i.isDefault = false;
        tags.push_back(i); pos = end;
    }
    return tags;
}

static GitHubReleaseInfo ParseReleaseInfo(const std::string& j) {
    GitHubReleaseInfo i;
    i.tagName = GitHubClient::ParseJsonString(j, "tag_name");
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.body = GitHubClient::ParseJsonString(j, "body");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    i.tarballUrl = GitHubClient::ParseJsonString(j, "tarball_url");
    i.zipballUrl = GitHubClient::ParseJsonString(j, "zipball_url");
    i.isPrerelease = GitHubClient::ParseJsonBool(j, "prerelease");
    i.isDraft = GitHubClient::ParseJsonBool(j, "draft");
    i.publishedAt = GitHubClient::ParseJsonFileTime(j, "published_at");
    i.createdAt = GitHubClient::ParseJsonFileTime(j, "created_at");
    return i;
}

std::vector<GitHubReleaseInfo> GitHubClient::ListReleases(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/releases?per_page=100";
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubReleaseInfo>(r, ParseReleaseInfo);
}

bool GitHubClient::StarRepo(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/user/starred/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo);
    HttpRequestOpts o; o.addContentType = false; SendHttpRequest(L"PUT", p, "", o); return true;
}

bool GitHubClient::UnstarRepo(const std::wstring& owner, const std::wstring& repo) {
    SendHttpDelete(L"/user/starred/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo)); return true;
}

bool GitHubClient::IsStarred(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/user/starred/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo);
    HttpRequestOpts o; o.lenientErrors = true; std::string r = SendHttpRequest(L"GET", p, "", o); return !r.empty();
}

bool GitHubClient::WatchRepo(const std::wstring& owner, const std::wstring& repo) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/subscription";
    HttpRequestOpts o; o.hasBody = o.addContentType = true; SendHttpRequest(L"PUT", p, R"({"subscribed":true})", o); return true;
}

bool GitHubClient::UnwatchRepo(const std::wstring& owner, const std::wstring& repo) {
    SendHttpDelete(L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/subscription"); return true;
}

std::vector<GitHubRepoInfo> GitHubClient::ListStarredRepos(int page) {
    std::wstring p = L"/user/starred?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubRepoInfo>(r, ParseRepoInfo);
}

std::vector<GitHubRepoInfo> GitHubClient::ListWatchedRepos(int page) {
    std::wstring p = L"/user/subscriptions?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubRepoInfo>(r, ParseRepoInfo);
}

static GitHubNotificationInfo ParseNotificationInfo(const std::string& j) {
    GitHubNotificationInfo i;
    i.id = GitHubClient::ParseJsonInt(j, "id");
    i.reason = GitHubClient::ParseJsonString(j, "reason");
    i.unread = GitHubClient::ParseJsonBool(j, "unread");
    i.updatedAt = GitHubClient::ParseJsonString(j, "updated_at");
    std::string sj = j.substr(j.find("\"subject\":"));
    i.subjectType = GitHubClient::ParseJsonString(sj, "type");
    i.subjectTitle = GitHubClient::ParseJsonString(sj, "title");
    std::wstring url = GitHubClient::ParseJsonString(sj, "url");
    size_t ls = url.rfind(L'/'); if (ls != std::wstring::npos) { try { i.subjectNumber = std::stoi(url.substr(ls + 1)); } catch (...) {} }
    std::string rj = j.substr(j.find("\"repository\":"));
    std::wstring fn = GitHubClient::ParseJsonString(rj, "full_name");
    size_t sp = fn.find(L'/'); if (sp != std::wstring::npos) { i.owner = fn.substr(0, sp); i.repo = fn.substr(sp + 1); }
    return i;
}

std::vector<GitHubNotificationInfo> GitHubClient::ListNotifications(bool all, int page) {
    std::wstring p = L"/notifications?all=" + std::wstring(all ? L"true" : L"false") + L"&per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubNotificationInfo>(r, ParseNotificationInfo);
}

bool GitHubClient::MarkNotificationsRead() {
    HttpRequestOpts o; o.hasBody = o.addContentType = o.handle204AsSpace = true;
    std::string r = SendHttpRequest(L"PUT", L"/notifications", "{}", o); return !r.empty();
}

static GitHubGistInfo ParseGistInfo(const std::string& j) {
    GitHubGistInfo i;
    i.id = GitHubClient::ParseJsonString(j, "id");
    i.description = GitHubClient::ParseJsonString(j, "description");
    i.isPublic = GitHubClient::ParseJsonBool(j, "public");
    i.updatedAt = GitHubClient::ParseJsonString(j, "updated_at");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    size_t fp = j.find("\"files\":"); if (fp != std::string::npos) {
        size_t fs = j.find('{', fp); if (fs != std::string::npos) {
            int d = 0;
            for (size_t k = fs; k < j.length(); k++) {
                if (j[k] == '{') d++; else if (j[k] == '}') { d--; if (d == 0) break; }
                else if (j[k] == '"' && d == 1) {
                    size_t ne = j.find('"', k + 1); if (ne != std::string::npos) {
                        i.filenames.push_back(GitHubClient::Utf8ToWide(j.substr(k + 1, ne - k - 1))); k = ne;
                    }
                }
            }
        }
    }
    i.fileCount = (int)i.filenames.size();
    return i;
}

std::vector<GitHubGistInfo> GitHubClient::ListGists(const std::wstring& user, int page) {
    std::wstring p = user.empty() ? L"/gists?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page)
        : L"/users/" + UrlEncodePathSegments(user) + L"/gists?per_page=" + std::to_wstring(s_config.itemsPerPage) + L"&page=" + std::to_wstring(page);
    std::string r = SendHttpGet(p); if (r.empty()) return {};
    return ParseJsonArray<GitHubGistInfo>(r, ParseGistInfo);
}

bool GitHubClient::DownloadArchive(const std::wstring& owner, const std::wstring& repo, const std::wstring& ref, const std::wstring& fmt, std::vector<BYTE>& out) {
    std::wstring p = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/" + fmt + L"/" + UrlEncodePathSegments(ref);
    out = SendHttpGetBinary(p); return !out.empty();
}

static GitHubFileInfo ParseFileInfo(const std::string& j) {
    GitHubFileInfo i;
    i.name = GitHubClient::ParseJsonString(j, "name");
    i.path = GitHubClient::ParseJsonString(j, "path");
    i.sha = GitHubClient::ParseJsonString(j, "sha");
    i.downloadUrl = GitHubClient::ParseJsonString(j, "download_url");
    i.htmlUrl = GitHubClient::ParseJsonString(j, "html_url");
    i.isDir = GitHubClient::ParseJsonString(j, "type") == L"dir";
    i.size = GitHubClient::ParseJsonInt64(j, "size");
    return i;
}

std::vector<GitHubFileInfo> GitHubClient::ListDirectory(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, const std::wstring& ref) {
    std::wstring ck = L"dir:" + owner + L"/" + repo + L":" + path + L"@" + ref;
    { std::lock_guard<std::mutex> lk(s_cacheMutex); auto it = s_dirCache.find(ck); if (it != s_dirCache.end()) { ULONGLONG now = GetTickCount64(); if (now - it->second.timestamp < (ULONGLONG)s_config.cacheTimeout * 1000) return it->second.files; } }
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path);
    if (!ref.empty()) ap += L"?ref=" + UrlEncodePathSegments(ref);
    std::string r = SendHttpGet(ap); if (r.empty()) return {};
    std::vector<GitHubFileInfo> files = ParseJsonArray<GitHubFileInfo>(r, ParseFileInfo);
    { std::lock_guard<std::mutex> lk(s_cacheMutex); DirCacheEntry e; e.files = files; e.timestamp = GetTickCount64(); s_dirCache[ck] = e; }
    return files;
}

bool GitHubClient::GetFileInfo(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, GitHubFileInfo& out, const std::wstring& ref) {
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path);
    if (!ref.empty()) ap += L"?ref=" + UrlEncodePathSegments(ref);
    std::string r = SendHttpGet(ap); if (r.empty()) return false;
    out = ParseFileInfo(r); return true;
}

bool GitHubClient::DownloadFile(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, std::vector<BYTE>& out, const std::wstring& ref) {
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path);
    if (!ref.empty()) ap += L"?ref=" + UrlEncodePathSegments(ref);
    std::string r = SendHttpGet(ap); if (r.empty()) return false;
    std::string content = WideToUtf8(ParseJsonString(r, "content"));
    std::string enc = WideToUtf8(ParseJsonString(r, "encoding"));
    if (enc == "base64") {
        std::string cleaned; for (char c : content) if (c != '\n' && c != '\r' && c != ' ') cleaned += c;
        out = Base64DecodeA(cleaned); return !out.empty();
    }
    return false;
}

bool GitHubClient::DownloadFileByUrl(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, std::vector<BYTE>& out, const std::wstring& ref) {
    GitHubFileInfo fi; if (!GetFileInfo(owner, repo, path, fi, ref)) return false;
    if (fi.downloadUrl.empty()) return false;
    std::wstring url = fi.downloadUrl;
    std::wstring host = url.substr(0, url.find(L'/', 8));
    std::wstring pth = url.substr(url.find(L'/', 8));
    if (!s_hSession) Init(); if (!s_hSession) return false;
    HINTERNET hConn = WinHttpConnect(s_hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0); if (!hConn) return false;
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", pth.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); return false; }
    std::wstring hdrs = L"User-Agent: GitHubVFS/1.0\r\n";
    if (WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)hdrs.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD avail = 0, rd = 0;
        do { WinHttpQueryDataAvailable(hReq, &avail); if (!avail) break; size_t off = out.size(); out.resize(off + avail); WinHttpReadData(hReq, out.data() + off, avail, &rd); if (rd < avail) out.resize(off + rd); } while (avail > 0);
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); return !out.empty();
}

bool GitHubClient::UploadFile(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, const std::vector<BYTE>& data, const std::wstring& msg, const std::wstring& branch) {
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path);
    std::string content = Base64EncodeA(data);
    std::string jb = R"({"message":")" + JsonEscape(WideToUtf8(msg)) + R"(","content":")" + content + R"(")";
    if (!branch.empty()) jb += R"(,"branch":")" + WideToUtf8(branch) + R"(")";
    jb += "}";
    HttpRequestOpts o; o.hasBody = o.addContentType = true;
    std::string r = SendHttpRequest(L"PUT", ap, jb, o); return !r.empty();
}

bool GitHubClient::DeleteGitHubFile(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, const std::wstring& msg, const std::wstring& sha) {
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path);
    std::string jb = R"({"message":")" + JsonEscape(WideToUtf8(msg)) + R"(","sha":")" + WideToUtf8(sha) + R"("})";
    HttpRequestOpts o; o.hasBody = o.addContentType = true;
    std::string r = SendHttpRequest(L"DELETE", ap, jb, o); return !r.empty();
}

bool GitHubClient::CreateGitHubDir(const std::wstring& owner, const std::wstring& repo, const std::wstring& path, const std::wstring& msg, const std::wstring& branch) {
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(path) + L"/.gitkeep";
    // .gitkeep 文件内容为空字节，base64 编码为 ""
    // GitHub API 要求 content 字段非空，空字节的 base64 是 ""
    std::vector<BYTE> emptyContent;
    std::string contentB64 = Base64EncodeA(emptyContent);
    if (contentB64.empty()) contentB64 = "";  // 确保至少有空字符串
    std::string jb = R"({"message":")" + JsonEscape(WideToUtf8(msg)) + R"(","content":")" + contentB64 + R"(")";
    if (!branch.empty()) jb += R"(,"branch":")" + WideToUtf8(branch) + R"(")";
    jb += "}";
    HttpRequestOpts o; o.hasBody = o.addContentType = true;
    std::string r = SendHttpRequest(L"PUT", ap, jb, o); return !r.empty();
}

bool GitHubClient::MoveGitHubFile(const std::wstring& owner, const std::wstring& repo, const std::wstring& oldPath, const std::wstring& newPath, const std::wstring& msg, const std::wstring& sha, const std::wstring& branch) {
    GitHubFileInfo fi; if (!GetFileInfo(owner, repo, oldPath, fi, L"")) return false;
    // 下载旧文件时指定分支，确保下载正确版本
    std::wstring downloadPath = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(oldPath);
    if (!branch.empty()) downloadPath += L"?ref=" + UrlEncode(branch);
    std::string content = Base64EncodeA(SendHttpGetBinary(downloadPath));
    std::string jb = R"({"message":")" + JsonEscape(WideToUtf8(msg)) + R"(","content":")" + content + R"(","sha":")" + WideToUtf8(fi.sha) + R"(")";
    if (!branch.empty()) jb += R"(,"branch":")" + WideToUtf8(branch) + R"(")";
    jb += "}";
    std::wstring ap = L"/repos/" + UrlEncodePathSegments(owner) + L"/" + UrlEncodePathSegments(repo) + L"/contents/" + UrlEncodePathSegments(newPath);
    HttpRequestOpts o; o.hasBody = o.addContentType = true;
    std::string r = SendHttpRequest(L"PUT", ap, jb, o);
    if (r.empty()) return false;
    return DeleteGitHubFile(owner, repo, oldPath, msg, fi.sha);
}

std::wstring GitHubClient::GetDefaultBranch(const std::wstring& owner, const std::wstring& repo) {
    GitHubRepoInfo i; if (GetRepoInfo(owner, repo, i)) return i.defaultBranch;
    return s_config.defaultBranchName.empty() ? L"main" : s_config.defaultBranchName;
}
