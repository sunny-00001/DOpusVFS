#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <strsafe.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include "resource.h"

#ifndef ERROR_INVALID_PATH
#define ERROR_INVALID_PATH 123L
#endif

#ifndef ERROR_OPEN_FAILED
#define ERROR_OPEN_FAILED 110L
#endif

typedef const BYTE* LPCBYTE;
typedef BYTE* LPBYTE;

#define VFSPLUGINVERSION 2
#include "vfs_plugins.h"
#include "plugin_support.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comctl32.lib")

static const GUID GUIDPlugin_Env = 
{ 0xB2C3D4E5, 0xF6A7, 0x8901, { 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x90 } };

#define ENV_VFS_PREFIX L"env://"
#define ENV_VFS_PREFIX_LEN 6

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

// 配置结构体
struct EnvVFSConfig {
    bool showSystem;
    bool showUser;
    bool showVolatile;
    int refreshInterval;
    
    bool colName;
    bool colValue;
    bool colType;
    bool colSource;
    int fontSize;
    
    std::wstring defEditor;
    bool confirmDelete;
    bool autoBackup;
    std::wstring backupPath;
    
    int cacheTimeout;
    bool verboseLog;
    bool enableTray;
    std::wstring logPath;
};

static EnvVFSConfig g_config;

// 导航页面枚举
enum NavPage {
    NAV_GENERAL = 0,
    NAV_DISPLAY,
    NAV_EDIT,
    NAV_ADVANCED,
    NAV_COUNT
};

static int g_currentNavPage = NAV_GENERAL;

// 自绘菜单数据
struct CustomMenuItem {
    LPCWSTR text;
    int id;
    bool isSeparator;
};

static CustomMenuItem g_menuItems[] = {
    {L"导出配置", IDM_EXPORT_CONFIG, false},
    {L"导入配置", IDM_IMPORT_CONFIG, false},
    {L"", 0, true},
    {L"恢复默认", IDM_RESTORE_DEFAULTS, false}
};

static HWND g_hMenuWnd = NULL;
static int g_hoveredItem = -1;

// 函数前向声明
static void InitDialogControls(HWND hDlg);
static void ResetToDefaults(HWND hDlg);
static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void ShowPopupMenu(HWND hDlg);

static const int pageGeneralCtrls[] = { 
    IDC_SHOW_SYS, IDC_SHOW_USER, IDC_SHOW_VOLATILE, 
    IDC_LBL_REFRESH, IDC_REFRESH_INTERVAL, 0 
};

static const int pageDisplayCtrls[] = { 
    IDC_LBL_COLUMNS, IDC_COL_NAME, IDC_COL_VALUE, IDC_COL_TYPE, IDC_COL_SOURCE,
    IDC_LBL_FONT_SIZE, IDC_FONT_SIZE, 0 
};

static const int pageEditCtrls[] = { 
    IDC_LBL_DEF_EDITOR, IDC_DEF_EDITOR, IDC_BROWSE_EDITOR,
    IDC_CHK_CONFIRM_DEL, IDC_CHK_AUTO_BACKUP,
    IDC_LBL_BACKUP_PATH, IDC_BACKUP_PATH, IDC_BROWSE_BACKUP, 0 
};

static const int pageAdvancedCtrls[] = { 
    IDC_LBL_CACHE_TMOUT, IDC_CACHE_TMOUT,
    IDC_CHK_VERBOSE_LOG, IDC_CHK_ENABLE_TRAY,
    IDC_LBL_LOG_PATH, IDC_LOG_PATH, IDC_BROWSE_LOG, 0 
};

static const int* g_pageControls[] = { 
    pageGeneralCtrls, pageDisplayCtrls, pageEditCtrls, pageAdvancedCtrls 
};

static bool IsEnvVfsPath(LPCWSTR pszPath) {
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, ENV_VFS_PREFIX, ENV_VFS_PREFIX_LEN) == 0;
}

static bool IsEnvRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    
    if (_wcsicmp(pszPath, ENV_VFS_PREFIX) == 0)
        return true;
    
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/')
        s.pop_back();
    
    if (_wcsicmp(s.c_str(), L"env:") == 0)
        return true;
    
    return false;
}

static std::wstring ParseEnvName(LPCWSTR pszPath) {
    if (!IsEnvVfsPath(pszPath)) return L"";
    
    std::wstring path = pszPath + ENV_VFS_PREFIX_LEN;
    
    while (!path.empty() && path.front() == L'/')
        path.erase(0, 1);
    
    while (!path.empty() && path.back() == L'/')
        path.pop_back();
    
    // 移除 .env 后缀
    const std::wstring suffix = L".env";
    if (path.length() > suffix.length()) {
        if (_wcsicmp(path.substr(path.length() - suffix.length()).c_str(), suffix.c_str()) == 0) {
            path = path.substr(0, path.length() - suffix.length());
        }
    }
    
    return path;
}

enum EnvType {
    ENV_TYPE_STRING = 0,
    ENV_TYPE_EXPAND_STRING = 1,
    ENV_TYPE_MULTI_STRING = 2,
    ENV_TYPE_UNKNOWN = 3
};

struct EnvInfo {
    std::wstring name;
    std::wstring value;
    EnvType type;
    std::wstring source;
    bool isSystem;
    bool isVolatile;
};

struct EnvFileContext {
    std::vector<BYTE> buffer;
    size_t readPos;
    bool isWrite;
    std::wstring varName;
    bool isSystem;
    bool isSubEntry;
    int entryIndex;
    bool isNewEntry;
};

static EnvType GetEnvType(DWORD type) {
    switch (type) {
        case REG_SZ: return ENV_TYPE_STRING;
        case REG_EXPAND_SZ: return ENV_TYPE_EXPAND_STRING;
        case REG_MULTI_SZ: return ENV_TYPE_MULTI_STRING;
        default: return ENV_TYPE_UNKNOWN;
    }
}

static std::wstring GetEnvTypeName(EnvType type) {
    switch (type) {
        case ENV_TYPE_STRING: return L"字符串";
        case ENV_TYPE_EXPAND_STRING: return L"可展开字符串";
        case ENV_TYPE_MULTI_STRING: return L"多行字符串";
        default: return L"未知";
    }
}

static std::wstring GetSourceName(bool isSystem, bool isVolatile) {
    if (isVolatile) return L"临时";
    if (isSystem) return L"系统";
    return L"用户";
}

static bool FindEnvVariable(const std::wstring& name, EnvInfo& info);

static bool IsMultiValueVar(const std::wstring& value) {
    return value.find(L';') != std::wstring::npos;
}

static std::vector<std::wstring> SplitEnvValue(const std::wstring& value) {
    std::vector<std::wstring> entries;
    size_t start = 0;
    size_t end = value.find(L';');
    while (end != std::wstring::npos) {
        std::wstring entry = value.substr(start, end - start);
        if (!entry.empty()) {
            entries.push_back(entry);
        }
        start = end + 1;
        end = value.find(L';', start);
    }
    if (start < value.length()) {
        std::wstring entry = value.substr(start);
        if (!entry.empty()) {
            entries.push_back(entry);
        }
    }
    return entries;
}

static std::wstring JoinEnvValue(const std::vector<std::wstring>& entries) {
    std::wstring result;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) result += L';';
        result += entries[i];
    }
    return result;
}

static bool IsSubDirPath(LPCWSTR pszPath) {
    if (!IsEnvVfsPath(pszPath)) return false;
    
    std::wstring path = pszPath + ENV_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    
    if (path.empty()) return false;
    
    size_t slashPos = path.find(L'/');
    if (slashPos == std::wstring::npos) return false;
    
    std::wstring varName = path.substr(0, slashPos);
    
    EnvInfo info;
    return FindEnvVariable(varName, info) && IsMultiValueVar(info.value);
}

static bool ParseVarDirPath(LPCWSTR pszPath, std::wstring& varName) {
    if (!IsEnvVfsPath(pszPath)) return false;
    
    std::wstring path = pszPath + ENV_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    
    if (path.empty()) return false;
    
    size_t slashPos = path.find(L'/');
    if (slashPos == std::wstring::npos) {
        varName = path;
    } else {
        varName = path.substr(0, slashPos);
    }
    
    return !varName.empty();
}

static bool ParseSubEntryPath(LPCWSTR pszPath, std::wstring& varName, int& entryIndex) {
    if (!IsEnvVfsPath(pszPath)) return false;
    
    std::wstring path = pszPath + ENV_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    
    size_t slashPos = path.find(L'/');
    if (slashPos == std::wstring::npos) return false;
    
    varName = path.substr(0, slashPos);
    std::wstring entryPart = path.substr(slashPos + 1);
    
    const std::wstring suffix = L".path";
    if (entryPart.length() > suffix.length()) {
        if (_wcsicmp(entryPart.substr(entryPart.length() - suffix.length()).c_str(), suffix.c_str()) == 0) {
            entryPart = entryPart.substr(0, entryPart.length() - suffix.length());
        }
    }
    
    entryIndex = _wtoi(entryPart.c_str());
    return entryIndex > 0;
}

static bool PathEntryExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return true;
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool GetSystemEnvVariables(std::vector<EnvInfo>& vars) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    WCHAR name[32767];
    BYTE data[32767];
    DWORD index = 0;
    DWORD nameSize, dataSize, type;
    
    while (true) {
        nameSize = _countof(name);
        dataSize = sizeof(data);
        
        if (RegEnumValueW(hKey, index, name, &nameSize, NULL, &type, data, &dataSize) != ERROR_SUCCESS) {
            break;
        }
        
        EnvInfo info;
        info.name = name;
        info.value = std::wstring((WCHAR*)data, dataSize / sizeof(WCHAR) - 1);
        info.type = GetEnvType(type);
        info.source = L"系统";
        info.isSystem = true;
        info.isVolatile = false;
        
        vars.push_back(info);
        index++;
    }
    
    RegCloseKey(hKey);
    return true;
}

static bool GetUserEnvVariables(std::vector<EnvInfo>& vars) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    WCHAR name[32767];
    BYTE data[32767];
    DWORD index = 0;
    DWORD nameSize, dataSize, type;
    
    while (true) {
        nameSize = _countof(name);
        dataSize = sizeof(data);
        
        if (RegEnumValueW(hKey, index, name, &nameSize, NULL, &type, data, &dataSize) != ERROR_SUCCESS) {
            break;
        }
        
        EnvInfo info;
        info.name = name;
        info.value = std::wstring((WCHAR*)data, dataSize / sizeof(WCHAR) - 1);
        info.type = GetEnvType(type);
        info.source = L"用户";
        info.isSystem = false;
        info.isVolatile = false;
        
        vars.push_back(info);
        index++;
    }
    
    RegCloseKey(hKey);
    return true;
}

static bool GetVolatileEnvVariables(std::vector<EnvInfo>& vars) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Volatile Environment", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    WCHAR name[32767];
    BYTE data[32767];
    DWORD index = 0;
    DWORD nameSize, dataSize, type;
    
    while (true) {
        nameSize = _countof(name);
        dataSize = sizeof(data);
        
        if (RegEnumValueW(hKey, index, name, &nameSize, NULL, &type, data, &dataSize) != ERROR_SUCCESS) {
            break;
        }
        
        EnvInfo info;
        info.name = name;
        info.value = std::wstring((WCHAR*)data, dataSize / sizeof(WCHAR) - 1);
        info.type = GetEnvType(type);
        info.source = L"临时";
        info.isSystem = false;
        info.isVolatile = true;
        
        vars.push_back(info);
        index++;
    }
    
    RegCloseKey(hKey);
    return true;
}

static bool GetAllEnvVariables(std::vector<EnvInfo>& vars) {
    vars.clear();
    GetSystemEnvVariables(vars);
    GetUserEnvVariables(vars);
    GetVolatileEnvVariables(vars);
    return true;
}

static bool FindEnvVariable(const std::wstring& name, EnvInfo& info) {
    std::vector<EnvInfo> vars;
    GetAllEnvVariables(vars);
    
    for (const auto& var : vars) {
        if (_wcsicmp(var.name.c_str(), name.c_str()) == 0) {
            info = var;
            return true;
        }
    }
    return false;
}

static std::wstring FormatEnvInfo(const EnvInfo& info) {
    std::wstring result;
    
    result += L"变量名: " + info.name + L"\r\n";
    result += L"类型: " + GetEnvTypeName(info.type) + L"\r\n";
    result += L"来源: " + info.source + L"\r\n";
    result += L"\r\n值:\r\n" + info.value + L"\r\n";
    
    return result;
}

static bool SetEnvVariable(const std::wstring& name, const std::wstring& value, bool isSystem, EnvType type) {
    HKEY hKey;
    LONG result;
    
    if (isSystem) {
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_SET_VALUE, &hKey);
    } else {
        result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey);
    }
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD regType;
    switch (type) {
        case ENV_TYPE_EXPAND_STRING: regType = REG_EXPAND_SZ; break;
        case ENV_TYPE_MULTI_STRING: regType = REG_MULTI_SZ; break;
        default: regType = REG_SZ; break;
    }
    
    result = RegSetValueExW(hKey, name.c_str(), 0, regType, 
                           (const BYTE*)value.c_str(), 
                           (DWORD)((value.length() + 1) * sizeof(WCHAR)));
    
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
        return true;
    }
    
    return false;
}

static bool DeleteEnvVariable(const std::wstring& name, bool isSystem) {
    HKEY hKey;
    LONG result;
    
    if (isSystem) {
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_SET_VALUE, &hKey);
    } else {
        result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey);
    }
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegDeleteValueW(hKey, name.c_str());
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
        return true;
    }
    
    return false;
}

static void OpenSystemPropertiesEnv() {
    ShellExecuteW(NULL, L"open", L"rundll32.exe", 
                  L"sysdm.cpl,EditEnvironmentVariables", NULL, SW_SHOWNORMAL);
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str) {
    if (!hHeap) return NULL;
    
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) {
        StringCchCopyW(buf, len, str.c_str());
    }
    return buf;
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) {
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) {
        Sleep(50);
    }
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    if (!lpVFSInfo) return FALSE;
    
    lpVFSInfo->idPlugin = GUIDPlugin_Env;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;
    
    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, ENV_VFS_PREFIX);
    
    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"环境变量");
    
    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, 
                      L"Windows 环境变量 - 浏览和管理系统环境变量");
    
    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");
    
    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");
    
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;
    
    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"shell32.dll", 14, &hIconLarge, &hIconSmall, 1);
    
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;
    
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    if (!lpszPrefix) return FALSE;
    if (cchPrefixMax < 8) return FALSE;
    
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"env://");
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData) {
    static VFSCUSTOMCOLUMNW columns[4];
    
    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"值";
    columns[0].lpszKey = L"value";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;
    
    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"类型";
    columns[1].lpszKey = L"type";
    columns[1].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[1].iID = 2;
    
    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"来源";
    columns[2].lpszKey = L"source";
    columns[2].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[2].iID = 3;
    
    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = NULL;
    columns[3].lpszLabel = L"系统";
    columns[3].lpszKey = L"system";
    columns[3].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[3].iID = 4;
    
    return columns;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetAllCustomColumnsW() {
    return VFS_GetCustomColumnsW(NULL);
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;
    
    std::vector<EnvInfo> vars;
    if (!GetAllEnvVariables(vars)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    
    int numItems = (int)vars.size();
    
    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
    
    if (!lpFDH) return FALSE;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;
    
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0)
            break;
        
        bool isMulti = IsMultiValueVar(vars[i].value);
        
        if (isMulti) {
            StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, vars[i].name.c_str());
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        } else {
            std::wstring fileName = vars[i].name + L".env";
            StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, fileName.c_str());
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        }
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastAccessTime = lpFileData[i].wfdData.ftCreationTime;
        lpFileData[i].wfdData.ftLastWriteTime = lpFileData[i].wfdData.ftCreationTime;
        
        lpFileData[i].iNumColumns = 4;
        lpFileData[i].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, 
                                                                       4 * sizeof(VFSFILEDATACOLUMNW));
        
        if (lpFileData[i].lpvfsColumnData) {
            if (isMulti) {
                std::vector<std::wstring> entries = SplitEnvValue(vars[i].value);
                std::wstring countStr = std::to_wstring(entries.size()) + L" 条记录";
                lpFileData[i].lpvfsColumnData[0].iColumnId = 1;
                lpFileData[i].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, countStr);
            } else {
                std::wstring shortValue = vars[i].value;
                if (shortValue.length() > 80)
                    shortValue = shortValue.substr(0, 80) + L"...";
                lpFileData[i].lpvfsColumnData[0].iColumnId = 1;
                lpFileData[i].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, shortValue);
            }
            
            lpFileData[i].lpvfsColumnData[1].iColumnId = 2;
            lpFileData[i].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap, GetEnvTypeName(vars[i].type));
            
            lpFileData[i].lpvfsColumnData[2].iColumnId = 3;
            lpFileData[i].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap, vars[i].source);
            
            lpFileData[i].lpvfsColumnData[3].iColumnId = 4;
            lpFileData[i].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap, vars[i].isSystem ? L"是" : L"否");
        }
    }
    
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int InternalReadSubDirectory(LPVFSREADDIRDATAW lpRDD, const std::wstring& varName) {
    EnvInfo info;
    if (!FindEnvVariable(varName, info)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    
    if (!IsMultiValueVar(info.value)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    
    std::vector<std::wstring> entries = SplitEnvValue(info.value);
    int numItems = (int)entries.size();
    
    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
    
    if (!lpFDH) return FALSE;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;
    
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0)
            break;
        
        std::wstring fileName = std::to_wstring(i + 1) + L".path";
        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, fileName.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData[i].wfdData.ftCreationTime = ft;
        lpFileData[i].wfdData.ftLastAccessTime = ft;
        lpFileData[i].wfdData.ftLastWriteTime = ft;
        lpFileData[i].wfdData.nFileSizeLow = (DWORD)(entries[i].length() * sizeof(WCHAR));
        
        lpFileData[i].iNumColumns = 4;
        lpFileData[i].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
                                                                       4 * sizeof(VFSFILEDATACOLUMNW));
        
        if (lpFileData[i].lpvfsColumnData) {
            lpFileData[i].lpvfsColumnData[0].iColumnId = 1;
            lpFileData[i].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, entries[i]);
            
            lpFileData[i].lpvfsColumnData[1].iColumnId = 2;
            lpFileData[i].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap, L"路径条目");
            
            lpFileData[i].lpvfsColumnData[2].iColumnId = 3;
            lpFileData[i].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap, info.source);
            
            lpFileData[i].lpvfsColumnData[3].iColumnId = 4;
            lpFileData[i].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap, info.isSystem ? L"是" : L"否");
        }
    }
    
    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, 
                                                      LPVFSREADDIRDATAW lpRDD) {
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
    case VFSREAD_PRINTDIR: {
        if (IsSubDirPath(lpRDD->lpszPath)) {
            std::wstring varName;
            if (ParseVarDirPath(lpRDD->lpszPath, varName)) {
                return InternalReadSubDirectory(lpRDD, varName);
            }
        }
        return InternalReadDirectory(lpRDD);
    }
    
    default:
        return TRUE;
    }
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                                           LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;
    
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 
                                                              sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    
    std::wstring envName = ParseEnvName(lpszPath);
    
    if (envName.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"环境变量");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszPath, subVarName, entryIdx)) {
            EnvInfo info;
            if (!FindEnvVariable(subVarName, info)) {
                HeapFree(hHeap, 0, lpFDH);
                return NULL;
            }
            
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            if (entryIdx < 1 || entryIdx >(int)entries.size()) {
                HeapFree(hHeap, 0, lpFDH);
                return NULL;
            }
            
            const std::wstring& entryValue = entries[entryIdx - 1];
            
            std::wstring fileName = std::to_wstring(entryIdx) + L".path";
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, fileName.c_str());
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            lpFileData->wfdData.nFileSizeLow = (DWORD)(entryValue.length() * sizeof(WCHAR));
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
            lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
            lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;
            
            lpFileData->iNumColumns = 4;
            lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                           4 * sizeof(VFSFILEDATACOLUMNW));
            if (lpFileData->lpvfsColumnData) {
                lpFileData->lpvfsColumnData[0].iColumnId = 1;
                lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, entryValue);
                
                lpFileData->lpvfsColumnData[1].iColumnId = 2;
                lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, L"路径条目");
                
                lpFileData->lpvfsColumnData[2].iColumnId = 3;
                lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, info.source);
                
                lpFileData->lpvfsColumnData[3].iColumnId = 4;
                lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, info.isSystem ? L"是" : L"否");
            }
            
            return lpFDH;
        }
    }
    
    {
        std::wstring dirVarName;
        if (IsSubDirPath(lpszPath) && ParseVarDirPath(lpszPath, dirVarName)) {
            EnvInfo info;
            if (!FindEnvVariable(dirVarName, info)) {
                HeapFree(hHeap, 0, lpFDH);
                return NULL;
            }
            
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, dirVarName.c_str());
            lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
            lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
            lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;
            
            lpFileData->iNumColumns = 4;
            lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                           4 * sizeof(VFSFILEDATACOLUMNW));
            if (lpFileData->lpvfsColumnData) {
                std::vector<std::wstring> entries = SplitEnvValue(info.value);
                std::wstring countStr = std::to_wstring(entries.size()) + L" 条记录";
                lpFileData->lpvfsColumnData[0].iColumnId = 1;
                lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, countStr);
                
                lpFileData->lpvfsColumnData[1].iColumnId = 2;
                lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, GetEnvTypeName(info.type));
                
                lpFileData->lpvfsColumnData[2].iColumnId = 3;
                lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, info.source);
                
                lpFileData->lpvfsColumnData[3].iColumnId = 4;
                lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, info.isSystem ? L"是" : L"否");
            }
            
            return lpFDH;
        }
    }
    
    EnvInfo info;
    bool found = FindEnvVariable(envName, info);
    
    if (!found) {
        HeapFree(hHeap, 0, lpFDH);
        return NULL;
    }
    
    if (IsMultiValueVar(info.value)) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, info.name.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
        lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;
        
        lpFileData->iNumColumns = 4;
        lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                       4 * sizeof(VFSFILEDATACOLUMNW));
        if (lpFileData->lpvfsColumnData) {
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            std::wstring countStr = std::to_wstring(entries.size()) + L" 条记录";
            lpFileData->lpvfsColumnData[0].iColumnId = 1;
            lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, countStr);
            
            lpFileData->lpvfsColumnData[1].iColumnId = 2;
            lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, GetEnvTypeName(info.type));
            
            lpFileData->lpvfsColumnData[2].iColumnId = 3;
            lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, info.source);
            
            lpFileData->lpvfsColumnData[3].iColumnId = 4;
            lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, info.isSystem ? L"是" : L"否");
        }
    } else {
        std::wstring envInfo = FormatEnvInfo(info);
        std::wstring fileName = info.name + L".env";
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, fileName.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData->wfdData.nFileSizeLow = (DWORD)(envInfo.length() * sizeof(WCHAR));
        lpFileData->wfdData.nFileSizeHigh = 0;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
        lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;
        
        lpFileData->iNumColumns = 4;
        lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                       4 * sizeof(VFSFILEDATACOLUMNW));
        if (lpFileData->lpvfsColumnData) {
            std::wstring shortValue = info.value;
            if (shortValue.length() > 80)
                shortValue = shortValue.substr(0, 80) + L"...";
            
            lpFileData->lpvfsColumnData[0].iColumnId = 1;
            lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, shortValue);
            
            lpFileData->lpvfsColumnData[1].iColumnId = 2;
            lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, GetEnvTypeName(info.type));
            
            lpFileData->lpvfsColumnData[2].iColumnId = 3;
            lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, info.source);
            
            lpFileData->lpvfsColumnData[3].iColumnId = 4;
            lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, info.isSystem ? L"是" : L"否");
        }
    }
    
    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                      LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
                                                      DWORD dwFlags, LPFILETIME lpFT) {
    if (!IsEnvVfsPath(lpszFile)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszFile, subVarName, entryIdx)) {
            EnvInfo info;
            if (!FindEnvVariable(subVarName, info)) {
                SetLastError(ERROR_OPEN_FAILED);
                return NULL;
            }
            
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            
            EnvFileContext* ctx = new EnvFileContext();
            if (!ctx) {
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return NULL;
            }
            
            ctx->readPos = 0;
            ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;
            ctx->varName = subVarName;
            ctx->isSystem = info.isSystem;
            ctx->isSubEntry = true;
            ctx->entryIndex = entryIdx;
            ctx->isNewEntry = false;
            
            if (entryIdx >= 1 && entryIdx <= (int)entries.size()) {
                const std::wstring& entryValue = entries[entryIdx - 1];
                std::wstring content = L"变量名: " + subVarName + L"\r\n";
                content += L"条目序号: " + std::to_wstring(entryIdx) + L"\r\n";
                content += L"类型: 路径条目\r\n";
                content += L"来源: " + info.source + L"\r\n";
                content += L"\r\n值:\r\n" + entryValue + L"\r\n";
                ctx->buffer.resize(content.length() * sizeof(WCHAR));
                memcpy(ctx->buffer.data(), content.c_str(), ctx->buffer.size());
            } else if (ctx->isWrite) {
                ctx->isNewEntry = true;
                ctx->entryIndex = (int)entries.size() + 1;
            } else {
                delete ctx;
                SetLastError(ERROR_OPEN_FAILED);
                return NULL;
            }
            
            return (HANDLE)ctx;
        }
    }
    
    std::wstring envName = ParseEnvName(lpszFile);
    if (envName.empty()) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }
    
    EnvInfo info;
    bool found = FindEnvVariable(envName, info);
    if (!found) {
        SetLastError(ERROR_OPEN_FAILED);
        return NULL;
    }
    
    if (IsMultiValueVar(info.value)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }
    
    EnvFileContext* ctx = new EnvFileContext();
    if (!ctx) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    
    ctx->readPos = 0;
    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;
    ctx->varName = info.name;
    ctx->isSystem = info.isSystem;
    ctx->isSubEntry = false;
    ctx->entryIndex = 0;
    ctx->isNewEntry = false;
    
    std::wstring envInfo = FormatEnvInfo(info);
    ctx->buffer.resize(envInfo.length() * sizeof(WCHAR));
    memcpy(ctx->buffer.data(), envInfo.c_str(), ctx->buffer.size());
    
    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                 HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    EnvFileContext* ctx = (EnvFileContext*)hFile;
    
    if (!ctx || ctx->isWrite)
        return FALSE;
    
    if (lpdwReadSize)
        *lpdwReadSize = 0;
    
    if (ctx->readPos >= ctx->buffer.size())
        return TRUE;
    
    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->buffer.size() - ctx->readPos));
    
    if (bytesToRead > 0 && lpData != NULL) {
        CopyMemory(lpData, ctx->buffer.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;
        
        if (lpdwReadSize)
            *lpdwReadSize = bytesToRead;
    }
    
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                  HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush,
                                                  LPDWORD lpdwWriteSize) {
    EnvFileContext* ctx = (EnvFileContext*)hFile;
    
    if (!ctx || !ctx->isWrite)
        return FALSE;
    
    if (lpdwWriteSize)
        *lpdwWriteSize = 0;
    
    if (dwSize == 0 || lpData == NULL)
        return TRUE;
    
    size_t newSize = ctx->readPos + dwSize;
    if (newSize > ctx->buffer.size()) {
        ctx->buffer.resize(newSize);
    }
    
    CopyMemory(ctx->buffer.data() + ctx->readPos, lpData, dwSize);
    ctx->readPos += dwSize;
    
    if (lpdwWriteSize)
        *lpdwWriteSize = dwSize;
    
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                 HANDLE hFile, __int64 iPos, DWORD dwMethod,
                                                 DWORD dwFlags, unsigned __int64* piNewPos) {
    EnvFileContext* ctx = (EnvFileContext*)hFile;
    
    if (!ctx)
        return FALSE;
    
    __int64 newPos = 0;
    
    switch (dwMethod) {
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
    
    if (ctx->isWrite) {
        if ((size_t)newPos > ctx->buffer.size()) {
            ctx->buffer.resize((size_t)newPos);
        }
        ctx->readPos = (size_t)newPos;
    } else {
        ctx->readPos = (size_t)min(newPos, (__int64)ctx->buffer.size());
    }
    
    if (piNewPos)
        *piNewPos = ctx->readPos;
    
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    EnvFileContext* ctx = (EnvFileContext*)hFile;
    
    if (!ctx) return;
    
    if (ctx->isWrite && ctx->isSubEntry && !ctx->varName.empty()) {
        EnvInfo info;
        if (FindEnvVariable(ctx->varName, info)) {
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            
            size_t contentLen = ctx->readPos / sizeof(WCHAR);
            std::wstring content((LPCWSTR)ctx->buffer.data(), contentLen);
            
            size_t valuePos = content.find(L"\r\n值:\r\n");
            std::wstring newValue;
            if (valuePos != std::wstring::npos) {
                newValue = content.substr(valuePos + 5);
                while (!newValue.empty() && (newValue.back() == L'\r' || newValue.back() == L'\n'))
                    newValue.pop_back();
            } else {
                newValue = content;
                while (!newValue.empty() && (newValue.back() == L'\r' || newValue.back() == L'\n'))
                    newValue.pop_back();
            }
            
            if (ctx->isNewEntry) {
                entries.push_back(newValue);
            } else if (ctx->entryIndex >= 1 && ctx->entryIndex <= (int)entries.size()) {
                entries[ctx->entryIndex - 1] = newValue;
            }
            
            std::wstring combined = JoinEnvValue(entries);
            SetEnvVariable(ctx->varName, combined, info.isSystem, info.type);
        }
    }
    
    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    if (!IsEnvVfsPath(lpszFile)) return FALSE;
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszFile, subVarName, entryIdx)) {
            EnvInfo info;
            if (!FindEnvVariable(subVarName, info)) {
                return FALSE;
            }
            
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            if (entryIdx < 1 || entryIdx > (int)entries.size()) {
                return FALSE;
            }
            
            int result = MessageBoxW(NULL,
                                    (L"确定要从 \"" + subVarName + L"\" 中删除第 " + std::to_wstring(entryIdx) + L" 条记录吗？\n\n" + entries[entryIdx - 1]).c_str(),
                                    L"确认删除条目", MB_YESNO | MB_ICONQUESTION);
            
            if (result != IDYES) {
                return FALSE;
            }
            
            entries.erase(entries.begin() + (entryIdx - 1));
            std::wstring combined = JoinEnvValue(entries);
            
            if (SetEnvVariable(subVarName, combined, info.isSystem, info.type)) {
                return TRUE;
            }
            
            MessageBoxW(NULL, L"删除条目失败，请确认您有足够的权限。", L"错误", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    
    std::wstring envName = ParseEnvName(lpszFile);
    if (envName.empty()) return FALSE;
    
    EnvInfo info;
    if (!FindEnvVariable(envName, info)) {
        return FALSE;
    }
    
    if (IsMultiValueVar(info.value)) {
        return FALSE;
    }
    
    int result = MessageBoxW(NULL, 
                            (L"确定要删除环境变量 \"" + envName + L"\" 吗？").c_str(), 
                            L"确认删除", MB_YESNO | MB_ICONQUESTION);
    
    if (result != IDYES) {
        return FALSE;
    }
    
    if (DeleteEnvVariable(envName, info.isSystem)) {
        return TRUE;
    }
    
    MessageBoxW(NULL, L"删除环境变量失败，请确认您有足够的权限。", L"错误", MB_OK | MB_ICONERROR);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     LPWSTR lpszOldName, LPWSTR lpszNewName) {
    if (!IsEnvVfsPath(lpszOldName) || !IsEnvVfsPath(lpszNewName)) return FALSE;
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszOldName, subVarName, entryIdx)) {
            return FALSE;
        }
    }
    
    std::wstring oldName = ParseEnvName(lpszOldName);
    std::wstring newName = ParseEnvName(lpszNewName);
    
    if (oldName.empty() || newName.empty()) return FALSE;
    
    EnvInfo info;
    if (!FindEnvVariable(oldName, info)) return FALSE;
    
    if (IsMultiValueVar(info.value)) return FALSE;
    
    if (!SetEnvVariable(newName, info.value, info.isSystem, info.type)) return FALSE;
    
    if (!DeleteEnvVariable(oldName, info.isSystem)) {
        MessageBoxW(NULL, L"重命名成功，但无法删除旧变量。", L"警告", MB_OK | MB_ICONWARNING);
        return TRUE;
    }
    
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                   LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return VFS_RenameFileW(hVFSData, lpFuncData, lpszOldName, lpszNewName);
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath,
                                                            LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;
    
    if (IsEnvRootPath(lpszPath)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"环境变量");
        return TRUE;
    }
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszPath, subVarName, entryIdx)) {
            std::wstring displayName = std::to_wstring(entryIdx) + L".path";
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, displayName.c_str());
            return TRUE;
        }
    }
    
    {
        std::wstring dirVarName;
        if (IsSubDirPath(lpszPath) && ParseVarDirPath(lpszPath, dirVarName)) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, dirVarName.c_str());
            return TRUE;
        }
    }
    
    std::wstring envName = ParseEnvName(lpszPath);
    if (envName.empty()) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"环境变量");
        return TRUE;
    }
    
    EnvInfo info;
    if (FindEnvVariable(envName, info) && IsMultiValueVar(info.value)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, envName.c_str());
        return TRUE;
    }
    
    std::wstring displayName = envName + L".env";
    StringCchCopyW(lpszDisplayName, cbDisplayNameMax, displayName.c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
                                                             LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    
    if (fRoot) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, ENV_VFS_PREFIX);
        return TRUE;
    }
    
    if (IsEnvRootPath(lpszPath)) {
        return FALSE;
    }
    
    StringCchCopyW(lpszNewPath, cbNewPathMax, ENV_VFS_PREFIX);
    return TRUE;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;
    
    {
        std::wstring subVarName;
        int entryIdx = 0;
        if (ParseSubEntryPath(lpszFiles, subVarName, entryIdx)) {
            EnvInfo info;
            if (!FindEnvVariable(subVarName, info)) return NULL;
            
            std::vector<std::wstring> entries = SplitEnvValue(info.value);
            if (entryIdx < 1 || entryIdx > (int)entries.size()) return NULL;
            
            std::wstring propText = L"变量名: " + subVarName + L"\r\n";
            propText += L"条目序号: " + std::to_wstring(entryIdx) + L" / " + std::to_wstring(entries.size()) + L"\r\n";
            propText += L"类型: 路径条目\r\n";
            propText += L"来源: " + info.source + L"\r\n";
            propText += L"\r\n值:\r\n" + entries[entryIdx - 1] + L"\r\n";
            
            MessageBoxW(hwndParent, propText.c_str(), (L"路径条目 - " + subVarName + L"[" + std::to_wstring(entryIdx) + L"]").c_str(), MB_OK | MB_ICONINFORMATION);
            return NULL;
        }
    }
    
    std::wstring envName = ParseEnvName(lpszFiles);
    if (envName.empty()) return NULL;
    
    EnvInfo info;
    if (!FindEnvVariable(envName, info)) return NULL;
    
    std::wstring propText = FormatEnvInfo(info);
    MessageBoxW(hwndParent, propText.c_str(), (L"环境变量 - " + envName).c_str(), MB_OK | MB_ICONINFORMATION);
    
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
                                                 LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        // Enable all standard file operations for default context menu
        *pAvail = VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_PROPERTIES |
                  VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_CLIPCOPY |
                  VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE |
                  VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_MOVE;
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
                                                      unsigned __int64* piFileSize) {
    if (hFile) {
        EnvFileContext* ctx = (EnvFileContext*)hFile;
        if (piFileSize)
            *piFileSize = ctx->buffer.size();
        return TRUE;
    } else if (lpszPath) {
        {
            std::wstring subVarName;
            int entryIdx = 0;
            if (ParseSubEntryPath(lpszPath, subVarName, entryIdx)) {
                EnvInfo info;
                if (FindEnvVariable(subVarName, info)) {
                    std::vector<std::wstring> entries = SplitEnvValue(info.value);
                    if (entryIdx >= 1 && entryIdx <= (int)entries.size()) {
                        if (piFileSize)
                            *piFileSize = entries[entryIdx - 1].length() * sizeof(WCHAR);
                        return TRUE;
                    }
                }
            }
        }
        
        std::wstring envName = ParseEnvName(lpszPath);
        if (!envName.empty()) {
            EnvInfo info;
            if (FindEnvVariable(envName, info)) {
                std::wstring envInfo = FormatEnvInfo(info);
                if (piFileSize)
                    *piFileSize = envInfo.length() * sizeof(WCHAR);
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) {
    return GetLastError();
}

static BOOL RunOpusCommand(LPCWSTR lpszCommand) {
    WCHAR opusDir[MAX_PATH] = {0};
    HMODULE hOpus = GetModuleHandleW(NULL);
    if (hOpus) {
        GetModuleFileNameW(hOpus, opusDir, MAX_PATH);
        WCHAR* lastSlash = wcsrchr(opusDir, L'\\');
        if (lastSlash) *lastSlash = L'\0';
    }
    if (opusDir[0] == L'\0') {
        DWORD size = MAX_PATH;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\GPSoftware\\Directory Opus", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            size = MAX_PATH;
            if (RegQueryValueExW(hKey, L"InstallPath", NULL, NULL,
                    (LPBYTE)opusDir, &size) != ERROR_SUCCESS) {
                opusDir[0] = L'\0';
            }
            RegCloseKey(hKey);
        }
    }
    if (opusDir[0] == L'\0') return FALSE;

    WCHAR cmdLine[2048];
    StringCchPrintfW(cmdLine, 2048, L"\"%s\\dopusrt.exe\" /cmd %s", opusDir, lpszCommand);

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};

    BOOL result = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return result;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(
    HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData)
{
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    // Double-click (no verb)
    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0) {
        if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }
        // Open file in editor (double-click on .env file)
        WCHAR cmd[1024];
        StringCchPrintfW(cmd, 1024, L"EnvOpen FILE=\"%s\"", lpVerbData->lpszPath);
        RunOpusCommand(cmd);
        return VFSCVRES_HANDLED;
    }

    // Handle custom context menu verbs
    if (_wcsicmp(lpVerbData->lpszVerb, L"EnvOpen") == 0) {
        WCHAR cmd[1024];
        StringCchPrintfW(cmd, 1024, L"EnvOpen FILE=\"%s\"", lpVerbData->lpszPath);
        RunOpusCommand(cmd);
        return VFSCVRES_HANDLED;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"EnvCopyValue") == 0) {
        // Copy environment variable value to clipboard
        std::wstring envName = ParseEnvName(lpVerbData->lpszPath);
        if (!envName.empty()) {
            EnvInfo info;
            if (FindEnvVariable(envName, info)) {
                if (OpenClipboard(lpVerbData->hwndParent)) {
                    EmptyClipboard();
                    size_t len = (info.value.length() + 1) * sizeof(WCHAR);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                    if (hMem) {
                        LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                        if (pMem) {
                            StringCchCopyW(pMem, info.value.length() + 1, info.value.c_str());
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        } else {
                            GlobalFree(hMem);
                        }
                    }
                    CloseClipboard();
                }
            }
        }
        return VFSCVRES_HANDLED;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"EnvCopyName") == 0) {
        // Copy environment variable name to clipboard
        std::wstring envName = ParseEnvName(lpVerbData->lpszPath);
        if (!envName.empty()) {
            if (OpenClipboard(lpVerbData->hwndParent)) {
                EmptyClipboard();
                size_t len = (envName.length() + 1) * sizeof(WCHAR);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                if (hMem) {
                    LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                    if (pMem) {
                        StringCchCopyW(pMem, envName.length() + 1, envName.c_str());
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    } else {
                        GlobalFree(hMem);
                    }
                }
                CloseClipboard();
            }
        }
        return VFSCVRES_HANDLED;
    }
    else if (_wcsicmp(lpVerbData->lpszVerb, L"EnvSysSettings") == 0) {
        OpenSystemPropertiesEnv();
        return VFSCVRES_HANDLED;
    }

    // Let DOpus handle default verbs (copy, paste, cut, delete, rename, properties)
    return VFSCVRES_DEFAULT;
}

// Custom context menu items (supplement the DOpus default menu)
// DOpus default menu provides: Copy, Paste, Cut, Delete, Rename, Properties
// We add env-specific items on top of those
static VFSCONTEXTMENUITEMW g_contextMenuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\x6253\x5F00(&O)", L"EnvOpen" },                    // 打开
    { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\x590D\x5236\x53D8\x91CF\x503C(&V)", L"EnvCopyValue" },    // 复制变量值
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\x590D\x5236\x53D8\x91CF\x540D(&N)", L"EnvCopyName" },     // 复制变量名
    { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\x7CFB\x7EDF\x73AF\x5883\x53D8\x91CF\x8BBE\x7F6E(&S)", L"EnvSysSettings" },  // 系统环境变量设置
};

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(
    HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpMenuData) return FALSE;

    // Enable DOpus default context menu (Copy, Paste, Cut, Delete, Rename, Properties)
    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = TRUE;
    // Custom items appear above the default menu items
    lpMenuData->fCustomItemsBelow = FALSE;
    lpMenuData->lpCustomItems = g_contextMenuItems;
    lpMenuData->iNumCustomItems = sizeof(g_contextMenuItems) / sizeof(g_contextMenuItems[0]);
    lpMenuData->fFreeCustomItems = FALSE;

    return TRUE;
}

// ========== 配置保存和加载函数 ==========

static std::wstring GetConfigFilePath() {
    WCHAR appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring configDir = std::wstring(appDataPath) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin";
        CreateDirectoryW(configDir.c_str(), NULL);
        return configDir + L"\\EnvVFS.ini";
    }
    return L"";
}

static void LoadDefaultConfig() {
    g_config.showSystem = true;
    g_config.showUser = true;
    g_config.showVolatile = true;
    g_config.refreshInterval = 30;
    
    g_config.colName = true;
    g_config.colValue = true;
    g_config.colType = true;
    g_config.colSource = true;
    g_config.fontSize = 9;
    
    g_config.defEditor = L"";
    g_config.confirmDelete = true;
    g_config.autoBackup = true;
    
    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    g_config.backupPath = tempPath;
    
    g_config.cacheTimeout = 60;
    g_config.verboseLog = false;
    g_config.enableTray = false;
    g_config.logPath = L"";
}

static bool LoadConfigFromFile() {
    std::wstring configFile = GetConfigFilePath();
    if (configFile.empty()) {
        LoadDefaultConfig();
        return false;
    }
    
    g_config.showSystem = GetPrivateProfileIntW(L"General", L"ShowSystem", 1, configFile.c_str()) != 0;
    g_config.showUser = GetPrivateProfileIntW(L"General", L"ShowUser", 1, configFile.c_str()) != 0;
    g_config.showVolatile = GetPrivateProfileIntW(L"General", L"ShowVolatile", 1, configFile.c_str()) != 0;
    g_config.refreshInterval = GetPrivateProfileIntW(L"General", L"RefreshInterval", 30, configFile.c_str());
    
    g_config.colName = GetPrivateProfileIntW(L"Display", L"ColName", 1, configFile.c_str()) != 0;
    g_config.colValue = GetPrivateProfileIntW(L"Display", L"ColValue", 1, configFile.c_str()) != 0;
    g_config.colType = GetPrivateProfileIntW(L"Display", L"ColType", 1, configFile.c_str()) != 0;
    g_config.colSource = GetPrivateProfileIntW(L"Display", L"ColSource", 1, configFile.c_str()) != 0;
    g_config.fontSize = GetPrivateProfileIntW(L"Display", L"FontSize", 9, configFile.c_str());
    
    WCHAR buffer[MAX_PATH];
    GetPrivateProfileStringW(L"Edit", L"DefEditor", L"", buffer, MAX_PATH, configFile.c_str());
    g_config.defEditor = buffer;
    
    g_config.confirmDelete = GetPrivateProfileIntW(L"Edit", L"ConfirmDelete", 1, configFile.c_str()) != 0;
    g_config.autoBackup = GetPrivateProfileIntW(L"Edit", L"AutoBackup", 1, configFile.c_str()) != 0;
    
    GetPrivateProfileStringW(L"Edit", L"BackupPath", L"", buffer, MAX_PATH, configFile.c_str());
    g_config.backupPath = buffer;
    
    g_config.cacheTimeout = GetPrivateProfileIntW(L"Advanced", L"CacheTimeout", 60, configFile.c_str());
    g_config.verboseLog = GetPrivateProfileIntW(L"Advanced", L"VerboseLog", 0, configFile.c_str()) != 0;
    g_config.enableTray = GetPrivateProfileIntW(L"Advanced", L"EnableTray", 0, configFile.c_str()) != 0;
    
    GetPrivateProfileStringW(L"Advanced", L"LogPath", L"", buffer, MAX_PATH, configFile.c_str());
    g_config.logPath = buffer;
    
    return true;
}

static bool SaveConfigToFile() {
    std::wstring configFile = GetConfigFilePath();
    if (configFile.empty()) return false;
    
    WCHAR buffer[32];
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.showSystem ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"ShowSystem", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.showUser ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"ShowUser", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.showVolatile ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"ShowVolatile", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.refreshInterval);
    WritePrivateProfileStringW(L"General", L"RefreshInterval", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.colName ? 1 : 0);
    WritePrivateProfileStringW(L"Display", L"ColName", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.colValue ? 1 : 0);
    WritePrivateProfileStringW(L"Display", L"ColValue", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.colType ? 1 : 0);
    WritePrivateProfileStringW(L"Display", L"ColType", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.colSource ? 1 : 0);
    WritePrivateProfileStringW(L"Display", L"ColSource", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.fontSize);
    WritePrivateProfileStringW(L"Display", L"FontSize", buffer, configFile.c_str());
    
    WritePrivateProfileStringW(L"Edit", L"DefEditor", g_config.defEditor.c_str(), configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.confirmDelete ? 1 : 0);
    WritePrivateProfileStringW(L"Edit", L"ConfirmDelete", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.autoBackup ? 1 : 0);
    WritePrivateProfileStringW(L"Edit", L"AutoBackup", buffer, configFile.c_str());
    
    WritePrivateProfileStringW(L"Edit", L"BackupPath", g_config.backupPath.c_str(), configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.cacheTimeout);
    WritePrivateProfileStringW(L"Advanced", L"CacheTimeout", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.verboseLog ? 1 : 0);
    WritePrivateProfileStringW(L"Advanced", L"VerboseLog", buffer, configFile.c_str());
    
    StringCchPrintfW(buffer, 32, L"%d", g_config.enableTray ? 1 : 0);
    WritePrivateProfileStringW(L"Advanced", L"EnableTray", buffer, configFile.c_str());
    
    WritePrivateProfileStringW(L"Advanced", L"LogPath", g_config.logPath.c_str(), configFile.c_str());
    
    return true;
}

static bool LoadConfigFromRegistry() {
    return LoadConfigFromFile();
}

static bool SaveConfigToRegistry() {
    return SaveConfigToFile();
}

// ========== 对话框辅助函数 ==========

static void BrowseForFile(HWND hDlg, int editCtrlId, LPCWSTR filter) {
    OPENFILENAMEW ofn;
    WCHAR fileName[MAX_PATH] = L"";
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(hDlg, editCtrlId, fileName);
    }
}

static void BrowseForFolder(HWND hDlg, int editCtrlId) {
    BROWSEINFOW bi;
    WCHAR path[MAX_PATH];
    LPITEMIDLIST pidl;
    
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hDlg;
    bi.pszDisplayName = path;
    bi.lpszTitle = L"选择文件夹";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, path)) {
            SetDlgItemTextW(hDlg, editCtrlId, path);
        }
        IMalloc* pMalloc;
        if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
            pMalloc->Free(pidl);
            pMalloc->Release();
        }
    }
}

static void ExportConfig(HWND hDlg) {
    WCHAR fileName[MAX_PATH] = L"EnvVFS_Config.ini";
    
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = L"配置文件 (*.ini)\0*.ini\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"ini";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        std::wstring configFile = GetConfigFilePath();
        if (!configFile.empty()) {
            if (CopyFileW(configFile.c_str(), fileName, FALSE)) {
                MessageBoxW(hDlg, L"配置已成功导出", L"导出成功", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hDlg, L"导出配置失败", L"错误", MB_OK | MB_ICONERROR);
            }
        }
    }
}

static void ImportConfig(HWND hDlg) {
    WCHAR fileName[MAX_PATH] = L"";
    
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = L"配置文件 (*.ini)\0*.ini\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        std::wstring configFile = GetConfigFilePath();
        if (!configFile.empty()) {
            if (CopyFileW(fileName, configFile.c_str(), FALSE)) {
                LoadConfigFromFile();
                InitDialogControls(hDlg);
                MessageBoxW(hDlg, L"配置已成功导入", L"导入成功", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hDlg, L"导入配置失败", L"错误", MB_OK | MB_ICONERROR);
            }
        }
    }
}

static void ShowPopupMenu(HWND hDlg);

// 自绘菜单窗口过程
static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            
            // 绘制背景
            HBRUSH hBrush = GetSysColorBrush(COLOR_MENU);
            FillRect(hdc, &rcClient, hBrush);
            
            // 绘制边框
            FrameRect(hdc, &rcClient, GetSysColorBrush(COLOR_WINDOWFRAME));
            
            // 获取字体
            HFONT hFont = (HFONT)SendMessageW(GetParent(hWnd), WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            // 绘制菜单项
            int itemCount = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
            int y = 2;
            
            for (int i = 0; i < itemCount; i++) {
                if (g_menuItems[i].isSeparator) {
                    // 绘制分隔符
                    RECT rcSep = {8, y + 4, rcClient.right - 8, y + 6};
                    FillRect(hdc, &rcSep, GetSysColorBrush(COLOR_GRAYTEXT));
                    y += 10;
                } else {
                    // 计算菜单项区域
                    SIZE size;
                    GetTextExtentPoint32W(hdc, g_menuItems[i].text, (int)wcslen(g_menuItems[i].text), &size);
                    
                    RECT rcItem = {2, y, rcClient.right - 2, y + size.cy + 6};
                    
                    // 绘制悬停背景
                    if (i == g_hoveredItem) {
                        HBRUSH hHoverBrush = GetSysColorBrush(COLOR_HIGHLIGHT);
                        FillRect(hdc, &rcItem, hHoverBrush);
                        SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                    } else {
                        SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));
                    }
                    
                    // 绘制文本
                    SetBkMode(hdc, TRANSPARENT);
                    RECT rcText = {10, y + 3, rcClient.right - 10, y + size.cy + 3};
                    DrawTextW(hdc, g_menuItems[i].text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    
                    y += size.cy + 6;
                }
            }
            
            SelectObject(hdc, hOldFont);
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            
            // 计算悬停的菜单项
            HDC hdc = GetDC(hWnd);
            HFONT hFont = (HFONT)SendMessageW(GetParent(hWnd), WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            int itemCount = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
            int y = 2;
            int newHoveredItem = -1;
            
            for (int i = 0; i < itemCount; i++) {
                if (g_menuItems[i].isSeparator) {
                    y += 10;
                } else {
                    SIZE size;
                    GetTextExtentPoint32W(hdc, g_menuItems[i].text, (int)wcslen(g_menuItems[i].text), &size);
                    
                    RECT rcItem = {2, y, 200, y + size.cy + 6};
                    
                    if (PtInRect(&rcItem, pt)) {
                        newHoveredItem = i;
                        break;
                    }
                    
                    y += size.cy + 6;
                }
            }
            
            SelectObject(hdc, hOldFont);
            ReleaseDC(hWnd, hdc);
            
            if (newHoveredItem != g_hoveredItem) {
                g_hoveredItem = newHoveredItem;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            
            // 计算点击的菜单项
            HDC hdc = GetDC(hWnd);
            HFONT hFont = (HFONT)SendMessageW(GetParent(hWnd), WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            int itemCount = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
            int y = 2;
            
            for (int i = 0; i < itemCount; i++) {
                if (g_menuItems[i].isSeparator) {
                    y += 10;
                } else {
                    SIZE size;
                    GetTextExtentPoint32W(hdc, g_menuItems[i].text, (int)wcslen(g_menuItems[i].text), &size);
                    
                    RECT rcItem = {2, y, 200, y + size.cy + 6};
                    
                    if (PtInRect(&rcItem, pt)) {
                        // 发送命令到父窗口
                        PostMessageW(GetParent(hWnd), WM_COMMAND, g_menuItems[i].id, 0);
                        break;
                    }
                    
                    y += size.cy + 6;
                }
            }
            
            SelectObject(hdc, hOldFont);
            ReleaseDC(hWnd, hdc);
            
            // 关闭菜单
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }
        
        case WM_KILLFOCUS:
            ShowWindow(hWnd, SW_HIDE);
            return 0;
    }
    
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static void ShowPopupMenu(HWND hDlg) {
    HWND hButton = GetDlgItem(hDlg, IDC_MENU_BUTTON);
    if (!hButton) return;
    
    RECT rcButton;
    GetWindowRect(hButton, &rcButton);
    
    RECT rcDlg;
    GetWindowRect(hDlg, &rcDlg);
    
    // 注册菜单窗口类（只注册一次）
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MenuWndProc;
        wc.hInstance = g_hModule;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszClassName = L"EnvVFSMenuClass";
        
        RegisterClassExW(&wc);
        registered = true;
    }
    
    // 计算菜单尺寸
    HDC hdc = GetDC(hDlg);
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    int maxWidth = 0;
    int totalHeight = 4; // 上下边距
    
    int itemCount = sizeof(g_menuItems) / sizeof(g_menuItems[0]);
    for (int i = 0; i < itemCount; i++) {
        if (g_menuItems[i].isSeparator) {
            totalHeight += 10;
        } else {
            SIZE size;
            GetTextExtentPoint32W(hdc, g_menuItems[i].text, (int)wcslen(g_menuItems[i].text), &size);
            if (size.cx > maxWidth) maxWidth = size.cx;
            totalHeight += size.cy + 6;
        }
    }
    
    SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);
    
    // 添加左右边距
    int menuWidth = maxWidth + 20;
    
    // 计算菜单位置：菜单右边缘距离窗口右边缘 4px
    int menuLeft = rcDlg.right - 4 - menuWidth;
    int menuTop = rcButton.bottom;
    
    // 创建或显示菜单窗口
    if (!g_hMenuWnd) {
        g_hMenuWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"EnvVFSMenuClass",
            NULL,
            WS_POPUP,
            menuLeft, menuTop,
            menuWidth, totalHeight,
            hDlg,
            NULL,
            g_hModule,
            NULL
        );
    } else {
        SetWindowPos(g_hMenuWnd, NULL, menuLeft, menuTop, menuWidth, totalHeight, SWP_NOZORDER);
    }
    
    g_hoveredItem = -1;
    ShowWindow(g_hMenuWnd, SW_SHOWNOACTIVATE);
    SetFocus(g_hMenuWnd);
}

static void ShowNavPage(HWND hDlg, int page) {
    g_currentNavPage = page;
    for (int i = 0; i < NAV_COUNT; i++) {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++) {
            HWND hwnd = GetDlgItem(hDlg, ctrls[j]);
            if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        }
    }
}

static void SetChineseText(HWND hDlg) {
    SetWindowTextW(hDlg, L"环境变量 VFS 配置");
    
    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList) {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < NAV_COUNT; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }
    
    SetDlgItemTextW(hDlg, IDOK, L"确定");
    SetDlgItemTextW(hDlg, IDCANCEL, L"取消");
    SetDlgItemTextW(hDlg, IDC_OPEN_ENV, L"系统设置");
    SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"重置");
    SetDlgItemTextW(hDlg, IDC_MENU_BUTTON, L"☰");
    
    SetDlgItemTextW(hDlg, IDC_SHOW_SYS, L"显示系统变量");
    SetDlgItemTextW(hDlg, IDC_SHOW_USER, L"显示用户变量");
    SetDlgItemTextW(hDlg, IDC_SHOW_VOLATILE, L"显示临时变量");
    SetDlgItemTextW(hDlg, IDC_LBL_REFRESH, L"刷新间隔(秒):");
    
    SetDlgItemTextW(hDlg, IDC_LBL_COLUMNS, L"默认列:");
    SetDlgItemTextW(hDlg, IDC_COL_NAME, L"名称");
    SetDlgItemTextW(hDlg, IDC_COL_VALUE, L"值");
    SetDlgItemTextW(hDlg, IDC_COL_TYPE, L"类型");
    SetDlgItemTextW(hDlg, IDC_COL_SOURCE, L"来源");
    SetDlgItemTextW(hDlg, IDC_LBL_FONT_SIZE, L"字体大小:");
    
    SetDlgItemTextW(hDlg, IDC_LBL_DEF_EDITOR, L"默认编辑器:");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_DEL, L"删除前确认");
    SetDlgItemTextW(hDlg, IDC_CHK_AUTO_BACKUP, L"自动备份");
    SetDlgItemTextW(hDlg, IDC_LBL_BACKUP_PATH, L"备份路径:");
    
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TMOUT, L"缓存超时(秒):");
    SetDlgItemTextW(hDlg, IDC_CHK_VERBOSE_LOG, L"详细日志");
    SetDlgItemTextW(hDlg, IDC_CHK_ENABLE_TRAY, L"启用托盘");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_PATH, L"日志路径:");
}

static void InitDialogControls(HWND hDlg) {
    SetChineseText(hDlg);
    
    // 创建工具提示控件
    HWND hTT = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL, 
                               WS_POPUP | TTS_ALWAYSTIP,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               hDlg, NULL, g_hModule, NULL);
    
    if (hTT) {
        // 添加工具提示
        struct ToolTipInfo { int ctrlId; LPCWSTR text; };
        ToolTipInfo tooltips[] = {
            { IDC_SHOW_SYS, L"显示系统级环境变量（需要管理员权限）" },
            { IDC_SHOW_USER, L"显示用户级环境变量" },
            { IDC_SHOW_VOLATILE, L"显示临时环境变量（当前会话）" },
            { IDC_REFRESH_INTERVAL, L"自动刷新列表的时间间隔（1-3600秒）" },
            { IDC_COL_NAME, L"在列表中显示变量名列" },
            { IDC_COL_VALUE, L"在列表中显示变量值列" },
            { IDC_COL_TYPE, L"在列表中显示变量类型列" },
            { IDC_COL_SOURCE, L"在列表中显示变量来源列" },
            { IDC_FONT_SIZE, L"设置显示字体大小（8-16）" },
            { IDC_DEF_EDITOR, L"编辑环境变量时使用的默认编辑器" },
            { IDC_CHK_CONFIRM_DEL, L"删除环境变量前显示确认对话框" },
            { IDC_CHK_AUTO_BACKUP, L"修改环境变量前自动备份" },
            { IDC_BACKUP_PATH, L"备份文件的保存路径" },
            { IDC_CACHE_TMOUT, L"缓存环境变量数据的时间（0-86400秒）" },
            { IDC_CHK_VERBOSE_LOG, L"启用详细日志记录" },
            { IDC_CHK_ENABLE_TRAY, L"在系统托盘显示图标" },
            { IDC_LOG_PATH, L"日志文件的保存路径" }
        };
        
        for (int i = 0; i < sizeof(tooltips) / sizeof(tooltips[0]); i++) {
            HWND hCtrl = GetDlgItem(hDlg, tooltips[i].ctrlId);
            if (hCtrl) {
                TOOLINFOW ti = {0};
                ti.cbSize = sizeof(TOOLINFOW);
                ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                ti.hwnd = hDlg;
                ti.uId = (UINT_PTR)hCtrl;
                ti.lpszText = (LPWSTR)tooltips[i].text;
                SendMessageW(hTT, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            }
        }
    }
    
    HWND hFontCombo = GetDlgItem(hDlg, IDC_FONT_SIZE);
    const int fontSizes[] = {8, 9, 10, 11, 12, 14, 16};
    for (int i = 0; i < 7; i++) {
        WCHAR buf[10];
        StringCchPrintfW(buf, 10, L"%d", fontSizes[i]);
        SendMessageW(hFontCombo, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    
    CheckDlgButton(hDlg, IDC_SHOW_SYS, g_config.showSystem ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_USER, g_config.showUser ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_VOLATILE, g_config.showVolatile ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_REFRESH_INTERVAL, g_config.refreshInterval, FALSE);
    
    CheckDlgButton(hDlg, IDC_COL_NAME, g_config.colName ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_VALUE, g_config.colValue ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_TYPE, g_config.colType ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_SOURCE, g_config.colSource ? BST_CHECKED : BST_UNCHECKED);
    
    WCHAR fontSizeBuf[10];
    StringCchPrintfW(fontSizeBuf, 10, L"%d", g_config.fontSize);
    int idx = (int)SendMessageW(hFontCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fontSizeBuf);
    SendMessageW(hFontCombo, CB_SETCURSEL, idx >= 0 ? idx : 1, 0);
    
    SetDlgItemTextW(hDlg, IDC_DEF_EDITOR, g_config.defEditor.c_str());
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_DEL, g_config.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_AUTO_BACKUP, g_config.autoBackup ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(hDlg, IDC_BACKUP_PATH, g_config.backupPath.c_str());
    
    SetDlgItemInt(hDlg, IDC_CACHE_TMOUT, g_config.cacheTimeout, FALSE);
    CheckDlgButton(hDlg, IDC_CHK_VERBOSE_LOG, g_config.verboseLog ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_ENABLE_TRAY, g_config.enableTray ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(hDlg, IDC_LOG_PATH, g_config.logPath.c_str());
    
    ShowNavPage(hDlg, g_currentNavPage);
}

static bool SaveDialogControls(HWND hDlg) {
    // 验证刷新间隔
    int refreshInterval = GetDlgItemInt(hDlg, IDC_REFRESH_INTERVAL, NULL, FALSE);
    if (refreshInterval < 1 || refreshInterval > 3600) {
        MessageBoxW(hDlg, L"刷新间隔必须在 1-3600 秒之间", L"输入错误", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(hDlg, IDC_REFRESH_INTERVAL));
        return false;
    }
    
    // 验证缓存超时
    int cacheTimeout = GetDlgItemInt(hDlg, IDC_CACHE_TMOUT, NULL, FALSE);
    if (cacheTimeout < 0 || cacheTimeout > 86400) {
        MessageBoxW(hDlg, L"缓存超时必须在 0-86400 秒之间", L"输入错误", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(hDlg, IDC_CACHE_TMOUT));
        return false;
    }
    
    // 验证字体大小
    HWND hFontCombo = GetDlgItem(hDlg, IDC_FONT_SIZE);
    int fontIdx = (int)SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
    if (fontIdx >= 0) {
        WCHAR buf[10];
        SendMessageW(hFontCombo, CB_GETLBTEXT, fontIdx, (LPARAM)buf);
        int fontSize = _wtoi(buf);
        if (fontSize < 8 || fontSize > 16) {
            MessageBoxW(hDlg, L"字体大小必须在 8-16 之间", L"输入错误", MB_OK | MB_ICONWARNING);
            SetFocus(hFontCombo);
            return false;
        }
        g_config.fontSize = fontSize;
    }
    
    // 保存配置
    g_config.showSystem = (IsDlgButtonChecked(hDlg, IDC_SHOW_SYS) == BST_CHECKED);
    g_config.showUser = (IsDlgButtonChecked(hDlg, IDC_SHOW_USER) == BST_CHECKED);
    g_config.showVolatile = (IsDlgButtonChecked(hDlg, IDC_SHOW_VOLATILE) == BST_CHECKED);
    g_config.refreshInterval = refreshInterval;
    
    g_config.colName = (IsDlgButtonChecked(hDlg, IDC_COL_NAME) == BST_CHECKED);
    g_config.colValue = (IsDlgButtonChecked(hDlg, IDC_COL_VALUE) == BST_CHECKED);
    g_config.colType = (IsDlgButtonChecked(hDlg, IDC_COL_TYPE) == BST_CHECKED);
    g_config.colSource = (IsDlgButtonChecked(hDlg, IDC_COL_SOURCE) == BST_CHECKED);
    
    WCHAR buffer[MAX_PATH];
    GetDlgItemTextW(hDlg, IDC_DEF_EDITOR, buffer, MAX_PATH);
    g_config.defEditor = buffer;
    
    g_config.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_DEL) == BST_CHECKED);
    g_config.autoBackup = (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_BACKUP) == BST_CHECKED);
    
    GetDlgItemTextW(hDlg, IDC_BACKUP_PATH, buffer, MAX_PATH);
    g_config.backupPath = buffer;
    
    g_config.cacheTimeout = cacheTimeout;
    g_config.verboseLog = (IsDlgButtonChecked(hDlg, IDC_CHK_VERBOSE_LOG) == BST_CHECKED);
    g_config.enableTray = (IsDlgButtonChecked(hDlg, IDC_CHK_ENABLE_TRAY) == BST_CHECKED);
    
    GetDlgItemTextW(hDlg, IDC_LOG_PATH, buffer, MAX_PATH);
    g_config.logPath = buffer;
    
    if (!SaveConfigToRegistry()) {
        return false;
    }
    
    // 应用配置 - 通知系统环境变量已更改
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment",
                        SMTO_ABORTIFHUNG, 5000, NULL);
    
    return true;
}

static void ResetToDefaults(HWND hDlg) {
    if (MessageBoxW(hDlg, L"确定要重置所有设置为默认值吗？", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        LoadDefaultConfig();
        InitDialogControls(hDlg);
    }
}

static void OpenSystemEnvSettings(HWND hDlg) {
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"open";
    sei.lpFile = L"rundll32.exe";
    sei.lpParameters = L"sysdm.cpl,EditEnvironmentVariables";
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

// ========== 配置对话框过程 ==========

INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            InitDialogControls(hDlg);
            return (INT_PTR)TRUE;
            
        case WM_MEASUREITEM:
            {
                LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
                if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST) {
                    lpmis->itemHeight = 24;
                }
            }
            return (INT_PTR)TRUE;
            
        case WM_DRAWITEM:
            {
                LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
                if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST) {
                    static const WCHAR* navLabels[] = {
                        L"常规",
                        L"显示",
                        L"编辑",
                        L"高级"
                    };
                    int idx = (int)lpdis->itemID;
                    if (idx >= 0 && idx < NAV_COUNT) {
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
            }
            return (INT_PTR)TRUE;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (SaveDialogControls(hDlg)) {
                        EndDialog(hDlg, IDOK);
                    }
                    return (INT_PTR)TRUE;
                    
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
                    
                case IDC_NAV_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                        int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < NAV_COUNT) {
                            ShowNavPage(hDlg, sel);
                        }
                    }
                    return (INT_PTR)TRUE;
                    
                case IDC_BROWSE_EDITOR:
                    BrowseForFile(hDlg, IDC_DEF_EDITOR, L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0");
                    return (INT_PTR)TRUE;
                    
                case IDC_BROWSE_BACKUP:
                    BrowseForFolder(hDlg, IDC_BACKUP_PATH);
                    return (INT_PTR)TRUE;
                    
                case IDC_BROWSE_LOG:
                    BrowseForFolder(hDlg, IDC_LOG_PATH);
                    return (INT_PTR)TRUE;
                    
                case IDC_MENU_BUTTON:
                    ShowPopupMenu(hDlg);
                    return (INT_PTR)TRUE;
                    
                case IDM_EXPORT_CONFIG:
                    ExportConfig(hDlg);
                    return (INT_PTR)TRUE;
                    
                case IDM_IMPORT_CONFIG:
                    ImportConfig(hDlg);
                    return (INT_PTR)TRUE;
                    
                case IDM_RESTORE_DEFAULTS:
                    ResetToDefaults(hDlg);
                    return (INT_PTR)TRUE;
                    
                case IDC_OPEN_ENV:
                    OpenSystemEnvSettings(hDlg);
                    return (INT_PTR)TRUE;
                    
                case IDC_RESET_DEFAULTS:
                    ResetToDefaults(hDlg);
                    return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

// ========== VFS 导出函数 ==========

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    // 加载配置
    LoadConfigFromRegistry();
    
    // 显示配置对话框
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_ENV_CONFIG), hWndParent, ConfigDialogProc, 0);
    
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    MessageBoxW(hWndParent,
               L"环境变量 VFS 插件 v1.0.0\n\n"
               L"(c) 2026\n\n"
               L"Windows 环境变量虚拟文件系统\n\n"
               L"功能特性:\n"
               L"- 浏览系统和用户环境变量\n"
               L"- 查看和编辑环境变量值\n"
               L"- 复制变量名和值\n"
               L"- 删除和重命名环境变量\n"
               L"- 自定义列显示\n"
               L"- 完整的配置对话框",
               L"关于 环境变量 VFS",
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
        LoadDefaultConfig();
        LoadConfigFromRegistry();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
