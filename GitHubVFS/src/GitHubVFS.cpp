#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <type_traits>
#include <fstream>
#include <sstream>
#include <ctime>

#include "vfs_plugins.h"
#include "plugin_support.h"
#include "GitHubClient.h"
#include "resource.h"
#include <exception>
#include <functional>

enum LogLevel { LOG_ERROR = 0, LOG_INFO = 1, LOG_DEBUG = 2 };

static LogLevel g_logLevel = LOG_INFO;

static void LogToFile(const WCHAR* message);
static void LogToFile(const std::wstring& message);

static void LogToFileEx(LogLevel level, const std::wstring& message) {
    if (level > g_logLevel) return;
    LogToFile(message);
}

static void LogToFileEx(LogLevel level, const WCHAR* message) {
    if (level > g_logLevel) return;
    LogToFile(message);
}

static void LogToFile(const std::wstring& message) {
    LogToFile(message.c_str());
}

static void LogToFile(const WCHAR* message) {
    if (!message) return;
    WCHAR appDataPath[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", appDataPath, MAX_PATH) == 0) return;

    WCHAR logDir[MAX_PATH * 2];
    StringCchPrintfW(logDir, MAX_PATH * 2, L"%s\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\GithubVFS", appDataPath);
    CreateDirectoryW(logDir, NULL);

    WCHAR logFile[MAX_PATH * 2];
    StringCchPrintfW(logFile, MAX_PATH * 2, L"%s\\debug.log", logDir);

    HANDLE hFile = CreateFileW(logFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    if (GetFileSize(hFile, NULL) == 0) {
        DWORD written;
        BYTE bom[] = { 0xFF, 0xFE };
        WriteFile(hFile, bom, 2, &written, NULL);
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    // Build line dynamically to avoid truncation for long messages
    WCHAR prefix[64];
    int prefixLen = swprintf_s(prefix, 64, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
                         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    size_t msgLen = wcslen(message);
    // Limit individual log lines to 4096 chars to prevent runaway logs
    size_t maxMsgLen = 4096 - prefixLen - 2; // -2 for \r\n
    if (msgLen > maxMsgLen) msgLen = maxMsgLen;

    std::wstring line;
    line.reserve(prefixLen + msgLen + 2);
    line.append(prefix, prefixLen);
    line.append(message, msgLen);
    line += L"\r\n";

    DWORD written2;
    WriteFile(hFile, line.c_str(), (DWORD)(line.length() * sizeof(WCHAR)), &written2, NULL);
    CloseHandle(hFile);
}

#define VFS_TRY try {
#define VFS_CATCH } catch (const std::exception& e) { \
    OutputDebugStringA("[GitHubVFS] std::exception: "); \
    OutputDebugStringA(e.what()); \
    OutputDebugStringA("\n"); \
    LogToFile(L"std::exception: " + Utf8ToWide_safe(e.what())); \
} catch (...) { \
    OutputDebugStringW(L"[GitHubVFS] unknown exception\n"); \
    LogToFile(L"unknown exception"); \
}

static std::wstring Utf8ToWide_safe(const char* str) {
    if (!str || !*str) return L"";
    try {
        return GitHubClient::Utf8ToWide(std::string(str));
    } catch (...) {
        return L"";
    }
}

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Advapi32.lib")

static const GUID GUIDPlugin_GitHub =
{ 0xF1A2B3C4, 0xD5E6, 0x4F7A, { 0x8B, 0x9C, 0x0D, 0x1E, 0x2F, 0x3A, 0x4B, 0x5C } };

#define GITHUB_VFS_PREFIX L"github://"
#define GITHUB_VFS_PREFIX_LEN 9

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

static bool IsGitHubVfsPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    return _wcsnicmp(pszPath, GITHUB_VFS_PREFIX, GITHUB_VFS_PREFIX_LEN) == 0;
}

static bool IsGitHubRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, GITHUB_VFS_PREFIX) == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    return _wcsicmp(s.c_str(), L"github:") == 0;
}

struct GitHubPathInfo {
    // 新字段（重构后使用）
    std::wstring repo;
    std::wstring path;
    GitHubContext context;
    int issueNumber;
    bool isDir;

    struct Query {
        std::wstring view;
        std::wstring ref;
        std::wstring search;
        std::wstring query;
        std::wstring owner;
        std::wstring state;
        std::wstring since;
        std::wstring lang;
        std::wstring from;
        int page;

        Query() : page(1) {}
    } query;

    std::wstring resolvedOwner;

    // 旧字段（兼容层，逐步删除）
    std::wstring owner;
    std::wstring searchQuery;
    std::wstring searchType;
    std::wstring ownerFilter;
    std::wstring ref;
    GitHubRefType refType;
    int page;
    std::wstring trendingSince;
    std::wstring trendingLang;
    std::wstring metaItem;

    GitHubPathInfo() {
        issueNumber = 0;
        page = 1;
        context = GITHUB_CTX_ROOT;
        isDir = false;
        refType = GITHUB_REF_DEFAULT;
    }
};

class CachedRepoList {
public:
    std::vector<GitHubRepoInfo> Get(std::function<std::vector<GitHubRepoInfo>()> loader) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ULONGLONG now = GetTickCount64();
            GitHubConfig& cfg = GitHubClient::GetConfig();
            ULONGLONG cacheTimeout = (ULONGLONG)cfg.cacheTimeout * 1000;
            if (!m_data.empty() && (now - m_cacheTime) < cacheTimeout) {
                return m_data;
            }
            if (m_loading) {
                return m_data;
            }
            m_loading = true;
        }

        std::vector<GitHubRepoInfo> repos;
        bool loadSuccess = false;
        try { repos = loader(); loadSuccess = true; } catch (...) { repos.clear(); }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (loadSuccess) {
                m_data = repos;
                m_cacheTime = GetTickCount64();
            }
            m_loading = false;
        }
        return repos;
    }

    std::vector<GitHubRepoInfo> GetData() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data;
    }

    void SetLoading(bool loading) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loading = loading;
    }

    void SetData(const std::vector<GitHubRepoInfo>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data = data;
        m_cacheTime = GetTickCount64();
        m_loading = false;
    }

private:
    std::vector<GitHubRepoInfo> m_data;
    ULONGLONG m_cacheTime = 0;
    bool m_loading = false;
    mutable std::mutex m_mutex;
};

static CachedRepoList g_cachedRepos;
static CachedRepoList g_cachedStarred;
static CachedRepoList g_cachedWatched;

struct SearchHistoryEntry {
    std::wstring type;
    std::wstring query;
    time_t timestamp;
};

static std::vector<SearchHistoryEntry> g_searchHistory;
static std::mutex g_searchHistoryMutex;

static void AddSearchHistory(const std::wstring& type, const std::wstring& query) {
    std::lock_guard<std::mutex> lock(g_searchHistoryMutex);
    
    SearchHistoryEntry entry;
    entry.type = type;
    entry.query = query;
    entry.timestamp = time(nullptr);
    
    for (auto it = g_searchHistory.begin(); it != g_searchHistory.end(); ++it) {
        if (it->type == type && it->query == query) {
            g_searchHistory.erase(it);
            break;
        }
    }
    
    g_searchHistory.insert(g_searchHistory.begin(), entry);
    
    if (g_searchHistory.size() > 5) {
        g_searchHistory.resize(5);
    }
}

static std::vector<SearchHistoryEntry> GetSearchHistory() {
    std::lock_guard<std::mutex> lock(g_searchHistoryMutex);
    return g_searchHistory;
}

static std::vector<GitHubCodeItem> g_cachedCodeResults;
static std::mutex g_codeResultsMutex;

static HANDLE g_reposLoadThread = NULL;

static DWORD WINAPI PreloadReposThread(LPVOID lpParam) {
    g_cachedRepos.SetLoading(true);

    std::vector<GitHubRepoInfo> repos;
    try { repos = GitHubClient::ListUserRepos(); } catch (...) { repos.clear(); }

    g_cachedRepos.SetData(repos);
    return 0;
}

static void StartPreloadRepos() {
    if (g_reposLoadThread) {
        if (WaitForSingleObject(g_reposLoadThread, 0) == WAIT_TIMEOUT) {
            return;
        }
        CloseHandle(g_reposLoadThread);
        g_reposLoadThread = NULL;
    }
    HANDLE hThread = CreateThread(NULL, 0, PreloadReposThread, NULL, 0, NULL);
    if (hThread) {
        g_reposLoadThread = hThread;
    }
}

static std::vector<GitHubRepoInfo> GetCachedRepos() {
    return g_cachedRepos.Get([]() { return GitHubClient::ListUserRepos(); });
}

static std::vector<GitHubRepoInfo> GetCachedStarredRepos() {
    return g_cachedStarred.Get([]() { return GitHubClient::ListStarredRepos(); });
}

static std::vector<GitHubRepoInfo> GetCachedWatchedRepos() {
    return g_cachedWatched.Get([]() { return GitHubClient::ListWatchedRepos(); });
}

static std::wstring GetDefaultOwner(const std::wstring& repoName) {
    std::vector<GitHubRepoInfo> repos = GetCachedRepos();
    for (const auto& r : repos) {
        if (r.name == repoName) {
            size_t pos = r.fullName.find(L'/');
            if (pos != std::wstring::npos) {
                return r.fullName.substr(0, pos);
            }
        }
    }
    GitHubConfig& cfg = GitHubClient::GetConfig();
    return cfg.currentUser;
}

static void CopyToClipboard(const std::wstring& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        size_t size = (text.length() + 1) * sizeof(WCHAR);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem) {
            LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
            if (pMem) {
                StringCchCopyW(pMem, text.length() + 1, text.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            } else {
                GlobalFree(hMem);
            }
        }
        CloseClipboard();
    }
}

static HWND g_hInputBoxParent = NULL;
static LPWSTR g_pInputBoxResult = NULL;
static int g_cInputBoxMax = 0;
static LPCWSTR g_pInputBoxPrompt = NULL;

static INT_PTR CALLBACK InputBoxDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        SetWindowTextW(GetDlgItem(hDlg, 1001), g_pInputBoxPrompt);
        SetFocus(GetDlgItem(hDlg, 1002));
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            GetDlgItemTextW(hDlg, 1002, g_pInputBoxResult, g_cInputBoxMax);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static bool InputBox(HWND hParent, LPCWSTR title, LPCWSTR prompt, LPWSTR result, int maxLen) {
    g_hInputBoxParent = hParent;
    g_pInputBoxResult = result;
    g_cInputBoxMax = maxLen;
    g_pInputBoxPrompt = prompt;

    result[0] = L'\0';

    HRSRC hRes = FindResourceW(g_hModule, MAKEINTRESOURCEW(402), RT_DIALOG);
    if (!hRes) {
        WCHAR buf[256] = {0};
        if (GetWindowTextW(hParent, buf, 256) == 0) {
            StringCchCopyW(result, maxLen, L"react");
            return true;
        }
        return false;
    }

    HGLOBAL hDlgTemplate = LoadResource(g_hModule, hRes);
    if (!hDlgTemplate) return false;

    LPCDLGTEMPLATEW pTemplate = (LPCDLGTEMPLATEW)LockResource(hDlgTemplate);
    if (!pTemplate) return false;

    INT_PTR ret = DialogBoxIndirectParamW(g_hModule, pTemplate, hParent, InputBoxDlgProc, 0);
    return (ret == IDOK);
}

static bool HasRepoInfoForVerb(const GitHubPathInfo& pathInfo, std::wstring& url) {
    if (pathInfo.path.empty()) {
        GitHubRepoInfo repoInfo;
        if (GitHubClient::GetRepoInfo(pathInfo.owner, pathInfo.repo, repoInfo)) {
            url = repoInfo.htmlUrl;
            return true;
        }
    } else {
        url = L"https://github.com/" + pathInfo.owner + L"/" + pathInfo.repo + L"/blob/" +
              GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo) + L"/" + pathInfo.path;
        return true;
    }
    return false;
}

static std::vector<std::wstring> SplitPath(const std::wstring& path) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = path.find(L'/', start)) != std::wstring::npos) {
        if (pos > start) {
            parts.push_back(path.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start < path.length()) {
        parts.push_back(path.substr(start));
    }
    return parts;
}

// 大写开头保留字匹配（根目录级）
static bool IsRootReservedWord(const std::wstring& seg) {
    return seg == L"Repos" || seg == L"Starred" || seg == L"Subscriptions" ||
           seg == L"Notifications" || seg == L"Gists" || seg == L"Trending" ||
           seg == L"Search";
}

// Search/ 下的搜索类型匹配
static bool IsSearchType(const std::wstring& seg) {
    return seg == L"Repos" || seg == L"Code" || seg == L"Users" ||
           seg == L"Issues" || seg == L"Commits" || seg == L"Topics";
}

// 仓库级保留字
static bool IsRepoSubdir(const std::wstring& seg) {
    return seg == L"Code" || seg == L"Issues" || seg == L"Pulls" ||
           seg == L"Branches" || seg == L"Tags" || seg == L"Releases";
}

// 判断字符串是否为十六进制
static bool IsHexString(const std::wstring& s) {
    for (wchar_t c : s) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F')))
            return false;
    }
    return true;
}

// 自动检测) ref 类型
static GitHubRefType DetectRefType(const std::wstring& ref) {
    if (ref.length() >= 7 && ref.length() <= 40 && IsHexString(ref))
        return GITHUB_REF_SHA;
    return GITHUB_REF_BRANCH;
}

static int ParsePageSegment(const std::wstring& seg) {
    if (seg.length() > 5 && seg.compare(0, 5, L"page:") == 0) {
        try { return std::stoi(seg.substr(5)); } catch (...) {}
    }
    return 0;
}

// ============================================================================
// ParseGitHubPath 重构版本：辅助函数声明（放在 GitHubPathInfo 定义之后）// ============================================================================
static std::wstring ExtractPathWithoutPrefix(LPCWSTR pszPath);
static void ParseQueryString(std::wstring& rest, GitHubPathInfo& info);
static std::wstring JoinPathSegments(const std::vector<std::wstring>& segs, size_t startIdx);
static int GetOwnerContextType(const std::wstring& rootWord);
static bool ResolveFlatRepo(const std::vector<std::wstring>& segs, GitHubPathInfo& info);
static void ParseRepoSubdir(const std::vector<std::wstring>& segs, GitHubPathInfo& info, size_t offset);
static void ParseRefSyntax(const std::vector<std::wstring>& segs, GitHubPathInfo& info, size_t offset);
static void ParseSearchPath(const std::vector<std::wstring>& segs, GitHubPathInfo& info);
static void ParseTrendingPath(const std::vector<std::wstring>& segs, GitHubPathInfo& info);
static GitHubPathInfo ParseRootPath(const std::vector<std::wstring>& segs, GitHubPathInfo& info);
static GitHubPathInfo ParseOwnerRepoPath(const std::vector<std::wstring>& segs, GitHubPathInfo& info);

static LPVFSFILEDATAHEADER AllocateFileDataHeader(HANDLE hHeap, int numItems);
static void AddVirtualFile(LPVFSFILEDATAW lpFileData, const std::wstring& name, DWORD size = 0);
static void AddVirtualDir(LPVFSFILEDATAW lpFileData, const std::wstring& name);
static int ReturnEmptyDirectory(LPVFSREADDIRDATAW lpRDD);
static int AddErrorFile(LPVFSREADDIRDATAW lpRDD, const std::wstring& errorTitle, const std::wstring& errorDetail);

static int ReadNotificationsDirectory(LPVFSREADDIRDATAW lpRDD);
static int ReadGistsDirectory(LPVFSREADDIRDATAW lpRDD);
static int ReadTrendingDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadIssuesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadPullsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadBranchesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadTagsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadReleasesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchReposDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchCodeDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchUsersDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchIssuesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchCommitsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);
static int ReadSearchTopicsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);

// ============================================================================
// ParseGitHubPath 重构版本：辅助函数定义
// ============================================================================

// 辅助函数：提取路径（不含前缀)
static std::wstring ExtractPathWithoutPrefix(LPCWSTR pszPath) {
    std::wstring rest = pszPath + GITHUB_VFS_PREFIX_LEN;
    while (!rest.empty() && rest.front() == L'/') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == L'/') rest.pop_back();
    return rest;
}

// 辅助函数：解析查询字符串
static void ParseQueryString(std::wstring& rest, GitHubPathInfo& info) {
    size_t queryPos = rest.find(L'?');
    if (queryPos == std::wstring::npos) return;

    std::wstring queryString = rest.substr(queryPos + 1);
    rest = rest.substr(0, queryPos);

    std::vector<std::wstring> params;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = queryString.find(L'&', start)) != std::wstring::npos) {
        if (pos > start) params.push_back(queryString.substr(start, pos - start));
        start = pos + 1;
    }
    if (start < queryString.length()) {
        params.push_back(queryString.substr(start));
    }

    for (const auto& param : params) {
        auto decodeValue = [](std::wstring value) {
            // Phase 1: Collect all decoded bytes as UTF-8 (handles multi-byte sequences correctly)
            std::string utf8Bytes;
            size_t i = 0;
            while (i < value.length()) {
                if (value[i] == L'%' && i + 2 < value.length()) {
                    wchar_t ch = 0;
                    try { ch = (wchar_t)std::wcstol(value.substr(i + 1, 2).c_str(), nullptr, 16); } catch (...) {}
                    if (ch != 0 && ch <= 0xFF) {
                        utf8Bytes += (char)ch;
                        i += 3;
                        continue;
                    }
                }
                if (value[i] == L'+') {
                    utf8Bytes += ' ';
                    i++;
                } else if (value[i] <= 0x7F) {
                    utf8Bytes += (char)value[i];
                    i++;
                } else {
                    // Already a decoded wide char; encode to UTF-8 then append
                    std::wstring tmp(1, value[i]);
                    utf8Bytes += GitHubClient::WideToUtf8(tmp);
                    i++;
                }
            }
            // Phase 2: Convert collected UTF-8 bytes to wide string
            if (!utf8Bytes.empty()) {
                return GitHubClient::Utf8ToWide(utf8Bytes);
            }
            return value;
        };

        if (param.length() > 5 && param.compare(0, 5, L"view=") == 0) {
            info.query.view = decodeValue(param.substr(5));
        }
        else if (param.length() > 4 && param.compare(0, 4, L"ref=") == 0) {
            info.query.ref = decodeValue(param.substr(4));
        }
        else if (param.length() > 7 && param.compare(0, 7, L"search=") == 0) {
            info.query.search = decodeValue(param.substr(7));
        }
        else if (param.length() > 6 && param.compare(0, 6, L"query=") == 0) {
            info.query.query = decodeValue(param.substr(6));
        }
        else if (param.length() > 2 && param.compare(0, 2, L"q=") == 0) {
            info.query.query = decodeValue(param.substr(2));
        }
        else if (param.length() > 6 && param.compare(0, 6, L"owner=") == 0) {
            info.query.owner = decodeValue(param.substr(6));
            // 同步 ownerFilter 以兼容现有代码路径
                        info.ownerFilter = info.query.owner;
        }
        else if (param.length() > 6 && param.compare(0, 6, L"since=") == 0) {
            info.query.since = decodeValue(param.substr(6));
        }
        else if (param.length() > 5 && param.compare(0, 5, L"lang=") == 0) {
            info.query.lang = decodeValue(param.substr(5));
        }
        else if (param.length() > 5 && param.compare(0, 5, L"page=") == 0) {
            try { info.query.page = std::stoi(param.substr(5)); } catch (...) {}
        }
    }
}

// 辅助函数：连接路径段
static std::wstring JoinPathSegments(const std::vector<std::wstring>& segs, size_t startIdx) {
    if (startIdx >= segs.size()) return L"";
    std::wstring result = segs[startIdx];
    for (size_t i = startIdx + 1; i < segs.size(); i++) {
        result += L"/" + segs[i];
    }
    return result;
}

// 辅助函数：获取 owner 上下文类型
static int GetOwnerContextType(const std::wstring& rootWord) {
    static const std::unordered_map<std::wstring, int> ownerCtxMap = {
        {L"Repos", GITHUB_CTX_REPOS_OWNER},
        {L"Starred", GITHUB_CTX_STARRED_OWNER},
        {L"Subscriptions", GITHUB_CTX_SUBSCRIPTIONS_OWNER}
    };
    auto it = ownerCtxMap.find(rootWord);
    return (it != ownerCtxMap.end()) ? it->second : GITHUB_CTX_ROOT;
}

// 辅助函数：扁平化逻辑（消除 Repos/Starred/Subscriptions 的重复代码）
static bool ResolveFlatRepo(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {

    if (segs.size() < 2) return false;  // 防御：至少需要 rootWord + repoName

    std::wstring repoName = segs[1];
    std::wstring realOwner;
    
    // 根据 segs[0] 决定从哪个缓存查询
        std::vector<GitHubRepoInfo> repos;
    if (segs[0] == L"Repos") {
        repos = GetCachedRepos();
    } else if (segs[0] == L"Starred") {
        repos = GetCachedStarredRepos();
    } else if (segs[0] == L"Subscriptions") {
        repos = GetCachedWatchedRepos();
    }
    
    // 查找真实 owner
    // 如果有 ownerFilter，只匹配该 owner；否则收集所有候选 owner
    std::vector<std::wstring> candidates;
    for (const auto& r : repos) {
        if (_wcsicmp(r.name.c_str(), repoName.c_str()) == 0) {
            size_t pos = r.fullName.find(L'/');
            std::wstring candidateOwner = (pos != std::wstring::npos) ? r.fullName.substr(0, pos) : r.fullName;
            // 应用 ownerFilter
            if (!info.ownerFilter.empty() && _wcsicmp(candidateOwner.c_str(), info.ownerFilter.c_str()) != 0)
                continue;
            candidates.push_back(candidateOwner);
        }
    }
    if (!candidates.empty()) {
        if (candidates.size() > 1 && info.ownerFilter.empty()) {
            LogToFile(L"ResolveFlatRepo: ambiguous repo name '" + repoName + L"' matches " + std::to_wstring(candidates.size()) + L" owners, using first");
        }
        realOwner = candidates[0];
    }
    
    if (!realOwner.empty()) {
        info.owner = realOwner;
        info.repo = repoName;
        info.context = GITHUB_CTX_REPO;
        info.isDir = true;
        if (segs.size() > 2) {
            info.path = JoinPathSegments(segs, 2);
        }
        return true;  // 成功重定向
        }
    
    return false;  // 失败，需要显示 owner 列表
}

// 子函数：处理仓库级保留字（消除 seg1 和 seg2 的重复代码）
static void ParseRepoSubdir(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info,
    size_t offset) {  // offset = 1 或 2

    if (offset >= segs.size()) return;  // 防御：越界检查
    const std::wstring& subdir = segs[offset];
    
    static const std::unordered_map<std::wstring, int> repoSubdirMap = {
        {L"Code", GITHUB_CTX_CODE},
        {L"Issues", GITHUB_CTX_ISSUES},
        {L"Pulls", GITHUB_CTX_PULLS},
        {L"Branches", GITHUB_CTX_BRANCHES},
        {L"Tags", GITHUB_CTX_TAGS},
        {L"Releases", GITHUB_CTX_RELEASES}
    };
    
    auto it = repoSubdirMap.find(subdir);
    if (it != repoSubdirMap.end()) {
        info.context = static_cast<GitHubContext>(it->second);
        info.isDir = true;
    } else {
        return;
    }

    // 特殊处理：需要解析后续路径的上下文
        if (subdir == L"Code") {
        // Code 子路径：有后续段时可能是文件，路径交给 ListDirectory 判断
        if (segs.size() > offset + 1) {
            info.path = JoinPathSegments(segs, offset + 1);
        }
    }
    else if (subdir == L"Branches" && segs.size() > offset) {
        if (segs.size() > offset + 1) {
            info.ref = segs[offset + 1];
            info.refType = GITHUB_REF_BRANCH;
        }
        if (segs.size() > offset + 2) {
            info.path = JoinPathSegments(segs, offset + 2);
        }
    }
    else if (subdir == L"Tags" && segs.size() > offset) {
        if (segs.size() > offset + 1) {
            info.ref = segs[offset + 1];
            info.refType = GITHUB_REF_TAG;
        }
        if (segs.size() > offset + 2) {
            info.path = JoinPathSegments(segs, offset + 2);
        }
    }
    else if ((subdir == L"Issues" || subdir == L"Pulls") && segs.size() > offset + 1) {
        // 先检查 page:N
        int pg = ParsePageSegment(segs[offset + 1]);
        if (pg > 0) {
            info.page = pg;
        } else {
            // 尝试解析 issue/PR 编号
            int num = 0;
            try { num = std::stoi(segs[offset + 1]); } catch (...) {}
            if (num > 0) info.issueNumber = num;
        }
    }
    else if (subdir == L"Releases" && segs.size() > offset + 1) {
        // Releases/{tagName}/ - 支持查看特定 Release
        info.ref = segs[offset + 1];
        info.refType = GITHUB_REF_TAG;
        if (segs.size() > offset + 2) {
            info.path = JoinPathSegments(segs, offset + 2);
        }
    }
}

// 子函数：处理 @{ref} 语法（消除 seg1 和 seg2 的重复代码）
static void ParseRefSyntax(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info,
    size_t offset) {  // offset = 1 或 2

    if (offset >= segs.size()) return;  // 防御：越界检查
    info.ref = segs[offset].substr(1);
    info.refType = DetectRefType(info.ref);
    info.context = static_cast<GitHubContext>(GITHUB_CTX_CODE);
    info.isDir = true;
    
    if (segs.size() > offset + 1) {
        info.path = JoinPathSegments(segs, offset + 1);
    }
}

// 子函数：处理搜索路径
static void ParseSearchPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    if (segs.size() == 1) {
        info.context = static_cast<GitHubContext>(GITHUB_CTX_SEARCH_CENTER);
        info.isDir = true;
        return;
    }
    
    const std::wstring& seg1 = segs[1];
    
    static const std::unordered_map<std::wstring, int> searchTypeMap = {
        {L"Repos", GITHUB_CTX_SEARCH_REPOS},
        {L"Code", GITHUB_CTX_SEARCH_CODE},
        {L"Users", GITHUB_CTX_SEARCH_USERS},
        {L"Issues", GITHUB_CTX_SEARCH_ISSUES},
        {L"Commits", GITHUB_CTX_SEARCH_COMMITS},
        {L"Topics", GITHUB_CTX_SEARCH_TOPICS},
        {L"Owner", GITHUB_CTX_SEARCH_OWNER}
    };
    
    auto it = searchTypeMap.find(seg1);
    if (it != searchTypeMap.end()) {
        info.context = static_cast<GitHubContext>(it->second);
        info.searchType = seg1;
        info.isDir = true;
    }
    
    // Search/{Type}/{query}/
    if (segs.size() >= 3) {
        info.searchQuery = segs[2];
        
        // 解析 contentSegs（排除 page:N）
                std::vector<std::wstring> contentSegs;
        for (size_t i = 3; i < segs.size(); i++) {
            int pg = ParsePageSegment(segs[i]);
            if (pg > 0) {
                info.page = pg;
            } else {
                contentSegs.push_back(segs[i]);
            }
        }
        
        // 重定向规则
                if (contentSegs.size() >= 2 && info.context == GITHUB_CTX_SEARCH_REPOS) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            info.context = static_cast<GitHubContext>(GITHUB_CTX_REPO);
            info.searchQuery.clear();
            info.searchType.clear();
            if (contentSegs.size() > 2) {
                info.path = JoinPathSegments(contentSegs, 2);
            }
            return;
        }
        
        if (contentSegs.size() >= 1 && info.context == static_cast<GitHubContext>(GITHUB_CTX_SEARCH_USERS)) {
            info.owner = contentSegs[0];
            info.context = static_cast<GitHubContext>(GITHUB_CTX_OWNER);
            info.searchQuery.clear();
            info.searchType.clear();
            return;
        }

        if (contentSegs.size() >= 3 && info.context == static_cast<GitHubContext>(GITHUB_CTX_SEARCH_ISSUES)) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            int num = 0;
            try { num = std::stoi(contentSegs[2]); } catch (...) {}
            if (num > 0) {
                info.issueNumber = num;
                info.context = static_cast<GitHubContext>(GITHUB_CTX_ISSUES);
                info.searchQuery.clear();
                info.searchType.clear();
                return;
            }
        }

        if (contentSegs.size() >= 2 && info.context == static_cast<GitHubContext>(GITHUB_CTX_SEARCH_CODE)) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            info.context = static_cast<GitHubContext>(GITHUB_CTX_CODE);
            info.searchQuery.clear();
            info.searchType.clear();
            if (contentSegs.size() > 2) {
                info.path = JoinPathSegments(contentSegs, 2);
            }
            return;
        }

        // Search/Commits/{q}/{owner}/{repo}/... - 仓库
        if (contentSegs.size() >= 2 && info.context == GITHUB_CTX_SEARCH_COMMITS) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            info.context = GITHUB_CTX_REPO;
            info.searchQuery.clear();
            info.searchType.clear();
            return;
        }

        // Search/Topics/{q}/{topic}/ - 保持搜索上下文（无重定向）        // Topics 搜索结果无需重定向到特定页面
    }
}

// 子函数：处理 Trending 路径
static void ParseTrendingPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {

    info.context = static_cast<GitHubContext>(GITHUB_CTX_TRENDING);
    info.isDir = true;

    if (segs.size() >= 2) {
        const std::wstring& seg1 = segs[1];
        if (seg1 == L"daily" || seg1 == L"weekly" || seg1 == L"monthly") {
            info.trendingSince = seg1;
        } else {
            // 先检查 page:N
            int pg = ParsePageSegment(seg1);
            if (pg > 0) {
                info.page = pg;
                info.trendingSince = L"daily";  // 默认
            } else {
                // seg1 不是时间范围也不是翻页，当作语言名（默认 daily）                info.trendingSince = L"daily";
                info.trendingLang = seg1;
            }
        }
    }

    if (segs.size() >= 3) {
        // 第3段可能是语言名或 page:N
        int pg = ParsePageSegment(segs[2]);
        if (pg > 0) {
            info.page = pg;
        } else if (info.trendingLang.empty()) {
            // 只有当语言名尚未设置时才将此段作为语言名            info.trendingLang = segs[2];
        }
    }

    // 第4段：Trending/daily/lang/page:N
    if (segs.size() >= 4) {
        int pg = ParsePageSegment(segs[3]);
        if (pg > 0) {
            info.page = pg;
        }
    }
}

// 子函数：处理根目录保留字
static GitHubPathInfo ParseRootPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    const std::wstring& seg0 = segs[0];
    
    static const std::unordered_map<std::wstring, int> rootContextMap = {
        {L"Repos", GITHUB_CTX_REPOS},
        {L"Starred", GITHUB_CTX_STARRED},
        {L"Subscriptions", GITHUB_CTX_SUBSCRIPTIONS},
        {L"Notifications", GITHUB_CTX_NOTIFICATIONS},
        {L"Gists", GITHUB_CTX_GISTS},
        {L"Trending", GITHUB_CTX_TRENDING},
        {L"Search", GITHUB_CTX_SEARCH_CENTER}
    };
    
    auto it = rootContextMap.find(seg0);
    if (it != rootContextMap.end()) {
        info.context = static_cast<GitHubContext>(it->second);
        info.isDir = true;
    }
    
    // 扁平化逻辑：统一处理 Repos/Starred/Subscriptions
    if (seg0 == L"Repos" || seg0 == L"Starred" || seg0 == L"Subscriptions") {
        if (segs.size() >= 2) {
            // 检查 page:N（列表翻页）
            int pg = ParsePageSegment(segs[1]);
            if (pg > 0) {
                info.page = pg;
                return info;
            }
            // 调用统一的函数处理扁平化
            if (ResolveFlatRepo(segs, info)) {
                return info;
            }
            info.context = static_cast<GitHubContext>(GetOwnerContextType(seg0));
            info.owner = segs[1];
            return info;
        }
    }

    // Gists 子路径处理：Gists/{gistId}/
    if (seg0 == L"Gists" && segs.size() >= 2) {
        int pg = ParsePageSegment(segs[1]);
        if (pg > 0) {
            info.page = pg;
        } else {
            info.metaItem = segs[1];  // gist ID
        }
        return info;
    }

    // Notifications 子路径处理：Notifications/{id}/ 或 Notifications/page:N/
    if (seg0 == L"Notifications" && segs.size() >= 2) {
        int pg = ParsePageSegment(segs[1]);
        if (pg > 0) {
            info.page = pg;
        } else {
            info.metaItem = segs[1];  // 通知标识
        }
        return info;
    }
    
    // Search 特殊处理
    if (seg0 == L"Search") {
        ParseSearchPath(segs, info);
        return info;
    }
    
    // Trending 特殊处理
    if (seg0 == L"Trending") {
        ParseTrendingPath(segs, info);
        return info;
    }
    
    return info;
}

// 子函数：处理 owner/repo 路径
static GitHubPathInfo ParseOwnerRepoPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {

    info.owner = segs[0];
    info.context = static_cast<GitHubContext>(GITHUB_CTX_OWNER);
    info.isDir = true;

    if (segs.size() == 1) return info;

    const std::wstring& seg1 = segs[1];

    // 检查 owner 或 page:N（owner 列表翻页）
        int pg1 = ParsePageSegment(seg1);
    if (pg1 > 0) {
        info.page = pg1;
        return info;
    }

    // 检查是否是仓库级保留字
    if (IsRepoSubdir(seg1)) {
        ParseRepoSubdir(segs, info, 1);
        return info;
    }

    // 检查是否是 @{ref} 语法
    if (seg1.length() > 1 && seg1[0] == L'@') {
        ParseRefSyntax(segs, info, 1);
        return info;
    }

    // 否则是 owner/repo
    info.repo = seg1;
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;

    if (segs.size() == 2) return info;

    const std::wstring& seg2 = segs[2];

    // 检查 repo 或 page:N（仓库内容翻页）
    int pg2 = ParsePageSegment(seg2);
    if (pg2 > 0) {
        info.page = pg2;
        return info;
    }

    // 检查 repo 下的仓库级保留字
    if (IsRepoSubdir(seg2)) {
        ParseRepoSubdir(segs, info, 2);
        return info;
    }

    // 检查 repo 下的 @{ref} 语法
    if (seg2.length() > 1 && seg2[0] == L'@') {
        ParseRefSyntax(segs, info, 2);
        return info;
    }

    info.context = static_cast<GitHubContext>(GITHUB_CTX_CODE);
    info.path = JoinPathSegments(segs, 2);
    return info;
}

// ============================================================================
// ParseGitHubPath 函数已删除，统一使用 ParseGitHubPath（见下方）
// ============================================================================

// ============================================================================
// 新版目录读取函数（重构后使用）// ============================================================================

static int ReadRootDirectoryNew(LPVFSREADDIRDATAW lpRDD) {
    GitHubConfig& cfg = GitHubClient::GetConfig();
    bool hasToken = !cfg.token.empty();

    if (!hasToken) {
        return AddErrorFile(lpRDD, L"⚠ 请配置 GitHub Token.txt", L"请在右键菜单中选择\"配置...\"");
    }

    std::vector<GitHubRepoInfo> repos = GetCachedRepos();
    if (repos.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"🔄 加载）..");
        lpRDD->lpFileData = lpFDH;
        StartPreloadRepos();
        return TRUE;
    }

    int numItems = (int)repos.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        AddVirtualDir(&lpFileData[i], repos[i].name);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadViewDirectoryNew(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    const std::wstring& view = pathInfo.query.view;
    
    if (view == L"starred") {
        std::vector<GitHubRepoInfo> repos = GetCachedStarredRepos();
        if (repos.empty()) {
            return ReturnEmptyDirectory(lpRDD);
        }
        int numItems = (int)repos.size();
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        for (int i = 0; i < numItems; i++) {
            AddVirtualDir(&lpFileData[i], repos[i].name);
        }
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
    
    if (view == L"watched") {
        std::vector<GitHubRepoInfo> repos = GetCachedWatchedRepos();
        if (repos.empty()) {
            return ReturnEmptyDirectory(lpRDD);
        }
        int numItems = (int)repos.size();
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        for (int i = 0; i < numItems; i++) {
            AddVirtualDir(&lpFileData[i], repos[i].name);
        }
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
    
    if (view == L"notifications") {
        return ReadNotificationsDirectory(lpRDD);
    }
    
    if (view == L"gists") {
        return ReadGistsDirectory(lpRDD);
    }
    
    if (view == L"trending") {
        return ReadTrendingDirectory(lpRDD, pathInfo);
    }
    
    if (view == L"issues") {
        return ReadIssuesDirectory(lpRDD, pathInfo);
    }
    
    if (view == L"pulls") {
        return ReadPullsDirectory(lpRDD, pathInfo);
    }
    
    if (view == L"branches") {
        return ReadBranchesDirectory(lpRDD, pathInfo);
    }
    
    if (view == L"tags") {
        return ReadTagsDirectory(lpRDD, pathInfo);
    }
    
    if (view == L"releases") {
        return ReadReleasesDirectory(lpRDD, pathInfo);
    }
    
    return AddErrorFile(lpRDD, L"⚠ 未知视图.txt", L"不支持的 view 参数: " + view);
}

static int ReadSearchDirectoryNew(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    const std::wstring& type = pathInfo.query.search;
    const std::wstring& q = pathInfo.query.query;
    int page = pathInfo.query.page;
    
    if (type == L"repos") {
        return ReadSearchReposDirectory(lpRDD, pathInfo);
    }
    
    if (type == L"code") {
        return ReadSearchCodeDirectory(lpRDD, pathInfo);
    }
    
    if (type == L"users") {
        return ReadSearchUsersDirectory(lpRDD, pathInfo);
    }
    
    if (type == L"issues") {
        return ReadSearchIssuesDirectory(lpRDD, pathInfo);
    }
    
    if (type == L"commits") {
        return ReadSearchCommitsDirectory(lpRDD, pathInfo);
    }
    
    if (type == L"topics") {
        return ReadSearchTopicsDirectory(lpRDD, pathInfo);
    }
    
    return AddErrorFile(lpRDD, L"⚠ 未知搜索类型.txt", L"不支持的 search 参数: " + type);
}

// ============================================================================
// 原有 ParseGitHubPath 函数 - Above（重构版本）
// ============================================================================

// ============================================================================
// ParseGitHubPath - 路径解析主函数
// ============================================================================
static GitHubPathInfo ParseGitHubPath(LPCWSTR pszPath) {
    GitHubPathInfo info;
    
    if (!pszPath || !IsGitHubVfsPath(pszPath)) return info;
    
    std::wstring rest = ExtractPathWithoutPrefix(pszPath);
    ParseQueryString(rest, info);
    
    if (rest.empty()) {
        if (!info.query.search.empty()) {
            info.context = GITHUB_CTX_SEARCH;
            info.isDir = true;
        } else if (!info.query.view.empty()) {
            info.context = GITHUB_CTX_VIEW;
            info.isDir = true;
        } else {
            info.context = GITHUB_CTX_ROOT;
            info.isDir = true;
        }
        return info;
    }
    
    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    for (const auto& seg : segs) {
        if (seg == L".." || seg == L".") {
            LogToFile(L"ParseGitHubPath: rejected path traversal in " + std::wstring(pszPath));
            return info;
        }
    }
    
    info.repo = segs[0];
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    
    if (segs.size() > 1) {
        info.path = JoinPathSegments(segs, 1);
    }
    
    if (!info.query.view.empty()) {
        info.context = GITHUB_CTX_VIEW;
    }
    
    if (!info.query.ref.empty() && info.ref.empty()) {
        info.ref = info.query.ref;
        info.refType = DetectRefType(info.ref);
    }
    
    return info;
}

// ============================================================================
// 原有 ParseGitHubPath 函数 - Above（重构版本）
// ============================================================================

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str) {
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

static std::wstring FormatFileSize(uint64_t size) {
    WCHAR buf[64];
    if (size < 1024) {
        StringCchPrintfW(buf, 64, L"%llu B", size);
    } else if (size < 1024 * 1024) {
        StringCchPrintfW(buf, 64, L"%.1f KB", (double)size / 1024.0);
    } else if (size < 1024ULL * 1024 * 1024) {
        StringCchPrintfW(buf, 64, L"%.1f MB", (double)size / (1024.0 * 1024.0));
    } else {
        StringCchPrintfW(buf, 64, L"%.1f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

static std::wstring GetVisibilityText(bool isPrivate) {
    return isPrivate ? L"\u79c1\u6709" : L"\u516c\u5f00";
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) {
    VFS_TRY
    OutputDebugStringW(L"[GitHubVFS] VFS_Init enter\n");
    LogToFile(L"VFS_Init: 插件初始化开始");
    GitHubClient::Init();
    LogToFile(L"VFS_Init: 插件初始化完成");
    OutputDebugStringW(L"[GitHubVFS] VFS_Init done\n");
    return TRUE;
    VFS_CATCH
    LogToFile(L"VFS_Init: 插件初始化异常");
    return FALSE;
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    VFS_TRY
    g_PluginUnloading.store(true);
    if (g_reposLoadThread) {
        WaitForSingleObject(g_reposLoadThread, 5000);
        CloseHandle(g_reposLoadThread);
        g_reposLoadThread = NULL;
    }
    GitHubClient::Cleanup();
    g_PluginUnloading.store(false);
    VFS_CATCH
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    VFS_TRY
    if (!lpVFSInfo) return FALSE;

    lpVFSInfo->idPlugin = GUIDPlugin_GitHub;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE |
                                 VFSCAPABILITY_SLOW |
                                 VFSCAPABILITY_RANDOMSEEK |
                                 VFSCAPABILITY_FILEDESCRIPTIONS |
                                 VFSCAPABILITY_LETMEDOPARENTS;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, GITHUB_VFS_PREFIX);

    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"GitHub");

    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
                       L"GitHub \u4ed3\u5e93\u865a\u62df\u6587\u4ef6\u7cfb\u7edf");

    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");

    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"https://github.com");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"shell32.dll", 14, &hIconLarge, &hIconSmall, 1);
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
    VFS_CATCH
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    if (!lpszPrefix) return FALSE;
    if (cchPrefixMax < 11) return FALSE;
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, GITHUB_VFS_PREFIX);
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData) {
    static VFSCUSTOMCOLUMNW columns[9];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\u63cf\u8ff0";
    columns[0].lpszKey = L"ghdesc";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\u8bed\u8a00";
    columns[1].lpszKey = L"ghlang";
    columns[1].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[1].iID = 2;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\u661f\u6807\u6570";
    columns[2].lpszKey = L"ghstars";
    columns[2].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER;
    columns[2].iID = 3;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\u53ef\u89c1\u6027";
    columns[3].lpszKey = L"ghvis";
    columns[3].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[3].iID = 4;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\u9ed8\u8ba4\u5206\u652f";
    columns[4].lpszKey = L"ghbranch";
    columns[4].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[4].iID = 5;

    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"Fork\u6570";
    columns[5].lpszKey = L"ghforks";
    columns[5].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER;
    columns[5].iID = 6;

    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\u5927\u5c0f";
    columns[6].lpszKey = L"ghsize";
    columns[6].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    columns[6].iID = 7;

    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[7].lpNext = &columns[8];
    columns[7].lpszLabel = L"\u66f4\u65b0\u65f6\u95f4";
    columns[7].lpszKey = L"ghupdated";
    columns[7].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[7].iID = 8;

    columns[8].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[8].lpNext = NULL;
    columns[8].lpszLabel = L"\u6240\u6709\u8005";
    columns[8].lpszKey = L"ghowner";
    columns[8].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[8].iID = 9;

    return columns;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetAllCustomColumnsW() {
    return VFS_GetCustomColumnsW(NULL);
}

static void SafeFileTimeToSystemTime(const FILETIME& ft, SYSTEMTIME& st) {
    if (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0) {
        GetSystemTime(&st);
    } else {
        FileTimeToSystemTime(&ft, &st);
    }
}

static void SetColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap,
                           const std::vector<std::pair<int, std::wstring>>& columns) {
    if (!lpFileData || !hHeap || hHeap == INVALID_HANDLE_VALUE || columns.empty()) return;

    lpFileData->iNumColumns = 0;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                    columns.size() * sizeof(VFSFILEDATACOLUMNW));
    if (!lpFileData->lpvfsColumnData) return;
    lpFileData->iNumColumns = (int)columns.size();

    for (int i = 0; i < (int)columns.size(); i++) {
        lpFileData->lpvfsColumnData[i].iColumnId = columns[i].first;
        lpFileData->lpvfsColumnData[i].lpszValue = AllocString(hHeap, columns[i].second);
    }
}

static std::wstring FormatFileTime(FILETIME ft) {
    WCHAR timeBuf[64] = {};
    SYSTEMTIME st;
    SafeFileTimeToSystemTime(ft, st);
    StringCchPrintfW(timeBuf, 64, L"%04d-%02d-%02d %02d:%02d",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return timeBuf;
}

static void FillRepoColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const GitHubRepoInfo& repoInfo) {
    if (!lpFileData || !hHeap || hHeap == INVALID_HANDLE_VALUE) return;

    std::wstring shortDesc = repoInfo.description;
    if (shortDesc.length() > 80) shortDesc = shortDesc.substr(0, 80) + L"...";

    std::wstring ownerName;
    size_t slashPos = repoInfo.fullName.find(L'/');
    if (slashPos != std::wstring::npos) {
        ownerName = repoInfo.fullName.substr(0, slashPos);
    }

    SetColumnData(lpFileData, hHeap, {
        {1, shortDesc},
        {2, repoInfo.language.empty() ? L"-" : repoInfo.language},
        {3, std::to_wstring(repoInfo.stargazersCount)},
        {4, GetVisibilityText(repoInfo.isPrivate)},
        {5, repoInfo.defaultBranch},
        {6, std::to_wstring(repoInfo.forksCount)},
        {7, FormatFileSize(repoInfo.size)},
        {8, FormatFileTime(repoInfo.pushedAt)},
        {9, ownerName.empty() ? L"-" : ownerName}
    });
}

static void FillFileColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const GitHubFileInfo& fileInfo) {
    if (!lpFileData || !hHeap || hHeap == INVALID_HANDLE_VALUE) return;

    SetColumnData(lpFileData, hHeap, {
        {1, fileInfo.isDir ? L"\u76ee\u5f55" : L"\u6587\u4ef6"},
        {2, L"-"},
        {3, L"-"},
        {4, L"-"},
        {5, L"-"},
        {6, L"-"},
        {7, fileInfo.isDir ? L"-" : FormatFileSize(fileInfo.size)},
        {8, FormatFileTime(fileInfo.updatedAt)},
        {9, L"-"}
    });
}

static LPVFSFILEDATAHEADER AllocateFileDataHeader(HANDLE hHeap, int numItems) {
    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, allocSize);
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;

    return lpFDH;
}

static int ReturnEmptyDirectory(LPVFSREADDIRDATAW lpRDD) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 0);
    if (!lpFDH) return FALSE;
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// Safely copy name to cFileName, truncating with "..." indicator if too long
static void SafeCopyFileName(WCHAR* dest, size_t destChars, const std::wstring& src) {
    if (destChars == 0) return;
    if (src.length() < destChars) {
        StringCchCopyW(dest, destChars, src.c_str());
    } else if (destChars > 4) {
        // Truncate and add ellipsis indicator (destChars > 4 guarantees truncateAt >= 1)
        size_t truncateAt = destChars - 4; // leave room for "..."
        StringCchCopyNW(dest, truncateAt + 1, src.c_str(), truncateAt);
        StringCchCatW(dest, destChars, L"...");
    } else {
        // Buffer too small for "..." suffix, just truncate to fit
        StringCchCopyNW(dest, destChars, src.c_str(), destChars - 1);
    }
}

template<typename ItemType>
static void FillCommonFileDataFields(LPVFSFILEDATAW lpFileData, HANDLE hAbortEvent,
                                      const std::vector<ItemType>& items, int numItems) {
    for (int i = 0; i < numItems; i++) {
        if (hAbortEvent && WaitForSingleObject(hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, items[i].name);

        if constexpr (std::is_same_v<ItemType, GitHubRepoInfo>) {
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            lpFileData[i].wfdData.ftLastWriteTime = items[i].pushedAt;
            lpFileData[i].wfdData.ftCreationTime = items[i].createdAt;
        } else {
            lpFileData[i].wfdData.dwFileAttributes = items[i].isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            lpFileData[i].wfdData.ftLastWriteTime = items[i].updatedAt;
        }

        ULARGE_INTEGER sz;
        sz.QuadPart = items[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;
    }
}

static void AddVirtualDir(LPVFSFILEDATAW lpFileData, const std::wstring& name) {
    SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, name);
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
}

static void AddVirtualFile(LPVFSFILEDATAW lpFileData, const std::wstring& name, DWORD size) {
    SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, name);
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
    lpFileData->wfdData.nFileSizeLow = size;
}

static int ReadRepoDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo);

static int ReadRootDirectory(LPVFSREADDIRDATAW lpRDD) {
    GitHubConfig& cfg = GitHubClient::GetConfig();
    bool hasToken = !cfg.token.empty();

    std::wstring username;
    if (hasToken && cfg.currentUser.empty()) {
        GitHubClient::TestConnection(username);
    }
    if (hasToken && !cfg.currentUser.empty()) {
        username = cfg.currentUser;
    }

    // 根目录：大写开头保留字目录（不再显示用户名目录，避免与 Repos/ 重复）
    int numItems = 7;  // Repos/Starred/Subscriptions/Notifications/Gists/Trending/Search
    if (!hasToken) numItems++;

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    int idx = 0;
    // 大写开头保留字目录
    AddVirtualDir(&lpFileData[idx++], L"Repos");
    AddVirtualDir(&lpFileData[idx++], L"Starred");
    AddVirtualDir(&lpFileData[idx++], L"Subscriptions");
    AddVirtualDir(&lpFileData[idx++], L"Notifications");
    AddVirtualDir(&lpFileData[idx++], L"Gists");
    AddVirtualDir(&lpFileData[idx++], L"Trending");
    AddVirtualDir(&lpFileData[idx++], L"Search");
    if (!hasToken) {
        AddVirtualFile(&lpFileData[idx++], L"\u26a0 \u8bf7\u914d\u7f6e GitHub Token.txt");
    }

    lpRDD->lpFileData = lpFDH;
    if (hasToken) StartPreloadRepos();
    return TRUE;
}

static int ReadReposDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    LogToFileEx(LOG_DEBUG, L"ReadReposDirectory: \u5f00\u59cb\u83b7\u53d6\u4ed3\u5e93\u5217\u8868");
    std::vector<GitHubRepoInfo> repos = GetCachedRepos();

    LogToFileEx(LOG_DEBUG, L"ReadReposDirectory: \u83b7\u53d6\u5230 " + std::to_wstring(repos.size()) + L" \u4e2a\u4ed3\u5e93");
    OutputDebugStringW((L"[GitHubVFS] ReadReposDirectory: " + std::to_wstring(repos.size()) + L" repos\n").c_str());

    if (repos.empty()) {
        return ReturnEmptyDirectory(lpRDD);
    }

    int numItems = (int)repos.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) {
        LogToFileEx(LOG_ERROR, L"ReadReposDirectory: AllocateFileDataHeader \u5931\u8d25");
        return FALSE;
    }

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, repos[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = repos[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = repos[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = repos[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, repos[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadOwnerFilteredDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo,
                                        const std::vector<GitHubRepoInfo>& repos) {
    std::vector<GitHubRepoInfo> filtered;
    for (const auto& r : repos) {
        size_t pos = r.fullName.find(L'/');
        std::wstring owner = (pos != std::wstring::npos) ? r.fullName.substr(0, pos) : r.fullName;
        if (_wcsicmp(owner.c_str(), pathInfo.owner.c_str()) == 0) {
            filtered.push_back(r);
        }
    }
    if (filtered.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)filtered.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, filtered[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = filtered[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = filtered[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = filtered[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, filtered[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadReposOwnerDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    std::vector<GitHubRepoInfo> repos = GetCachedRepos();
    if (repos.empty()) repos = GitHubClient::ListUserRepos();

    for (const auto& r : repos) {
        if (_wcsicmp(r.name.c_str(), pathInfo.owner.c_str()) == 0) {
            GitHubPathInfo repoPathInfo = pathInfo;
            size_t pos = r.fullName.find(L'/');
            repoPathInfo.owner = (pos != std::wstring::npos) ? r.fullName.substr(0, pos) : r.fullName;
            repoPathInfo.repo = r.name;
            repoPathInfo.context = GITHUB_CTX_REPO;
            return ReadRepoDirectory(lpRDD, repoPathInfo);
        }
    }

    return ReadOwnerFilteredDirectory(lpRDD, pathInfo, repos);
}

static int ReadSearchCodeDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubCodeSearchResult codeResult;
    try { codeResult = GitHubClient::SearchCode(pathInfo.searchQuery); } catch (...) { codeResult.items.clear(); }
    if (codeResult.items.empty()) return ReturnEmptyDirectory(lpRDD);
    int numItems = (int)codeResult.items.size();

    {
        std::lock_guard<std::mutex> lock(g_codeResultsMutex);
        g_cachedCodeResults = codeResult.items;
    }

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        std::wstring displayName = L"[" + codeResult.items[i].repoFullName + L"] " + codeResult.items[i].path;
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, displayName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u4ee3\u7801"},
            {2, codeResult.items[i].repoLanguage.empty() ? L"-" : codeResult.items[i].repoLanguage},
            {3, std::to_wstring(codeResult.items[i].repoStars)},
            {4, L"-"},
            {5, L"-"},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSearchUsersDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubUserSearchResult userResult;
    try { userResult = GitHubClient::SearchUsers(pathInfo.searchQuery); } catch (...) { userResult.users.clear(); }
    if (userResult.users.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)userResult.users.size();

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, userResult.users[i].login);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u7528\u6237"},
            {2, userResult.users[i].name.empty() ? L"-" : userResult.users[i].name},
            {3, std::to_wstring(userResult.users[i].publicRepos)},
            {4, std::to_wstring(userResult.users[i].followers)},
            {5, userResult.users[i].location.empty() ? L"-" : userResult.users[i].location},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSearchIssuesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubIssueSearchResult issueResult;
    try { issueResult = GitHubClient::SearchIssues(pathInfo.searchQuery, pathInfo.page); } catch (...) { issueResult.items.clear(); }
    if (issueResult.items.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)issueResult.items.size();

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        const auto& item = issueResult.items[i];
        std::wstring displayName = L"[" + item.owner + L"/" + item.repo + L"] #" + std::to_wstring(item.number);
        if (item.isPullRequest) {
            displayName = L"[PR] " + displayName;
        }
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, displayName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData[i].wfdData.ftLastWriteTime = item.updatedAt;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, item.isPullRequest ? L"PR" : L"Issue"},
            {2, item.title},
            {3, item.state},
            {4, item.author},
            {5, std::to_wstring(item.comments)},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadTrendingDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    // Trending/ 无时间范围时显示时间选项
    if (pathInfo.trendingSince.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 3);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualDir(&lpFileData[0], L"daily");
        AddVirtualDir(&lpFileData[1], L"weekly");
        AddVirtualDir(&lpFileData[2], L"monthly");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    // Trending/{since}/ 无语言时显示语言列表
    if (pathInfo.trendingLang.empty()) {
        // 显示常见语言列表
        static const wchar_t* popularLangs[] = {
            L"javascript", L"python", L"typescript", L"java", L"csharp",
            L"cpp", L"go", L"rust", L"ruby", L"php",
            L"swift", L"kotlin", L"c", L"shell", L"scala",
            L"r", L"dart", L"lua", L"perl", L"haskell"
        };
        int numLangs = sizeof(popularLangs) / sizeof(popularLangs[0]);
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numLangs);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        for (int i = 0; i < numLangs; i++) {
            AddVirtualDir(&lpFileData[i], popularLangs[i]);
        }
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    // Trending/{since}/{language}/ - 搜索热门仓库
    // 用 SearchRepos 按星标排序模拟 Trending
    std::wstring query = L"language:" + pathInfo.trendingLang;
    std::wstring sort = L"stars";
    // 注意: 当前 SearchRepos 签名不支持 sort 参数，先按默认搜索
        GitHubSearchResult searchResult;
    try { searchResult = GitHubClient::SearchRepos(query, pathInfo.page); } catch (...) { searchResult.repos.clear(); }
    if (searchResult.repos.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)searchResult.repos.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, searchResult.repos[i].fullName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = searchResult.repos[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = searchResult.repos[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = searchResult.repos[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, searchResult.repos[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadTopicsDirectory(LPVFSREADDIRDATAW lpRDD) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
    if (!lpFDH) return FALSE;
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u4e3b\u9898\u5173\u952e\u8bcd.txt");
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// ═══ v3.1 新增: Search Center 目录 ═══
static int ReadSearchCenterDirectory(LPVFSREADDIRDATAW lpRDD) {
    // Search/ 显示 7 种搜索类型入口（v3.2 新增 Owner）
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 7);
    if (!lpFDH) return FALSE;
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualDir(&lpFileData[0], L"Repos");
    AddVirtualDir(&lpFileData[1], L"Code");
    AddVirtualDir(&lpFileData[2], L"Users");
    AddVirtualDir(&lpFileData[3], L"Issues");
    AddVirtualDir(&lpFileData[4], L"Commits");
    AddVirtualDir(&lpFileData[5], L"Topics");
    AddVirtualDir(&lpFileData[6], L"Owner");
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// Search/Owner/ - owner 列表（v3.2 新增）
static int ReadSearchOwnerDirectory(LPVFSREADDIRDATAW lpRDD) {
    // 显示提示文件，用户可以输入 owner 名称
    
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
    if (!lpFDH) return FALSE;
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u7528\u6237\u540d.txt");
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// Search/Repos/{query}/
static int ReadSearchReposDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty() || pathInfo.searchQuery == L"*") {
        // 如果有 ownerFilter，显示该 owner 的仓库
                if (!pathInfo.ownerFilter.empty()) {
            std::vector<GitHubRepoInfo> repos;
            try { repos = GitHubClient::ListUserRepos(pathInfo.ownerFilter); } catch (...) { repos.clear(); }
            if (repos.empty()) return ReturnEmptyDirectory(lpRDD);

            int numItems = (int)repos.size();
            LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
            if (!lpFDH) return FALSE;

            LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
            for (int i = 0; i < numItems; i++) {
                SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, repos[i].name);
                lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                lpFileData[i].wfdData.ftLastWriteTime = repos[i].pushedAt;
                lpFileData[i].wfdData.ftCreationTime = repos[i].createdAt;

                ULARGE_INTEGER sz;
                sz.QuadPart = repos[i].size;
                lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
                lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

                FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, repos[i]);
            }

            lpRDD->lpFileData = lpFDH;
            return TRUE;
        }
        
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubSearchResult searchResult;
    try { searchResult = GitHubClient::SearchRepos(pathInfo.searchQuery, pathInfo.page); } catch (...) { searchResult.repos.clear(); }
    if (searchResult.repos.empty()) return ReturnEmptyDirectory(lpRDD);

    // 如果有 ownerFilter，过滤结果
        std::vector<GitHubRepoInfo> filtered;
    if (!pathInfo.ownerFilter.empty()) {
        for (const auto& repo : searchResult.repos) {
            size_t pos = repo.fullName.find(L'/');
            std::wstring owner = (pos != std::wstring::npos) ? repo.fullName.substr(0, pos) : repo.fullName;
            if (_wcsicmp(owner.c_str(), pathInfo.ownerFilter.c_str()) == 0) {
                filtered.push_back(repo);
            }
        }
        if (filtered.empty()) return ReturnEmptyDirectory(lpRDD);
        
        int numItems = (int)filtered.size();
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
        if (!lpFDH) return FALSE;

        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        for (int i = 0; i < numItems; i++) {
            SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, filtered[i].name);
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            lpFileData[i].wfdData.ftLastWriteTime = filtered[i].pushedAt;
            lpFileData[i].wfdData.ftCreationTime = filtered[i].createdAt;

            ULARGE_INTEGER sz;
            sz.QuadPart = filtered[i].size;
            lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
            lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

            FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, filtered[i]);
        }

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    int numItems = (int)searchResult.repos.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, searchResult.repos[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = searchResult.repos[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = searchResult.repos[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = searchResult.repos[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, searchResult.repos[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// Search/Commits/{query}/
static int ReadSearchCommitsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubCommitSearchResult commitResult;
    try { commitResult = GitHubClient::SearchCommits(pathInfo.searchQuery, pathInfo.page); } catch (...) { commitResult.items.clear(); }
    if (commitResult.items.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)commitResult.items.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        std::wstring displayName = commitResult.items[i].sha.substr(0, 7) + L" " + commitResult.items[i].message;
        // SafeCopyFileName 自带截断+省略号逻辑，无需手动 resize
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, displayName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u63d0\u4ea4"},
            {2, commitResult.items[i].sha.substr(0, 7)},
            {3, commitResult.items[i].author},
            {4, commitResult.items[i].owner + L"/" + commitResult.items[i].repo}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

// Search/Topics/{query}/
static int ReadSearchTopicsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u8f93\u5165\u4e3b\u9898\u5173\u952e\u8bcd.txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    GitHubTopicSearchResult topicResult;
    try { topicResult = GitHubClient::SearchTopics(pathInfo.searchQuery, pathInfo.page); } catch (...) { topicResult.items.clear(); }
    if (topicResult.items.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)topicResult.items.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, topicResult.items[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u4e3b\u9898"},
            {2, topicResult.items[i].description},
            {3, std::to_wstring(topicResult.items[i].repoCount)}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadStarredDirectory(LPVFSREADDIRDATAW lpRDD) {
    std::vector<GitHubRepoInfo> starred = GetCachedStarredRepos();
    if (starred.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)starred.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, starred[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = starred[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = starred[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = starred[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, starred[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadStarredOwnerDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    std::vector<GitHubRepoInfo> starred;
    try { starred = GitHubClient::ListStarredRepos(); } catch (...) { starred.clear(); }
    if (starred.empty()) return ReturnEmptyDirectory(lpRDD);
    return ReadOwnerFilteredDirectory(lpRDD, pathInfo, starred);
}

static int ReadSubscriptionsDirectory(LPVFSREADDIRDATAW lpRDD) {
    std::vector<GitHubRepoInfo> watched;
    try { watched = GitHubClient::ListWatchedRepos(); } catch (...) { watched.clear(); }
    if (watched.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)watched.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, watched[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = watched[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = watched[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = watched[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, watched[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSubscriptionsOwnerDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    std::vector<GitHubRepoInfo> watched;
    try { watched = GitHubClient::ListWatchedRepos(); } catch (...) { watched.clear(); }
    if (watched.empty()) return ReturnEmptyDirectory(lpRDD);
    return ReadOwnerFilteredDirectory(lpRDD, pathInfo, watched);
}

static int ReadNotificationsDirectory(LPVFSREADDIRDATAW lpRDD) {
    std::vector<GitHubNotificationInfo> notifications;
    try { notifications = GitHubClient::ListNotifications(); } catch (...) { notifications.clear(); }
    if (notifications.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)notifications.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        std::wstring displayName = notifications[i].subjectTitle;
        if (displayName.empty()) displayName = std::to_wstring(notifications[i].id);
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, displayName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u901a\u77e5"},
            {2, notifications[i].subjectType},
            {3, notifications[i].owner + L"/" + notifications[i].repo},
            {4, notifications[i].reason},
            {5, notifications[i].unread ? L"\u25cf" : L"\u25cb"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadGistsDirectory(LPVFSREADDIRDATAW lpRDD) {
    std::vector<GitHubGistInfo> gists;
    try { gists = GitHubClient::ListGists(); } catch (...) { gists.clear(); }
    if (gists.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)gists.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        std::wstring displayName = gists[i].description;
        if (displayName.empty()) displayName = gists[i].id;
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, displayName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"Gist"},
            {2, gists[i].id},
            {3, gists[i].isPublic ? L"Public" : L"Secret"},
            {4, std::to_wstring(gists[i].fileCount) + L" files"},
            {5, gists[i].updatedAt}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadIssuesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    if (pathInfo.issueNumber > 0) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        std::wstring name = L"#" + std::to_wstring(pathInfo.issueNumber);
        AddVirtualFile(&lpFileData[0], name + L".txt");
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    // 支持 ?since=closed / ?since=all 查询参数控制状态过滤
    std::wstring state = L"open";
    if (!pathInfo.query.since.empty()) {
        if (pathInfo.query.since == L"closed" || pathInfo.query.since == L"all") {
            state = pathInfo.query.since;
        }
    }

    std::vector<GitHubIssueInfo> issues;
    try { issues = GitHubClient::ListIssues(pathInfo.owner, pathInfo.repo, state); } catch (...) { issues.clear(); }
    int numItems = 0;
    for (int i = 0; i < (int)issues.size(); i++) {
        if (!issues[i].isPullRequest) numItems++;
    }
    if (numItems == 0) return ReturnEmptyDirectory(lpRDD);

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    int idx = 0;
    for (int i = 0; i < (int)issues.size(); i++) {
        if (issues[i].isPullRequest) continue;

        std::wstring name = L"#" + std::to_wstring(issues[i].number) + L" " + issues[i].title;
        SafeCopyFileName(lpFileData[idx].wfdData.cFileName, MAX_PATH, name);
        lpFileData[idx].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData[idx].wfdData.ftCreationTime = issues[i].createdAt;
        lpFileData[idx].wfdData.ftLastWriteTime = issues[i].updatedAt;

        SetColumnData(&lpFileData[idx], lpRDD->hMemHeap, {
            {1, L"Issue"},
            {2, std::to_wstring(issues[i].number)},
            {3, issues[i].state},
            {4, issues[i].author},
            {5, std::to_wstring(issues[i].comments)},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
        idx++;
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadPullsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    // 支持 ?since=closed / ?since=all 查询参数控制状态过滤
    std::wstring state = L"open";
    if (!pathInfo.query.since.empty()) {
        if (pathInfo.query.since == L"closed" || pathInfo.query.since == L"all") {
            state = pathInfo.query.since;
        }
    }

    std::vector<GitHubIssueInfo> pulls;
    try { pulls = GitHubClient::ListPullRequests(pathInfo.owner, pathInfo.repo, state); } catch (...) { pulls.clear(); }
    if (pulls.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)pulls.size();

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        std::wstring name = L"#" + std::to_wstring(pulls[i].number) + L" " + pulls[i].title;
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData[i].wfdData.ftCreationTime = pulls[i].createdAt;
        lpFileData[i].wfdData.ftLastWriteTime = pulls[i].updatedAt;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"PR"},
            {2, std::to_wstring(pulls[i].number)},
            {3, pulls[i].state},
            {4, pulls[i].author},
            {5, std::to_wstring(pulls[i].comments)},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadRefListDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo,
                                 const std::wstring& typeLabel,
                                 std::function<std::vector<GitHubBranchInfo>()> loader) {
    if (!pathInfo.ref.empty()) {
        std::wstring ref = pathInfo.ref;
        std::vector<GitHubFileInfo> files;
        try { files = GitHubClient::ListDirectory(pathInfo.owner, pathInfo.repo, pathInfo.path, ref); } catch (...) { files.clear(); }
        if (files.empty()) return ReturnEmptyDirectory(lpRDD);

        int numItems = (int)files.size();

        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
        if (!lpFDH) return FALSE;

        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        FillCommonFileDataFields(lpFileData, lpRDD->hAbortEvent, files, numItems);
        for (int i = 0; i < numItems; i++) {
            FillFileColumnData(&lpFileData[i], lpRDD->hMemHeap, files[i]);
        }

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    std::vector<GitHubBranchInfo> refs;
    try { refs = loader(); } catch (...) { refs.clear(); }
    if (refs.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)refs.size();

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, refs[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, typeLabel},
            {2, refs[i].name},
            {3, refs[i].isDefault ? L"\u2713" : L"-"},
            {4, refs[i].sha.length() >= 7 ? refs[i].sha.substr(0, 7) : refs[i].sha},
            {5, L"-"},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadBranchesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    return ReadRefListDirectory(lpRDD, pathInfo, L"\u5206\u652f",
        [&pathInfo]() { return GitHubClient::ListBranches(pathInfo.owner, pathInfo.repo); });
}

static int ReadTagsDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    return ReadRefListDirectory(lpRDD, pathInfo, L"\u6807\u7b7e",
        [&pathInfo]() { return GitHubClient::ListTags(pathInfo.owner, pathInfo.repo); });
}

static int ReadReleasesDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    std::vector<GitHubReleaseInfo> releases;
    try { releases = GitHubClient::ListReleases(pathInfo.owner, pathInfo.repo); } catch (...) { releases.clear(); }
    if (releases.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)releases.size();

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, releases[i].tagName);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData[i].wfdData.ftCreationTime = releases[i].createdAt;
        lpFileData[i].wfdData.ftLastWriteTime = releases[i].publishedAt;

        SetColumnData(&lpFileData[i], lpRDD->hMemHeap, {
            {1, L"\u53d1\u5e03"},
            {2, releases[i].tagName},
            {3, releases[i].name},
            {4, releases[i].isPrerelease ? L"\u9884\u53d1\u5e03" : L"\u6b63\u5f0f"},
            {5, releases[i].isDraft ? L"\u8349\u7a3f" : L"-"},
            {6, L"-"},
            {7, L"-"},
            {8, L"-"},
            {9, L"-"}
        });
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int AddErrorFile(LPVFSREADDIRDATAW lpRDD, const std::wstring& errorTitle, const std::wstring& errorDetail);

static int ReadOwnerDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    // v3.1: {owner}/ 只显示该用户的公开仓库列表，不再有子目录保留字
    std::vector<GitHubRepoInfo> repos;

    // 优先使用 ListUserRepos(owner) 获取用户公开仓库
    try { repos = GitHubClient::ListUserRepos(pathInfo.owner); } catch (...) { repos.clear(); }

    // Fallback: 通过搜索 API
    if (repos.empty()) {
        GitHubSearchResult searchResult;
        try { searchResult = GitHubClient::SearchUserRepos(pathInfo.owner); } catch (...) { searchResult.repos.clear(); }
        repos = searchResult.repos;
    }
    if (repos.empty()) {
        GitHubSearchResult searchResult;
        try { searchResult = GitHubClient::SearchOrgRepos(pathInfo.owner); } catch (...) { searchResult.repos.clear(); }
        repos = searchResult.repos;
    }

    OutputDebugStringW((L"[GitHubVFS] ReadOwnerDirectory: " + pathInfo.owner + L", repos=" + std::to_wstring(repos.size()) + L"\n").c_str());

    if (repos.empty()) return ReturnEmptyDirectory(lpRDD);

    int numItems = (int)repos.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < (int)repos.size(); i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, repos[i].name);
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        lpFileData[i].wfdData.ftLastWriteTime = repos[i].pushedAt;
        lpFileData[i].wfdData.ftCreationTime = repos[i].createdAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = repos[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillRepoColumnData(&lpFileData[i], lpRDD->hMemHeap, repos[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadRepoDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    std::wstring owner = pathInfo.query.owner.empty() ? 
        (pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner) : 
        pathInfo.query.owner;
    
    std::wstring ref = pathInfo.query.ref.empty() ? pathInfo.ref : pathInfo.query.ref;
    try {
        if (ref.empty()) {
            ref = GitHubClient::GetDefaultBranch(owner, pathInfo.repo);
        }
    } catch (...) {
        ref = L"main";
    }
    if (ref.empty()) ref = L"main";

    OutputDebugStringW((L"[GitHubVFS] ReadRepoDirectory: " + owner + L"/" + pathInfo.repo +
        L" path=[" + pathInfo.path + L"] ref=[" + ref + L"]\n").c_str());

    std::vector<GitHubFileInfo> files;
    try { files = GitHubClient::ListDirectory(owner, pathInfo.repo, pathInfo.path, ref); } catch (...) { files.clear(); }

    OutputDebugStringW((L"[GitHubVFS] ListDirectory returned " + std::to_wstring(files.size()) + L" files\n").c_str());

    if (files.empty()) {
        std::vector<std::wstring> fallbackRefs = {L"main", L"master"};
        for (const auto& fallbackRef : fallbackRefs) {
            if (ref == fallbackRef) continue;
            OutputDebugStringW((L"[GitHubVFS] Retrying with ref=" + fallbackRef + L"\n").c_str());
            try { files = GitHubClient::ListDirectory(owner, pathInfo.repo, pathInfo.path, fallbackRef); } catch (...) { files.clear(); }
            if (!files.empty()) break;
        }
    }

    int numItems = (int)files.size();
    if (numItems == 0) return ReturnEmptyDirectory(lpRDD);

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    for (int i = 0; i < (int)files.size(); i++) {
        SafeCopyFileName(lpFileData[i].wfdData.cFileName, MAX_PATH, files[i].name);
        lpFileData[i].wfdData.dwFileAttributes = files[i].isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        lpFileData[i].wfdData.ftLastWriteTime = files[i].updatedAt;
        lpFileData[i].wfdData.ftCreationTime = files[i].updatedAt;

        ULARGE_INTEGER sz;
        sz.QuadPart = files[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;

        FillFileColumnData(&lpFileData[i], lpRDD->hMemHeap, files[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int AddErrorFile(LPVFSREADDIRDATAW lpRDD, const std::wstring& errorTitle, const std::wstring& errorDetail) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualFile(&lpFileData[0], errorTitle);
    lpFileData[0].wfdData.dwFileAttributes = FILE_ATTRIBUTE_READONLY;

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    OutputDebugStringW(L"[GitHubVFS] InternalReadDirectory enter\n");

    GitHubPathInfo pathInfo;
    try {
        pathInfo = ParseGitHubPath(lpRDD->lpszPath);
    } catch (...) {
        OutputDebugStringW(L"[GitHubVFS] ParseGitHubPath exception\n");
        LogToFileEx(LOG_ERROR, L"InternalReadDirectory: ParseGitHubPath \u5f02\u5e38");
        return AddErrorFile(lpRDD, L"\u26a0 \u8def\u5f84\u89e3\u6790\u5f02\u5e38.txt", L"");
    }

    WCHAR ctxBuf[256];
    StringCchPrintfW(ctxBuf, 256, L"InternalReadDirectory: context=%d", pathInfo.context);
    LogToFileEx(LOG_DEBUG, ctxBuf);

    OutputDebugStringW(L"[GitHubVFS] InternalReadDirectory parsed\n");

    try {
        switch (pathInfo.context) {
        case GITHUB_CTX_ROOT:
            OutputDebugStringW(L"[GitHubVFS] InternalReadDirectory ROOT\n");
            if (!pathInfo.query.search.empty()) {
                return ReadSearchDirectoryNew(lpRDD, pathInfo);
            } else if (!pathInfo.query.view.empty()) {
                return ReadViewDirectoryNew(lpRDD, pathInfo);
            } else {
                return ReadRootDirectoryNew(lpRDD);
            }

        case GITHUB_CTX_REPO:
            return ReadRepoDirectory(lpRDD, pathInfo);

        case GITHUB_CTX_VIEW:
            return ReadViewDirectoryNew(lpRDD, pathInfo);

        case GITHUB_CTX_SEARCH:
            return ReadSearchDirectoryNew(lpRDD, pathInfo);

        default:
            return AddErrorFile(lpRDD,
                L"\u26a0 \u65e0\u6cd5\u89e3\u6790\u8def\u5f84.txt",
                std::wstring(lpRDD->lpszPath) + L" \u8def\u5f84\u65e0\u6548");
        }
    } catch (const std::exception& e) {
        OutputDebugStringA("[GitHubVFS] ReadDirectory exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return AddErrorFile(lpRDD, L"\u26a0 \u8bfb\u53d6\u5f02\u5e38.txt",
            Utf8ToWide_safe(e.what()));
    } catch (...) {
        OutputDebugStringW(L"[GitHubVFS] ReadDirectory unknown exception\n");
        return AddErrorFile(lpRDD, L"\u26a0 \u8bfb\u53d6\u5f02\u5e38.txt", L"");
    }
}

static int InternalReadDirectorySafe(LPVFSREADDIRDATAW lpRDD) {
    VFS_TRY
    if (!lpRDD) return FALSE;

    switch (lpRDD->vfsReadOp) {
    case VFSREAD_FREEDIRCLOSE:
    case VFSREAD_FREEDIR:
    case VFSREAD_CHANGEDIR:
        return TRUE;

    case VFSREAD_NORMAL:
    case VFSREAD_REFRESH:
    case VFSREAD_PARENT:
    case VFSREAD_ROOT:
    case VFSREAD_BACK:
    case VFSREAD_FORWARD:
    case VFSREAD_PRINTDIR:
        return InternalReadDirectory(lpRDD);

    default:
        return TRUE;
    }
    VFS_CATCH
    return FALSE;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                         LPVFSREADDIRDATAW lpRDD) {
    if (lpRDD && lpRDD->lpszPath) {
        WCHAR dbgBuf[512];
        StringCchPrintfW(dbgBuf, 512, L"VFS_ReadDirectoryW: %s", lpRDD->lpszPath);
        LogToFile(dbgBuf);
    }
    __try {
        int result = InternalReadDirectorySafe(lpRDD);
        LogToFile(L"VFS_ReadDirectoryW: \u8fd4\u56de");
        return result;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_ReadDirectoryW SEH exception\n");
        LogToFile(L"VFS_ReadDirectoryW: SEH \u5f02\u5e38\u6355\u83b7");
        return FALSE;
    }
}

static LPVFSFILEDATAHEADER InternalGetFileInfoSafe(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    VFS_TRY
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    OutputDebugStringW(L"[GitHubVFS] VFS_GetFileInformationW enter\n");

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                 sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    switch (pathInfo.context) {
    case GITHUB_CTX_ROOT:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"GitHub");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_REPOS:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Repos");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_REPOS_OWNER:
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.owner);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_STARRED:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Starred");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_STARRED_OWNER:
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.owner);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SUBSCRIPTIONS:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Subscriptions");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SUBSCRIPTIONS_OWNER:
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.owner);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_NOTIFICATIONS:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Notifications");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_GISTS:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Gists");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_CODE:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Code");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u4ee3\u7801\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_USERS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Users");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u7528\u6237\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_ISSUES:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Issues");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"Issue\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_REPOS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Repos");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u4ed3\u5e93\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_COMMITS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Commits");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u63d0\u4ea4\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_TOPICS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Topics");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u4e3b\u9898\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_OWNER:
        if (!pathInfo.ownerFilter.empty()) {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u7528\u6237: %s", pathInfo.ownerFilter.c_str());
        } else {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Owner");
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_SEARCH_CENTER:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Search");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_TRENDING:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Trending");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_OWNER:
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.owner);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_REPO:
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.repo);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
        return lpFDH;

    case GITHUB_CTX_CODE:
        if (pathInfo.path.empty()) {
            SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.repo);
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
            return lpFDH;
        }
        break;

    case GITHUB_CTX_ISSUES:
        if (pathInfo.issueNumber > 0) {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"#%d", pathInfo.issueNumber);
        } else {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Issues");
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_PULLS:
        if (pathInfo.issueNumber > 0) {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH, L"PR #%d", pathInfo.issueNumber);
        } else {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Pulls");
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_BRANCHES:
        if (!pathInfo.ref.empty() && pathInfo.path.empty()) {
            SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.ref);
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
            return lpFDH;
        }
        if (!pathInfo.ref.empty() && !pathInfo.path.empty()) {
            break;
        }
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Branches");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_TAGS:
        if (!pathInfo.ref.empty() && pathInfo.path.empty()) {
            SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.ref);
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
            return lpFDH;
        }
        if (!pathInfo.ref.empty() && !pathInfo.path.empty()) {
            break;
        }
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Tags");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    case GITHUB_CTX_RELEASES:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Releases");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;

    default:
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"GitHub");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        return lpFDH;
    }

    if (pathInfo.owner.empty() || pathInfo.repo.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"GitHub");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        return lpFDH;
    }

    if (pathInfo.path.empty()) {
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.repo);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
        return lpFDH;
    }

    {
        std::wstring nameStr = pathInfo.path;
        size_t pos = nameStr.find_last_of(L'/');
        if (pos != std::wstring::npos) nameStr = nameStr.substr(pos + 1);
        SafeCopyFileName(lpFileData->wfdData.cFileName, MAX_PATH, nameStr);
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
    }

    return lpFDH;
    VFS_CATCH
    return NULL;
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                                              LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    __try {
        return InternalGetFileInfoSafe(hVFSData, lpFuncData, lpszPath, hHeap, dwFlags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_GetFileInformationW SEH exception\n");
        return NULL;
    }
}

static HANDLE VFS_CreateFileW_Impl(LPVFSFUNCDATA lpFuncData,
                                                         LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
                                                         DWORD dwFlags, LPFILETIME lpFT) {
    if (!IsGitHubVfsPath(lpszFile)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszFile);

    if (wcsstr(lpszFile, L"\u8bf7\u914d\u7f6e GitHub Token")) {
        GitHubContentInfo* ctx = new GitHubContentInfo();
        ctx->readPos = 0;
        ctx->isWrite = false;
        ctx->virtualType = VIRTUAL_FILE_NONE;
        ctx->hTempFile = NULL;
        ctx->modified = false;
        std::wstring configPath = GitHubClient::GetConfigFilePath();
        std::string configPathA;
        configPathA.reserve(configPath.size());
        for (wchar_t wc : configPath) configPathA += (char)wc;

        std::string msg = "GitHub VFS - Token Configuration\n"
            "================================\n\n"
            "Please configure your GitHub Personal Access Token.\n\n"
            "Steps:\n"
            "1. Go to https://github.com/settings/tokens\n"
            "2. Generate a new token (classic)\n"
            "3. Select scopes: repo, read:org, notifications\n"
            "4. Copy the token\n"
            "5. Edit config file: " + configPathA + "\n"
            "6. Set Token=ghp_your_token_here\n"
            "7. Set AuthMode=0 (Token mode)\n"
            "8. Restart Directory Opus\n\n"
            "Config file location:\n" + configPathA + "\n";
        ctx->data.assign(msg.begin(), msg.end());
        return (HANDLE)ctx;
    }

    if (pathInfo.repo.empty() || pathInfo.path.empty()) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    bool isWrite = (dwMode & GENERIC_WRITE) != 0;
    GitHubContentInfo* ctx = new GitHubContentInfo();
    ctx->readPos = 0;
    ctx->isWrite = isWrite;
    ctx->owner = pathInfo.owner;
    ctx->repo = pathInfo.repo;
    ctx->path = pathInfo.path;
    ctx->virtualType = VIRTUAL_FILE_NONE;
    ctx->hTempFile = NULL;
    ctx->modified = false;
    memset(&ctx->originalModTime, 0, sizeof(FILETIME));

    WCHAR tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);

    GUID guid;
    CoCreateGuid(&guid);
    WCHAR guidStr[40];
    StringCchPrintfW(guidStr, 40, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    std::wstring fileName = pathInfo.path;
    size_t lastSlash = fileName.find_last_of(L'/');
    if (lastSlash != std::wstring::npos) fileName = fileName.substr(lastSlash + 1);

    size_t maxFileNameLen = MAX_PATH - wcslen(tempDir) - wcslen(guidStr) - 10;
    if (fileName.length() > maxFileNameLen && maxFileNameLen > 4) {
        size_t extPos = fileName.find_last_of(L'.');
        if (extPos != std::wstring::npos && fileName.length() - extPos <= 5) {
            fileName = fileName.substr(0, maxFileNameLen - (fileName.length() - extPos)) + fileName.substr(extPos);
        } else {
            fileName = fileName.substr(0, maxFileNameLen);
        }
    }

    std::wstring finalTempPath = std::wstring(tempDir) + L"ghv_" + guidStr + L"_" + fileName;
    ctx->tempFilePath = finalTempPath;

    if (!isWrite) {
        std::wstring ref = pathInfo.ref;
        if (ref.empty()) {
            try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }
        }
        if (ref.empty()) ref = L"main";

        OutputDebugStringW((L"[GitHubVFS] VFS_CreateFileW: downloading " + pathInfo.owner + L"/" + pathInfo.repo +
            L"/" + pathInfo.path + L" ref=" + ref + L"\n").c_str());
        LogToFile(L"VFS_CreateFileW: \u4e0b\u8f7d " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + pathInfo.path);

        std::vector<BYTE> fileData;
        bool downloadOk = false;
        try { downloadOk = GitHubClient::DownloadFile(pathInfo.owner, pathInfo.repo, pathInfo.path, fileData, ref); } catch (...) { downloadOk = false; }
        if (!downloadOk) {
            OutputDebugStringW(L"[GitHubVFS] DownloadFile failed, trying download_url\n");
            fileData.clear();
            try { downloadOk = GitHubClient::DownloadFileByUrl(pathInfo.owner, pathInfo.repo, pathInfo.path, fileData, ref); } catch (...) { downloadOk = false; }
        }

        if (!downloadOk || fileData.empty()) {
            OutputDebugStringW(L"[GitHubVFS] All download methods failed\n");
            LogToFile(L"VFS_CreateFileW: \u4e0b\u8f7d\u5931\u8d25 " + pathInfo.path);
            DeleteFileW(finalTempPath.c_str());
            delete ctx;
            SetLastError(ERROR_FILE_NOT_FOUND);
            return NULL;
        }

        GitHubConfig& cfg = GitHubClient::GetConfig();
        if (cfg.largeFileWarn && cfg.largeFileSizeMB > 0) {
            size_t warnThreshold = (size_t)cfg.largeFileSizeMB * 1024 * 1024;
            if (fileData.size() > warnThreshold) {
                std::wstring sizeStr = FormatFileSize(fileData.size());
                LogToFile(L"VFS_CreateFileW: \u5927\u6587\u4ef6\u4e0b\u8f7d " + sizeStr + L" " + pathInfo.path);
            }
        }

        HANDLE hFile = CreateFileW(finalTempPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            delete ctx;
            SetLastError(ERROR_FILE_NOT_FOUND);
            return NULL;
        }
        DWORD written = 0;
        WriteFile(hFile, fileData.data(), (DWORD)fileData.size(), &written, NULL);
        CloseHandle(hFile);

        if (written != (DWORD)fileData.size()) {
            DeleteFileW(finalTempPath.c_str());
            delete ctx;
            SetLastError(ERROR_IO_DEVICE);
            return NULL;
        }

        LogToFile(L"VFS_CreateFileW: \u4e0b\u8f7d\u5b8c\u6210 " + FormatFileSize(fileData.size()) + L" " + pathInfo.path);
        OutputDebugStringW((L"[GitHubVFS] Downloaded " + std::to_wstring(fileData.size()) + L" bytes to temp file\n").c_str());
    }

    HANDLE hTemp = CreateFileW(finalTempPath.c_str(), GENERIC_READ | (isWrite ? GENERIC_WRITE : 0),
                               FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTemp == INVALID_HANDLE_VALUE) {
        DeleteFileW(finalTempPath.c_str());
        delete ctx;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return NULL;
    }

    ctx->hTempFile = hTemp;

    if (!isWrite) {
        GetFileTime(hTemp, NULL, NULL, &ctx->originalModTime);
    }

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                         LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
                                                         DWORD dwFlags, LPFILETIME lpFT) {
    __try {
        return VFS_CreateFileW_Impl(lpFuncData, lpszFile, dwMode, dwFlagsAndAttr, dwFlags, lpFT);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_CreateFileW: Access Violation caught\n");
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }
}

static BOOL VFS_ReadFile_Impl(HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    GitHubContentInfo* ctx = (GitHubContentInfo*)hFile;
    if (!ctx) return FALSE;

    if (ctx->virtualType != VIRTUAL_FILE_NONE || ctx->hTempFile == NULL) {
        if (lpdwReadSize) *lpdwReadSize = 0;
        if (ctx && ctx->readPos < ctx->data.size()) {
            DWORD bytesToRead = min(dwSize, (DWORD)(ctx->data.size() - ctx->readPos));
            if (bytesToRead > 0 && lpData) {
                CopyMemory(lpData, ctx->data.data() + ctx->readPos, bytesToRead);
                ctx->readPos += bytesToRead;
                if (lpdwReadSize) *lpdwReadSize = bytesToRead;
            }
        }
        return TRUE;
    }

    if (ctx->isWrite) return FALSE;

    DWORD bytesRead = 0;
    BOOL res = ::ReadFile(ctx->hTempFile, lpData, dwSize, &bytesRead, NULL);
    if (lpdwReadSize) *lpdwReadSize = bytesRead;
    return res;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    __try {
        return VFS_ReadFile_Impl(hFile, lpData, dwSize, lpdwReadSize);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

static BOOL VFS_WriteFile_Impl(HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize) {
    GitHubContentInfo* ctx = (GitHubContentInfo*)hFile;
    if (!ctx || !ctx->isWrite || ctx->hTempFile == NULL) return FALSE;

    if (lpdwWriteSize) *lpdwWriteSize = 0;
    if (dwSize == 0 || !lpData) return TRUE;

    DWORD written = 0;
    BOOL res = ::WriteFile(ctx->hTempFile, lpData, dwSize, &written, NULL);
    if (res && written > 0) {
        if (!ctx->modified) {
            LogToFile(L"VFS_WriteFile: \u5f00\u59cb\u4fee\u6539 " + ctx->owner + L"/" + ctx->repo + L"/" + ctx->path);
        }
        ctx->modified = true;
        if (lpdwWriteSize) *lpdwWriteSize = written;
    }
    if (fFlush) FlushFileBuffers(ctx->hTempFile);
    return res;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush,
                                                     LPDWORD lpdwWriteSize) {
    __try {
        return VFS_WriteFile_Impl(hFile, lpData, dwSize, fFlush, lpdwWriteSize);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

static BOOL VFS_SeekFile_Impl(HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos) {
    GitHubContentInfo* ctx = (GitHubContentInfo*)hFile;
    if (!ctx) return FALSE;

    if (ctx->virtualType != VIRTUAL_FILE_NONE || ctx->hTempFile == NULL) {
        __int64 newPos = 0;
        switch (dwMethod) {
        case FILE_BEGIN: newPos = iPos; break;
        case FILE_CURRENT: newPos = ctx->readPos + iPos; break;
        case FILE_END: newPos = ctx->data.size() + iPos; break;
        default: return FALSE;
        }
        if (newPos < 0) return FALSE;
        ctx->readPos = (size_t)min(newPos, (__int64)ctx->data.size());
        if (piNewPos) *piNewPos = ctx->readPos;
        return TRUE;
    }

    LARGE_INTEGER liPos;
    liPos.QuadPart = iPos;
    DWORD method = FILE_BEGIN;
    if (dwMethod == FILE_CURRENT) method = FILE_CURRENT;
    else if (dwMethod == FILE_END) method = FILE_END;

    LARGE_INTEGER newPos;
    BOOL res = SetFilePointerEx(ctx->hTempFile, liPos, &newPos, method);
    if (res && piNewPos) *piNewPos = newPos.QuadPart;
    return res;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                    HANDLE hFile, __int64 iPos, DWORD dwMethod,
                                                    DWORD dwFlags, unsigned __int64* piNewPos) {
    __try {
        return VFS_SeekFile_Impl(hFile, iPos, dwMethod, dwFlags, piNewPos);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

static void VFS_CloseFile_Impl(HANDLE hFile) {
    GitHubContentInfo* ctx = (GitHubContentInfo*)hFile;
    if (!ctx) return;

    if (ctx->virtualType != VIRTUAL_FILE_NONE) {
        delete ctx;
        return;
    }

    if (ctx->hTempFile) {
        CloseHandle(ctx->hTempFile);
        ctx->hTempFile = NULL;
    }

    if (ctx->isWrite && ctx->modified && !ctx->tempFilePath.empty()) {
        LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u4fee\u6539 " + ctx->owner + L"/" + ctx->repo + L"/" + ctx->path);
        OutputDebugStringW((L"[GitHubVFS] VFS_CloseFile: uploading modified file " + ctx->path + L"\n").c_str());

        HANDLE hFile2 = CreateFileW(ctx->tempFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile2 != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fileSize;
            GetFileSizeEx(hFile2, &fileSize);
            if (fileSize.QuadPart >= 0 && fileSize.QuadPart <= 0xFFFFFFFF) {
                std::vector<BYTE> fileData;
                if (fileSize.QuadPart > 0) {
                    fileData.resize((size_t)fileSize.QuadPart);
                    DWORD totalRead = 0;
                    BOOL readOk = TRUE;
                    while (totalRead < (DWORD)fileData.size()) {
                        DWORD bytesRead = 0;
                        DWORD toRead = (DWORD)fileData.size() - totalRead;
                        if (!::ReadFile(hFile2, fileData.data() + totalRead, toRead, &bytesRead, NULL) || bytesRead == 0) {
                            readOk = FALSE;
                            break;
                        }
                        totalRead += bytesRead;
                    }
                    CloseHandle(hFile2);
                    if (!readOk || totalRead != (DWORD)fileData.size()) {
                        LogToFile(L"VFS_CloseFile: \u8bfb\u53d6\u4e34\u65f6\u6587\u4ef6\u5931\u8d25 " + ctx->path);
                        OutputDebugStringW(L"[GitHubVFS] VFS_CloseFile: failed to read temp file\n");
                    } else {
                        GitHubConfig& cfg = GitHubClient::GetConfig();
                        std::wstring message;
                        if (!cfg.commitMessageTemplate.empty()) {
                            message = cfg.commitMessageTemplate + L": " + ctx->path;
                        } else {
                            message = L"\u901a\u8fc7 GitHubVFS \u4e0a\u4f20: " + ctx->path;
                        }

                        GitHubConfig& cfgRef = GitHubClient::GetConfig();
                        if (cfgRef.largeFileWarn && cfgRef.largeFileSizeMB > 0) {
                            size_t warnThreshold = (size_t)cfgRef.largeFileSizeMB * 1024 * 1024;
                            if (fileData.size() > warnThreshold) {
                                LogToFile(L"VFS_CloseFile: \u5927\u6587\u4ef6\u4e0a\u4f20 " + FormatFileSize(fileData.size()) + L" " + ctx->path);
                            }
                        }

                        try {
                            bool uploadOk = GitHubClient::UploadFile(ctx->owner, ctx->repo, ctx->path, fileData, message);
                            if (uploadOk) {
                                LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u6210\u529f " + FormatFileSize(fileData.size()) + L" " + ctx->path);
                            } else {
                                LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u5931\u8d25 " + ctx->path);
                            }
                        } catch (const std::exception& e) {
                            LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u5f02\u5e38 " + Utf8ToWide_safe(e.what()) + L" " + ctx->path);
                            OutputDebugStringW(L"[GitHubVFS] UploadFile failed\n");
                        } catch (...) {
                            LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u5f02\u5e38 " + ctx->path);
                            OutputDebugStringW(L"[GitHubVFS] UploadFile failed\n");
                        }
                    }
                } else {
                    CloseHandle(hFile2);
                    GitHubConfig& cfg = GitHubClient::GetConfig();
                    std::wstring message;
                    if (!cfg.commitMessageTemplate.empty()) {
                        message = cfg.commitMessageTemplate + L": " + ctx->path;
                    } else {
                        message = L"\u901a\u8fc7 GitHubVFS \u4e0a\u4f20: " + ctx->path;
                    }
                    try {
                        bool uploadOk = GitHubClient::UploadFile(ctx->owner, ctx->repo, ctx->path, fileData, message);
                        if (uploadOk) {
                            LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u6210\u529f (\u7a7a\u6587\u4ef6) " + ctx->path);
                        } else {
                            LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u5931\u8d25 (\u7a7a\u6587\u4ef6) " + ctx->path);
                        }
                    } catch (...) {
                        LogToFile(L"VFS_CloseFile: \u4e0a\u4f20\u5f02\u5e38 (\u7a7a\u6587\u4ef6) " + ctx->path);
                        OutputDebugStringW(L"[GitHubVFS] UploadFile failed\n");
                    }
                }
            } else {
                CloseHandle(hFile2);
                LogToFile(L"VFS_CloseFile: \u6587\u4ef6\u8fc7\u5927\u65e0\u6cd5\u4e0a\u4f20 " + FormatFileSize((size_t)fileSize.QuadPart) + L" " + ctx->path);
                OutputDebugStringW(L"[GitHubVFS] VFS_CloseFile: file too large to upload\n");
            }
        } else {
            LogToFile(L"VFS_CloseFile: \u65e0\u6cd5\u6253\u5f00\u4e34\u65f6\u6587\u4ef6 " + ctx->tempFilePath);
        }
    }

    if (!ctx->tempFilePath.empty()) {
        DeleteFileW(ctx->tempFilePath.c_str());
    }

    delete ctx;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    __try {
        VFS_CloseFile_Impl(hFile);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    if (!IsGitHubVfsPath(lpszFile)) return FALSE;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszFile);
    if (pathInfo.repo.empty() || pathInfo.path.empty()) return FALSE;

    GitHubFileInfo fileInfo;
    if (!GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) return FALSE;
    if (fileInfo.isDir) return FALSE;

    GitHubConfig& cfg = GitHubClient::GetConfig();
    if (cfg.largeFileWarn) {
        std::wstring fileName = pathInfo.path;
        size_t lastSlash = fileName.find_last_of(L'/');
        if (lastSlash != std::wstring::npos) fileName = fileName.substr(lastSlash + 1);
        LogToFile(L"VFS_DeleteFileW: \u5220\u9664\u6587\u4ef6 " + fileName + L" (" + FormatFileSize(fileInfo.size) + L")");
    }

    std::wstring message;
    if (!cfg.commitMessageTemplate.empty()) {
        message = cfg.commitMessageTemplate + L": \u5220\u9664 " + pathInfo.path;
    } else {
        message = L"\u901a\u8fc7 GitHubVFS \u5220\u9664: " + pathInfo.path;
    }

    bool result = GitHubClient::DeleteGitHubFile(pathInfo.owner, pathInfo.repo, pathInfo.path, message, fileInfo.sha);
    if (result) {
        LogToFile(L"VFS_DeleteFileW: \u5220\u9664\u6210\u529f " + pathInfo.path);
        GitHubClient::InvalidateCache();
    } else {
        LogToFile(L"VFS_DeleteFileW: \u5220\u9664\u5931\u8d25 " + pathInfo.path);
    }
    return result;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    if (!IsGitHubVfsPath(lpszPath)) return FALSE;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);
    if (pathInfo.repo.empty()) return FALSE;

    LogToFile(L"VFS_CreateDirectoryW: \u521b\u5efa\u76ee\u5f55 " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + pathInfo.path);

    GitHubConfig& cfg = GitHubClient::GetConfig();
    std::wstring message;
    if (!cfg.commitMessageTemplate.empty()) {
        message = cfg.commitMessageTemplate + L": \u521b\u5efa\u76ee\u5f55 " + pathInfo.path;
    } else {
        message = L"\u901a\u8fc7 GitHubVFS \u521b\u5efa\u76ee\u5f55: " + pathInfo.path;
    }
    bool result = GitHubClient::CreateGitHubDir(pathInfo.owner, pathInfo.repo, pathInfo.path, message);
    if (result) {
        LogToFile(L"VFS_CreateDirectoryW: \u521b\u5efa\u6210\u529f " + pathInfo.path);
    } else {
        LogToFile(L"VFS_CreateDirectoryW: \u521b\u5efa\u5931\u8d25 " + pathInfo.path);
    }
    return result;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    if (!IsGitHubVfsPath(lpszPath)) return FALSE;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);
    if (pathInfo.repo.empty() || pathInfo.path.empty()) return FALSE;

    LogToFile(L"VFS_RemoveDirectoryW: \u5220\u9664\u76ee\u5f55 " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + pathInfo.path);

    std::wstring ref;
    try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }
    if (ref.empty()) ref = L"main";

    GitHubConfig& cfg = GitHubClient::GetConfig();
    std::wstring message;
    if (!cfg.commitMessageTemplate.empty()) {
        message = cfg.commitMessageTemplate + L": \u5220\u9664\u76ee\u5f55 " + pathInfo.path;
    } else {
        message = L"\u901a\u8fc7 GitHubVFS \u5220\u9664\u76ee\u5f55: " + pathInfo.path;
    }

    std::vector<GitHubFileInfo> files = GitHubClient::ListDirectory(pathInfo.owner, pathInfo.repo, pathInfo.path, ref);
    bool allOk = true;
    for (const auto& fi : files) {
        if (fi.isDir) {
            WCHAR subVfsPath[MAX_PATH * 2];
            StringCchPrintfW(subVfsPath, MAX_PATH * 2, L"github:///%s/%s/%s",
                pathInfo.owner.c_str(), pathInfo.repo.c_str(), fi.path.c_str());
            if (!VFS_RemoveDirectoryW(hVFSData, lpFuncData, subVfsPath)) {
                allOk = false;
            }
        } else {
            std::wstring fileMessage;
            if (!cfg.commitMessageTemplate.empty()) {
                fileMessage = cfg.commitMessageTemplate + L": \u5220\u9664 " + fi.path;
            } else {
                fileMessage = L"\u901a\u8fc7 GitHubVFS \u5220\u9664: " + fi.path;
            }
            if (!GitHubClient::DeleteGitHubFile(pathInfo.owner, pathInfo.repo, fi.path, fileMessage, fi.sha)) {
                LogToFile(L"VFS_RemoveDirectoryW: \u5220\u9664\u6587\u4ef6\u5931\u8d25 " + fi.path);
                allOk = false;
            } else {
                LogToFile(L"VFS_RemoveDirectoryW: \u5220\u9664\u6587\u4ef6\u6210\u529f " + fi.path);
            }
        }
    }

    if (allOk) {
        LogToFile(L"VFS_RemoveDirectoryW: \u5220\u9664\u76ee\u5f55\u6210\u529f " + pathInfo.path);
        GitHubClient::InvalidateCache();
    } else {
        LogToFile(L"VFS_RemoveDirectoryW: \u5220\u9664\u76ee\u5f55\u90e8\u5206\u5931\u8d25 " + pathInfo.path);
    }
    return allOk;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                       LPWSTR lpszOldName, LPWSTR lpszNewName) {
    if (!IsGitHubVfsPath(lpszOldName) || !IsGitHubVfsPath(lpszNewName)) return FALSE;

    GitHubPathInfo oldInfo = ParseGitHubPath(lpszOldName);
    GitHubPathInfo newInfo = ParseGitHubPath(lpszNewName);

    if (oldInfo.owner != newInfo.owner || oldInfo.repo != newInfo.repo) return FALSE;
    if (oldInfo.path.empty()) return FALSE;

    LogToFile(L"VFS_RenameFileW: \u91cd\u547d\u540d " + oldInfo.path + L" -> " + newInfo.path);

    GitHubFileInfo fileInfo;
    if (!GitHubClient::GetFileInfo(oldInfo.owner, oldInfo.repo, oldInfo.path, fileInfo)) return FALSE;

    GitHubConfig& cfg = GitHubClient::GetConfig();
    std::wstring message;
    if (!cfg.commitMessageTemplate.empty()) {
        message = cfg.commitMessageTemplate + L": \u91cd\u547d\u540d " + oldInfo.path + L" -> " + newInfo.path;
    } else {
        message = L"\u901a\u8fc7 GitHubVFS \u91cd\u547d\u540d: " + oldInfo.path + L" -> " + newInfo.path;
    }

    if (fileInfo.isDir) {
        std::wstring ref;
        try { ref = GitHubClient::GetDefaultBranch(oldInfo.owner, oldInfo.repo); } catch (...) { ref = L"main"; }
        if (ref.empty()) ref = L"main";

        std::function<void(const std::wstring&, const std::vector<GitHubFileInfo>&, bool&)> moveDirContents;
        moveDirContents = [&](const std::wstring& dirPath, const std::vector<GitHubFileInfo>& items, bool& ok) {
            for (const auto& fi : items) {
                std::wstring oldSubPath = fi.path;
                std::wstring newSubPath = newInfo.path + oldSubPath.substr(oldInfo.path.length());
                if (fi.isDir) {
                    std::vector<GitHubFileInfo> subFiles = GitHubClient::ListDirectory(oldInfo.owner, oldInfo.repo, oldSubPath, ref);
                    moveDirContents(oldSubPath, subFiles, ok);
                } else {
                    if (!GitHubClient::MoveGitHubFile(oldInfo.owner, oldInfo.repo, oldSubPath, newSubPath, message, fi.sha, ref)) {
                        LogToFile(L"VFS_RenameFileW: \u79fb\u52a8\u6587\u4ef6\u5931\u8d25 " + oldSubPath + L" -> " + newSubPath);
                        ok = false;
                    } else {
                        LogToFile(L"VFS_RenameFileW: \u79fb\u52a8\u6587\u4ef6\u6210\u529f " + oldSubPath + L" -> " + newSubPath);
                    }
                }
            }
        };

        std::vector<GitHubFileInfo> files = GitHubClient::ListDirectory(oldInfo.owner, oldInfo.repo, oldInfo.path, ref);
        bool allOk = true;
        moveDirContents(oldInfo.path, files, allOk);

        if (allOk) {
            LogToFile(L"VFS_RenameFileW: \u76ee\u5f55\u91cd\u547d\u540d\u6210\u529f " + oldInfo.path + L" -> " + newInfo.path);
            GitHubClient::InvalidateCache();
        } else {
            LogToFile(L"VFS_RenameFileW: \u76ee\u5f55\u91cd\u547d\u540d\u90e8\u5206\u5931\u8d25 " + oldInfo.path + L" -> " + newInfo.path);
        }
        return allOk;
    }

    bool result = GitHubClient::MoveGitHubFile(oldInfo.owner, oldInfo.repo, oldInfo.path, newInfo.path,
                                   message, fileInfo.sha);
    if (result) {
        LogToFile(L"VFS_RenameFileW: \u91cd\u547d\u540d\u6210\u529f " + oldInfo.path + L" -> " + newInfo.path);
        GitHubClient::InvalidateCache();
    } else {
        LogToFile(L"VFS_RenameFileW: \u91cd\u547d\u540d\u5931\u8d25 " + oldInfo.path + L" -> " + newInfo.path);
    }
    return result;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return VFS_RenameFileW(hVFSData, lpFuncData, lpszOldName, lpszNewName);
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath,
                                                               LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;

    OutputDebugStringW(L"[GitHubVFS] VFS_GetPathDisplayNameW enter\n");

    if (IsGitHubRootPath(lpszPath)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"GitHub");
        return TRUE;
    }

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);

    switch (pathInfo.context) {
    case GITHUB_CTX_ROOT:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"GitHub");
        return TRUE;

    case GITHUB_CTX_REPOS:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4ed3\u5e93");
        return TRUE;

    case GITHUB_CTX_REPOS_OWNER:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.owner.c_str());
        return TRUE;

    case GITHUB_CTX_STARRED:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u661f\u6807");
        return TRUE;

    case GITHUB_CTX_STARRED_OWNER:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.owner.c_str());
        return TRUE;

    case GITHUB_CTX_SUBSCRIPTIONS:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u5173\u6ce8");
        return TRUE;

    case GITHUB_CTX_SUBSCRIPTIONS_OWNER:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.owner.c_str());
        return TRUE;

    case GITHUB_CTX_NOTIFICATIONS:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u901a\u77e5");
        return TRUE;

    case GITHUB_CTX_GISTS:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Gists");
        return TRUE;

    case GITHUB_CTX_SEARCH_CODE:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4ee3\u7801\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"\u4ee3\u7801: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_USERS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u7528\u6237\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"\u7528\u6237: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_ISSUES:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Issues \u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"Issues: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_REPOS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4ed3\u5e93\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"\u4ed3\u5e93: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_COMMITS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u63d0\u4ea4\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"\u63d0\u4ea4: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_TOPICS:
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4e3b\u9898\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"\u4e3b\u9898: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;

    case GITHUB_CTX_SEARCH_CENTER:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u641c\u7d22");
        return TRUE;

    case GITHUB_CTX_TRENDING:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u8d8b\u52bf");
        return TRUE;

    case GITHUB_CTX_OWNER:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.owner.c_str());
        return TRUE;

    case GITHUB_CTX_REPO:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.repo.c_str());
        return TRUE;

    case GITHUB_CTX_CODE:
        if (pathInfo.path.empty()) {
            if (pathInfo.ref.empty()) {
                StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.repo.c_str());
            } else {
                StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"@%s", pathInfo.ref.c_str());
            }
        } else {
            std::wstring nameStr = pathInfo.path;
            size_t pos = nameStr.find_last_of(L'/');
            if (pos != std::wstring::npos) nameStr = nameStr.substr(pos + 1);
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, nameStr.c_str());
        }
        return TRUE;

    case GITHUB_CTX_ISSUES:
        if (pathInfo.issueNumber > 0) {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"Issue #%d", pathInfo.issueNumber);
        } else {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Issues");
        }
        return TRUE;

    case GITHUB_CTX_PULLS:
        if (pathInfo.issueNumber > 0) {
            StringCchPrintfW(lpszDisplayName, cbDisplayNameMax, L"PR #%d", pathInfo.issueNumber);
        } else {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Pull Requests");
        }
        return TRUE;

    case GITHUB_CTX_BRANCHES:
        if (!pathInfo.ref.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.ref.c_str());
        } else {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u5206\u652f");
        }
        return TRUE;

    case GITHUB_CTX_TAGS:
        if (!pathInfo.ref.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, pathInfo.ref.c_str());
        } else {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u6807\u7b7e");
        }
        return TRUE;

    case GITHUB_CTX_RELEASES:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u53d1\u5e03");
        return TRUE;

    default:
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"GitHub");
        return TRUE;
    }
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
                                                              LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;

    if (fRoot) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, GITHUB_VFS_PREFIX);
        return TRUE;
    }

    if (IsGitHubRootPath(lpszPath)) return FALSE;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);

    if (pathInfo.context == GITHUB_CTX_ROOT) return FALSE;

    switch (pathInfo.context) {
    case GITHUB_CTX_REPOS:
    case GITHUB_CTX_REPOS_OWNER:
    case GITHUB_CTX_STARRED:
    case GITHUB_CTX_STARRED_OWNER:
    case GITHUB_CTX_SUBSCRIPTIONS:
    case GITHUB_CTX_SUBSCRIPTIONS_OWNER:
    case GITHUB_CTX_NOTIFICATIONS:
    case GITHUB_CTX_GISTS:
    case GITHUB_CTX_SEARCH_CODE:
    case GITHUB_CTX_SEARCH_USERS:
    case GITHUB_CTX_SEARCH_ISSUES:
    case GITHUB_CTX_SEARCH_REPOS:
    case GITHUB_CTX_SEARCH_COMMITS:
    case GITHUB_CTX_SEARCH_TOPICS:
    case GITHUB_CTX_SEARCH_OWNER:
    case GITHUB_CTX_SEARCH_CENTER:
    case GITHUB_CTX_TRENDING:
        StringCchCopyW(lpszNewPath, cbNewPathMax, GITHUB_VFS_PREFIX);
        return TRUE;

    case GITHUB_CTX_OWNER:
        StringCchCopyW(lpszNewPath, cbNewPathMax, GITHUB_VFS_PREFIX);
        return TRUE;

    case GITHUB_CTX_REPO:
        StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s", pathInfo.owner.c_str());
        return TRUE;

    case GITHUB_CTX_CODE:
        if (pathInfo.path.empty()) {
            StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s",
                pathInfo.owner.c_str(), pathInfo.repo.c_str());
        } else {
            size_t lastSlash = pathInfo.path.find_last_of(L'/');
            if (lastSlash != std::wstring::npos) {
                std::wstring parentPath = pathInfo.path.substr(0, lastSlash);
                if (pathInfo.ref.empty()) {
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str(), parentPath.c_str());
                } else {
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/@%s/%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str(),
                        pathInfo.ref.c_str(), parentPath.c_str());
                }
            } else {
                if (pathInfo.ref.empty()) {
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str());
                } else {
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/@%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str(), pathInfo.ref.c_str());
                }
            }
        }
        return TRUE;

    case GITHUB_CTX_ISSUES:
    case GITHUB_CTX_PULLS:
        if (pathInfo.issueNumber > 0) {
            StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/%s",
                pathInfo.owner.c_str(), pathInfo.repo.c_str(),
                pathInfo.context == GITHUB_CTX_ISSUES ? L"Issues" : L"Pulls");
        } else {
            StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s",
                pathInfo.owner.c_str(), pathInfo.repo.c_str());
        }
        return TRUE;

    case GITHUB_CTX_BRANCHES:
    case GITHUB_CTX_TAGS:
        if (!pathInfo.ref.empty()) {
            if (pathInfo.path.empty()) {
                StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/%s",
                    pathInfo.owner.c_str(), pathInfo.repo.c_str(),
                    pathInfo.context == GITHUB_CTX_BRANCHES ? L"Branches" : L"Tags");
            } else {
                size_t lastSlash = pathInfo.path.find_last_of(L'/');
                if (lastSlash != std::wstring::npos) {
                    std::wstring parentPath = pathInfo.path.substr(0, lastSlash);
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/%s/%s/%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str(),
                        pathInfo.context == GITHUB_CTX_BRANCHES ? L"Branches" : L"Tags",
                        pathInfo.ref.c_str(), parentPath.c_str());
                } else {
                    StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s/%s/%s",
                        pathInfo.owner.c_str(), pathInfo.repo.c_str(),
                        pathInfo.context == GITHUB_CTX_BRANCHES ? L"Branches" : L"Tags",
                        pathInfo.ref.c_str());
                }
            }
        } else {
            StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s",
                pathInfo.owner.c_str(), pathInfo.repo.c_str());
        }
        return TRUE;

    case GITHUB_CTX_RELEASES:
        StringCchPrintfW(lpszNewPath, cbNewPathMax, L"github:///%s/%s",
            pathInfo.owner.c_str(), pathInfo.repo.c_str());
        return TRUE;

    default:
        StringCchCopyW(lpszNewPath, cbNewPathMax, GITHUB_VFS_PREFIX);
        return TRUE;
    }
}

static INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void ShowPropertiesDialog(HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszFiles);

    if (pathInfo.repo.empty()) return;

    if (pathInfo.path.empty()) {
        GitHubRepoInfo repoInfo;
        if (!GitHubClient::GetRepoInfo(pathInfo.owner, pathInfo.repo, repoInfo)) {
            MessageBoxW(hwndParent,
                        L"\u65e0\u6cd5\u83b7\u53d6\u4ed3\u5e93\u4fe1\u606f\u3002\n\u53ef\u80fd\u7f51\u7edc\u4e0d\u53ef\u7528\u6216 Token \u65e0\u6548\u3002",
                        L"GitHub VFS \u9519\u8bef",
                        MB_ICONERROR | MB_OK);
            return;
        }

        WCHAR msg[4096];
        StringCchPrintfW(msg, 4096,
                         L"\u4ed3\u5e93\u540d\u79f0:\t%s\n"
                         L"\u63cf\u8ff0:\t%s\n"
                         L"\u8bed\u8a00:\t%s\n"
                         L"\u53ef\u89c1\u6027:\t%s\n"
                         L"\u661f\u6807\u6570:\t%I64d\n"
                         L"Fork\u6570:\t%I64d\n"
                         L"Issue\u6570:\t%I64d\n"
                         L"\u9ed8\u8ba4\u5206\u652f:\t%s\n"
                         L"\u5927\u5c0f:\t%s\n"
                         L"\u514b\u9686URL:\t%s\n"
                         L"\u94fe\u63a5:\t%s",
                         repoInfo.fullName.c_str(),
                         repoInfo.description.empty() ? L"-" : repoInfo.description.c_str(),
                         repoInfo.language.empty() ? L"-" : repoInfo.language.c_str(),
                         GetVisibilityText(repoInfo.isPrivate).c_str(),
                         repoInfo.stargazersCount,
                         repoInfo.forksCount,
                         repoInfo.openIssuesCount,
                         repoInfo.defaultBranch.c_str(),
                         FormatFileSize(repoInfo.size).c_str(),
                         repoInfo.cloneUrl.c_str(),
                         repoInfo.htmlUrl.c_str());

        MessageBoxW(hwndParent, msg, L"GitHub \u4ed3\u5e93\u5c5e\u6027", MB_ICONINFORMATION | MB_OK);
    } else {
        GitHubFileInfo fileInfo;
        if (GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) {
            std::wstring ref;
            try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }

            std::wstring githubUrl = L"https://github.com/" + pathInfo.owner + L"/" + pathInfo.repo +
                L"/blob/" + ref + L"/" + pathInfo.path;

            WCHAR msg[4096];
            StringCchPrintfW(msg, 4096,
                             L"\u6587\u4ef6\u540d:\t%s\n"
                             L"\u8def\u5f84:\t%s\n"
                             L"\u7c7b\u578b:\t%s\n"
                             L"\u5927\u5c0f:\t%s\n"
                             L"SHA:\t%s\n"
                             L"\u4ed3\u5e93:\t%s/%s\n"
                             L"\u5206\u652f:\t%s\n"
                             L"\u94fe\u63a5:\t%s",
                             fileInfo.name.c_str(),
                             fileInfo.path.c_str(),
                             fileInfo.isDir ? L"\u76ee\u5f55" : L"\u6587\u4ef6",
                             fileInfo.isDir ? L"-" : FormatFileSize(fileInfo.size).c_str(),
                             fileInfo.sha.c_str(),
                             pathInfo.owner.c_str(),
                             pathInfo.repo.c_str(),
                             ref.c_str(),
                             githubUrl.c_str());

            MessageBoxW(hwndParent, msg, L"GitHub \u6587\u4ef6\u5c5e\u6027", MB_ICONINFORMATION | MB_OK);
        }
    }
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                           LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData) {
    if (!lpszFiles || !lpMenuData) return FALSE;

    OutputDebugStringW(L"[GitHubVFS] VFS_GetContextMenuW enter\n");
    LogToFile(std::wstring(L"VFS_GetContextMenuW: ") + lpszFiles);

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszFiles);

    // 使用线程安全的局部 vector（移除 static 以避免多线程并发访问问题）
        std::vector<VFSCONTEXTMENUITEMW> menuItems;
    menuItems.reserve(16);

    auto AddItem = [&](DWORD flags, const WCHAR* label, const WCHAR* cmd) {
        VFSCONTEXTMENUITEMW item = { sizeof(VFSCONTEXTMENUITEMW), flags, const_cast<LPWSTR>(label), const_cast<LPWSTR>(cmd) };
        menuItems.push_back(item);
    };

    auto AddSep = [&]() {
        AddItem(VFSCMF_SEPARATOR, L"", L"");
    };

    bool isInRepo = !pathInfo.repo.empty();
    bool hasFilePath = !pathInfo.path.empty();

    bool isDir = false;
    if (hasFilePath) {
        std::wstring owner = pathInfo.query.owner.empty() ? 
            (pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner) : 
            pathInfo.query.owner;
        GitHubFileInfo fi;
        if (GitHubClient::GetFileInfo(owner, pathInfo.repo, pathInfo.path, fi)) {
            isDir = fi.isDir;
        }
    }

    if (pathInfo.context == GITHUB_CTX_ROOT) {
        AddItem(0, L"🔍 搜索仓库...(&S)", L"$gh_search_repos");
        AddItem(0, L"🔍 搜索代码...(&C)", L"$gh_search_code");
        AddItem(0, L"🔍 搜索用户...(&U)", L"$gh_search_users");
        AddItem(0, L"🔍 搜索 Issues...(&I)", L"$gh_search_issues");
        AddSep();
        AddItem(0, L"⭐ 我的星标仓库", L"$gh_view_starred");
        AddItem(0, L"👁️ 我关注的仓库", L"$gh_view_watched");
        AddItem(0, L"🔔 我的通知", L"$gh_view_notifications");
        AddItem(0, L"📝 我的 Gists", L"$gh_view_gists");
        AddItem(0, L"🔥 Trending", L"$gh_view_trending");
        AddSep();
        AddItem(0, L"🔄 刷新缓存(&R)", L"$gh_refresh");
        AddSep();
        AddItem(0, L"⚙️ 插件设置...(&P)", L"$gh_config");
    }
    else if (pathInfo.context == GITHUB_CTX_REPOS || pathInfo.context == GITHUB_CTX_STARRED ||
             pathInfo.context == GITHUB_CTX_SUBSCRIPTIONS) {
        AddItem(0, L"\u641c\u7d22\u4ed3\u5e93...(&S)", L"$gh_search");
        AddItem(0, L"\u641c\u7d22\u4ee3\u7801...(&C)", L"$gh_search_code");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_REPOS_OWNER || pathInfo.context == GITHUB_CTX_STARRED_OWNER ||
             pathInfo.context == GITHUB_CTX_SUBSCRIPTIONS_OWNER) {
        AddItem(0, L"\u641c\u7d22\u4ed3\u5e93...(&S)", L"$gh_search");
        AddSep();
        AddItem(0, L"\u5728\u6d4f\u89c8\u5668\u4e2d\u67e5\u770b\u7528\u6237(&B)", L"$gh_open_owner_browser");
        AddItem(0, L"\u590d\u5236\u7528\u6237\u4e3b\u9875 URL(&U)", L"$gh_copy_owner_url");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_SEARCH_CENTER) {
        AddItem(0, L"\u641c\u7d22\u4ed3\u5e93...(&S)", L"$gh_search");
        AddItem(0, L"\u641c\u7d22\u4ee3\u7801...(&C)", L"$gh_search_code");
        AddItem(0, L"\u641c\u7d22\u7528\u6237...(&U)", L"$gh_search_users");
        AddItem(0, L"\u641c\u7d22 Issues...(&I)", L"$gh_search_issues");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_SEARCH_REPOS || pathInfo.context == GITHUB_CTX_SEARCH_CODE ||
             pathInfo.context == GITHUB_CTX_SEARCH_USERS || pathInfo.context == GITHUB_CTX_SEARCH_ISSUES ||
             pathInfo.context == GITHUB_CTX_SEARCH_COMMITS || pathInfo.context == GITHUB_CTX_SEARCH_TOPICS) {
        AddItem(0, L"\u91cd\u65b0\u641c\u7d22...(&S)", L"$gh_search");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_TRENDING) {
        AddItem(0, L"\u6309\u65e5\u6392\u5e8d(&D)", L"$gh_trending_daily");
        AddItem(0, L"\u6309\u5468\u6392\u5e8d(&W)", L"$gh_trending_weekly");
        AddItem(0, L"\u6309\u6708\u6392\u5e8d(&M)", L"$gh_trending_monthly");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_NOTIFICATIONS) {
        AddItem(0, L"\u6807\u8bb0\u5168\u90e8\u5df2\u8bfb(&A)", L"$gh_mark_all_read");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (pathInfo.context == GITHUB_CTX_GISTS) {
        AddItem(0, L"\u521b\u5efa Gist...(&N)", L"$gh_create_gist");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }
    else if (isInRepo && !hasFilePath) {
        AddItem(0, L"\u6253\u5f00(&O)", L"$gh_open");
        AddSep();
        AddItem(0, L"\u590d\u5236(&C)", L"Clipboard COPY");
        AddItem(0, L"\u526a\u5207(&T)", L"Clipboard CUT");
        AddItem(0, L"\u7c98\u8d34(&P)", L"Clipboard PASTE");
        AddSep();
        AddItem(0, L"\u65b0\u5efa\u6587\u4ef6...(&N)", L"$gh_new_file");
        AddItem(0, L"\u65b0\u5efa\u6587\u4ef6\u5939...(&W)", L"$gh_new_folder");
        AddSep();
        AddItem(0, L"\u5728\u6d4f\u89c8\u5668\u4e2d\u6253\u5f00(&B)", L"$gh_open_browser");
        AddItem(0, L"\u590d\u5236\u514b\u9686 URL(&G)", L"$gh_copy_clone_url");
        AddSep();
        AddItem(0, L"\u67e5\u770b Issues(&I)", L"$gh_view_issues");
        AddItem(0, L"\u67e5\u770b Pull Requests(&P)", L"$gh_view_prs");
        AddItem(0, L"\u67e5\u770b Releases(&L)", L"$gh_view_releases");
        AddItem(0, L"\u67e5\u770b\u5206\u652f(&H)", L"$gh_view_branches");
        AddSep();
        AddItem(0, L"\u590d\u5236\u4ed3\u5e93\u5730\u5740(&Y)", L"$gh_copy_repo_url");
        AddItem(0, L"\u590d\u5236 SSH URL(&K)", L"$gh_copy_ssh_url");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
        AddSep();
        AddItem(0, L"\u5c5e\u6027(&O)", L"$gh_properties");
    }
    else if (isInRepo && hasFilePath && isDir) {
        AddItem(0, L"\u6253\u5f00(&O)", L"$gh_open");
        AddSep();
        AddItem(0, L"\u590d\u5236(&C)", L"Clipboard COPY");
        AddItem(0, L"\u526a\u5207(&T)", L"Clipboard CUT");
        AddItem(0, L"\u7c98\u8d34(&P)", L"Clipboard PASTE");
        AddSep();
        AddItem(0, L"\u65b0\u5efa\u6587\u4ef6...(&N)", L"$gh_new_file");
        AddItem(0, L"\u65b0\u5efa\u6587\u4ef6\u5939...(&W)", L"$gh_new_folder");
        AddSep();
        AddItem(0, L"\u91cd\u547d\u540d(&M)", L"Rename");
        AddItem(0, L"\u5220\u9664(&D)", L"Delete");
        AddSep();
        AddItem(0, L"\u5728\u6d4f\u89c8\u5668\u4e2d\u6253\u5f00(&B)", L"$gh_open_browser");
        AddItem(0, L"\u590d\u5236\u76ee\u5f55\u8def\u5f84(&Y)", L"$gh_copy_dir_path");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
        AddSep();
        AddItem(0, L"\u5c5e\u6027(&O)", L"$gh_properties");
    }
    else if (isInRepo && hasFilePath && !isDir) {
        AddItem(0, L"\u6253\u5f00(&O)", L"$gh_open");
        AddItem(0, L"\u4e0b\u8f7d\u5230\u672c\u5730...(&V)", L"$gh_download");
        AddSep();
        AddItem(0, L"\u590d\u5236(&C)", L"Clipboard COPY");
        AddItem(0, L"\u526a\u5207(&T)", L"Clipboard CUT");
        AddSep();
        AddItem(0, L"\u91cd\u547d\u540d(&M)", L"Rename");
        AddItem(0, L"\u5220\u9664(&D)", L"Delete");
        AddSep();
        AddItem(0, L"\u5728\u6d4f\u89c8\u5668\u4e2d\u6253\u5f00(&B)", L"$gh_open_browser");
        AddItem(0, L"\u590d\u5236\u539f\u59cb\u6587\u4ef6 URL(&U)", L"$gh_copy_raw_url");
        AddItem(0, L"\u590d\u5236 SHA(&A)", L"$gh_copy_sha");
        AddItem(0, L"\u590d\u5236\u6587\u4ef6\u8def\u5f84(&Y)", L"$gh_copy_file_path");
        AddSep();
        AddItem(0, L"\u67e5\u770b\u6587\u4ef6\u5386\u53f2(&H)", L"$gh_view_history");
        AddItem(0, L"\u67e5\u770b\u8d23\u5f52(&B)", L"$gh_view_blame");
        AddSep();
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
        AddSep();
        AddItem(0, L"\u5c5e\u6027(&O)", L"$gh_properties");
    }
    else if (isInRepo) {
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
        AddSep();
        AddItem(0, L"\u5c5e\u6027(&O)", L"$gh_properties");
    }
    else {
        AddItem(0, L"\u5237\u65b0\u7f13\u5b58(&R)", L"$gh_refresh");
    }

    if (menuItems.empty()) {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = FALSE;
    lpMenuData->lpCustomItems = menuItems.data();
    lpMenuData->iNumCustomItems = (int)menuItems.size();
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                       LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    LogToFile(std::wstring(L"VFS_ContextVerbW: path=") + lpVerbData->lpszPath +
              (lpVerbData->lpszVerb ? (L" verb=" + std::wstring(lpVerbData->lpszVerb)) : L" verb=(null)"));

    GitHubPathInfo pathInfo = ParseGitHubPath(lpVerbData->lpszPath);

    bool isDefaultOpen = (lpVerbData->lpszVerb == NULL || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0);

    // 只有在默认打开操作（双击）时才自动进入目录，其他菜单命令需要正常处理
    if (isDefaultOpen && (lpVerbData->dwFlags & DOPUSCVF_ISDIR)) {
        if (pathInfo.isDir && pathInfo.context != GITHUB_CTX_ROOT) {
            if (pathInfo.context == GITHUB_CTX_SEARCH_OWNER) {
                WCHAR username[256] = {0};
                if (InputBox(lpVerbData->hwndParent, L"\u641c\u7d22\u7528\u6237\u4ed3\u5e93", L"\u8bf7\u8f93\u5165\u7528\u6237\u540d:", username, 256)) {
                    std::wstring newPath = L"github:///Search/Repos?owner=" + std::wstring(username);
                    StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
                    return VFSCVRES_CHANGEDIR;
                }
                return VFSCVRES_HANDLED;
            }
            
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }
    }

    if (isDefaultOpen && (pathInfo.context == GITHUB_CTX_REPOS || pathInfo.context == GITHUB_CTX_SEARCH_CODE || pathInfo.context == GITHUB_CTX_SEARCH_USERS) && !pathInfo.searchQuery.empty()) {
        std::wstring pathStr = lpVerbData->lpszPath ? lpVerbData->lpszPath : L"";
        std::wstring searchDir = L"github:///" + pathInfo.searchQuery + L"/";
        std::wstring fileName;
        if (pathStr.length() > searchDir.length() && _wcsnicmp(pathStr.c_str(), searchDir.c_str(), searchDir.length()) == 0) {
            fileName = pathStr.substr(searchDir.length());
        }
        if (!fileName.empty() && fileName.front() == L'[') {
            std::lock_guard<std::mutex> lock(g_codeResultsMutex);
            for (const auto& codeItem : g_cachedCodeResults) {
                std::wstring displayName = L"[" + codeItem.repoFullName + L"] " + codeItem.path;
                if (displayName == fileName) {
                    if (!codeItem.htmlUrl.empty()) {
                        ShellExecuteW(NULL, L"open", codeItem.htmlUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    }
                    return VFSCVRES_HANDLED;
                }
            }
        }
    }

    if (isDefaultOpen && !pathInfo.path.empty()) {
        GitHubFileInfo fileInfo;
        if (GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) {
            if (fileInfo.isDir) {
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                return VFSCVRES_CHANGEDIR;
            }
        }
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_search_repos") == 0) {
        WCHAR query[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"搜索 GitHub 仓库", L"请输入搜索关键词:", query, 256)) {
            std::wstring queryStr = query;
            AddSearchHistory(L"repos", queryStr);
            std::wstring newPath = L"github:///?search=repos&q=" + queryStr;
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_search_code") == 0) {
        WCHAR query[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"搜索 GitHub 代码", L"请输入代码搜索关键词:", query, 256)) {
            std::wstring queryStr = query;
            AddSearchHistory(L"code", queryStr);
            std::wstring newPath = L"github:///?search=code&q=" + queryStr;
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_search_users") == 0) {
        WCHAR query[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"搜索 GitHub 用户", L"请输入用户搜索关键词:", query, 256)) {
            std::wstring queryStr = query;
            AddSearchHistory(L"users", queryStr);
            std::wstring newPath = L"github:///?search=users&q=" + queryStr;
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_search_issues") == 0) {
        WCHAR query[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"搜索 GitHub Issues", L"请输入 Issues 搜索关键词", query, 256)) {
            std::wstring queryStr = query;
            AddSearchHistory(L"issues", queryStr);
            std::wstring newPath = L"github:///?search=issues&q=" + queryStr;
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_starred") == 0) {
        std::wstring newPath = L"github:///?view=starred";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_watched") == 0) {
        std::wstring newPath = L"github:///?view=watched";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_notifications") == 0) {
        std::wstring newPath = L"github:///?view=notifications";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_gists") == 0) {
        std::wstring newPath = L"github:///?view=gists";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_trending") == 0) {
        std::wstring newPath = L"github:///?view=trending";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_search") == 0) {
        WCHAR query[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"\u641c\u7d22 GitHub \u4ed3\u5e93", L"\u8bf7\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd:", query, 256)) {
            std::wstring newPath = L"github:///Search/Repos/" + std::wstring(query);
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_open_browser") == 0) {
        std::wstring url;
        if (HasRepoInfoForVerb(pathInfo, url)) {
            ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_clone_url") == 0) {
        GitHubRepoInfo repoInfo;
        if (GitHubClient::GetRepoInfo(pathInfo.owner, pathInfo.repo, repoInfo)) {
            CopyToClipboard(repoInfo.cloneUrl);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_issues") == 0) {
        std::wstring owner = pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner;
        LogToFile(L"gh_view_issues: pathInfo.owner=" + pathInfo.owner + L", pathInfo.repo=" + pathInfo.repo + L", resolved owner=" + owner);
        if (owner.empty()) {
            LogToFile(L"gh_view_issues: owner is empty, cannot navigate");
            return VFSCVRES_HANDLED;
        }
        std::wstring newPath = L"github://" + owner + L"/" + pathInfo.repo + L"/Issues";
        LogToFile(L"gh_view_issues: newPath=" + newPath);
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_prs") == 0) {
        std::wstring owner = pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner;
        LogToFile(L"gh_view_prs: pathInfo.owner=" + pathInfo.owner + L", pathInfo.repo=" + pathInfo.repo + L", resolved owner=" + owner);
        if (owner.empty()) {
            LogToFile(L"gh_view_prs: owner is empty, cannot navigate");
            return VFSCVRES_HANDLED;
        }
        std::wstring newPath = L"github://" + owner + L"/" + pathInfo.repo + L"/Pulls";
        LogToFile(L"gh_view_prs: newPath=" + newPath);
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_releases") == 0) {
        std::wstring owner = pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner;
        LogToFile(L"gh_view_releases: pathInfo.owner=" + pathInfo.owner + L", pathInfo.repo=" + pathInfo.repo + L", resolved owner=" + owner);
        if (owner.empty()) {
            LogToFile(L"gh_view_releases: owner is empty, cannot navigate");
            return VFSCVRES_HANDLED;
        }
        std::wstring newPath = L"github://" + owner + L"/" + pathInfo.repo + L"/Releases";
        LogToFile(L"gh_view_releases: newPath=" + newPath);
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_repo_url") == 0) {
        GitHubRepoInfo repoInfo;
        if (GitHubClient::GetRepoInfo(pathInfo.owner, pathInfo.repo, repoInfo)) {
            CopyToClipboard(repoInfo.htmlUrl);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_raw_url") == 0) {
        std::wstring url = L"https://raw.githubusercontent.com/" + pathInfo.owner + L"/" + pathInfo.repo +
                           L"/" + GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo) +
                           L"/" + pathInfo.path;
        CopyToClipboard(url);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_sha") == 0) {
        GitHubFileInfo fileInfo;
        if (GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) {
            CopyToClipboard(fileInfo.sha);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_refresh") == 0) {
        GitHubClient::InvalidateCache();
        if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        }
        return VFSCVRES_CHANGE;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_open") == 0) {
        if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }
        if (!pathInfo.path.empty()) {
            GitHubFileInfo fileInfo;
            if (GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) {
                if (fileInfo.isDir) {
                    StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                    return VFSCVRES_CHANGEDIR;
                }
            }
        }
        // 如果是仓库根目录，进入仓库
        if (!pathInfo.repo.empty() && pathInfo.path.empty()) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_config") == 0) {
        INT_PTR result = DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_GITHUB_CONFIG),
                                          lpVerbData->hwndParent, ConfigDlgProc, 0);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_open_owner_browser") == 0) {
        std::wstring url = L"https://github.com/" + pathInfo.owner;
        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_owner_url") == 0) {
        std::wstring url = L"https://github.com/" + pathInfo.owner;
        CopyToClipboard(url);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_branches") == 0) {
        std::wstring owner = pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner;
        std::wstring newPath = L"github://" + owner + L"/" + pathInfo.repo + L"/Branches";
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_ssh_url") == 0) {
        GitHubRepoInfo repoInfo;
        if (GitHubClient::GetRepoInfo(pathInfo.owner, pathInfo.repo, repoInfo)) {
            std::wstring sshUrl = L"git@github.com:" + pathInfo.owner + L"/" + pathInfo.repo + L".git";
            CopyToClipboard(sshUrl);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_download") == 0) {
        if (!pathInfo.path.empty()) {
            LogToFile(L"VFS_ContextVerbW: \u4e0b\u8f7d\u5230\u672c\u5730 " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + pathInfo.path);

            std::wstring ref;
            try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }
            if (ref.empty()) ref = L"main";

            std::vector<BYTE> fileData;
            bool downloadOk = false;
            try { downloadOk = GitHubClient::DownloadFile(pathInfo.owner, pathInfo.repo, pathInfo.path, fileData, ref); } catch (...) {}
            if (!downloadOk) {
                try { downloadOk = GitHubClient::DownloadFileByUrl(pathInfo.owner, pathInfo.repo, pathInfo.path, fileData, ref); } catch (...) {}
            }

            if (downloadOk && !fileData.empty()) {
                std::wstring fileName = pathInfo.path;
                size_t lastSlash = fileName.find_last_of(L'/');
                if (lastSlash != std::wstring::npos) fileName = fileName.substr(lastSlash + 1);

                WCHAR savePath[MAX_PATH] = {0};
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = lpVerbData->hwndParent;
                ofn.lpstrFile = savePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"\u6240\u6709\u6587\u4ef6\0*.*\0\0";
                ofn.lpstrDefExt = L"";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                ofn.lpstrTitle = L"\u4fdd\u5b58\u6587\u4ef6";
                StringCchCopyW(savePath, MAX_PATH, fileName.c_str());

                if (GetSaveFileNameW(&ofn)) {
                    HANDLE hFile = CreateFileW(savePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD written = 0;
                        WriteFile(hFile, fileData.data(), (DWORD)fileData.size(), &written, NULL);
                        CloseHandle(hFile);
                        std::wstring sizeStr = FormatFileSize(fileData.size());
                        LogToFile(L"VFS_ContextVerbW: \u4e0b\u8f7d\u5b8c\u6210 " + sizeStr + L" -> " + std::wstring(savePath));
                        MessageBoxW(lpVerbData->hwndParent,
                            (L"\u6587\u4ef6\u5927\u5c0f: " + sizeStr + L"\n\u5df2\u4fdd\u5b58\u5230: " + std::wstring(savePath)).c_str(),
                            L"\u4e0b\u8f7d\u5b8c\u6210", MB_OK | MB_ICONINFORMATION);
                    } else {
                        LogToFile(L"VFS_ContextVerbW: \u65e0\u6cd5\u521b\u5efa\u6587\u4ef6 " + std::wstring(savePath));
                        MessageBoxW(lpVerbData->hwndParent, L"\u65e0\u6cd5\u521b\u5efa\u6587\u4ef6", L"\u4e0b\u8f7d\u5931\u8d25", MB_OK | MB_ICONERROR);
                    }
                }
            } else {
                LogToFile(L"VFS_ContextVerbW: \u4e0b\u8f7d\u5931\u8d25 " + pathInfo.path);
                MessageBoxW(lpVerbData->hwndParent, L"\u65e0\u6cd5\u4e0b\u8f7d\u6587\u4ef6", L"\u4e0b\u8f7d\u5931\u8d25", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_new_file") == 0) {
        WCHAR fileName[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"\u65b0\u5efa\u6587\u4ef6", L"\u8bf7\u8f93\u5165\u6587\u4ef6\u540d:", fileName, 256)) {
            std::wstring newFilePath = pathInfo.path;
            if (!newFilePath.empty() && newFilePath.back() != L'/') newFilePath += L'/';
            newFilePath += fileName;

            LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6 " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + newFilePath);

            GitHubConfig& cfg = GitHubClient::GetConfig();
            std::wstring message;
            if (!cfg.commitMessageTemplate.empty()) {
                message = cfg.commitMessageTemplate + L": \u65b0\u5efa\u6587\u4ef6 " + newFilePath;
            } else {
                message = L"\u901a\u8fc7 GitHubVFS \u65b0\u5efa\u6587\u4ef6: " + newFilePath;
            }

            std::vector<BYTE> emptyData;
            if (GitHubClient::UploadFile(pathInfo.owner, pathInfo.repo, newFilePath, emptyData, message)) {
                LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6\u6210\u529f " + newFilePath);
                GitHubClient::InvalidateCache();
                if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) {
                    StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                }
                return VFSCVRES_CHANGE;
            } else {
                LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6\u5931\u8d25 " + newFilePath);
                MessageBoxW(lpVerbData->hwndParent, L"\u65e0\u6cd5\u521b\u5efa\u6587\u4ef6", L"\u9519\u8bef", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_new_folder") == 0) {
        WCHAR folderName[256] = {0};
        if (InputBox(lpVerbData->hwndParent, L"\u65b0\u5efa\u6587\u4ef6\u5939", L"\u8bf7\u8f93\u5165\u6587\u4ef6\u5939\u540d:", folderName, 256)) {
            std::wstring newDirPath = pathInfo.path;
            if (!newDirPath.empty() && newDirPath.back() != L'/') newDirPath += L'/';
            newDirPath += folderName;
            newDirPath += L"/.gitkeep";

            LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6\u5939 " + pathInfo.owner + L"/" + pathInfo.repo + L"/" + std::wstring(folderName));

            GitHubConfig& cfg = GitHubClient::GetConfig();
            std::wstring message;
            if (!cfg.commitMessageTemplate.empty()) {
                message = cfg.commitMessageTemplate + L": \u65b0\u5efa\u6587\u4ef6\u5939 " + std::wstring(folderName);
            } else {
                message = L"\u901a\u8fc7 GitHubVFS \u65b0\u5efa\u6587\u4ef6\u5939: " + std::wstring(folderName);
            }

            std::vector<BYTE> emptyData;
            if (GitHubClient::UploadFile(pathInfo.owner, pathInfo.repo, newDirPath, emptyData, message)) {
                LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6\u5939\u6210\u529f " + std::wstring(folderName));
                GitHubClient::InvalidateCache();
                if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) {
                    StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                }
                return VFSCVRES_CHANGE;
            } else {
                LogToFile(L"VFS_ContextVerbW: \u65b0\u5efa\u6587\u4ef6\u5939\u5931\u8d25 " + std::wstring(folderName));
                MessageBoxW(lpVerbData->hwndParent, L"\u65e0\u6cd5\u521b\u5efa\u6587\u4ef6\u5939", L"\u9519\u8bef", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_copy_dir_path") == 0 || _wcsicmp(lpVerbData->lpszVerb, L"gh_copy_file_path") == 0) {
        std::wstring fullPath = pathInfo.owner + L"/" + pathInfo.repo + L"/" + pathInfo.path;
        CopyToClipboard(fullPath);
        LogToFile(L"VFS_ContextVerbW: \u590d\u5236\u8def\u5f84 " + fullPath);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_history") == 0) {
        std::wstring ref;
        try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }
        std::wstring url = L"https://github.com/" + pathInfo.owner + L"/" + pathInfo.repo + L"/commits/" + ref + L"/" + pathInfo.path;
        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_view_blame") == 0) {
        std::wstring ref;
        try { ref = GitHubClient::GetDefaultBranch(pathInfo.owner, pathInfo.repo); } catch (...) { ref = L"main"; }
        std::wstring url = L"https://github.com/" + pathInfo.owner + L"/" + pathInfo.repo + L"/blame/" + ref + L"/" + pathInfo.path;
        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_properties") == 0) {
        ShowPropertiesDialog(lpVerbData->hwndParent, lpVerbData->lpszPath);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_trending_daily") == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"gh_trending_weekly") == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"gh_trending_monthly") == 0) {
        std::wstring since = L"daily";
        if (_wcsicmp(lpVerbData->lpszVerb, L"gh_trending_weekly") == 0) since = L"weekly";
        else if (_wcsicmp(lpVerbData->lpszVerb, L"gh_trending_monthly") == 0) since = L"monthly";
        std::wstring newPath = L"github:///Trending?since=" + since;
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, newPath.c_str());
        return VFSCVRES_CHANGEDIR;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_mark_all_read") == 0) {
        LogToFile(L"VFS_ContextVerbW: \u6807\u8bb0\u6240\u6709\u901a\u77e5\u5df2\u8bfb");
        try {
            bool ok = GitHubClient::MarkNotificationsRead();
            if (ok) {
                LogToFile(L"VFS_ContextVerbW: \u6807\u8bb0\u901a\u77e5\u5df2\u8bfb\u6210\u529f");
            } else {
                LogToFile(L"VFS_ContextVerbW: \u6807\u8bb0\u901a\u77e5\u5df2\u8bfb\u5931\u8d25");
            }
        } catch (...) {
            LogToFile(L"VFS_ContextVerbW: \u6807\u8bb0\u901a\u77e5\u5df2\u8bfb\u5f02\u5e38");
        }
        GitHubClient::InvalidateCache();
        if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        }
        return VFSCVRES_CHANGE;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"gh_create_gist") == 0) {
        ShellExecuteW(NULL, L"open", L"https://gist.github.com/", NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    // 未匹配的菜单命令，返回 HANDLED 避免系统尝试打开文件
    LogToFile(L"VFS_ContextVerbW: unhandled verb=" + (lpVerbData->lpszVerb ? std::wstring(lpVerbData->lpszVerb) : L"(null)"));
    return VFSCVRES_HANDLED;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                        HWND hwndParent, LPWSTR lpszFiles) {
    ShowPropertiesDialog(hwndParent, lpszFiles);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
                                                    LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_MOVE | VFSFUNCAVAIL_DELETE |
                  VFSFUNCAVAIL_MAKEDIR | VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_PROPERTIES |
                  VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE;
        return TRUE;
    }

    case VFSPROP_GETVALIDACTIONS:
        return TRUE;

    case VFSPROP_GETFOLDERICON:
        return FALSE;

    case VFSPROP_SHOWTHUMBNAILS:
    case VFSPROP_USEFULLRENAME:
    case VFSPROP_CANSHOWSUBFOLDERS:
    case VFSPROP_SUPPORTPATHCOMPLETION:
        *reinterpret_cast<LPBOOL>(lpPropData) = TRUE;
        return TRUE;

    case VFSPROP_SHOWFILEINFO:
        *reinterpret_cast<LPBOOL>(lpPropData) = TRUE;
        return TRUE;

    case VFSPROP_ISEXTRACTABLE:
        *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
        return TRUE;
    }

    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                        LPWSTR lpszPath, HANDLE hFile,
                                                        unsigned __int64* piFileSize) {
    if (hFile) {
        GitHubContentInfo* ctx = (GitHubContentInfo*)hFile;
        if (ctx->hTempFile) {
            LARGE_INTEGER fileSize;
            if (GetFileSizeEx(ctx->hTempFile, &fileSize)) {
                if (piFileSize) *piFileSize = fileSize.QuadPart;
                return TRUE;
            }
        }
        if (piFileSize) *piFileSize = ctx->data.size();
        return TRUE;
    } else if (lpszPath) {
        GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);
        if (!pathInfo.repo.empty() && !pathInfo.path.empty()) {
            GitHubFileInfo fileInfo;
            if (GitHubClient::GetFileInfo(pathInfo.owner, pathInfo.repo, pathInfo.path, fileInfo)) {
                if (piFileSize) *piFileSize = fileInfo.size;
                return TRUE;
            }
        }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) {
    return GetLastError();
}

struct ConfigDialogState {
    HANDLE hTestThread;
    bool testRunning;
    int currentAuthMode;
    std::wstring oauthDeviceCode;
    std::wstring oauthUserCode;
};

static int g_currentPage = 0;

static const int pageAuthCommonCtrls[] = { IDC_GRP_AUTH, IDC_LBL_AUTH_MODE, IDC_COMBO_AUTH_MODE, IDC_BTN_TEST, IDC_LBL_STATUS, 0 };
static const int pageTokenCtrls[] = { IDC_LBL_TOKEN, IDC_EDIT_TOKEN, IDC_LBL_TOKEN_HINT, 0 };
static const int pageBasicCtrls[] = { IDC_LBL_USERNAME, IDC_EDIT_USERNAME, IDC_LBL_PASSWORD, IDC_EDIT_PASSWORD, IDC_LBL_BASIC_HINT, 0 };
static const int pageOAuthCtrls[] = { IDC_LBL_OAUTH_CODE, IDC_EDIT_OAUTH_CODE, IDC_BTN_START_OAUTH, IDC_LBL_OAUTH_STATUS, IDC_LBL_OAUTH_HINT, 0 };
static const int pageConnCtrls[] = { IDC_GRP_CONNECTION, IDC_LBL_API_URL, IDC_EDIT_API_URL, IDC_LBL_TIMEOUT, IDC_EDIT_TIMEOUT, IDC_LBL_CACHE, IDC_EDIT_CACHE, IDC_LBL_PROXY_TYPE, IDC_COMBO_PROXY_TYPE, IDC_LBL_PROXY_HOST, IDC_EDIT_PROXY_HOST, IDC_LBL_PROXY_PORT, IDC_EDIT_PROXY_PORT, 0 };
static const int pageDisplayCtrls[] = { IDC_GRP_DISPLAY, IDC_CHK_SHOW_FORKS, IDC_CHK_SHOW_ARCHIVED, IDC_CHK_SHOW_PRIVATE, IDC_CHK_SHOW_DESC, IDC_LBL_REPO_SORT, IDC_COMBO_REPO_SORT, IDC_LBL_ITEMS_PER_PAGE, IDC_EDIT_ITEMS_PER_PAGE, IDC_LBL_DEFAULT_BRANCH, IDC_EDIT_DEFAULT_BRANCH, 0 };
static const int pageOpsCtrls[] = { IDC_GRP_OPERATIONS, IDC_LBL_COMMIT_MSG, IDC_EDIT_COMMIT_MSG, IDC_CHK_AUTO_REFRESH, IDC_LBL_REFRESH_INTERVAL, IDC_EDIT_REFRESH_INTERVAL, IDC_CHK_LARGE_FILE_WARN, IDC_LBL_LARGE_FILE_SIZE, IDC_EDIT_LARGE_FILE_SIZE, IDC_CHK_CONFIRM_DELETE, 0 };
static const int pageAdvCtrls[] = { IDC_GRP_ADVANCED, IDC_LBL_PATH_MODE, IDC_COMBO_PATH_MODE, IDC_LBL_META_PREFIX, IDC_EDIT_META_PREFIX, IDC_LBL_REF_KEYWORD, IDC_EDIT_REF_KEYWORD, IDC_CHK_ENABLE_ALIASES, IDC_CHK_ENABLE_OLD_PATHS, 0 };
static const int* g_pageControls[] = { pageAuthCommonCtrls, pageConnCtrls, pageDisplayCtrls, pageOpsCtrls, pageAdvCtrls };

static void ShowAuthModeControls(HWND hDlg, int authMode) {
    for (int i = 0; pageTokenCtrls[i] != 0; i++) {
        HWND hwnd = GetDlgItem(hDlg, pageTokenCtrls[i]);
        if (hwnd) ShowWindow(hwnd, (authMode == 0) ? SW_SHOW : SW_HIDE);
    }
    for (int i = 0; pageBasicCtrls[i] != 0; i++) {
        HWND hwnd = GetDlgItem(hDlg, pageBasicCtrls[i]);
        if (hwnd) ShowWindow(hwnd, (authMode == 1) ? SW_SHOW : SW_HIDE);
    }
    for (int i = 0; pageOAuthCtrls[i] != 0; i++) {
        HWND hwnd = GetDlgItem(hDlg, pageOAuthCtrls[i]);
        if (hwnd) ShowWindow(hwnd, (authMode == 2) ? SW_SHOW : SW_HIDE);
    }
}

static void ShowNavPage(HWND hDlg, int page) {
    g_currentPage = page;
    for (int i = 0; i < 5; i++) {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++) {
            HWND hwnd = GetDlgItem(hDlg, ctrls[j]);
            if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        }
    }
    if (page == 0) {
        ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (state) ShowAuthModeControls(hDlg, state->currentAuthMode);
    } else {
        for (int i = 0; pageTokenCtrls[i] != 0; i++) {
            HWND hwnd = GetDlgItem(hDlg, pageTokenCtrls[i]);
            if (hwnd) ShowWindow(hwnd, SW_HIDE);
        }
        for (int i = 0; pageBasicCtrls[i] != 0; i++) {
            HWND hwnd = GetDlgItem(hDlg, pageBasicCtrls[i]);
            if (hwnd) ShowWindow(hwnd, SW_HIDE);
        }
        for (int i = 0; pageOAuthCtrls[i] != 0; i++) {
            HWND hwnd = GetDlgItem(hDlg, pageOAuthCtrls[i]);
            if (hwnd) ShowWindow(hwnd, SW_HIDE);
        }
    }
}

static void SetChineseText(HWND hDlg) {
    SetWindowTextW(hDlg, L"GitHub VFS \u914d\u7f6e");
    
    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList) {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 5; i++) {
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        }
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88");
    SetDlgItemTextW(hDlg, IDC_BTN_APPLY, L"\u5e94\u7528");
    SetDlgItemTextW(hDlg, IDC_BTN_RESET, L"\u91cd\u7f6e\u9ed8\u8ba4");
    SetDlgItemTextW(hDlg, IDC_BTN_TEST, L"\u6d4b\u8bd5\u8fde\u63a5");

    SetDlgItemTextW(hDlg, IDC_GRP_AUTH, L"\u8ba4\u8bc1\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_LBL_AUTH_MODE, L"\u8ba4\u8bc1\u65b9\u5f0f:");
    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_AUTH_MODE);
    if (hCombo) {
        SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Token \u8ba4\u8bc1");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u8d26\u53f7\u5bc6\u7801");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"OAuth \u8bbe\u5907\u6388\u6743");
    }

    SetDlgItemTextW(hDlg, IDC_LBL_TOKEN, L"\u8bbf\u95ee\u4ee4\u724c:");
    SetDlgItemTextW(hDlg, IDC_LBL_TOKEN_HINT, 
        L"\u5728 GitHub Settings > Developer settings > Personal access tokens \u4e2d\u751f\u6210");

    SetDlgItemTextW(hDlg, IDC_LBL_USERNAME, L"\u7528\u6237\u540d:");
    SetDlgItemTextW(hDlg, IDC_LBL_PASSWORD, L"\u5bc6\u7801:");
    SetDlgItemTextW(hDlg, IDC_LBL_BASIC_HINT, 
        L"\u6ce8: GitHub \u5df2\u5f9f\u7528\u5bc6\u7801\u8ba4\u8bc1\uff0c\u5efa\u8bae\u4f7f\u7528 Token \u8ba4\u8bc1");

    SetDlgItemTextW(hDlg, IDC_LBL_OAUTH_CODE, L"\u8bbe\u5907\u7801:");
    SetDlgItemTextW(hDlg, IDC_BTN_START_OAUTH, L"\u542f\u52a8\u6388\u6743");
    SetDlgItemTextW(hDlg, IDC_LBL_OAUTH_HINT, 
        L"\u70b9\u51fb\u542f\u52a8\u6388\u6743\uff0c\u5728\u6d4f\u89c8\u5668\u4e2d\u8f93\u5165\u663e\u793a\u7684\u8bbe\u5907\u7801");

    SetDlgItemTextW(hDlg, IDC_GRP_CONNECTION, L"\u8fde\u63a5\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_LBL_API_URL, L"API \u5730\u5740:");
    SetDlgItemTextW(hDlg, IDC_LBL_TIMEOUT, L"\u8fde\u63a5\u8d85\u65f6(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE, L"\u7f13\u5b58\u8d85\u65f6(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_PROXY_TYPE, L"\u4ee3\u7406\u7c7b\u578b:");
    HWND hComboProxy = GetDlgItem(hDlg, IDC_COMBO_PROXY_TYPE);
    if (hComboProxy) {
        SendMessageW(hComboProxy, CB_RESETCONTENT, 0, 0);
        SendMessageW(hComboProxy, CB_ADDSTRING, 0, (LPARAM)L"\u65e0\u4ee3\u7406");
        SendMessageW(hComboProxy, CB_ADDSTRING, 0, (LPARAM)L"HTTP \u4ee3\u7406");
        SendMessageW(hComboProxy, CB_ADDSTRING, 0, (LPARAM)L"SOCKS5 \u4ee3\u7406");
    }
    SetDlgItemTextW(hDlg, IDC_LBL_PROXY_HOST, L"\u4ee3\u7406\u670d\u52a1\u5668:");
    SetDlgItemTextW(hDlg, IDC_LBL_PROXY_PORT, L"\u7aef\u53e3:");

    SetDlgItemTextW(hDlg, IDC_GRP_DISPLAY, L"\u663e\u793a\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_CHK_SHOW_FORKS, L"\u663e\u793a Fork \u4ed3\u5e93");
    SetDlgItemTextW(hDlg, IDC_CHK_SHOW_ARCHIVED, L"\u663e\u793a\u5f52\u6863\u4ed3\u5e93");
    SetDlgItemTextW(hDlg, IDC_CHK_SHOW_PRIVATE, L"\u663e\u793a\u79c1\u6709\u4ed3\u5e93");
    SetDlgItemTextW(hDlg, IDC_CHK_SHOW_DESC, L"\u663e\u793a\u4ed3\u5e93\u63cf\u8ff0");
    SetDlgItemTextW(hDlg, IDC_LBL_REPO_SORT, L"\u4ed3\u5e93\u6392\u5e8f:");
    HWND hComboSort = GetDlgItem(hDlg, IDC_COMBO_REPO_SORT);
    if (hComboSort) {
        SendMessageW(hComboSort, CB_RESETCONTENT, 0, 0);
        SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"\u66f4\u65b0\u65f6\u95f4");
        SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"\u540d\u79f0");
        SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"\u661f\u6807\u6570");
        SendMessageW(hComboSort, CB_ADDSTRING, 0, (LPARAM)L"\u521b\u5efa\u65f6\u95f4");
    }
    SetDlgItemTextW(hDlg, IDC_LBL_ITEMS_PER_PAGE, L"\u6bcf\u9875\u6570\u91cf:");
    SetDlgItemTextW(hDlg, IDC_LBL_DEFAULT_BRANCH, L"\u9ed8\u8ba4\u5206\u652f\u540d:");

    SetDlgItemTextW(hDlg, IDC_GRP_OPERATIONS, L"\u64cd\u4f5c\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_LBL_COMMIT_MSG, L"\u63d0\u4ea4\u6d88\u606f\u6a21\u677f:");
    SetDlgItemTextW(hDlg, IDC_CHK_AUTO_REFRESH, L"\u81ea\u52a8\u5237\u65b0");
    SetDlgItemTextW(hDlg, IDC_LBL_REFRESH_INTERVAL, L"\u5237\u65b0\u95f4\u9694(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_CHK_LARGE_FILE_WARN, L"\u5927\u6587\u4ef6\u8b66\u544a");
    SetDlgItemTextW(hDlg, IDC_LBL_LARGE_FILE_SIZE, L"\u8b66\u544a\u9608\u503c(MB):");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_DELETE, L"\u5220\u9664\u524d\u786e\u8ba4");

    SetDlgItemTextW(hDlg, IDC_GRP_ADVANCED, L"\u9ad8\u7ea7\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_LBL_PATH_MODE, L"\u8def\u5f84\u6a21\u5f0f:");
    HWND hComboPath = GetDlgItem(hDlg, IDC_COMBO_PATH_MODE);
    if (hComboPath) {
        SendMessageW(hComboPath, CB_RESETCONTENT, 0, 0);
        SendMessageW(hComboPath, CB_ADDSTRING, 0, (LPARAM)L"\u4e25\u683c\u6a21\u5f0f");
        SendMessageW(hComboPath, CB_ADDSTRING, 0, (LPARAM)L"\u5bbd\u677e\u6a21\u5f0f");
    }
    SetDlgItemTextW(hDlg, IDC_LBL_META_PREFIX, L"\u5143\u6570\u636e\u524d\u7f00:");
    SetDlgItemTextW(hDlg, IDC_LBL_REF_KEYWORD, L"Ref \u5173\u952e\u5b57:");
    SetDlgItemTextW(hDlg, IDC_CHK_ENABLE_ALIASES, L"\u542f\u7528\u522b\u540d");
    SetDlgItemTextW(hDlg, IDC_CHK_ENABLE_OLD_PATHS, L"\u542f\u7528\u65e7\u8def\u5f84");
}

static void InitDialogControls(HWND hDlg) {
    GitHubConfig& config = GitHubClient::GetConfig();
    
    SetDlgItemTextW(hDlg, IDC_EDIT_TOKEN, config.token.c_str());
    SetDlgItemTextW(hDlg, IDC_EDIT_USERNAME, config.username.c_str());
    SetDlgItemTextW(hDlg, IDC_EDIT_PASSWORD, config.password.c_str());
    SetDlgItemTextW(hDlg, IDC_EDIT_API_URL, config.apiUrl.c_str());
    SetDlgItemInt(hDlg, IDC_EDIT_TIMEOUT, config.connTimeout, FALSE);
    SetDlgItemInt(hDlg, IDC_EDIT_CACHE, config.cacheTimeout, FALSE);

    int authMode = 0;
    if (config.authMode == GITHUB_AUTH_BASIC) authMode = 1;
    else if (config.authMode == GITHUB_AUTH_OAUTH) authMode = 2;
    
    ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    if (state) state->currentAuthMode = authMode;
    
    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_AUTH_MODE);
    if (hCombo) SendMessageW(hCombo, CB_SETCURSEL, authMode, 0);

    HWND hComboProxy = GetDlgItem(hDlg, IDC_COMBO_PROXY_TYPE);
    if (hComboProxy) SendMessageW(hComboProxy, CB_SETCURSEL, (int)config.proxyType, 0);
    SetDlgItemTextW(hDlg, IDC_EDIT_PROXY_HOST, config.proxyHost.c_str());
    SetDlgItemInt(hDlg, IDC_EDIT_PROXY_PORT, config.proxyPort, FALSE);

    CheckDlgButton(hDlg, IDC_CHK_SHOW_FORKS, config.showForks ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_ARCHIVED, config.showArchived ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_PRIVATE, config.showPrivate ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_DESC, config.showDescription ? BST_CHECKED : BST_UNCHECKED);
    HWND hComboSort = GetDlgItem(hDlg, IDC_COMBO_REPO_SORT);
    if (hComboSort) SendMessageW(hComboSort, CB_SETCURSEL, (int)config.repoSort, 0);
    SetDlgItemInt(hDlg, IDC_EDIT_ITEMS_PER_PAGE, config.itemsPerPage, FALSE);
    SetDlgItemTextW(hDlg, IDC_EDIT_DEFAULT_BRANCH, config.defaultBranchName.c_str());

    SetDlgItemTextW(hDlg, IDC_EDIT_COMMIT_MSG, config.commitMessageTemplate.c_str());
    CheckDlgButton(hDlg, IDC_CHK_AUTO_REFRESH, config.autoRefresh ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_REFRESH_INTERVAL, config.refreshInterval, FALSE);
    CheckDlgButton(hDlg, IDC_CHK_LARGE_FILE_WARN, config.largeFileWarn ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_LARGE_FILE_SIZE, config.largeFileSizeMB, FALSE);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_DELETE, BST_CHECKED);

    HWND hComboPath = GetDlgItem(hDlg, IDC_COMBO_PATH_MODE);
    if (hComboPath) SendMessageW(hComboPath, CB_SETCURSEL, (int)config.pathMode, 0);
    SetDlgItemTextW(hDlg, IDC_EDIT_META_PREFIX, config.metaPrefix.c_str());
    SetDlgItemTextW(hDlg, IDC_EDIT_REF_KEYWORD, config.refKeyword.c_str());
    CheckDlgButton(hDlg, IDC_CHK_ENABLE_ALIASES, config.enableAliases ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_ENABLE_OLD_PATHS, config.enableOldPaths ? BST_CHECKED : BST_UNCHECKED);
    
    SetChineseText(hDlg);
    ShowNavPage(hDlg, 0);
}

static DWORD WINAPI TestConnectionThread(LPVOID lpParam) {
    HWND hDlg = (HWND)lpParam;
    if (!hDlg || !IsWindow(hDlg)) return 0;

    std::wstring username;
    bool success = GitHubClient::TestConnection(username);

    if (IsWindow(hDlg)) {
        if (success) {
            SetDlgItemTextW(hDlg, IDC_LBL_STATUS, 
                (L"\u2713 \u8fde\u63a5\u6210\u529f! \u7528\u6237: " + username).c_str());
            GitHubClient::GetConfig().currentUser = username;
        } else {
            SetDlgItemTextW(hDlg, IDC_LBL_STATUS, 
                L"\u2717 \u8fde\u63a5\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u8ba4\u8bc1\u4fe1\u606f");
        }
    }
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_TEST), TRUE);
    return 0;
}

static bool SaveDialogControls(HWND hDlg) {
    GitHubConfig& config = GitHubClient::GetConfig();
    WCHAR buf[2048];

    ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    int authMode = state ? state->currentAuthMode : 0;

    if (authMode == 0) {
        GetDlgItemTextW(hDlg, IDC_EDIT_TOKEN, buf, 2048);
        config.token = buf;
        config.authMode = GITHUB_AUTH_TOKEN;
        config.username.clear();
        config.password.clear();
    } else if (authMode == 1) {
        GetDlgItemTextW(hDlg, IDC_EDIT_USERNAME, buf, 2048);
        config.username = buf;
        GetDlgItemTextW(hDlg, IDC_EDIT_PASSWORD, buf, 2048);
        config.password = buf;
        config.authMode = GITHUB_AUTH_BASIC;
        config.token.clear();
    } else if (authMode == 2) {
        GetDlgItemTextW(hDlg, IDC_EDIT_OAUTH_CODE, buf, 2048);
        config.token = buf;
        config.authMode = GITHUB_AUTH_OAUTH;
        config.username.clear();
        config.password.clear();
    }

    GetDlgItemTextW(hDlg, IDC_EDIT_API_URL, buf, 2048);
    config.apiUrl = buf;
    if (config.apiUrl.empty()) config.apiUrl = L"api.github.com";

    config.connTimeout = GetDlgItemInt(hDlg, IDC_EDIT_TIMEOUT, NULL, FALSE);
    if (config.connTimeout < 1) config.connTimeout = 5;

    config.cacheTimeout = GetDlgItemInt(hDlg, IDC_EDIT_CACHE, NULL, FALSE);
    if (config.cacheTimeout < 1) config.cacheTimeout = 60;

    HWND hComboProxy = GetDlgItem(hDlg, IDC_COMBO_PROXY_TYPE);
    if (hComboProxy) {
        int proxySel = (int)SendMessageW(hComboProxy, CB_GETCURSEL, 0, 0);
        if (proxySel < 0) proxySel = 0;
        config.proxyType = (GitHubProxyType)proxySel;
    }
    GetDlgItemTextW(hDlg, IDC_EDIT_PROXY_HOST, buf, 2048);
    config.proxyHost = buf;
    config.proxyPort = GetDlgItemInt(hDlg, IDC_EDIT_PROXY_PORT, NULL, FALSE);

    config.showForks = (IsDlgButtonChecked(hDlg, IDC_CHK_SHOW_FORKS) == BST_CHECKED);
    config.showArchived = (IsDlgButtonChecked(hDlg, IDC_CHK_SHOW_ARCHIVED) == BST_CHECKED);
    config.showPrivate = (IsDlgButtonChecked(hDlg, IDC_CHK_SHOW_PRIVATE) == BST_CHECKED);
    config.showDescription = (IsDlgButtonChecked(hDlg, IDC_CHK_SHOW_DESC) == BST_CHECKED);
    HWND hComboSort = GetDlgItem(hDlg, IDC_COMBO_REPO_SORT);
    if (hComboSort) {
        int sortSel = (int)SendMessageW(hComboSort, CB_GETCURSEL, 0, 0);
        if (sortSel < 0) sortSel = 0;
        config.repoSort = (GitHubRepoSort)sortSel;
    }
    config.itemsPerPage = GetDlgItemInt(hDlg, IDC_EDIT_ITEMS_PER_PAGE, NULL, FALSE);
    if (config.itemsPerPage < 1) config.itemsPerPage = 30;
    GetDlgItemTextW(hDlg, IDC_EDIT_DEFAULT_BRANCH, buf, 2048);
    config.defaultBranchName = buf;

    GetDlgItemTextW(hDlg, IDC_EDIT_COMMIT_MSG, buf, 2048);
    config.commitMessageTemplate = buf;
    config.autoRefresh = (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_REFRESH) == BST_CHECKED);
    config.refreshInterval = GetDlgItemInt(hDlg, IDC_EDIT_REFRESH_INTERVAL, NULL, FALSE);
    if (config.refreshInterval < 1) config.refreshInterval = 60;
    config.largeFileWarn = (IsDlgButtonChecked(hDlg, IDC_CHK_LARGE_FILE_WARN) == BST_CHECKED);
    config.largeFileSizeMB = GetDlgItemInt(hDlg, IDC_EDIT_LARGE_FILE_SIZE, NULL, FALSE);
    if (config.largeFileSizeMB < 1) config.largeFileSizeMB = 10;

    HWND hComboPath = GetDlgItem(hDlg, IDC_COMBO_PATH_MODE);
    if (hComboPath) {
        int pathSel = (int)SendMessageW(hComboPath, CB_GETCURSEL, 0, 0);
        if (pathSel < 0) pathSel = 0;
        config.pathMode = (GitHubPathMode)pathSel;
    }
    GetDlgItemTextW(hDlg, IDC_EDIT_META_PREFIX, buf, 2048);
    config.metaPrefix = buf;
    GetDlgItemTextW(hDlg, IDC_EDIT_REF_KEYWORD, buf, 2048);
    config.refKeyword = buf;
    config.enableAliases = (IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_ALIASES) == BST_CHECKED);
    config.enableOldPaths = (IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_OLD_PATHS) == BST_CHECKED);

    return true;
}

static void ResetConfigDefaults(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_EDIT_TOKEN, L"");
    SetDlgItemTextW(hDlg, IDC_EDIT_USERNAME, L"");
    SetDlgItemTextW(hDlg, IDC_EDIT_PASSWORD, L"");
    SetDlgItemTextW(hDlg, IDC_EDIT_API_URL, L"api.github.com");
    SetDlgItemInt(hDlg, IDC_EDIT_TIMEOUT, 5, FALSE);
    SetDlgItemInt(hDlg, IDC_EDIT_CACHE, 60, FALSE);

    HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_AUTH_MODE);
    if (hCombo) SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
    HWND hComboProxy = GetDlgItem(hDlg, IDC_COMBO_PROXY_TYPE);
    if (hComboProxy) SendMessageW(hComboProxy, CB_SETCURSEL, 0, 0);
    SetDlgItemTextW(hDlg, IDC_EDIT_PROXY_HOST, L"");
    SetDlgItemInt(hDlg, IDC_EDIT_PROXY_PORT, 0, FALSE);

    CheckDlgButton(hDlg, IDC_CHK_SHOW_FORKS, BST_CHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_ARCHIVED, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_PRIVATE, BST_CHECKED);
    CheckDlgButton(hDlg, IDC_CHK_SHOW_DESC, BST_CHECKED);
    HWND hComboSort = GetDlgItem(hDlg, IDC_COMBO_REPO_SORT);
    if (hComboSort) SendMessageW(hComboSort, CB_SETCURSEL, 0, 0);
    SetDlgItemInt(hDlg, IDC_EDIT_ITEMS_PER_PAGE, 30, FALSE);
    SetDlgItemTextW(hDlg, IDC_EDIT_DEFAULT_BRANCH, L"main");

    SetDlgItemTextW(hDlg, IDC_EDIT_COMMIT_MSG, L"");
    CheckDlgButton(hDlg, IDC_CHK_AUTO_REFRESH, BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_REFRESH_INTERVAL, 60, FALSE);
    CheckDlgButton(hDlg, IDC_CHK_LARGE_FILE_WARN, BST_CHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_LARGE_FILE_SIZE, 10, FALSE);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_DELETE, BST_CHECKED);

    HWND hComboPath = GetDlgItem(hDlg, IDC_COMBO_PATH_MODE);
    if (hComboPath) SendMessageW(hComboPath, CB_SETCURSEL, 0, 0);
    SetDlgItemTextW(hDlg, IDC_EDIT_META_PREFIX, L"");
    SetDlgItemTextW(hDlg, IDC_EDIT_REF_KEYWORD, L"");
    CheckDlgButton(hDlg, IDC_CHK_ENABLE_ALIASES, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_ENABLE_OLD_PATHS, BST_UNCHECKED);

    ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    if (state) {
        state->currentAuthMode = 0;
        ShowAuthModeControls(hDlg, 0);
    }
}

static INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        ConfigDialogState* state = new ConfigDialogState();
        ZeroMemory(state, sizeof(ConfigDialogState));
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)state);
        InitDialogControls(hDlg);
        return TRUE;
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST) {
            lpmis->itemHeight = 24;
        }
        return TRUE;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST) {
            static const WCHAR* navLabels[] = { 
                L"\u8ba4\u8bc1\u8bbe\u7f6e", 
                L"\u8fde\u63a5\u8bbe\u7f6e", 
                L"\u663e\u793a\u8bbe\u7f6e", 
                L"\u64cd\u4f5c\u8bbe\u7f6e", 
                L"\u9ad8\u7ea7\u8bbe\u7f6e" 
            };
            int idx = (int)lpdis->itemID;
            if (idx >= 0 && idx < 5) {
                BOOL selected = (lpdis->itemState & ODS_SELECTED);
                HBRUSH hBrush = selected ? CreateSolidBrush(RGB(0, 120, 215)) : GetSysColorBrush(COLOR_WINDOW);
                FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
                if (selected) DeleteObject(hBrush);
                SetTextColor(lpdis->hDC, selected ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 5) {
                    ShowNavPage(hDlg, sel);
                }
            }
            return TRUE;

        case IDC_COMBO_AUTH_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_AUTH_MODE);
                int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 3) {
                    ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
                    if (state) {
                        state->currentAuthMode = sel;
                        ShowAuthModeControls(hDlg, sel);
                    }
                }
            }
            return TRUE;

        case IDC_BTN_TEST: {
            ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (!state || state->testRunning) return TRUE;

            SaveDialogControls(hDlg);
            GitHubClient::ResetSession();

            SetDlgItemTextW(hDlg, IDC_LBL_STATUS, L"\u6b63\u5728\u6d4b\u8bd5\u8fde\u63a5...");
            EnableWindow(GetDlgItem(hDlg, IDC_BTN_TEST), FALSE);

            state->testRunning = true;
            state->hTestThread = CreateThread(NULL, 0, TestConnectionThread, hDlg, 0, NULL);
            return TRUE;
        }

        case IDC_BTN_APPLY: {
            if (SaveDialogControls(hDlg)) {
                GitHubClient::SaveConfig();
                SetDlgItemTextW(hDlg, IDC_LBL_STATUS, L"\u2713 \u914d\u7f6e\u5df2\u4fdd\u5b58");
            }
            return TRUE;
        }

        case IDC_BTN_RESET: {
            ResetConfigDefaults(hDlg);
            return TRUE;
        }

        case IDOK: {
            ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (state && state->testRunning) {
                state->testRunning = false;
                if (state->hTestThread) {
                    WaitForSingleObject(state->hTestThread, 2000);
                    CloseHandle(state->hTestThread);
                }
            }

            if (SaveDialogControls(hDlg)) {
                GitHubClient::SaveConfig();
                EndDialog(hDlg, IDOK);
            }
            return TRUE;
        }

        case IDCANCEL: {
            ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (state && state->testRunning) {
                state->testRunning = false;
                if (state->hTestThread) {
                    WaitForSingleObject(state->hTestThread, 2000);
                    CloseHandle(state->hTestThread);
                }
            }
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;

    case WM_DESTROY: {
        ConfigDialogState* state = (ConfigDialogState*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
        if (state) {
            if (state->testRunning && state->hTestThread) {
                WaitForSingleObject(state->hTestThread, 2000);
                CloseHandle(state->hTestThread);
            }
            delete state;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, 0);
        }
        break;
    }
    }
    return FALSE;
}

struct GitHubFindData {
    std::vector<WIN32_FIND_DATAW> entries;
    size_t currentIndex;
    GitHubFindData() : currentIndex(0) {}
};

static void WfdFromPathInfo(const GitHubPathInfo& pathInfo, LPWIN32_FIND_DATAW pwfd) {
    ZeroMemory(pwfd, sizeof(WIN32_FIND_DATAW));

    if (pathInfo.context == GITHUB_CTX_ROOT) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"GitHub");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_REPOS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Repos");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_REPOS_OWNER) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, pathInfo.owner.c_str());
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_STARRED) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Starred");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_STARRED_OWNER) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, pathInfo.owner.c_str());
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SUBSCRIPTIONS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Subscriptions");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SUBSCRIPTIONS_OWNER) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, pathInfo.owner.c_str());
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_NOTIFICATIONS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Notifications");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_GISTS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Gists");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_TRENDING) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Trending");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_CENTER) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Search");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_REPOS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Repos");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_CODE) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Code");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_USERS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Users");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_ISSUES) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Issues");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_COMMITS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Commits");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_SEARCH_TOPICS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Topics");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_OWNER) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, pathInfo.owner.c_str());
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_REPO) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, pathInfo.repo.c_str());
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_CODE) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Code");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_ISSUES) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Issues");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_PULLS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Pulls");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_BRANCHES) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Branches");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_TAGS) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Tags");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else if (pathInfo.context == GITHUB_CTX_RELEASES) {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"Releases");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    } else {
        StringCchCopyW(pwfd->cFileName, MAX_PATH, L"GitHub");
        pwfd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    }
    GetSystemTimeAsFileTime(&pwfd->ftLastWriteTime);
}

extern "C" __declspec(dllexport) int VFS_GetFileDescriptionW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                              LPWSTR lpszPath, LPWSTR lpszDescription, int cchDescriptionMax) {
    VFS_TRY
    if (!lpszPath || !lpszDescription || cchDescriptionMax <= 0) return FALSE;
    if (!IsGitHubVfsPath(lpszPath)) return FALSE;

    GitHubPathInfo pathInfo = ParseGitHubPath(lpszPath);

    switch (pathInfo.context) {
    case GITHUB_CTX_ROOT:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"GitHub Virtual File System");
        return TRUE;
    case GITHUB_CTX_REPOS:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"My repositories");
        return TRUE;
    case GITHUB_CTX_REPOS_OWNER:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"My repositories by this owner");
        return TRUE;
    case GITHUB_CTX_STARRED:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Starred repositories");
        return TRUE;
    case GITHUB_CTX_STARRED_OWNER:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Starred repositories by this owner");
        return TRUE;
    case GITHUB_CTX_SUBSCRIPTIONS:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Watched repositories");
        return TRUE;
    case GITHUB_CTX_SUBSCRIPTIONS_OWNER:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Watched repositories by this owner");
        return TRUE;
    case GITHUB_CTX_NOTIFICATIONS:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"GitHub notifications");
        return TRUE;
    case GITHUB_CTX_GISTS:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"GitHub Gists");
        return TRUE;
    case GITHUB_CTX_SEARCH_CODE:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Code search");
        return TRUE;
    case GITHUB_CTX_SEARCH_USERS:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"User search");
        return TRUE;
    case GITHUB_CTX_SEARCH_ISSUES:
        StringCchCopyW(lpszDescription, cchDescriptionMax, L"Issues search");
        return TRUE;
    default:
        return FALSE;
    }
    VFS_CATCH
    return FALSE;
}

static void AddFindDirEntry(GitHubFindData* findData, const std::wstring& name) {
    WIN32_FIND_DATAW wfd = {};
    SafeCopyFileName(wfd.cFileName, MAX_PATH, name);
    wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    GetSystemTimeAsFileTime(&wfd.ftLastWriteTime);
    GetSystemTimeAsFileTime(&wfd.ftCreationTime);
    findData->entries.push_back(wfd);
}

static void AddFindFileEntry(GitHubFindData* findData, const std::wstring& name, DWORD size = 0) {
    WIN32_FIND_DATAW wfd = {};
    SafeCopyFileName(wfd.cFileName, MAX_PATH, name);
    wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    wfd.nFileSizeLow = size;
    GetSystemTimeAsFileTime(&wfd.ftLastWriteTime);
    findData->entries.push_back(wfd);
}

static void AddFindRepoEntry(GitHubFindData* findData, const GitHubRepoInfo& repo, bool useFullName = true) {
    WIN32_FIND_DATAW wfd = {};
    StringCchCopyW(wfd.cFileName, MAX_PATH, useFullName ? repo.fullName.c_str() : repo.name.c_str());
    wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    wfd.ftLastWriteTime = repo.pushedAt;
    wfd.ftCreationTime = repo.createdAt;
    ULARGE_INTEGER sz;
    sz.QuadPart = repo.size;
    wfd.nFileSizeHigh = sz.HighPart;
    wfd.nFileSizeLow = sz.LowPart;
    findData->entries.push_back(wfd);
}

static void AddFindFileRepoEntry(GitHubFindData* findData, const GitHubFileInfo& file) {
    WIN32_FIND_DATAW wfd = {};
    SafeCopyFileName(wfd.cFileName, MAX_PATH, file.name);
    wfd.dwFileAttributes = file.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    wfd.ftLastWriteTime = file.updatedAt;
    ULARGE_INTEGER sz;
    sz.QuadPart = file.size;
    wfd.nFileSizeHigh = sz.HighPart;
    wfd.nFileSizeLow = sz.LowPart;
    findData->entries.push_back(wfd);
}

static HANDLE VFS_FindFirstFileW_Internal(LPVFSFUNCDATA lpVFSData,
                                           LPWSTR lpszPath, LPWIN32_FIND_DATAW lpwfdData,
                                           HANDLE hAbortEvent) {
    if (!lpszPath || !lpwfdData) return NULL;

    ZeroMemory(lpwfdData, sizeof(WIN32_FIND_DATAW));

    if (!IsGitHubVfsPath(lpszPath)) return NULL;

    std::wstring pathStr(lpszPath);
    std::wstring dirPath = pathStr;
    std::wstring filePattern = L"*";

    size_t lastSlash = pathStr.find_last_of(L"/\\");
    if (lastSlash != std::wstring::npos && lastSlash > GITHUB_VFS_PREFIX_LEN) {
        std::wstring lastPart = pathStr.substr(lastSlash + 1);
        if (lastPart.find_first_of(L"*?") != std::wstring::npos) {
            filePattern = lastPart;
            dirPath = pathStr.substr(0, lastSlash);
            while (!dirPath.empty() && dirPath.back() == L'/') dirPath.pop_back();
        }
    }

    GitHubPathInfo pathInfo;
    try {
        pathInfo = ParseGitHubPath(dirPath.c_str());
    } catch (...) {
        return NULL;
    }

    if (filePattern != L"*") {
        WfdFromPathInfo(pathInfo, lpwfdData);
        return (HANDLE)1;
    }

    GitHubFindData* findData = new (std::nothrow) GitHubFindData();
    if (!findData) return NULL;

    try {
        switch (pathInfo.context) {
        case GITHUB_CTX_ROOT: {
            if (!pathInfo.query.search.empty()) {
                GitHubSearchResult searchResult;
                try { searchResult = GitHubClient::SearchRepos(pathInfo.query.query, pathInfo.query.page); } catch (...) { searchResult.repos.clear(); }
                for (const auto& r : searchResult.repos) {
                    AddFindRepoEntry(findData, r, true);
                }
            } else if (!pathInfo.query.view.empty()) {
                if (pathInfo.query.view == L"starred") {
                    std::vector<GitHubRepoInfo> starred;
                    try { starred = GitHubClient::ListStarredRepos(); } catch (...) { starred.clear(); }
                    for (const auto& r : starred) {
                        AddFindRepoEntry(findData, r, true);
                    }
                } else if (pathInfo.query.view == L"subscriptions") {
                    std::vector<GitHubRepoInfo> watched;
                    try { watched = GitHubClient::ListWatchedRepos(); } catch (...) { watched.clear(); }
                    for (const auto& r : watched) {
                        AddFindRepoEntry(findData, r, true);
                    }
                } else if (pathInfo.query.view == L"trending") {
                    GitHubSearchResult trendingResult;
                    try { trendingResult = GitHubClient::SearchRepos(L"", 1); } catch (...) { trendingResult.repos.clear(); }
                    for (const auto& r : trendingResult.repos) {
                        AddFindRepoEntry(findData, r, true);
                    }
                }
            } else {
                std::vector<GitHubRepoInfo> repos = GetCachedRepos();
                if (repos.empty()) repos = GitHubClient::ListUserRepos();
                for (const auto& r : repos) {
                    AddFindRepoEntry(findData, r, false);
                }
            }
            break;
        }

        case GITHUB_CTX_REPO: {
            std::wstring owner = pathInfo.query.owner.empty() ? 
                (pathInfo.owner.empty() ? GetDefaultOwner(pathInfo.repo) : pathInfo.owner) : 
                pathInfo.query.owner;
            std::wstring ref = pathInfo.query.ref;
            try {
                if (ref.empty()) ref = GitHubClient::GetDefaultBranch(owner, pathInfo.repo);
            } catch (...) { ref = L"main"; }
            if (ref.empty()) ref = L"main";

            std::vector<GitHubFileInfo> files;
            try { files = GitHubClient::ListDirectory(owner, pathInfo.repo, pathInfo.path, ref); } catch (...) { files.clear(); }
            for (const auto& f : files) {
                AddFindFileRepoEntry(findData, f);
            }
            break;
        }

        case GITHUB_CTX_VIEW: {
            if (pathInfo.query.view == L"issues") {
                std::vector<GitHubIssueInfo> issues;
                try { issues = GitHubClient::ListIssues(pathInfo.owner, pathInfo.repo, pathInfo.query.state.empty() ? L"open" : pathInfo.query.state); } catch (...) { issues.clear(); }
                for (const auto& iss : issues) {
                    if (iss.isPullRequest) continue;
                    std::wstring name = L"#" + std::to_wstring(iss.number) + L" " + iss.title;
                    WIN32_FIND_DATAW wfd = {};
                    SafeCopyFileName(wfd.cFileName, MAX_PATH, name);
                    wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                    wfd.ftCreationTime = iss.createdAt;
                    wfd.ftLastWriteTime = iss.updatedAt;
                    findData->entries.push_back(wfd);
                }
            } else if (pathInfo.query.view == L"pulls") {
                std::vector<GitHubIssueInfo> pulls;
                try { pulls = GitHubClient::ListPullRequests(pathInfo.owner, pathInfo.repo, pathInfo.query.state.empty() ? L"open" : pathInfo.query.state); } catch (...) { pulls.clear(); }
                for (const auto& pr : pulls) {
                    std::wstring name = L"#" + std::to_wstring(pr.number) + L" " + pr.title;
                    WIN32_FIND_DATAW wfd = {};
                    SafeCopyFileName(wfd.cFileName, MAX_PATH, name);
                    wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                    wfd.ftCreationTime = pr.createdAt;
                    wfd.ftLastWriteTime = pr.updatedAt;
                    findData->entries.push_back(wfd);
                }
            } else if (pathInfo.query.view == L"branches") {
                std::vector<GitHubBranchInfo> branches;
                try { branches = GitHubClient::ListBranches(pathInfo.owner, pathInfo.repo); } catch (...) { branches.clear(); }
                for (const auto& b : branches) {
                    AddFindDirEntry(findData, b.name);
                }
            } else if (pathInfo.query.view == L"tags") {
                std::vector<GitHubBranchInfo> tags;
                try { tags = GitHubClient::ListTags(pathInfo.owner, pathInfo.repo); } catch (...) { tags.clear(); }
                for (const auto& t : tags) {
                    AddFindDirEntry(findData, t.name);
                }
            } else if (pathInfo.query.view == L"releases") {
                std::vector<GitHubReleaseInfo> releases;
                try { releases = GitHubClient::ListReleases(pathInfo.owner, pathInfo.repo); } catch (...) { releases.clear(); }
                for (const auto& rel : releases) {
                    WIN32_FIND_DATAW wfd = {};
                    SafeCopyFileName(wfd.cFileName, MAX_PATH, rel.tagName);
                    wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                    wfd.ftCreationTime = rel.createdAt;
                    wfd.ftLastWriteTime = rel.publishedAt;
                    findData->entries.push_back(wfd);
                }
            }
            break;
        }

        case GITHUB_CTX_SEARCH: {
            if (pathInfo.query.search == L"repos") {
                GitHubSearchResult searchResult;
                try { searchResult = GitHubClient::SearchRepos(pathInfo.query.query, pathInfo.query.page); } catch (...) { searchResult.repos.clear(); }
                for (const auto& r : searchResult.repos) {
                    AddFindRepoEntry(findData, r, true);
                }
            } else if (pathInfo.query.search == L"code") {
                GitHubCodeSearchResult codeResult;
                try { codeResult = GitHubClient::SearchCode(pathInfo.query.query); } catch (...) { codeResult.items.clear(); }
                for (const auto& item : codeResult.items) {
                    std::wstring displayName = L"[" + item.repoFullName + L"] " + item.path;
                    AddFindFileEntry(findData, displayName);
                }
            } else if (pathInfo.query.search == L"users") {
                GitHubUserSearchResult userResult;
                try { userResult = GitHubClient::SearchUsers(pathInfo.query.query); } catch (...) { userResult.users.clear(); }
                for (const auto& u : userResult.users) {
                    AddFindDirEntry(findData, u.login);
                }
            } else if (pathInfo.query.search == L"issues") {
                AddFindFileEntry(findData, L"Issue\u641c\u7d22\u529f\u80fd\u5f00\u53d1\u4e2d.txt");
            }
            break;
        }

        default: {
            WfdFromPathInfo(pathInfo, lpwfdData);
            delete findData;
            return (HANDLE)1;
        }
        }
    } catch (const std::exception& e) {
        OutputDebugStringA("[GitHubVFS] FindFirstFile exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        delete findData;
        return NULL;
    } catch (...) {
        OutputDebugStringW(L"[GitHubVFS] FindFirstFile unknown exception\n");
        delete findData;
        return NULL;
    }

    if (!findData->entries.empty()) {
        *lpwfdData = findData->entries[0];
        findData->currentIndex = 1;
        return (HANDLE)findData;
    }

    delete findData;
    return NULL;
}

extern "C" __declspec(dllexport) HANDLE VFS_FindFirstFileW(HANDLE hVFSData, LPVFSFUNCDATA lpVFSData,
                                                            LPWSTR lpszPath, LPWIN32_FIND_DATAW lpwfdData,
                                                            HANDLE hAbortEvent) {
    __try {
        return VFS_FindFirstFileW_Internal(lpVFSData, lpszPath, lpwfdData, hAbortEvent);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_FindFirstFileW SEH exception\n");
        return NULL;
    }
}

extern "C" __declspec(dllexport) BOOL VFS_FindNextFileW(HANDLE hVFSData, LPVFSFUNCDATA lpVFSData,
                                                         HANDLE hFind, LPWIN32_FIND_DATAW lpwfdData) {
    __try {
        if (!hFind || hFind == (HANDLE)1 || !lpwfdData) return FALSE;

        GitHubFindData* findData = (GitHubFindData*)hFind;
        if (findData->currentIndex < findData->entries.size()) {
            *lpwfdData = findData->entries[findData->currentIndex++];
            return TRUE;
        }
        return FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_FindNextFileW SEH exception\n");
        return FALSE;
    }
}

extern "C" __declspec(dllexport) void VFS_FindClose(HANDLE hVFSData, HANDLE hFind) {
    __try {
        if (hFind && hFind != (HANDLE)1) {
            GitHubFindData* findData = (GitHubFindData*)hFind;
            delete findData;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OutputDebugStringW(L"[GitHubVFS] VFS_FindClose SEH exception\n");
    }
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    INT_PTR result = DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_GITHUB_CONFIG),
                                      hWndParent, ConfigDlgProc, 0);
    if (result == IDOK && hWndNotify) {
        PostMessageW(hWndNotify, DVFSPLUGINMSG_REINITIALIZE, 0, dwNotifyData);
    }
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    MessageBoxW(hWndParent,
                L"GitHub VFS \u63d2\u4ef6 v1.0.0\n\n"
                L"(c) 2026\n\n"
                L"GitHub \u4ed3\u5e93\u865a\u62df\u6587\u4ef6\u7cfb\u7edf\n\n"
                L"\u529f\u80fd\u7279\u6027:\n"
                L"  \u2022 \u591a\u79cd\u767b\u5f55\u65b9\u5f0f (Token/\u8d26\u53f7\u5bc6\u7801/OAuth)\n"
                L"  \u2022 \u6d4f\u89c8 GitHub \u4ed3\u5e93\u6587\u4ef6\n"
                L"  \u2022 \u4e0b\u8f7d\u548c\u4e0a\u4f20\u6587\u4ef6\n"
                L"  \u2022 \u81ea\u5b9a\u4e49\u5217\u663e\u793a\n"
                L"  \u2022 \u53f3\u952e\u83dc\u5355\u64cd\u4f5c\n"
                L"  \u2022 \u4ee3\u7406\u670d\u52a1\u5668\u652f\u6301\n"
                L"  \u2022 \u7f13\u5b58\u652f\u6301",
                L"\u5173\u4e8e GitHub VFS",
                MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    if (pUSBSafeData) {
        pUSBSafeData->pszOtherExports[0] = L'\0';
    }
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
