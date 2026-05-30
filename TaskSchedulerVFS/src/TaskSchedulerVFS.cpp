#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <strsafe.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <comdef.h>
#include <taskschd.h>

#ifndef ERROR_INVALID_PATH
#define ERROR_INVALID_PATH 123L
#endif

#ifndef ERROR_OPEN_FAILED
#define ERROR_OPEN_FAILED 110L
#endif

typedef const BYTE* LPCBYTE;
typedef BYTE* LPBYTE;

#ifndef DOPUS_PLUGIN_HELPER
#define DOPUS_PLUGIN_HELPER
#endif
#ifndef VFSPLUGINVERSION
#define VFSPLUGINVERSION 2
#endif
#include "vfs_plugins.h"
#include "plugin_support.h"
#include "resource.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Taskschd.lib")

static const GUID GUIDPlugin_TaskScheduler =
{ 0xD4E5F6A7, 0xB8C9, 0x0123, { 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23 } };

#define TASKSCHD_VFS_PREFIX L"taskschd://"
#define TASKSCHD_VFS_PREFIX_LEN 11

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);
static int g_currentPage = 0;

struct TaskSchdConfig
{
    bool autoRefresh;
    int refreshInterval;
    int defaultAction;
    bool confirmDelete;
    bool showSystemTasks;
    bool colState;
    bool colTriggers;
    bool colLastRun;
    bool colNextRun;
    bool colAuthor;
    bool colExecutable;
    bool colUserId;
    bool colResult;
    bool colDescription;
    int killMethod;
    bool confirmRun;
    bool warnSystem;
    bool autoRefreshAction;
    bool confirmEnable;
    int cacheTimeout;
    bool verboseLog;
    std::wstring logPath;
    std::wstring configPath;

    TaskSchdConfig()
    {
        autoRefresh = true;
        refreshInterval = 30;
        defaultAction = 0;
        confirmDelete = true;
        showSystemTasks = false;
        colState = true;
        colTriggers = true;
        colLastRun = true;
        colNextRun = true;
        colAuthor = true;
        colExecutable = false;
        colUserId = true;
        colResult = true;
        colDescription = false;
        killMethod = 0;
        confirmRun = false;
        warnSystem = true;
        autoRefreshAction = true;
        confirmEnable = false;
        cacheTimeout = 60;
        verboseLog = false;
        logPath = L"";
        configPath = L"";
    }
};

static TaskSchdConfig g_config;

static std::wstring GetPluginDataDir()
{
    WCHAR appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::wstring basePath = std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data";
        std::wstring vfsPath = basePath + L"\\VFSPlugin";
        std::wstring dir = vfsPath + L"\\TaskSchedulerVFS";
        CreateDirectoryW(basePath.c_str(), NULL);
        CreateDirectoryW(vfsPath.c_str(), NULL);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    WCHAR modulePath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);
    std::wstring dir(modulePath);
    size_t lastSlash = dir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) dir = dir.substr(0, lastSlash);
    return dir;
}

static std::wstring GetLogFilePath()
{
    if (!g_config.logPath.empty())
        return g_config.logPath;

    return GetPluginDataDir() + L"\\TaskSchedulerVFS.log";
}

static CRITICAL_SECTION g_logCS;

static void LogMessage(LPCWSTR format, ...)
{
    if (!g_config.verboseLog) return;

    va_list args;
    va_start(args, format);

    WCHAR buf[2048];
    StringCchVPrintfW(buf, 2048, format, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);

    WCHAR line[4096];
    StringCchPrintfW(line, 4096, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, buf);

    std::wstring logPath = GetLogFilePath();
    EnterCriticalSection(&g_logCS);
    HANDLE hFile = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart == 0)
        {
            WCHAR bom = 0xFEFF;
            WriteFile(hFile, &bom, sizeof(WCHAR), &written, NULL);
        }
        else if (fileSize.QuadPart > 10 * 1024 * 1024)
        {
            CloseHandle(hFile);
            std::wstring oldPath = logPath + L".old";
            MoveFileExW(logPath.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING);
            hFile = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                LeaveCriticalSection(&g_logCS);
                return;
            }
            WCHAR bom = 0xFEFF;
            WriteFile(hFile, &bom, sizeof(WCHAR), &written, NULL);
        }
        SetFilePointer(hFile, 0, NULL, FILE_END);
        WriteFile(hFile, line, (DWORD)(wcslen(line) * sizeof(WCHAR)), &written, NULL);
        CloseHandle(hFile);
    }
    LeaveCriticalSection(&g_logCS);
}

static std::wstring GetDefaultConfigFilePath()
{
    return GetPluginDataDir() + L"\\TaskSchedulerVFS.ini";
}

static std::wstring GetConfigFilePath()
{
    if (!g_config.configPath.empty())
        return g_config.configPath;

    return GetDefaultConfigFilePath();
}

static void LoadConfig()
{
    std::wstring defaultIniPath = GetDefaultConfigFilePath();
    LPCWSTR section = L"TaskSchedulerVFS";

    WCHAR bufPath[1024] = {};
    GetPrivateProfileStringW(section, L"ConfigPath", L"", bufPath, 1024, defaultIniPath.c_str());
    g_config.configPath = bufPath;

    WCHAR bufLog[1024] = {};
    GetPrivateProfileStringW(section, L"LogPath", L"", bufLog, 1024, defaultIniPath.c_str());
    g_config.logPath = bufLog;

    std::wstring iniPath = GetConfigFilePath();

    g_config.autoRefresh = GetPrivateProfileIntW(section, L"AutoRefresh", g_config.autoRefresh ? 1 : 0, iniPath.c_str()) != 0;
    g_config.refreshInterval = GetPrivateProfileIntW(section, L"RefreshInterval", g_config.refreshInterval, iniPath.c_str());
    g_config.defaultAction = GetPrivateProfileIntW(section, L"DefaultAction", g_config.defaultAction, iniPath.c_str());
    g_config.confirmDelete = GetPrivateProfileIntW(section, L"ConfirmDelete", g_config.confirmDelete ? 1 : 0, iniPath.c_str()) != 0;
    g_config.showSystemTasks = GetPrivateProfileIntW(section, L"ShowSystemTasks", g_config.showSystemTasks ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colState = GetPrivateProfileIntW(section, L"ColState", g_config.colState ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colTriggers = GetPrivateProfileIntW(section, L"ColTriggers", g_config.colTriggers ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colLastRun = GetPrivateProfileIntW(section, L"ColLastRun", g_config.colLastRun ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colNextRun = GetPrivateProfileIntW(section, L"ColNextRun", g_config.colNextRun ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colAuthor = GetPrivateProfileIntW(section, L"ColAuthor", g_config.colAuthor ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colExecutable = GetPrivateProfileIntW(section, L"ColExecutable", g_config.colExecutable ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colUserId = GetPrivateProfileIntW(section, L"ColUserId", g_config.colUserId ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colResult = GetPrivateProfileIntW(section, L"ColResult", g_config.colResult ? 1 : 0, iniPath.c_str()) != 0;
    g_config.colDescription = GetPrivateProfileIntW(section, L"ColDescription", g_config.colDescription ? 1 : 0, iniPath.c_str()) != 0;
    g_config.killMethod = GetPrivateProfileIntW(section, L"KillMethod", g_config.killMethod, iniPath.c_str());
    g_config.confirmRun = GetPrivateProfileIntW(section, L"ConfirmRun", g_config.confirmRun ? 1 : 0, iniPath.c_str()) != 0;
    g_config.warnSystem = GetPrivateProfileIntW(section, L"WarnSystem", g_config.warnSystem ? 1 : 0, iniPath.c_str()) != 0;
    g_config.autoRefreshAction = GetPrivateProfileIntW(section, L"AutoRefreshAction", g_config.autoRefreshAction ? 1 : 0, iniPath.c_str()) != 0;
    g_config.confirmEnable = GetPrivateProfileIntW(section, L"ConfirmEnable", g_config.confirmEnable ? 1 : 0, iniPath.c_str()) != 0;
    g_config.cacheTimeout = GetPrivateProfileIntW(section, L"CacheTimeout", g_config.cacheTimeout, iniPath.c_str());
    g_config.verboseLog = GetPrivateProfileIntW(section, L"VerboseLog", g_config.verboseLog ? 1 : 0, iniPath.c_str()) != 0;
}

static void SaveConfig()
{
    std::wstring iniPath = GetConfigFilePath();
    std::wstring defaultIniPath = GetDefaultConfigFilePath();
    LPCWSTR section = L"TaskSchedulerVFS";

    WritePrivateProfileStringW(section, L"AutoRefresh", std::to_wstring(g_config.autoRefresh ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"RefreshInterval", std::to_wstring(g_config.refreshInterval).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"DefaultAction", std::to_wstring(g_config.defaultAction).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ConfirmDelete", std::to_wstring(g_config.confirmDelete ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ShowSystemTasks", std::to_wstring(g_config.showSystemTasks ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColState", std::to_wstring(g_config.colState ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColTriggers", std::to_wstring(g_config.colTriggers ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColLastRun", std::to_wstring(g_config.colLastRun ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColNextRun", std::to_wstring(g_config.colNextRun ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColAuthor", std::to_wstring(g_config.colAuthor ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColExecutable", std::to_wstring(g_config.colExecutable ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColUserId", std::to_wstring(g_config.colUserId ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColResult", std::to_wstring(g_config.colResult ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ColDescription", std::to_wstring(g_config.colDescription ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"KillMethod", std::to_wstring(g_config.killMethod).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ConfirmRun", std::to_wstring(g_config.confirmRun ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"WarnSystem", std::to_wstring(g_config.warnSystem ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"AutoRefreshAction", std::to_wstring(g_config.autoRefreshAction ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ConfirmEnable", std::to_wstring(g_config.confirmEnable ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"CacheTimeout", std::to_wstring(g_config.cacheTimeout).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"VerboseLog", std::to_wstring(g_config.verboseLog ? 1 : 0).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"LogPath", g_config.logPath.empty() ? NULL : g_config.logPath.c_str(), defaultIniPath.c_str());
    WritePrivateProfileStringW(section, L"ConfigPath", g_config.configPath.empty() ? NULL : g_config.configPath.c_str(), defaultIniPath.c_str());
}

static bool IsTaskSchdVfsPath(LPCWSTR pszPath)
{
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, TASKSCHD_VFS_PREFIX, TASKSCHD_VFS_PREFIX_LEN) == 0;
}

static bool IsTaskSchdRootPath(LPCWSTR pszPath)
{
    if (!pszPath) return false;

    if (_wcsicmp(pszPath, TASKSCHD_VFS_PREFIX) == 0)
        return true;

    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/')
        s.pop_back();

    if (_wcsicmp(s.c_str(), L"taskschd:") == 0)
        return true;

    return false;
}

static std::wstring ParseTaskPath(LPCWSTR pszPath)
{
    if (!IsTaskSchdVfsPath(pszPath)) return L"";

    std::wstring path = pszPath + TASKSCHD_VFS_PREFIX_LEN;

    while (!path.empty() && path.front() == L'/')
        path.erase(0, 1);

    while (!path.empty() && path.back() == L'/')
        path.pop_back();

    return path;
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str)
{
    if (!hHeap) return NULL;

    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf)
    {
        StringCchCopyW(buf, len, str.c_str());
    }
    return buf;
}

static std::wstring GetTaskStateName(TASK_STATE state)
{
    static const std::map<TASK_STATE, std::wstring> stateNames = {
        {TASK_STATE_UNKNOWN, L"\u672a\u77e5"},
        {TASK_STATE_DISABLED, L"\u5df2\u7981\u7528"},
        {TASK_STATE_QUEUED, L"\u6392\u961f\u4e2d"},
        {TASK_STATE_READY, L"\u5c31\u7eea"},
        {TASK_STATE_RUNNING, L"\u6b63\u5728\u8fd0\u884c"}
    };
    auto it = stateNames.find(state);
    if (it != stateNames.end())
        return it->second;
    return L"\u672a\u77e5";
}

struct TaskInfo
{
    std::wstring name;
    std::wstring path;
    std::wstring description;
    std::wstring author;
    std::wstring triggers;
    std::wstring executable;
    std::wstring arguments;
    std::wstring workingDir;
    std::wstring userId;
    TASK_STATE state;
    bool enabled;
    SYSTEMTIME lastRunTime;
    SYSTEMTIME nextRunTime;
    LONG lastResult;
};

struct FolderInfo
{
    std::wstring name;
    std::wstring path;
    int taskCount;
};

struct TaskCache
{
    std::vector<FolderInfo> folders;
    std::vector<TaskInfo> tasks;
    std::wstring folderPath;
    DWORD timestamp;
    bool valid;

    TaskCache() : timestamp(0), valid(false) {}
};

static TaskCache g_taskCache;
static CRITICAL_SECTION g_cacheCS;

static void InitCacheCS()
{
    InitializeCriticalSection(&g_cacheCS);
    InitializeCriticalSection(&g_logCS);
}

static void FreeCacheCS()
{
    DeleteCriticalSection(&g_cacheCS);
    DeleteCriticalSection(&g_logCS);
}

static bool IsCacheValid(const std::wstring& folderPath)
{
    if (!g_taskCache.valid) return false;
    if (g_taskCache.folderPath != folderPath) return false;

    DWORD elapsed = GetTickCount() - g_taskCache.timestamp;
    return elapsed < (DWORD)(g_config.cacheTimeout * 1000);
}

static void UpdateCache(const std::wstring& folderPath, const std::vector<FolderInfo>& folders, const std::vector<TaskInfo>& tasks)
{
    LogMessage(L"UpdateCache: 更新缓存 路径=%s 文件夹=%d 任务=%d", folderPath.c_str(), (int)folders.size(), (int)tasks.size());
    EnterCriticalSection(&g_cacheCS);
    g_taskCache.folders = folders;
    g_taskCache.tasks = tasks;
    g_taskCache.folderPath = folderPath;
    g_taskCache.timestamp = GetTickCount();
    g_taskCache.valid = true;
    LeaveCriticalSection(&g_cacheCS);
}

static bool GetCachedData(const std::wstring& folderPath, std::vector<FolderInfo>& folders, std::vector<TaskInfo>& tasks)
{
    EnterCriticalSection(&g_cacheCS);
    bool valid = IsCacheValid(folderPath);
    if (valid)
    {
        folders = g_taskCache.folders;
        tasks = g_taskCache.tasks;
    }
    LeaveCriticalSection(&g_cacheCS);
    return valid;
}

static void InvalidateCache()
{
    LogMessage(L"InvalidateCache: 缓存已失效");
    EnterCriticalSection(&g_cacheCS);
    g_taskCache.valid = false;
    LeaveCriticalSection(&g_cacheCS);
}

static bool IsTaskFolderPath(LPCWSTR pszPath)
{
    if (!IsTaskSchdVfsPath(pszPath)) return false;
    std::wstring subPath = ParseTaskPath(pszPath);
    if (subPath.empty()) return false;
    return subPath.find(L'/') == std::wstring::npos && !subPath.empty();
}

static std::wstring GetFolderFromPath(LPCWSTR pszPath)
{
    std::wstring subPath = ParseTaskPath(pszPath);
    if (subPath.empty()) return L"";
    size_t slashPos = subPath.find(L'/');
    if (slashPos == std::wstring::npos) return subPath;
    return L"";
}

static std::wstring GetSubFolderPath(LPCWSTR pszPath)
{
    std::wstring subPath = ParseTaskPath(pszPath);
    if (subPath.empty()) return L"";
    size_t slashPos = subPath.find(L'/');
    if (slashPos == std::wstring::npos) return L"";
    return subPath.substr(slashPos + 1);
}

static bool IsFolderPath(const std::wstring& path)
{
    if (path.empty()) return true;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pFolder = NULL;
    std::wstring folderPath = path.empty() ? L"\\" : path;
    hr = pService->GetFolder(_bstr_t(folderPath.c_str()), &pFolder);
    pService->Release();
    if (needUninitialize) CoUninitialize();

    if (SUCCEEDED(hr) && pFolder)
    {
        pFolder->Release();
        return true;
    }
    return false;
}

static bool GetFolderInfo(const std::wstring& folderPath, FolderInfo& info)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pFolder = NULL;
    std::wstring path = folderPath.empty() ? L"\\" : folderPath;
    hr = pService->GetFolder(_bstr_t(path.c_str()), &pFolder);
    pService->Release();

    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    BSTR bstrName = NULL;
    pFolder->get_Name(&bstrName);
    info.name = bstrName ? bstrName : L"";
    SysFreeString(bstrName);

    BSTR bstrPath = NULL;
    pFolder->get_Path(&bstrPath);
    info.path = bstrPath ? bstrPath : L"";
    SysFreeString(bstrPath);

    IRegisteredTaskCollection* pTasks = NULL;
    if (SUCCEEDED(pFolder->GetTasks(0, &pTasks)))
    {
        LONG taskCount = 0;
        pTasks->get_Count(&taskCount);
        info.taskCount = (int)taskCount;
        pTasks->Release();
    }

    pFolder->Release();
    if (needUninitialize) CoUninitialize();
    return true;
}

static bool IsSystemTask(const std::wstring& taskPath)
{
    if (taskPath.empty()) return false;

    if (taskPath.find(L"\\Microsoft\\") == 0) return true;
    if (taskPath.find(L"/Microsoft/") == 0) return true;

    return false;
}

static std::wstring FormatSystemTime(const SYSTEMTIME& st)
{
    if (st.wYear == 0 || st.wYear == 1601)
        return L"\u4ece\u672a\u8fd0\u884c";

    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L"%04d/%02d/%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::wstring GetLastResultText(LONG hr)
{
    if (hr == 0)
        return L"\u6210\u529f";
    if (hr == SCHED_S_TASK_HAS_NOT_RUN)
        return L"\u5c1a\u672a\u8fd0\u884c";
    if (hr == SCHED_S_TASK_DISABLED)
        return L"\u5df2\u7981\u7528";
    if (hr == SCHED_S_TASK_NO_MORE_RUNS)
        return L"\u65e0\u66f4\u591a\u8ba1\u5212\u8fd0\u884c";
    if (hr == SCHED_S_TASK_NOT_SCHEDULED)
        return L"\u672a\u8ba1\u5212";

    _com_error err(hr);
    std::wstring result = L"\u9519\u8bef: ";
    BSTR descBstr = err.Description();
    if (descBstr != NULL && ::SysStringLen(descBstr) > 0)
        result += (LPCWSTR)descBstr;
    else
    {
        WCHAR hexBuf[32];
        StringCchPrintfW(hexBuf, 32, L"0x%08X", (DWORD)hr);
        result += hexBuf;
    }
    return result;
}

static void FillFolderColumnData(LPVFSFILEDATACOLUMNW lpColumnData, HANDLE hHeap, const FolderInfo& folder)
{
    if (!lpColumnData) return;

    lpColumnData[0].iColumnId = 1;
    lpColumnData[0].lpszValue = AllocString(hHeap, L"\u6587\u4ef6\u5939");

    lpColumnData[1].iColumnId = 2;
    lpColumnData[1].lpszValue = AllocString(hHeap, L"");

    lpColumnData[2].iColumnId = 3;
    lpColumnData[2].lpszValue = AllocString(hHeap, L"");

    lpColumnData[3].iColumnId = 4;
    lpColumnData[3].lpszValue = AllocString(hHeap, L"");

    lpColumnData[4].iColumnId = 5;
    lpColumnData[4].lpszValue = AllocString(hHeap, L"");

    lpColumnData[5].iColumnId = 6;
    lpColumnData[5].lpszValue = AllocString(hHeap, std::to_wstring(folder.taskCount) + L" \u4e2a\u4efb\u52a1");

    lpColumnData[6].iColumnId = 7;
    lpColumnData[6].lpszValue = AllocString(hHeap, L"");

    lpColumnData[7].iColumnId = 8;
    lpColumnData[7].lpszValue = AllocString(hHeap, L"");

    lpColumnData[8].iColumnId = 9;
    lpColumnData[8].lpszValue = AllocString(hHeap, folder.path);
}

static void FillTaskColumnData(LPVFSFILEDATACOLUMNW lpColumnData, HANDLE hHeap, const TaskInfo& task)
{
    if (!lpColumnData) return;

    std::wstring shortDesc = task.description;
    if (shortDesc.length() > 60)
        shortDesc = shortDesc.substr(0, 60) + L"...";

    lpColumnData[0].iColumnId = 1;
    lpColumnData[0].lpszValue = AllocString(hHeap, GetTaskStateName(task.state));

    lpColumnData[1].iColumnId = 2;
    lpColumnData[1].lpszValue = AllocString(hHeap, task.triggers);

    lpColumnData[2].iColumnId = 3;
    lpColumnData[2].lpszValue = AllocString(hHeap, FormatSystemTime(task.lastRunTime));

    lpColumnData[3].iColumnId = 4;
    lpColumnData[3].lpszValue = AllocString(hHeap, FormatSystemTime(task.nextRunTime));

    lpColumnData[4].iColumnId = 5;
    lpColumnData[4].lpszValue = AllocString(hHeap, task.author);

    lpColumnData[5].iColumnId = 6;
    lpColumnData[5].lpszValue = AllocString(hHeap, task.executable);

    lpColumnData[6].iColumnId = 7;
    lpColumnData[6].lpszValue = AllocString(hHeap, task.userId);

    lpColumnData[7].iColumnId = 8;
    lpColumnData[7].lpszValue = AllocString(hHeap, GetLastResultText(task.lastResult));

    lpColumnData[8].iColumnId = 9;
    lpColumnData[8].lpszValue = AllocString(hHeap, shortDesc);
}

static std::wstring GetTriggerTypeText(ITrigger* pTrigger)
{
    TASK_TRIGGER_TYPE2 type;
    if (FAILED(pTrigger->get_Type(&type)))
        return L"\u672a\u77e5";

    switch (type)
    {
    case TASK_TRIGGER_EVENT: return L"\u4e8b\u4ef6";
    case TASK_TRIGGER_TIME: return L"\u65f6\u95f4";
    case TASK_TRIGGER_DAILY: return L"\u6bcf\u65e5";
    case TASK_TRIGGER_WEEKLY: return L"\u6bcf\u5468";
    case TASK_TRIGGER_MONTHLY: return L"\u6bcf\u6708";
    case TASK_TRIGGER_MONTHLYDOW: return L"\u6708\u4e2d\u661f\u671f";
    case TASK_TRIGGER_IDLE: return L"\u7a7a\u95f2";
    case TASK_TRIGGER_REGISTRATION: return L"\u6ce8\u518c";
    case TASK_TRIGGER_BOOT: return L"\u542f\u52a8";
    case TASK_TRIGGER_LOGON: return L"\u767b\u5f55";
    case TASK_TRIGGER_SESSION_STATE_CHANGE: return L"\u4f1a\u8bdd\u53d8\u66f4";
    default: return L"\u5176\u4ed6";
    }
}

struct TaskFileContext
{
    std::vector<BYTE> buffer;
    size_t readPos;
    bool isWrite;
};

static bool GetTaskInfo(ITaskService* pService, const std::wstring& taskPath, TaskInfo& info)
{
    ITaskFolder* pRootFolder = NULL;
    HRESULT hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) return false;

    IRegisteredTask* pTask = NULL;
    hr = pRootFolder->GetTask(_bstr_t(taskPath.c_str()), &pTask);
    pRootFolder->Release();

    if (FAILED(hr)) return false;

    BSTR bstrName = NULL;
    pTask->get_Name(&bstrName);
    info.name = bstrName ? bstrName : L"";
    SysFreeString(bstrName);

    BSTR bstrPath = NULL;
    pTask->get_Path(&bstrPath);
    info.path = bstrPath ? bstrPath : L"";
    SysFreeString(bstrPath);

    TASK_STATE state;
    pTask->get_State(&state);
    info.state = state;

    VARIANT_BOOL enabled = VARIANT_TRUE;
    pTask->get_Enabled(&enabled);
    info.enabled = (enabled == VARIANT_TRUE);

    DATE lastRun = 0;
    pTask->get_LastRunTime(&lastRun);
    if (lastRun != 0)
    {
        SYSTEMTIME st;
        VariantTimeToSystemTime(lastRun, &st);
        info.lastRunTime = st;
    }
    else
    {
        memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
    }

    DATE nextRun = 0;
    pTask->get_NextRunTime(&nextRun);
    if (nextRun != 0)
    {
        SYSTEMTIME st;
        VariantTimeToSystemTime(nextRun, &st);
        info.nextRunTime = st;
    }
    else
    {
        memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
    }

    pTask->get_LastTaskResult(&info.lastResult);

    ITaskDefinition* pDef = NULL;
    hr = pTask->get_Definition(&pDef);
    if (SUCCEEDED(hr))
    {
        IRegistrationInfo* pRegInfo = NULL;
        hr = pDef->get_RegistrationInfo(&pRegInfo);
        if (SUCCEEDED(hr) && pRegInfo)
        {
            BSTR bstrDesc = NULL;
            pRegInfo->get_Description(&bstrDesc);
            info.description = bstrDesc ? bstrDesc : L"";
            SysFreeString(bstrDesc);

            BSTR bstrAuth = NULL;
            pRegInfo->get_Author(&bstrAuth);
            info.author = bstrAuth ? bstrAuth : L"";
            SysFreeString(bstrAuth);

            pRegInfo->Release();
        }

        ITriggerCollection* pTriggers = NULL;
        hr = pDef->get_Triggers(&pTriggers);
        if (SUCCEEDED(hr) && pTriggers)
        {
            LONG count = 0;
            pTriggers->get_Count(&count);

            std::wstring triggerText;
            for (LONG ti = 1; ti <= count; ti++)
            {
                ITrigger* pTrigger = NULL;
                if (SUCCEEDED(pTriggers->get_Item(ti, &pTrigger)))
                {
                    if (!triggerText.empty())
                        triggerText += L", ";
                    triggerText += GetTriggerTypeText(pTrigger);
                    pTrigger->Release();
                }
            }

            if (triggerText.empty())
                triggerText = L"\u65e0\u89e6\u53d1\u5668";
            info.triggers = triggerText;

            pTriggers->Release();
        }

        IActionCollection* pActions = NULL;
        hr = pDef->get_Actions(&pActions);
        if (SUCCEEDED(hr) && pActions)
        {
            LONG actCount = 0;
            pActions->get_Count(&actCount);
            for (LONG ai = 1; ai <= actCount; ai++)
            {
                IAction* pAction = NULL;
                if (SUCCEEDED(pActions->get_Item(ai, &pAction)))
                {
                    TASK_ACTION_TYPE actionType;
                    pAction->get_Type(&actionType);

                    if (actionType == TASK_ACTION_EXEC)
                    {
                        IExecAction* pExec = NULL;
                        if (SUCCEEDED(pAction->QueryInterface(__uuidof(IExecAction), (void**)&pExec)))
                        {
                            BSTR bstrPath = NULL;
                            pExec->get_Path(&bstrPath);
                            info.executable = bstrPath ? bstrPath : L"";
                            SysFreeString(bstrPath);

                            BSTR bstrArgs = NULL;
                            pExec->get_Arguments(&bstrArgs);
                            info.arguments = bstrArgs ? bstrArgs : L"";
                            SysFreeString(bstrArgs);

                            BSTR bstrDir = NULL;
                            pExec->get_WorkingDirectory(&bstrDir);
                            info.workingDir = bstrDir ? bstrDir : L"";
                            SysFreeString(bstrDir);

                            pExec->Release();
                        }
                    }

                    pAction->Release();
                }
            }
            pActions->Release();
        }

        IPrincipal* pPrincipal = NULL;
        hr = pDef->get_Principal(&pPrincipal);
        if (SUCCEEDED(hr) && pPrincipal)
        {
            BSTR bstrUserId = NULL;
            pPrincipal->get_UserId(&bstrUserId);
            info.userId = bstrUserId ? bstrUserId : L"";
            SysFreeString(bstrUserId);
            pPrincipal->Release();
        }

        pDef->Release();
    }

    pTask->Release();
    return true;
}

static bool EnumFolders(ITaskFolder* pParentFolder, const std::wstring& parentPath, std::vector<FolderInfo>& folders)
{
    ITaskFolderCollection* pFolderCollection = NULL;
    HRESULT hr = pParentFolder->GetFolders(0, &pFolderCollection);
    if (FAILED(hr)) return false;

    LONG count = 0;
    pFolderCollection->get_Count(&count);

    for (LONG i = 1; i <= count; i++)
    {
        ITaskFolder* pFolder = NULL;
        hr = pFolderCollection->get_Item(_variant_t(i), &pFolder);
        if (FAILED(hr)) continue;

        FolderInfo info;
        info.taskCount = 0;

        BSTR bstrName = NULL;
        pFolder->get_Name(&bstrName);
        info.name = bstrName ? bstrName : L"";
        SysFreeString(bstrName);

        BSTR bstrPath = NULL;
        pFolder->get_Path(&bstrPath);
        info.path = bstrPath ? bstrPath : L"";
        SysFreeString(bstrPath);

        IRegisteredTaskCollection* pTasks = NULL;
        if (SUCCEEDED(pFolder->GetTasks(0, &pTasks)))
        {
            LONG taskCount = 0;
            pTasks->get_Count(&taskCount);
            info.taskCount = (int)taskCount;
            pTasks->Release();
        }

        folders.push_back(info);
        pFolder->Release();
    }

    pFolderCollection->Release();
    return true;
}

static bool EnumFoldersForPath(const std::wstring& folderPath, std::vector<FolderInfo>& folders)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pFolder = NULL;
    std::wstring path = folderPath.empty() ? L"\\" : folderPath;
    hr = pService->GetFolder(_bstr_t(path.c_str()), &pFolder);
    pService->Release();

    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    bool result = EnumFolders(pFolder, folderPath, folders);
    pFolder->Release();
    if (needUninitialize) CoUninitialize();
    return result;
}

static bool EnumTasks(std::vector<TaskInfo>& tasks, const std::wstring& folderPath = L"", bool showHidden = true)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    std::wstring path = folderPath.empty() ? L"\\" : folderPath;
    hr = pService->GetFolder(_bstr_t(path.c_str()), &pRootFolder);
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    IRegisteredTaskCollection* pTaskCollection = NULL;
    hr = pRootFolder->GetTasks(showHidden ? TASK_ENUM_HIDDEN : 0, &pTaskCollection);
    pRootFolder->Release();

    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    LONG count = 0;
    pTaskCollection->get_Count(&count);

    for (LONG i = 1; i <= count; i++)
    {
        IRegisteredTask* pTask = NULL;
        hr = pTaskCollection->get_Item(_variant_t(i), &pTask);
        if (FAILED(hr)) continue;

        TaskInfo info;
        memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
        memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
        info.state = TASK_STATE_UNKNOWN;
        info.enabled = true;
        info.lastResult = 0;

        BSTR bstrName = NULL;
        pTask->get_Name(&bstrName);
        info.name = bstrName ? bstrName : L"";
        SysFreeString(bstrName);

        BSTR bstrPath = NULL;
        pTask->get_Path(&bstrPath);
        info.path = bstrPath ? bstrPath : L"";
        SysFreeString(bstrPath);

        TASK_STATE state;
        pTask->get_State(&state);
        info.state = state;

        VARIANT_BOOL enabled = VARIANT_TRUE;
        pTask->get_Enabled(&enabled);
        info.enabled = (enabled == VARIANT_TRUE);

        DATE lastRun = 0;
        pTask->get_LastRunTime(&lastRun);
        if (lastRun != 0)
        {
            SYSTEMTIME st;
            VariantTimeToSystemTime(lastRun, &st);
            info.lastRunTime = st;
        }

        DATE nextRun = 0;
        pTask->get_NextRunTime(&nextRun);
        if (nextRun != 0)
        {
            SYSTEMTIME st;
            VariantTimeToSystemTime(nextRun, &st);
            info.nextRunTime = st;
        }

        pTask->get_LastTaskResult(&info.lastResult);

        ITaskDefinition* pDef = NULL;
        hr = pTask->get_Definition(&pDef);
        if (SUCCEEDED(hr))
        {
            IRegistrationInfo* pRegInfo = NULL;
            hr = pDef->get_RegistrationInfo(&pRegInfo);
            if (SUCCEEDED(hr) && pRegInfo)
            {
                BSTR bstrDesc = NULL;
                pRegInfo->get_Description(&bstrDesc);
                info.description = bstrDesc ? bstrDesc : L"";
                SysFreeString(bstrDesc);

                BSTR bstrAuth = NULL;
                pRegInfo->get_Author(&bstrAuth);
                info.author = bstrAuth ? bstrAuth : L"";
                SysFreeString(bstrAuth);

                pRegInfo->Release();
            }

            ITriggerCollection* pTriggers = NULL;
            hr = pDef->get_Triggers(&pTriggers);
            if (SUCCEEDED(hr) && pTriggers)
            {
                std::wstring triggerText;
                LONG trigCount = 0;
                pTriggers->get_Count(&trigCount);
                for (LONG ti = 1; ti <= trigCount; ti++)
                {
                    ITrigger* pTrigger = NULL;
                    if (SUCCEEDED(pTriggers->get_Item(ti, &pTrigger)))
                    {
                        if (!triggerText.empty())
                            triggerText += L", ";
                        triggerText += GetTriggerTypeText(pTrigger);
                        pTrigger->Release();
                    }
                }
                if (triggerText.empty())
                    triggerText = L"\u65e0\u89e6\u53d1\u5668";
                info.triggers = triggerText;
                pTriggers->Release();
            }

            IActionCollection* pActions = NULL;
            hr = pDef->get_Actions(&pActions);
            if (SUCCEEDED(hr) && pActions)
            {
                LONG actCount = 0;
                pActions->get_Count(&actCount);
                for (LONG ai = 1; ai <= actCount; ai++)
                {
                    IAction* pAction = NULL;
                    if (SUCCEEDED(pActions->get_Item(ai, &pAction)))
                    {
                        TASK_ACTION_TYPE actionType;
                        pAction->get_Type(&actionType);
                        if (actionType == TASK_ACTION_EXEC)
                        {
                            IExecAction* pExec = NULL;
                            if (SUCCEEDED(pAction->QueryInterface(__uuidof(IExecAction), (void**)&pExec)))
                            {
                                BSTR bstrPath = NULL;
                                pExec->get_Path(&bstrPath);
                                info.executable = bstrPath ? bstrPath : L"";
                                SysFreeString(bstrPath);

                                BSTR bstrArgs = NULL;
                                pExec->get_Arguments(&bstrArgs);
                                info.arguments = bstrArgs ? bstrArgs : L"";
                                SysFreeString(bstrArgs);

                                BSTR bstrDir = NULL;
                                pExec->get_WorkingDirectory(&bstrDir);
                                info.workingDir = bstrDir ? bstrDir : L"";
                                SysFreeString(bstrDir);

                                pExec->Release();
                            }
                        }
                        pAction->Release();
                    }
                }
                pActions->Release();
            }

            IPrincipal* pPrincipal = NULL;
            hr = pDef->get_Principal(&pPrincipal);
            if (SUCCEEDED(hr) && pPrincipal)
            {
                BSTR bstrUserId = NULL;
                pPrincipal->get_UserId(&bstrUserId);
                info.userId = bstrUserId ? bstrUserId : L"";
                SysFreeString(bstrUserId);
                pPrincipal->Release();
            }

            pDef->Release();
        }

        pTask->Release();
        tasks.push_back(info);
    }

    pTaskCollection->Release();
    pService->Release();
    if (needUninitialize) CoUninitialize();
    return true;
}

static std::wstring FormatTaskInfo(const TaskInfo& info)
{
    std::wstring result;

    result += L"\u4efb\u52a1\u540d\u79f0: " + info.name + L"\r\n";
    result += L"\u4efb\u52a1\u8def\u5f84: " + info.path + L"\r\n";
    result += L"\u72b6\u6001: " + GetTaskStateName(info.state) + L"\r\n";
    result += L"\u662f\u5426\u542f\u7528: " + (info.enabled ? std::wstring(L"\u662f") : std::wstring(L"\u5426")) + L"\r\n";
    result += L"\u63cf\u8ff0: " + info.description + L"\r\n";
    result += L"\u4f5c\u8005: " + info.author + L"\r\n";
    result += L"\u89e6\u53d1\u5668: " + info.triggers + L"\r\n";
    result += L"\u53ef\u6267\u884c\u6587\u4ef6: " + info.executable + L"\r\n";
    result += L"\u53c2\u6570: " + info.arguments + L"\r\n";
    result += L"\u5de5\u4f5c\u76ee\u5f55: " + info.workingDir + L"\r\n";
    result += L"\u8fd0\u884c\u7528\u6237: " + info.userId + L"\r\n";
    result += L"\u4e0a\u6b21\u8fd0\u884c: " + FormatSystemTime(info.lastRunTime) + L"\r\n";
    result += L"\u4e0b\u6b21\u8fd0\u884c: " + FormatSystemTime(info.nextRunTime) + L"\r\n";
    result += L"\u4e0a\u6b21\u7ed3\u679c: " + GetLastResultText(info.lastResult) + L"\r\n";

    return result;
}

static bool RunTaskAction(const std::wstring& taskPath, DWORD action)
{
    LogMessage(L"RunTaskAction: 执行操作 %d 于任务 %s", action, taskPath.c_str());

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    IRegisteredTask* pTask = NULL;
    hr = pRootFolder->GetTask(_bstr_t(taskPath.c_str()), &pTask);
    pRootFolder->Release();

    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    BOOL result = FALSE;

    switch (action)
    {
    case 1:
    {
        hr = pTask->Run(_variant_t(), NULL);
        result = SUCCEEDED(hr);
        break;
    }
    case 2:
    {
        hr = pTask->Stop(0);
        if (FAILED(hr) && g_config.killMethod == 1)
        {
            Sleep(500);
            hr = pTask->Stop(0);
        }
        result = SUCCEEDED(hr);
        break;
    }
    case 3:
    {
        pTask->put_Enabled(VARIANT_FALSE);
        hr = pTask->put_Enabled(VARIANT_TRUE);
        result = SUCCEEDED(hr);
        break;
    }
    case 4:
    {
        hr = pTask->put_Enabled(VARIANT_FALSE);
        result = SUCCEEDED(hr);
        break;
    }
    case 5:
    {
        hr = pTask->put_Enabled(VARIANT_TRUE);
        result = SUCCEEDED(hr);
        break;
    }
    case 6:
    {
        hr = pTask->Stop(0);
        if (FAILED(hr) && g_config.killMethod == 1)
        {
            Sleep(500);
            hr = pTask->Stop(0);
        }
        Sleep(300);
        hr = pTask->Run(_variant_t(), NULL);
        result = SUCCEEDED(hr);
        break;
    }
    }

    pTask->Release();
    pService->Release();
    if (needUninitialize) CoUninitialize();
    return result != FALSE;
}

static bool DeleteTaskAction(const std::wstring& taskPath)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    hr = pRootFolder->DeleteTask(_bstr_t(taskPath.c_str()), 0);
    pRootFolder->Release();
    pService->Release();
    if (needUninitialize) CoUninitialize();

    return SUCCEEDED(hr);
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData)
{
    LoadConfig();
    InitCacheCS();
    LogMessage(L"VFS_Init: 插件初始化完成");
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit()
{
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0)
    {
        Sleep(50);
    }
    FreeCacheCS();
}

static HWND g_hwndMsgWindow = NULL;
static HANDLE g_hRefreshThread = NULL;
static volatile bool g_bRefreshThreadRunning = false;

static DWORD WINAPI AutoRefreshThread(LPVOID lpParam)
{
    while (g_bRefreshThreadRunning)
    {
        Sleep(g_config.refreshInterval * 1000);
        if (!g_bRefreshThreadRunning) break;

        if (g_config.autoRefresh)
        {
            InvalidateCache();
            if (g_hwndMsgWindow)
                PostMessageW(g_hwndMsgWindow, DVFSPLUGINMSG_REINITIALIZE, 0, 0);
        }
    }
    return 0;
}

static void StartAutoRefreshThread()
{
    if (g_hRefreshThread != NULL) return;
    if (!g_config.autoRefresh) return;

    g_bRefreshThreadRunning = true;
    g_hRefreshThread = CreateThread(NULL, 0, AutoRefreshThread, NULL, 0, NULL);
}

static void StopAutoRefreshThread()
{
    g_bRefreshThreadRunning = false;
    if (g_hRefreshThread != NULL)
    {
        WaitForSingleObject(g_hRefreshThread, 5000);
        CloseHandle(g_hRefreshThread);
        g_hRefreshThread = NULL;
    }
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow)
{
    g_hwndMsgWindow = hwndMsgWindow;
    StartAutoRefreshThread();
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData)
{
    StopAutoRefreshThread();
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo)
{
    if (!lpVFSInfo) return FALSE;
    
    lpVFSInfo->idPlugin = GUIDPlugin_TaskScheduler;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, TASKSCHD_VFS_PREFIX);

    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"\u4efb\u52a1\u8ba1\u5212");

    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
            L"Windows \u4efb\u52a1\u8ba1\u5212 - \u6d4f\u89c8\u548c\u7ba1\u7406\u8ba1\u5212\u4efb\u52a1");

    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");

    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"shell32.dll", 14, &hIconLarge, &hIconSmall, 1);
    
    if (!hIconLarge)
    {
        ExtractIconExW(L"taskschd.msc", 0, &hIconLarge, &hIconSmall, 1);
    }
    
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax)
{
    if (!lpszPrefix) return FALSE;
    if (cchPrefixMax < 13) return FALSE;
    
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"taskschd://");
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData)
{
    static VFSCUSTOMCOLUMNW columns[9];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\u72b6\u6001";
    columns[0].lpszKey = L"tskstate";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\u89e6\u53d1\u5668";
    columns[1].lpszKey = L"tsktriggers";
    columns[1].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[1].iID = 2;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\u4e0a\u6b21\u8fd0\u884c";
    columns[2].lpszKey = L"tsklastrun";
    columns[2].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[2].iID = 3;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\u4e0b\u6b21\u8fd0\u884c";
    columns[3].lpszKey = L"tsknextrun";
    columns[3].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[3].iID = 4;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\u4f5c\u8005";
    columns[4].lpszKey = L"tskauthor";
    columns[4].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[4].iID = 5;

    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"\u53ef\u6267\u884c\u6587\u4ef6";
    columns[5].lpszKey = L"tskexe";
    columns[5].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[5].iID = 6;

    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\u8fd0\u884c\u7528\u6237";
    columns[6].lpszKey = L"tskuser";
    columns[6].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[6].iID = 7;

    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[7].lpNext = &columns[8];
    columns[7].lpszLabel = L"\u4e0a\u6b21\u7ed3\u679c";
    columns[7].lpszKey = L"tskresult";
    columns[7].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[7].iID = 8;

    columns[8].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[8].lpNext = NULL;
    columns[8].lpszLabel = L"\u63cf\u8ff0";
    columns[8].lpszKey = L"tskdesc";
    columns[8].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[8].iID = 9;

    return columns;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    LogMessage(L"InternalReadDirectory: 读取目录 %s", lpRDD->lpszPath);

    std::wstring folderPath = ParseTaskPath(lpRDD->lpszPath);

    std::vector<FolderInfo> folders;
    std::vector<TaskInfo> tasks;

    if (g_config.cacheTimeout > 0 && GetCachedData(folderPath, folders, tasks))
    {
    }
    else
    {
        EnumFoldersForPath(folderPath, folders);
        EnumTasks(tasks, folderPath, g_config.showSystemTasks);
        if (g_config.cacheTimeout > 0)
            UpdateCache(folderPath, folders, tasks);
    }

    int numFolders = (int)folders.size();
    int numTasks = (int)tasks.size();
    int numItems = numFolders + numTasks;

    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);

    if (!lpFDH) return FALSE;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    int itemIndex = 0;

    for (int i = 0; i < numFolders; i++)
    {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0)
            break;

        StringCchCopyW(lpFileData[itemIndex].wfdData.cFileName, MAX_PATH, folders[i].name.c_str());
        lpFileData[itemIndex].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[itemIndex].wfdData.ftCreationTime);
        lpFileData[itemIndex].wfdData.ftLastAccessTime = lpFileData[itemIndex].wfdData.ftCreationTime;
        lpFileData[itemIndex].wfdData.ftLastWriteTime = lpFileData[itemIndex].wfdData.ftCreationTime;

        const int NUM_COLS = 9;
        lpFileData[itemIndex].iNumColumns = NUM_COLS;
        lpFileData[itemIndex].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
            NUM_COLS * sizeof(VFSFILEDATACOLUMNW));

        FillFolderColumnData(lpFileData[itemIndex].lpvfsColumnData, lpRDD->hMemHeap, folders[i]);

        itemIndex++;
    }

    for (int i = 0; i < numTasks; i++)
    {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0)
            break;

        StringCchCopyW(lpFileData[itemIndex].wfdData.cFileName, MAX_PATH, tasks[i].name.c_str());
        lpFileData[itemIndex].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        GetSystemTimeAsFileTime(&lpFileData[itemIndex].wfdData.ftCreationTime);
        lpFileData[itemIndex].wfdData.ftLastAccessTime = lpFileData[itemIndex].wfdData.ftCreationTime;
        lpFileData[itemIndex].wfdData.ftLastWriteTime = lpFileData[itemIndex].wfdData.ftCreationTime;

        if (!tasks[i].enabled)
        {
            lpFileData[itemIndex].wfdData.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
        }

        const int NUM_COLS = 9;
        lpFileData[itemIndex].iNumColumns = NUM_COLS;
        lpFileData[itemIndex].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
            NUM_COLS * sizeof(VFSFILEDATACOLUMNW));

        FillTaskColumnData(lpFileData[itemIndex].lpvfsColumnData, lpRDD->hMemHeap, tasks[i]);

        itemIndex++;
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD) return FALSE;

    switch (lpRDD->vfsReadOp)
    {
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
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags)
{
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    std::wstring taskPath = ParseTaskPath(lpszPath);

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    if (taskPath.empty())
    {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u4efb\u52a1\u8ba1\u5212");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (IsFolderPath(taskPath))
    {
        FolderInfo folderInfo;
        if (GetFolderInfo(taskPath, folderInfo))
        {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, folderInfo.name.c_str());
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);

            const int NUM_COLS = 9;
            lpFileData->iNumColumns = NUM_COLS;
            lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                NUM_COLS * sizeof(VFSFILEDATACOLUMNW));

            FillFolderColumnData(lpFileData->lpvfsColumnData, hHeap, folderInfo);

            return lpFDH;
        }
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        HeapFree(hHeap, 0, lpFDH);
        if (needUninitialize) CoUninitialize();
        return NULL;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        HeapFree(hHeap, 0, lpFDH);
        if (needUninitialize) CoUninitialize();
        return NULL;
    }

    TaskInfo info;
    memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
    memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
    bool found = GetTaskInfo(pService, taskPath, info);
    pService->Release();
    if (needUninitialize) CoUninitialize();

    if (!found)
    {
        HeapFree(hHeap, 0, lpFDH);
        return NULL;
    }

    std::wstring taskInfo = FormatTaskInfo(info);

    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, info.name.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    lpFileData->wfdData.nFileSizeLow = (DWORD)(taskInfo.length() * sizeof(WCHAR));
    lpFileData->wfdData.nFileSizeHigh = 0;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
    lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
    lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;

    if (!info.enabled)
    {
        lpFileData->wfdData.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    }

    const int NUM_COLS = 9;
    lpFileData->iNumColumns = NUM_COLS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        NUM_COLS * sizeof(VFSFILEDATACOLUMNW));

    FillTaskColumnData(lpFileData->lpvfsColumnData, hHeap, info);

    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
    DWORD dwFlags, LPFILETIME lpFT)
{
    if (!IsTaskSchdVfsPath(lpszFile))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    std::wstring taskPath = ParseTaskPath(lpszFile);
    if (taskPath.empty())
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        SetLastError(ERROR_OPEN_FAILED);
        return NULL;
    }

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr))
    {
        pService->Release();
        if (needUninitialize) CoUninitialize();
        SetLastError(ERROR_OPEN_FAILED);
        return NULL;
    }

    TaskInfo info;
    memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
    memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
    bool found = GetTaskInfo(pService, taskPath, info);
    pService->Release();
    if (needUninitialize) CoUninitialize();

    if (!found)
    {
        SetLastError(ERROR_OPEN_FAILED);
        return NULL;
    }

    TaskFileContext* ctx = new TaskFileContext();
    if (!ctx)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    ctx->readPos = 0;
    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;

    std::wstring taskInfo = FormatTaskInfo(info);
    ctx->buffer.resize(2 + taskInfo.length() * sizeof(WCHAR));
    WCHAR bom = 0xFEFF;
    memcpy(ctx->buffer.data(), &bom, 2);
    memcpy(ctx->buffer.data() + 2, taskInfo.c_str(), taskInfo.length() * sizeof(WCHAR));

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize)
{
    TaskFileContext* ctx = (TaskFileContext*)hFile;

    if (!ctx || ctx->isWrite)
        return FALSE;

    if (lpdwReadSize)
        *lpdwReadSize = 0;

    if (ctx->readPos >= ctx->buffer.size())
        return TRUE;

    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->buffer.size() - ctx->readPos));

    if (bytesToRead > 0 && lpData != NULL)
    {
        CopyMemory(lpData, ctx->buffer.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;

        if (lpdwReadSize)
            *lpdwReadSize = bytesToRead;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush,
    LPDWORD lpdwWriteSize)
{
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, __int64 iPos, DWORD dwMethod,
    DWORD dwFlags, unsigned __int64* piNewPos)
{
    TaskFileContext* ctx = (TaskFileContext*)hFile;

    if (!ctx)
        return FALSE;

    __int64 newPos = 0;

    switch (dwMethod)
    {
    case FILE_BEGIN:
        newPos = iPos;
        break;
    case FILE_CURRENT:
        newPos = ctx->readPos + iPos;
        break;
    case FILE_END:
        newPos = ctx->buffer.size() + iPos;
        break;
    default:
        return FALSE;
    }

    if (newPos < 0)
        return FALSE;

    ctx->readPos = (size_t)min(newPos, (__int64)ctx->buffer.size());

    if (piNewPos)
        *piNewPos = ctx->readPos;

    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile)
{
    TaskFileContext* ctx = (TaskFileContext*)hFile;

    if (!ctx) return;

    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwFlags, int iSecurePasses)
{
    if (!IsTaskSchdVfsPath(lpszFile)) return FALSE;

    std::wstring taskPath = ParseTaskPath(lpszFile);
    if (taskPath.empty()) return FALSE;

    if (g_config.confirmDelete)
    {
        if (MessageBoxW(NULL,
            L"\u786e\u5b9a\u8981\u5220\u9664\u6b64\u4efb\u52a1\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002",
            L"\u786e\u8ba4\u5220\u9664",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        {
            return FALSE;
        }
    }

    return DeleteTaskAction(taskPath);
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags)
{
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags)
{
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszOldName, LPWSTR lpszNewName)
{
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszOldName, LPWSTR lpszNewName)
{
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath,
    LPWSTR lpszDisplayName, int cbDisplayNameMax)
{
    if (!lpszPath || !lpszDisplayName) return FALSE;

    if (IsTaskSchdRootPath(lpszPath))
    {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4efb\u52a1\u8ba1\u5212");
        return TRUE;
    }

    std::wstring taskPath = ParseTaskPath(lpszPath);
    if (taskPath.empty())
    {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u4efb\u52a1\u8ba1\u5212");
        return TRUE;
    }

    size_t lastSlash = taskPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
    {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, taskPath.substr(lastSlash + 1).c_str());
    }
    else
    {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, taskPath.c_str());
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
    LPWSTR lpszNewPath, int cbNewPathMax)
{
    if (!lpszPath || !lpszNewPath) return FALSE;

    if (fRoot)
    {
        StringCchCopyW(lpszNewPath, cbNewPathMax, TASKSCHD_VFS_PREFIX);
        return TRUE;
    }

    if (IsTaskSchdRootPath(lpszPath))
    {
        return FALSE;
    }

    std::wstring subPath = ParseTaskPath(lpszPath);
    if (subPath.empty())
    {
        StringCchCopyW(lpszNewPath, cbNewPathMax, TASKSCHD_VFS_PREFIX);
        return TRUE;
    }

    size_t lastSlash = subPath.find_last_of(L"/\\");
    if (lastSlash == std::wstring::npos)
    {
        StringCchCopyW(lpszNewPath, cbNewPathMax, TASKSCHD_VFS_PREFIX);
        return TRUE;
    }

    std::wstring parentSubPath = subPath.substr(0, lastSlash);
    if (parentSubPath.empty())
    {
        StringCchCopyW(lpszNewPath, cbNewPathMax, TASKSCHD_VFS_PREFIX);
        return TRUE;
    }

    std::wstring parentPath = std::wstring(TASKSCHD_VFS_PREFIX) + parentSubPath;
    StringCchCopyW(lpszNewPath, cbNewPathMax, parentPath.c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpszFiles || !lpMenuData) return FALSE;

    std::wstring taskPath = ParseTaskPath(lpszFiles);
    if (taskPath.empty())
    {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    if (IsFolderPath(taskPath))
    {
        VFSCONTEXTMENUITEMW* folderItems = (VFSCONTEXTMENUITEMW*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 3 * sizeof(VFSCONTEXTMENUITEMW));
        if (!folderItems)
        {
            lpMenuData->fAllowContextMenu = FALSE;
            return TRUE;
        }
        int itemCount = 0;

        folderItems[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        folderItems[itemCount].dwFlags = 0;
        folderItems[itemCount].lpszLabel = L"\u6253\u5f00";
        folderItems[itemCount].lpszCommand = L"$open_folder";
        itemCount++;

        folderItems[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        folderItems[itemCount].dwFlags = VFSCMF_SEPARATOR;
        folderItems[itemCount].lpszLabel = L"";
        folderItems[itemCount].lpszCommand = L"";
        itemCount++;

        folderItems[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        folderItems[itemCount].dwFlags = 0;
        folderItems[itemCount].lpszLabel = L"\u6253\u5f00\u4efb\u52a1\u8ba1\u5212\u7ba1\u7406\u5668";
        folderItems[itemCount].lpszCommand = L"$tsk_open";
        itemCount++;

        lpMenuData->fAllowContextMenu = TRUE;
        lpMenuData->fDefaultContextMenu = FALSE;
        lpMenuData->fCustomItemsBelow = TRUE;
        lpMenuData->lpCustomItems = folderItems;
        lpMenuData->iNumCustomItems = itemCount;
        lpMenuData->fFreeCustomItems = TRUE;

        return TRUE;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);

    TaskInfo info;
    memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
    memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
    bool found = false;

    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            found = GetTaskInfo(pService, taskPath, info);
        }
        pService->Release();
    }

    if (needUninitialize) CoUninitialize();

    if (!found)
    {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    VFSCONTEXTMENUITEMW* items = (VFSCONTEXTMENUITEMW*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 12 * sizeof(VFSCONTEXTMENUITEMW));
    if (!items)
    {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }
    int itemCount = 0;

    if (info.state != TASK_STATE_RUNNING)
    {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u8fd0\u884c";
        items[itemCount].lpszCommand = L"$run";
        itemCount++;
    }

    if (info.state == TASK_STATE_RUNNING)
    {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u7ec8\u6b62";
        items[itemCount].lpszCommand = L"$stop";
        itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u91cd\u542f";
        items[itemCount].lpszCommand = L"$restart";
        itemCount++;
    }

    if (info.enabled)
    {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u7981\u7528";
        items[itemCount].lpszCommand = L"$disable";
        itemCount++;
    }
    else
    {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u542f\u7528";
        items[itemCount].lpszCommand = L"$enable";
        itemCount++;
    }

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = L"";
    items[itemCount].lpszCommand = L"";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u5c5e\u6027";
    items[itemCount].lpszCommand = L"$tsk_properties";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u6253\u5f00\u4efb\u52a1\u8ba1\u5212\u7ba1\u7406\u5668";
    items[itemCount].lpszCommand = L"$tsk_open";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = L"";
    items[itemCount].lpszCommand = L"";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u590d\u5236\u4efb\u52a1\u8def\u5f84";
    items[itemCount].lpszCommand = L"$copypath";
    itemCount++;

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = TRUE;
    lpMenuData->lpCustomItems = items;
    lpMenuData->iNumCustomItems = itemCount;
    lpMenuData->fFreeCustomItems = TRUE;

    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPVFSCONTEXTVERBDATAW lpVerbData)
{
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    std::wstring taskPath = ParseTaskPath(lpVerbData->lpszPath);
    if (taskPath.empty()) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"open") == 0)
    {
        return VFSCVRES_DEFAULT;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"open_folder") == 0)
    {
        return VFSCVRES_DEFAULT;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"tsk_properties") == 0)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool needUninitialize = SUCCEEDED(hr);

        ITaskService* pService = NULL;
        hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
            __uuidof(ITaskService), (void**)&pService);
        if (SUCCEEDED(hr))
        {
            hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
            if (SUCCEEDED(hr))
            {
                TaskInfo info;
                memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
                memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
                if (GetTaskInfo(pService, taskPath, info))
                {
                    std::wstring msg = FormatTaskInfo(info);
                    MessageBoxW(lpVerbData->hwndParent, msg.c_str(),
                        L"\u4efb\u52a1\u5c5e\u6027", MB_OK | MB_ICONINFORMATION);
                }
            }
            pService->Release();
        }
        if (needUninitialize) CoUninitialize();
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"tsk_open") == 0)
    {
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = L"taskschd.msc";
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copypath") == 0)
    {
        if (OpenClipboard(NULL))
        {
            EmptyClipboard();
            size_t len = (wcslen(lpVerbData->lpszPath) + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hMem)
            {
                LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                if (pMem)
                {
                    wcscpy_s(pMem, len / sizeof(WCHAR), lpVerbData->lpszPath);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    DWORD action = 0;
    if (_wcsicmp(lpVerbData->lpszVerb, L"run") == 0)
    {
        if (g_config.warnSystem && IsSystemTask(taskPath))
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u6b64\u4efb\u52a1\u4e3a\u7cfb\u7edf\u4efb\u52a1\uff0c\u8fd0\u884c\u53ef\u80fd\u5f71\u54cd\u7cfb\u7edf\u7a33\u5b9a\u6027\u3002\n\u786e\u5b9a\u8981\u7ee7\u7eed\u5417\uff1f",
                L"\u7cfb\u7edf\u4efb\u52a1\u8b66\u544a",
                MB_YESNO | MB_ICONWARNING) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        if (g_config.confirmRun)
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u786e\u5b9a\u8981\u8fd0\u884c\u6b64\u4efb\u52a1\u5417\uff1f",
                L"\u786e\u8ba4\u8fd0\u884c",
                MB_YESNO | MB_ICONQUESTION) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        action = 1;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"stop") == 0)
    {
        if (g_config.warnSystem && IsSystemTask(taskPath))
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u6b64\u4efb\u52a1\u4e3a\u7cfb\u7edf\u4efb\u52a1\uff0c\u7ec8\u6b62\u53ef\u80fd\u5f71\u54cd\u7cfb\u7edf\u7a33\u5b9a\u6027\u3002\n\u786e\u5b9a\u8981\u7ee7\u7eed\u5417\uff1f",
                L"\u7cfb\u7edf\u4efb\u52a1\u8b66\u544a",
                MB_YESNO | MB_ICONWARNING) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        action = 2;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"restart") == 0)
        action = 6;
    else if (_wcsicmp(lpVerbData->lpszVerb, L"disable") == 0)
    {
        if (g_config.warnSystem && IsSystemTask(taskPath))
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u6b64\u4efb\u52a1\u4e3a\u7cfb\u7edf\u4efb\u52a1\uff0c\u7981\u7528\u53ef\u80fd\u5f71\u54cd\u7cfb\u7edf\u7a33\u5b9a\u6027\u3002\n\u786e\u5b9a\u8981\u7ee7\u7eed\u5417\uff1f",
                L"\u7cfb\u7edf\u4efb\u52a1\u8b66\u544a",
                MB_YESNO | MB_ICONWARNING) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        if (g_config.confirmEnable)
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u786e\u5b9a\u8981\u7981\u7528\u6b64\u4efb\u52a1\u5417\uff1f",
                L"\u786e\u8ba4\u7981\u7528",
                MB_YESNO | MB_ICONQUESTION) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        action = 4;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"enable") == 0)
    {
        if (g_config.confirmEnable)
        {
            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u786e\u5b9a\u8981\u542f\u7528\u6b64\u4efb\u52a1\u5417\uff1f",
                L"\u786e\u8ba4\u542f\u7528",
                MB_YESNO | MB_ICONQUESTION) != IDYES)
            {
                return VFSCVRES_HANDLED;
            }
        }
        action = 5;
    }
    else
        return VFSCVRES_DEFAULT;

    if (RunTaskAction(taskPath, action))
    {
        InvalidateCache();
        if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
        {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        }
        return VFSCVRES_CHANGE;
    }

    MessageBoxW(lpVerbData->hwndParent,
        L"\u64cd\u4f5c\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002",
        L"\u4efb\u52a1\u63a7\u5236\u9519\u8bef",
        MB_OK | MB_ICONERROR);

    return VFSCVRES_FAIL;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HWND hwndParent, LPWSTR lpszFiles)
{
    if (!lpszFiles) return NULL;

    std::wstring taskPath = ParseTaskPath(lpszFiles);
    if (taskPath.empty()) return NULL;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);
    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            TaskInfo info;
            memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
            memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
            if (GetTaskInfo(pService, taskPath, info))
            {
                std::wstring msg = FormatTaskInfo(info);
                MessageBoxW(hwndParent, msg.c_str(),
                    L"\u4efb\u52a1\u5c5e\u6027", MB_OK | MB_ICONINFORMATION);
            }
        }
        pService->Release();
    }
    if (needUninitialize) CoUninitialize();

    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileIconW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, LPINT lpiSysIconIndex, HICON* phLargeIcon, HICON* phSmallIcon,
    LPBOOL lpfDestroyIcons, LPWSTR lspzCacheName, int cchCacheNameMax, LPINT lpiCacheIndex)
{
    if (!lpszFile) return FALSE;

    std::wstring taskPath = ParseTaskPath(lpszFile);
    if (taskPath.empty()) return FALSE;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITaskService), (void**)&pService);

    TASK_STATE taskState = TASK_STATE_UNKNOWN;
    bool isEnabled = true;
    bool isSystem = IsSystemTask(taskPath);

    if (SUCCEEDED(hr))
    {
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (SUCCEEDED(hr))
        {
            ITaskFolder* pFolder = NULL;
            hr = pService->GetFolder(_bstr_t(L"\\"), &pFolder);
            if (SUCCEEDED(hr))
            {
                IRegisteredTask* pTask = NULL;
                hr = pFolder->GetTask(_bstr_t(taskPath.c_str()), &pTask);
                if (SUCCEEDED(hr))
                {
                    pTask->get_State(&taskState);
                    VARIANT_BOOL enabled = VARIANT_TRUE;
                    pTask->get_Enabled(&enabled);
                    isEnabled = (enabled == VARIANT_TRUE);
                    pTask->Release();
                }
                pFolder->Release();
            }
        }
        pService->Release();
    }

    if (needUninitialize) CoUninitialize();

    int iconIndex = 0;
    if (taskState == TASK_STATE_RUNNING)
        iconIndex = 1;
    else if (!isEnabled)
        iconIndex = 2;
    else if (isSystem)
        iconIndex = 3;

    if (lpiSysIconIndex)
        *lpiSysIconIndex = -1;

    if (lspzCacheName && cchCacheNameMax > 0)
    {
        WCHAR cacheName[64];
        StringCchPrintfW(cacheName, 64, L"TaskSchedulerVFS_%d", iconIndex);
        StringCchCopyW(lspzCacheName, cchCacheNameMax, cacheName);
    }

    if (lpiCacheIndex)
        *lpiCacheIndex = iconIndex;

    if (lpfDestroyIcons)
        *lpfDestroyIcons = FALSE;

    HICON hIcon = NULL;
    if (taskState == TASK_STATE_RUNNING)
    {
        ExtractIconExW(L"shell32.dll", 44, &hIcon, NULL, 1);
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    else if (!isEnabled)
    {
        ExtractIconExW(L"shell32.dll", 277, &hIcon, NULL, 1);
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    else if (isSystem)
    {
        hIcon = LoadIcon(NULL, IDI_SHIELD);
    }
    else
    {
        ExtractIconExW(L"shell32.dll", 2, &hIcon, NULL, 1);
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    if (phLargeIcon)
        *phLargeIcon = hIcon;
    if (phSmallIcon)
        *phSmallIcon = hIcon;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
    LPVOID lpData1, LPVOID lpData2, LPVOID lpData3)
{
    switch (propId)
    {
    case VFSPROP_FUNCAVAILABILITY:
    {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_PROPERTIES |
            VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT;
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
        *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
        return TRUE;

    case VFSPROP_ISEXTRACTABLE:
        *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
        return TRUE;
    }

    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszPath, HANDLE hFile,
    unsigned __int64* piFileSize)
{
    if (hFile)
    {
        TaskFileContext* ctx = (TaskFileContext*)hFile;
        if (piFileSize)
            *piFileSize = ctx->buffer.size();
        return TRUE;
    }
    else if (lpszPath)
    {
        std::wstring taskPath = ParseTaskPath(lpszPath);
        if (!taskPath.empty())
        {
            HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
            bool needUninitialize = SUCCEEDED(hr);

            ITaskService* pService = NULL;
            hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                __uuidof(ITaskService), (void**)&pService);
            if (SUCCEEDED(hr))
            {
                hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
                if (SUCCEEDED(hr))
                {
                    TaskInfo info;
                    memset(&info.lastRunTime, 0, sizeof(SYSTEMTIME));
                    memset(&info.nextRunTime, 0, sizeof(SYSTEMTIME));
                    if (GetTaskInfo(pService, taskPath, info))
                    {
                        std::wstring taskInfo = FormatTaskInfo(info);
                        if (piFileSize)
                            *piFileSize = taskInfo.length() * sizeof(WCHAR);
                        pService->Release();
                        if (needUninitialize) CoUninitialize();
                        return TRUE;
                    }
                }
                pService->Release();
            }
            if (needUninitialize) CoUninitialize();
        }
    }

    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData)
{
    return GetLastError();
}

static const int PAGE_GENERAL = 0;
static const int PAGE_DISPLAY = 1;
static const int PAGE_ACTIONS = 2;
static const int PAGE_ADVANCED = 3;

static const int pageGeneralCtrls[] = { IDC_AUTO_REFRESH, IDC_LBL_REFRESH_INTERVAL, IDC_REFRESH_INTERVAL, IDC_LBL_DEFAULT_ACTION, IDC_DEF_ACTION, IDC_CHK_CONFIRM_DELETE, IDC_SHOW_SYSTEM_TASKS, 0 };
static const int pageDisplayCtrls[] = { IDC_LBL_COLUMNS, IDC_COL_STATE, IDC_COL_TRIGGERS, IDC_COL_LASTRUN, IDC_COL_NEXTRUN, IDC_COL_AUTHOR, IDC_COL_EXECUTABLE, IDC_COL_USERID, IDC_COL_RESULT, IDC_COL_DESCRIPTION, 0 };
static const int pageActionsCtrls[] = { IDC_LBL_KILL_METHOD, IDC_KILL_METHOD, IDC_CHK_CONFIRM_RUN, IDC_CHK_WARN_SYSTEM, IDC_CHK_AUTO_REFRESH_ACTION, IDC_CHK_CONFIRM_ENABLE, 0 };
static const int pageAdvancedCtrls[] = { IDC_LBL_CACHE_TIMEOUT, IDC_CACHE_TIMEOUT, IDC_CHK_VERBOSE_LOG, IDC_LBL_CONFIG_PATH, IDC_CONFIG_PATH, IDC_BROWSE_CONFIG, IDC_LBL_LOG_PATH, IDC_LOG_PATH, IDC_BROWSE_LOG, IDC_OPEN_DATA_DIR, 0 };

static const int* g_pageControls[] = { pageGeneralCtrls, pageDisplayCtrls, pageActionsCtrls, pageAdvancedCtrls };

static void ShowNavPage(HWND hDlg, int page)
{
    g_currentPage = page;
    for (int i = 0; i < 4; i++)
    {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++) { HWND hwnd = GetDlgItem(hDlg, ctrls[j]); if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE); }
    }
}

static void SetChineseText(HWND hDlg)
{
    SetWindowTextW(hDlg, L"\u4efb\u52a1\u8ba1\u5212 VFS \u914d\u7f6e");
    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList) { SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24); SendMessageW(hList, LB_RESETCONTENT, 0, 0); for (int i = 0; i < 4; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L""); SendMessageW(hList, LB_SETCURSEL, 0, 0); }

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a"); SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88"); SetDlgItemTextW(hDlg, IDC_APPLY, L"\u5e94\u7528");
    SetDlgItemTextW(hDlg, IDC_OPEN_TASKSCHD, L"\u6253\u5f00\u4efb\u52a1\u8ba1\u5212"); SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\u6062\u590d\u9ed8\u8ba4");

    SetDlgItemTextW(hDlg, IDC_AUTO_REFRESH, L"\u542f\u7528\u81ea\u52a8\u5237\u65b0");
    SetDlgItemTextW(hDlg, IDC_LBL_REFRESH_INTERVAL, L"\u5237\u65b0\u95f4\u9694(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_DEFAULT_ACTION, L"\u9ed8\u8ba4\u64cd\u4f5c:");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_DELETE, L"\u5220\u9664\u524d\u786e\u8ba4");
    SetDlgItemTextW(hDlg, IDC_SHOW_SYSTEM_TASKS, L"\u663e\u793a\u7cfb\u7edf\u4efb\u52a1");

    SetDlgItemTextW(hDlg, IDC_LBL_COLUMNS, L"\u663e\u793a\u5217:");
    SetDlgItemTextW(hDlg, IDC_COL_STATE, L"\u72b6\u6001");
    SetDlgItemTextW(hDlg, IDC_COL_TRIGGERS, L"\u89e6\u53d1\u5668");
    SetDlgItemTextW(hDlg, IDC_COL_LASTRUN, L"\u4e0a\u6b21\u8fd0\u884c");
    SetDlgItemTextW(hDlg, IDC_COL_NEXTRUN, L"\u4e0b\u6b21\u8fd0\u884c");
    SetDlgItemTextW(hDlg, IDC_COL_AUTHOR, L"\u4f5c\u8005");
    SetDlgItemTextW(hDlg, IDC_COL_EXECUTABLE, L"\u53ef\u6267\u884c\u6587\u4ef6");
    SetDlgItemTextW(hDlg, IDC_COL_USERID, L"\u7528\u6237");
    SetDlgItemTextW(hDlg, IDC_COL_RESULT, L"\u4e0a\u6b21\u7ed3\u679c");
    SetDlgItemTextW(hDlg, IDC_COL_DESCRIPTION, L"\u63cf\u8ff0");

    SetDlgItemTextW(hDlg, IDC_LBL_KILL_METHOD, L"\u64cd\u4f5c\u65b9\u5f0f:");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_RUN, L"\u8fd0\u884c\u524d\u786e\u8ba4");
    SetDlgItemTextW(hDlg, IDC_CHK_WARN_SYSTEM, L"\u8b66\u544a\u7cfb\u7edf\u4efb\u52a1");
    SetDlgItemTextW(hDlg, IDC_CHK_AUTO_REFRESH_ACTION, L"\u64cd\u4f5c\u540e\u81ea\u52a8\u5237\u65b0");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_ENABLE, L"\u542f\u7528/\u7981\u7528\u524d\u786e\u8ba4");

    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TIMEOUT, L"\u7f13\u5b58\u8d85\u65f6(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_CHK_VERBOSE_LOG, L"\u8be6\u7ec6\u65e5\u5fd7");
    SetDlgItemTextW(hDlg, IDC_LBL_CONFIG_PATH, L"\u914d\u7f6e\u6587\u4ef6\u8def\u5f84:");
    SetDlgItemTextW(hDlg, IDC_BROWSE_CONFIG, L"...");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_PATH, L"\u65e5\u5fd7\u6587\u4ef6\u8def\u5f84:");
    SetDlgItemTextW(hDlg, IDC_BROWSE_LOG, L"...");
    SetDlgItemTextW(hDlg, IDC_OPEN_DATA_DIR, L"\u6253\u5f00\u6570\u636e\u76ee\u5f55");
}

static void InitDialogControls(HWND hDlg)
{
    SetChineseText(hDlg);
    CheckDlgButton(hDlg, IDC_AUTO_REFRESH, g_config.autoRefresh ? BST_CHECKED : BST_UNCHECKED);
    WCHAR buf[32]; swprintf_s(buf, L"%d", g_config.refreshInterval); SetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL, buf);
    HWND hCombo = GetDlgItem(hDlg, IDC_DEF_ACTION);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u67e5\u770b\u5c5e\u6027"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u8fd0\u884c\u4efb\u52a1"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u6253\u5f00\u7ba1\u7406\u5668"); SendMessageW(hCombo, CB_SETCURSEL, g_config.defaultAction, 0);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_DELETE, g_config.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_SYSTEM_TASKS, g_config.showSystemTasks ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(hDlg, IDC_COL_STATE, g_config.colState ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_TRIGGERS, g_config.colTriggers ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_LASTRUN, g_config.colLastRun ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_NEXTRUN, g_config.colNextRun ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_AUTHOR, g_config.colAuthor ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_EXECUTABLE, g_config.colExecutable ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_USERID, g_config.colUserId ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_RESULT, g_config.colResult ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DESCRIPTION, g_config.colDescription ? BST_CHECKED : BST_UNCHECKED);
    hCombo = GetDlgItem(hDlg, IDC_KILL_METHOD);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u6e29\u548c\u505c\u6b62"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u5f3a\u5236\u505c\u6b62(\u91cd\u8bd5)"); SendMessageW(hCombo, CB_SETCURSEL, g_config.killMethod, 0);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_RUN, g_config.confirmRun ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_WARN_SYSTEM, g_config.warnSystem ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_AUTO_REFRESH_ACTION, g_config.autoRefreshAction ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_ENABLE, g_config.confirmEnable ? BST_CHECKED : BST_UNCHECKED);

    swprintf_s(buf, L"%d", g_config.cacheTimeout); SetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf);
    CheckDlgButton(hDlg, IDC_CHK_VERBOSE_LOG, g_config.verboseLog ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(hDlg, IDC_CONFIG_PATH, g_config.configPath.empty() ? GetConfigFilePath().c_str() : g_config.configPath.c_str());
    SetDlgItemTextW(hDlg, IDC_LOG_PATH, g_config.logPath.empty() ? GetLogFilePath().c_str() : g_config.logPath.c_str());

    ShowNavPage(hDlg, PAGE_GENERAL);
}

static bool SaveDialogControls(HWND hDlg)
{
    g_config.autoRefresh = (IsDlgButtonChecked(hDlg, IDC_AUTO_REFRESH) == BST_CHECKED);
    WCHAR buf[32]; GetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL, buf, 32); g_config.refreshInterval = _wtoi(buf); if (g_config.refreshInterval < 1) g_config.refreshInterval = 1;
    HWND hCombo = GetDlgItem(hDlg, IDC_DEF_ACTION); g_config.defaultAction = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    g_config.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_DELETE) == BST_CHECKED);
    g_config.showSystemTasks = (IsDlgButtonChecked(hDlg, IDC_SHOW_SYSTEM_TASKS) == BST_CHECKED);

    g_config.colState = (IsDlgButtonChecked(hDlg, IDC_COL_STATE) == BST_CHECKED);
    g_config.colTriggers = (IsDlgButtonChecked(hDlg, IDC_COL_TRIGGERS) == BST_CHECKED);
    g_config.colLastRun = (IsDlgButtonChecked(hDlg, IDC_COL_LASTRUN) == BST_CHECKED);
    g_config.colNextRun = (IsDlgButtonChecked(hDlg, IDC_COL_NEXTRUN) == BST_CHECKED);
    g_config.colAuthor = (IsDlgButtonChecked(hDlg, IDC_COL_AUTHOR) == BST_CHECKED);
    g_config.colExecutable = (IsDlgButtonChecked(hDlg, IDC_COL_EXECUTABLE) == BST_CHECKED);
    g_config.colUserId = (IsDlgButtonChecked(hDlg, IDC_COL_USERID) == BST_CHECKED);
    g_config.colResult = (IsDlgButtonChecked(hDlg, IDC_COL_RESULT) == BST_CHECKED);
    g_config.colDescription = (IsDlgButtonChecked(hDlg, IDC_COL_DESCRIPTION) == BST_CHECKED);
    hCombo = GetDlgItem(hDlg, IDC_KILL_METHOD); g_config.killMethod = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    g_config.confirmRun = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_RUN) == BST_CHECKED);
    g_config.warnSystem = (IsDlgButtonChecked(hDlg, IDC_CHK_WARN_SYSTEM) == BST_CHECKED);
    g_config.autoRefreshAction = (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_REFRESH_ACTION) == BST_CHECKED);
    g_config.confirmEnable = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_ENABLE) == BST_CHECKED);

    GetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf, 32); g_config.cacheTimeout = _wtoi(buf); if (g_config.cacheTimeout < 0) g_config.cacheTimeout = 0;
    g_config.verboseLog = (IsDlgButtonChecked(hDlg, IDC_CHK_VERBOSE_LOG) == BST_CHECKED);
    WCHAR pathBuf[1024]; GetDlgItemTextW(hDlg, IDC_LOG_PATH, pathBuf, 1024);
    if (wcslen(pathBuf) > 0 && wcsrchr(pathBuf, L'\\') == NULL && wcsrchr(pathBuf, L'/') == NULL)
    {
        MessageBoxW(hDlg, L"\u65e5\u5fd7\u8def\u5f84\u5fc5\u987b\u662f\u5b8c\u6574\u7684\u6587\u4ef6\u8def\u5f84", L"\u8def\u5f84\u9519\u8bef", MB_OK | MB_ICONWARNING);
        return false;
    }
    g_config.logPath = pathBuf;
    WCHAR configBuf[1024]; GetDlgItemTextW(hDlg, IDC_CONFIG_PATH, configBuf, 1024);
    if (wcslen(configBuf) > 0 && wcsrchr(configBuf, L'\\') == NULL && wcsrchr(configBuf, L'/') == NULL)
    {
        MessageBoxW(hDlg, L"\u914d\u7f6e\u8def\u5f84\u5fc5\u987b\u662f\u5b8c\u6574\u7684\u6587\u4ef6\u8def\u5f84", L"\u8def\u5f84\u9519\u8bef", MB_OK | MB_ICONWARNING);
        return false;
    }
    g_config.configPath = configBuf;

    SaveConfig();
    return true;
}

static void ResetToDefaults(HWND hDlg) { g_config = TaskSchdConfig(); InitDialogControls(hDlg); MessageBoxW(hDlg, L"\u5df2\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION); }
static void OpenTaskScheduler(HWND hDlg) { SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = L"taskschd.msc"; sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei); }
static void BrowseLogPath(HWND hDlg) { WCHAR curPath[MAX_PATH] = {}; GetDlgItemTextW(hDlg, IDC_LOG_PATH, curPath, MAX_PATH); OPENFILENAMEW ofn = {}; WCHAR szFile[MAX_PATH] = {}; wcsncpy_s(szFile, curPath, MAX_PATH - 1); ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hDlg; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH; ofn.lpstrFilter = L"\u65e5\u5fd7\u6587\u4ef6 (*.log)\0*.log\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0"; ofn.lpstrDefExt = L"log"; ofn.lpstrTitle = L"\u9009\u62e9\u65e5\u5fd7\u6587\u4ef6\u8def\u5f84"; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST; if (GetSaveFileNameW(&ofn)) SetDlgItemTextW(hDlg, IDC_LOG_PATH, szFile); }
static void BrowseConfigPath(HWND hDlg) { WCHAR curPath[MAX_PATH] = {}; GetDlgItemTextW(hDlg, IDC_CONFIG_PATH, curPath, MAX_PATH); OPENFILENAMEW ofn = {}; WCHAR szFile[MAX_PATH] = {}; wcsncpy_s(szFile, curPath, MAX_PATH - 1); ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hDlg; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH; ofn.lpstrFilter = L"\u914d\u7f6e\u6587\u4ef6 (*.ini)\0*.ini\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0"; ofn.lpstrDefExt = L"ini"; ofn.lpstrTitle = L"\u9009\u62e9\u914d\u7f6e\u6587\u4ef6\u8def\u5f84"; ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST; if (GetSaveFileNameW(&ofn)) SetDlgItemTextW(hDlg, IDC_CONFIG_PATH, szFile); }

INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG: LoadConfig(); InitDialogControls(hDlg); return (INT_PTR)TRUE;
    case WM_MEASUREITEM: { LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam; if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST) lpmis->itemHeight = 24; } return (INT_PTR)TRUE;
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST)
        {
            static const WCHAR* navLabels[] = { L"\u5e38\u89c4\u8bbe\u7f6e", L"\u663e\u793a\u8bbe\u7f6e", L"\u4efb\u52a1\u64cd\u4f5c", L"\u9ad8\u7ea7\u8bbe\u7f6e" };
            int idx = (int)lpdis->itemID;
            if (idx >= 0 && idx < 4)
            {
                BOOL selected = (lpdis->itemState & ODS_SELECTED);
                HBRUSH hBrush = selected ? CreateSolidBrush(RGB(0, 120, 215)) : GetSysColorBrush(COLOR_WINDOW);
                FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
                if (selected) DeleteObject(hBrush);
                SetTextColor(lpdis->hDC, selected ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                rcText.right -= 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }
    return (INT_PTR)TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK: if (SaveDialogControls(hDlg)) EndDialog(hDlg, IDOK); return (INT_PTR)TRUE;
        case IDCANCEL: EndDialog(hDlg, IDCANCEL); return (INT_PTR)TRUE;
        case IDC_APPLY: SaveDialogControls(hDlg); return (INT_PTR)TRUE;
        case IDC_NAV_LIST: if (HIWORD(wParam) == LBN_SELCHANGE) { HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST); int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0); if (sel >= 0 && sel < 4) ShowNavPage(hDlg, sel); } return (INT_PTR)TRUE;
        case IDC_RESET_DEFAULTS: ResetToDefaults(hDlg); return (INT_PTR)TRUE;
        case IDC_OPEN_TASKSCHD: OpenTaskScheduler(hDlg); return (INT_PTR)TRUE;
        case IDC_BROWSE_LOG: BrowseLogPath(hDlg); return (INT_PTR)TRUE;
        case IDC_BROWSE_CONFIG: BrowseConfigPath(hDlg); return (INT_PTR)TRUE;
        case IDC_OPEN_DATA_DIR:
        {
            std::wstring dataDir = GetPluginDataDir();
            ShellExecuteW(NULL, L"open", dataDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return (INT_PTR)TRUE;
        }
        }
        break;
    case WM_CLOSE: EndDialog(hDlg, IDCANCEL); return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData)
{
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_TASKSCHD_CONFIG), hWndParent, ConfigDialogProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent)
{
    MessageBoxW(hWndParent,
        L"\u4efb\u52a1\u8ba1\u5212 VFS \u63d2\u4ef6 v1.0.0\n\n"
        L"(c) 2026\n\n"
        L"Windows \u4efb\u52a1\u8ba1\u5212\u865a\u62df\u6587\u4ef6\u7cfb\u7edf\n\n"
        L"\u529f\u80fd\u7279\u6027\uff1a\n"
        L"- \u6d4f\u89c8\u6240\u6709\u8ba1\u5212\u4efb\u52a1\n"
        L"- \u67e5\u770b\u4efb\u52a1\u8be6\u7ec6\u4fe1\u606f\n"
        L"- \u8fd0\u884c/\u505c\u6b62/\u542f\u7528/\u7981\u7528\u4efb\u52a1\n"
        L"- \u81ea\u5b9a\u4e49\u5217\u663e\u793a\n"
        L"- \u53f3\u952e\u83dc\u5355\u64cd\u4f5c\n"
        L"- \u53ef\u914d\u7f6e\u663e\u793a\u548c\u64cd\u4f5c\u9009\u9879",
        L"\u5173\u4e8e \u4efb\u52a1\u8ba1\u5212 VFS",
        MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
    if (pUSBSafeData)
    {
        pUSBSafeData->pszOtherExports[0] = L'\0';
    }
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
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
