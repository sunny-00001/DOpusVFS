#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include <sstream>
#include <atomic>
#include <chrono>
#include <mutex>
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
#pragma comment(lib, "Ole32.lib")

static const GUID GUIDPlugin_WiFi =
{ 0xE5F6A7B8, 0xC9D0, 0x1234, { 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34 } };

#define WIFI_VFS_PREFIX L"wifi://"
#define WIFI_VFS_PREFIX_LEN 7

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

static bool IsWifiVfsPath(LPCWSTR pszPath) {
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, WIFI_VFS_PREFIX, WIFI_VFS_PREFIX_LEN) == 0;
}

static bool IsWifiRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, WIFI_VFS_PREFIX) == 0)
        return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/')
        s.pop_back();
    if (_wcsicmp(s.c_str(), L"wifi:") == 0)
        return true;
    return false;
}

static std::wstring ParseWifiName(LPCWSTR pszPath) {
    if (!IsWifiVfsPath(pszPath)) return L"";
    std::wstring path = pszPath + WIFI_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/')
        path.erase(0, 1);
    while (!path.empty() && path.back() == L'/')
        path.pop_back();
    return path;
}

static bool IsValidWifiName(const std::wstring& name) {
    if (name.empty()) return false;
    if (name.length() > 32) return false;
    for (wchar_t c : name) {
        if (c == L'"' || c == L'&' || c == L'|' || c == L'<' || c == L'>' ||
            c == L'\n' || c == L'\r' || c == L'\t' || c == L'\0') {
            return false;
        }
    }
    return true;
}

static std::wstring XmlEscape(const std::wstring& str) {
    std::wstring result;
    for (wchar_t c : str) {
        switch (c) {
            case L'<': result += L"&lt;"; break;
            case L'>': result += L"&gt;"; break;
            case L'&': result += L"&amp;"; break;
            case L'"': result += L"&quot;"; break;
            case L'\'': result += L"&apos;"; break;
            default: result += c; break;
        }
    }
    return result;
}

static std::wstring GenerateRandomTempFileName(const std::wstring& basePath) {
    GUID guid;
    CoCreateGuid(&guid);
    std::wstring fileName = basePath + L"wifi_" +
        std::to_wstring(guid.Data1) + L"_" +
        std::to_wstring(guid.Data2) + L"_" +
        std::to_wstring(guid.Data3) + L".xml";
    return fileName;
}

struct WifiProfileInfo {
    std::wstring ssid;
    std::wstring password;
    std::wstring authType;
    std::wstring cipher;
    std::wstring connectionMode;
    std::wstring connectionType;
    bool isHidden;
    bool hasPassword;
};

struct WifiFileContext {
    std::vector<BYTE> buffer;
    size_t readPos;
    bool isWrite;
    std::wstring ssid;
};

static std::vector<WifiProfileInfo> g_cachedProfiles;
static std::chrono::steady_clock::time_point g_lastCacheUpdate;
static const std::chrono::seconds CACHE_EXPIRE_TIME(30);
static std::mutex g_cacheMutex;

static std::wstring RunNetshCommand(const std::wstring& args) {
    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) return L"";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = L"cmd.exe /c chcp 65001 >nul & netsh " + args;
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    PROCESS_INFORMATION pi = { 0 };
    BOOL res = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE,
                              CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!res) {
        CloseHandle(hReadPipe);
        return L"";
    }

    std::string output;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0) {
        output.append(buf, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    if (output.empty()) {
        OutputDebugStringW(L"[WIFIVFS] RunNetshCommand: raw output is empty");
        return L"";
    }

    OutputDebugStringW((L"[WIFIVFS] RunNetshCommand: raw output length=" + std::to_wstring(output.length())).c_str());

    int wlen = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.length(), NULL, 0);
    if (wlen <= 0) {
        OutputDebugStringW(L"[WIFIVFS] RunNetshCommand: MultiByteToWideChar failed");
        return L"";
    }
    std::wstring result(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.length(), &result[0], wlen);
    return result;
}

static std::vector<std::wstring> GetWifiProfileList() {
    std::vector<std::wstring> profiles;
    std::wstring output = RunNetshCommand(L"wlan show profiles");
    if (output.empty()) {
        OutputDebugStringW(L"[WIFIVFS] GetWifiProfileList: netsh output is empty");
        return profiles;
    }

    OutputDebugStringW(L"[WIFIVFS] GetWifiProfileList: parsing output");

    std::wregex pattern(L":\\s*(\\S.*)");
    std::wistringstream stream(output);
    std::wstring line;
    std::wstring lastHeader;

    while (std::getline(stream, line)) {
        while (!line.empty() && line.back() == L'\r')
            line.pop_back();

        if (line.find(L"用户配置文件") != std::wstring::npos ||
            line.find(L"User profiles") != std::wstring::npos ||
            line.find(L"User Profiles") != std::wstring::npos) {
            lastHeader = L"profiles";
            continue;
        }
        if (line.find(L"组策略") != std::wstring::npos ||
            line.find(L"Group policy") != std::wstring::npos ||
            line.find(L"Group Policy") != std::wstring::npos) {
            lastHeader = L"policy";
            continue;
        }

        if (line.find(L'<') != std::wstring::npos && line.find(L'>') != std::wstring::npos)
            continue;

        std::wsmatch match;
        if (std::regex_search(line, match, pattern) && match.size() > 1) {
            std::wstring name = match[1].str();
            while (!name.empty() && (name.back() == L'\r' || name.back() == L'\n' || name.back() == L' '))
                name.pop_back();
            while (!name.empty() && name.front() == L' ')
                name.erase(0, 1);
            if (!name.empty()) {
                profiles.push_back(name);
                OutputDebugStringW((L"[WIFIVFS] Found profile: " + name).c_str());
            }
        }
    }

    OutputDebugStringW((L"[WIFIVFS] GetWifiProfileList: found " + std::to_wstring(profiles.size()) + L" profiles").c_str());
    return profiles;
}

static WifiProfileInfo GetWifiProfileDetail(const std::wstring& profileName) {
    WifiProfileInfo info;
    info.ssid = profileName;
    info.hasPassword = false;
    info.isHidden = false;

    if (!IsValidWifiName(profileName)) {
        OutputDebugStringW((L"[WIFIVFS] GetWifiProfileDetail: invalid profile name: " + profileName).c_str());
        return info;
    }

    std::wstring args = L"wlan show profile name=\"" + profileName + L"\" key=clear";
    std::wstring output = RunNetshCommand(args);
    if (output.empty()) return info;

    std::wistringstream stream(output);
    std::wstring line;
    std::wstring currentSection;

    while (std::getline(stream, line)) {
        while (!line.empty() && line.back() == L'\r')
            line.pop_back();

        if (line.find(L"安全设置") != std::wstring::npos ||
            line.find(L"Security settings") != std::wstring::npos) {
            currentSection = L"security";
            continue;
        }
        if (line.find(L"连接设置") != std::wstring::npos ||
            line.find(L"Connection settings") != std::wstring::npos) {
            currentSection = L"connection";
            continue;
        }

        auto findValue = [&](const std::wstring& keyCn, const std::wstring& keyEn) -> std::wstring {
            size_t pos = line.find(keyCn);
            if (pos == std::wstring::npos) pos = line.find(keyEn);
            if (pos == std::wstring::npos) return L"";
            size_t colonPos = line.find(L':', pos);
            if (colonPos == std::wstring::npos) return L"";
            std::wstring val = line.substr(colonPos + 1);
            while (!val.empty() && val.front() == L' ')
                val.erase(0, 1);
            while (!val.empty() && val.back() == L' ')
                val.pop_back();
            return val;
        };

        if (currentSection == L"security") {
            std::wstring auth = findValue(L"身份验证", L"Authentication");
            if (!auth.empty()) info.authType = auth;

            std::wstring cipher = findValue(L"加密", L"Cipher");
            if (!cipher.empty()) info.cipher = cipher;

            std::wstring key = findValue(L"安全密钥", L"Security key");
            if (!key.empty()) {
                info.hasPassword = (key == L"存在" || key == L"Present");
            }

            std::wstring pwd = findValue(L"密钥内容", L"Key Content");
            if (!pwd.empty()) {
                info.password = pwd;
                info.hasPassword = true;
            }
        }

        if (currentSection == L"connection") {
            std::wstring mode = findValue(L"连接模式", L"Connection mode");
            if (!mode.empty()) info.connectionMode = mode;

            std::wstring type = findValue(L"网络类型", L"Network type");
            if (!type.empty()) info.connectionType = type;
        }
    }

    return info;
}

static std::vector<WifiProfileInfo> GetAllWifiProfiles(bool forceRefresh = false) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    
    auto now = std::chrono::steady_clock::now();
    if (!forceRefresh && !g_cachedProfiles.empty() && 
        (now - g_lastCacheUpdate) < CACHE_EXPIRE_TIME) {
        OutputDebugStringW(L"[WIFIVFS] GetAllWifiProfiles: using cached data");
        return g_cachedProfiles;
    }

    OutputDebugStringW(L"[WIFIVFS] GetAllWifiProfiles: refreshing cache");
    g_cachedProfiles.clear();
    auto profileNames = GetWifiProfileList();
    for (const auto& name : profileNames) {
        WifiProfileInfo info = GetWifiProfileDetail(name);
        g_cachedProfiles.push_back(info);
    }
    g_lastCacheUpdate = now;
    return g_cachedProfiles;
}

static void InvalidateWifiCache() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_cachedProfiles.clear();
    OutputDebugStringW(L"[WIFIVFS] InvalidateWifiCache: cache cleared");
}

static bool FindWifiProfile(const std::wstring& ssid, WifiProfileInfo& info) {
    auto profiles = GetAllWifiProfiles();
    for (const auto& p : profiles) {
        if (_wcsicmp(p.ssid.c_str(), ssid.c_str()) == 0) {
            info = p;
            return true;
        }
    }
    return false;
}

static std::wstring FormatWifiInfo(const WifiProfileInfo& info) {
    std::wstring result;
    result += L"WiFi 名称 (SSID):\t" + info.ssid + L"\r\n";
    result += L"密码:\t\t\t" + (info.hasPassword ? info.password : L"(无密码/开放网络)") + L"\r\n";
    result += L"身份验证:\t\t" + (info.authType.empty() ? L"未知" : info.authType) + L"\r\n";
    result += L"加密方式:\t\t" + (info.cipher.empty() ? L"未知" : info.cipher) + L"\r\n";
    result += L"连接模式:\t\t" + (info.connectionMode.empty() ? L"未知" : info.connectionMode) + L"\r\n";
    result += L"网络类型:\t\t" + (info.connectionType.empty() ? L"未知" : info.connectionType) + L"\r\n";
    return result;
}

static bool SetWifiPassword(const std::wstring& ssid, const std::wstring& newPassword,
                            const std::wstring& authType = L"WPA2PSK",
                            const std::wstring& encryption = L"AES") {
    if (!IsValidWifiName(ssid)) {
        OutputDebugStringW((L"[WIFIVFS] SetWifiPassword: invalid ssid: " + ssid).c_str());
        return false;
    }

    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);

    std::wstring xmlPath = GenerateRandomTempFileName(tempPath);

    std::wstring escapedSsid = XmlEscape(ssid);
    std::wstring escapedPassword = XmlEscape(newPassword);

    std::wstring xmlContent = L"<?xml version=\"1.0\"?>\r\n"
        L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">\r\n"
        L"<name>" + escapedSsid + L"</name>\r\n"
        L"<SSIDConfig>\r\n"
        L"<SSID>\r\n"
        L"<name>" + escapedSsid + L"</name>\r\n"
        L"</SSID>\r\n"
        L"</SSIDConfig>\r\n"
        L"<connectionType>ESS</connectionType>\r\n"
        L"<connectionMode>auto</connectionMode>\r\n"
        L"<MSM>\r\n"
        L"<security>\r\n"
        L"<authEncryption>\r\n"
        L"<authentication>" + authType + L"</authentication>\r\n"
        L"<encryption>" + encryption + L"</encryption>\r\n"
        L"<useOneX>false</useOneX>\r\n"
        L"</authEncryption>\r\n"
        L"<sharedKey>\r\n"
        L"<keyType>passPhrase</keyType>\r\n"
        L"<protected>false</protected>\r\n"
        L"<keyMaterial>" + escapedPassword + L"</keyMaterial>\r\n"
        L"</sharedKey>\r\n"
        L"</security>\r\n"
        L"</MSM>\r\n"
        L"</WLANProfile>\r\n";

    FILE* f = _wfopen(xmlPath.c_str(), L"w,ccs=UTF-8");
    if (!f) return false;
    fwprintf(f, L"%s", xmlContent.c_str());
    fclose(f);

    std::wstring args = L"wlan add profile filename=\"" + xmlPath + L"\" user=all";
    std::wstring output = RunNetshCommand(args);

    DeleteFileW(xmlPath.c_str());

    return output.find(L"已添加") != std::wstring::npos ||
           output.find(L"successfully") != std::wstring::npos ||
           output.find(L"added") != std::wstring::npos;
}

static bool DeleteWifiProfile(const std::wstring& ssid) {
    if (!IsValidWifiName(ssid)) {
        OutputDebugStringW((L"[WIFIVFS] DeleteWifiProfile: invalid ssid: " + ssid).c_str());
        return false;
    }
    std::wstring args = L"wlan delete profile name=\"" + ssid + L"\"";
    std::wstring output = RunNetshCommand(args);
    return output.find(L"已删除") != std::wstring::npos ||
           output.find(L"successfully") != std::wstring::npos ||
           output.find(L"deleted") != std::wstring::npos;
}

static bool ConnectWifi(const std::wstring& ssid) {
    if (!IsValidWifiName(ssid)) {
        OutputDebugStringW((L"[WIFIVFS] ConnectWifi: invalid ssid: " + ssid).c_str());
        return false;
    }
    std::wstring args = L"wlan connect name=\"" + ssid + L"\"";
    std::wstring output = RunNetshCommand(args);
    return output.find(L"已成功") != std::wstring::npos ||
           output.find(L"successfully") != std::wstring::npos;
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str) {
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) {
    OutputDebugStringW(L"[WIFIVFS] VFS_Init called");
    if (pInitData) {
        OutputDebugStringW(L"[WIFIVFS] pInitData valid");
    } else {
        OutputDebugStringW(L"[WIFIVFS] pInitData is NULL");
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) Sleep(50);
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    OutputDebugStringW(L"[WIFIVFS] VFS_IdentifyW called");
    if (!lpVFSInfo) {
        OutputDebugStringW(L"[WIFIVFS] lpVFSInfo is NULL, returning FALSE");
        return FALSE;
    }

    OutputDebugStringW(L"[WIFIVFS] Filling plugin info");

    lpVFSInfo->idPlugin = GUIDPlugin_WiFi;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, WIFI_VFS_PREFIX);

    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"WiFi 管理");

    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
                      L"WiFi 网络管理 - 查看、编辑已保存的WiFi列表和密码");

    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");

    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"shell32.dll", 17, &hIconLarge, &hIconSmall, 1);
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    if (!lpszPrefix) return FALSE;
    if (cchPrefixMax < 9) return FALSE;
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"wifi://");
    return TRUE;
}

static VFSCUSTOMCOLUMNW g_customColumns[] = {
    { sizeof(VFSCUSTOMCOLUMNW), &g_customColumns[1], L"密码", NULL, 0, 0 },
    { sizeof(VFSCUSTOMCOLUMNW), &g_customColumns[2], L"身份验证", NULL, 0, 1 },
    { sizeof(VFSCUSTOMCOLUMNW), &g_customColumns[3], L"加密方式", NULL, 0, 2 },
    { sizeof(VFSCUSTOMCOLUMNW), &g_customColumns[4], L"连接模式", NULL, 0, 3 },
    { sizeof(VFSCUSTOMCOLUMNW), NULL, L"网络类型", NULL, 0, 4 },
};

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData) {
    return g_customColumns;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    std::vector<WifiProfileInfo> profiles = GetAllWifiProfiles();
    int numItems = (int)profiles.size();

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

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, profiles[i].ssid.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastAccessTime = lpFileData[i].wfdData.ftCreationTime;
        lpFileData[i].wfdData.ftLastWriteTime = lpFileData[i].wfdData.ftCreationTime;

        lpFileData[i].iNumColumns = 5;
        lpFileData[i].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
            5 * sizeof(VFSFILEDATACOLUMNW));

        if (lpFileData[i].lpvfsColumnData) {
            std::wstring pwdDisplay = profiles[i].hasPassword ? profiles[i].password : L"(开放)";
            if (pwdDisplay.length() > 60)
                pwdDisplay = pwdDisplay.substr(0, 60) + L"...";

            lpFileData[i].lpvfsColumnData[0].iColumnId = 0;
            lpFileData[i].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, pwdDisplay);

            lpFileData[i].lpvfsColumnData[1].iColumnId = 1;
            lpFileData[i].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap,
                profiles[i].authType.empty() ? L"未知" : profiles[i].authType);

            lpFileData[i].lpvfsColumnData[2].iColumnId = 2;
            lpFileData[i].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap,
                profiles[i].cipher.empty() ? L"未知" : profiles[i].cipher);

            lpFileData[i].lpvfsColumnData[3].iColumnId = 3;
            lpFileData[i].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap,
                profiles[i].connectionMode.empty() ? L"未知" : profiles[i].connectionMode);

            lpFileData[i].lpvfsColumnData[4].iColumnId = 4;
            lpFileData[i].lpvfsColumnData[4].lpszValue = AllocString(lpRDD->hMemHeap,
                profiles[i].connectionType.empty() ? L"未知" : profiles[i].connectionType);
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
    case VFSREAD_PRINTDIR:
        return InternalReadDirectory(lpRDD);

    default:
        return TRUE;
    }
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    std::wstring wifiName = ParseWifiName(lpszPath);

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    if (wifiName.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"WiFi 管理");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    WifiProfileInfo info;
    bool found = FindWifiProfile(wifiName, info);

    if (!found) {
        HeapFree(hHeap, 0, lpFDH);
        return NULL;
    }

    std::wstring wifiInfo = FormatWifiInfo(info);

    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, info.ssid.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    lpFileData->wfdData.nFileSizeLow = (DWORD)(wifiInfo.length() * sizeof(WCHAR));
    lpFileData->wfdData.nFileSizeHigh = 0;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
    lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
    lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;

    lpFileData->iNumColumns = 5;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        5 * sizeof(VFSFILEDATACOLUMNW));

    if (lpFileData->lpvfsColumnData) {
        std::wstring pwdDisplay = info.hasPassword ? info.password : L"(开放)";
        if (pwdDisplay.length() > 60)
            pwdDisplay = pwdDisplay.substr(0, 60) + L"...";

        lpFileData->lpvfsColumnData[0].iColumnId = 0;
        lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, pwdDisplay);

        lpFileData->lpvfsColumnData[1].iColumnId = 1;
        lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap,
            info.authType.empty() ? L"未知" : info.authType);

        lpFileData->lpvfsColumnData[2].iColumnId = 2;
        lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap,
            info.cipher.empty() ? L"未知" : info.cipher);

        lpFileData->lpvfsColumnData[3].iColumnId = 3;
        lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap,
            info.connectionMode.empty() ? L"未知" : info.connectionMode);

        lpFileData->lpvfsColumnData[4].iColumnId = 4;
        lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap,
            info.connectionType.empty() ? L"未知" : info.connectionType);
    }

    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
    DWORD dwFlags, LPFILETIME lpFT) {
    if (!IsWifiVfsPath(lpszFile)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    std::wstring wifiName = ParseWifiName(lpszFile);
    if (wifiName.empty()) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    WifiProfileInfo info;
    bool found = FindWifiProfile(wifiName, info);
    if (!found) {
        SetLastError(ERROR_OPEN_FAILED);
        return NULL;
    }

    WifiFileContext* ctx = new WifiFileContext();
    if (!ctx) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    ctx->readPos = 0;
    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;
    ctx->ssid = info.ssid;

    std::wstring wifiInfo = FormatWifiInfo(info);
    ctx->buffer.resize(wifiInfo.length() * sizeof(WCHAR));
    memcpy(ctx->buffer.data(), wifiInfo.c_str(), ctx->buffer.size());

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    WifiFileContext* ctx = (WifiFileContext*)hFile;
    if (!ctx || ctx->isWrite) return FALSE;

    if (lpdwReadSize) *lpdwReadSize = 0;
    if (ctx->readPos >= ctx->buffer.size()) return TRUE;

    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->buffer.size() - ctx->readPos));
    if (bytesToRead > 0 && lpData != NULL) {
        CopyMemory(lpData, ctx->buffer.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;
        if (lpdwReadSize) *lpdwReadSize = bytesToRead;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush,
    LPDWORD lpdwWriteSize) {
    WifiFileContext* ctx = (WifiFileContext*)hFile;
    if (!ctx || !ctx->isWrite) return FALSE;

    if (lpdwWriteSize) *lpdwWriteSize = 0;
    if (dwSize == 0 || lpData == NULL) return TRUE;

    size_t newSize = ctx->readPos + dwSize;
    if (newSize > ctx->buffer.size()) ctx->buffer.resize(newSize);

    CopyMemory(ctx->buffer.data() + ctx->readPos, lpData, dwSize);
    ctx->readPos += dwSize;
    if (lpdwWriteSize) *lpdwWriteSize = dwSize;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, __int64 iPos, DWORD dwMethod,
    DWORD dwFlags, unsigned __int64* piNewPos) {
    WifiFileContext* ctx = (WifiFileContext*)hFile;
    if (!ctx) return FALSE;

    __int64 newPos = 0;
    switch (dwMethod) {
    case FILE_BEGIN: newPos = iPos; break;
    case FILE_CURRENT: newPos = ctx->readPos + iPos; break;
    case FILE_END: newPos = ctx->buffer.size() + iPos; break;
    default: return FALSE;
    }

    if (newPos < 0) return FALSE;

    if (ctx->isWrite) {
        if ((size_t)newPos > ctx->buffer.size()) ctx->buffer.resize((size_t)newPos);
        ctx->readPos = (size_t)newPos;
    } else {
        ctx->readPos = (size_t)min(newPos, (__int64)ctx->buffer.size());
    }

    if (piNewPos) *piNewPos = ctx->readPos;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    WifiFileContext* ctx = (WifiFileContext*)hFile;
    if (!ctx) return;

    if (ctx->isWrite && ctx->buffer.size() > 0) {
        std::wstring content((WCHAR*)ctx->buffer.data(),
            ctx->buffer.size() / sizeof(WCHAR));

        std::wstring newPassword;
        size_t pwdPos = content.find(L"密码:");
        if (pwdPos == std::wstring::npos)
            pwdPos = content.find(L"密码：");
        if (pwdPos != std::wstring::npos) {
            size_t tabPos = content.find(L'\t', pwdPos);
            if (tabPos != std::wstring::npos) {
                size_t lineStart = tabPos + 1;
                size_t lineEnd = content.find(L'\r', lineStart);
                if (lineEnd == std::wstring::npos) lineEnd = content.find(L'\n', lineStart);
                if (lineEnd == std::wstring::npos) lineEnd = content.length();
                newPassword = content.substr(lineStart, lineEnd - lineStart);
                while (!newPassword.empty() && newPassword.back() == L'\t')
                    newPassword.pop_back();
                while (!newPassword.empty() && newPassword.front() == L' ')
                    newPassword.erase(0, 1);
                while (!newPassword.empty() && newPassword.back() == L' ')
                    newPassword.pop_back();
            }
        }

        if (!newPassword.empty()) {
            if (SetWifiPassword(ctx->ssid, newPassword)) {
                InvalidateWifiCache();
            }
        }
    }

    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwFlags, int iSecurePasses) {
    if (!IsWifiVfsPath(lpszFile)) return FALSE;

    std::wstring wifiName = ParseWifiName(lpszFile);
    if (wifiName.empty()) return FALSE;

    int result = MessageBoxW(NULL,
        (L"确定要忘记WiFi网络 \"" + wifiName + L"\" 吗？\n\n此操作将从系统中删除该WiFi配置文件。").c_str(),
        L"确认忘记网络", MB_YESNO | MB_ICONQUESTION);

    if (result != IDYES) return FALSE;

    if (DeleteWifiProfile(wifiName)) {
        InvalidateWifiCache();
        return TRUE;
    }

    MessageBoxW(NULL, L"删除WiFi配置文件失败，请确认您有足够的权限。", L"错误", MB_OK | MB_ICONERROR);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath,
    LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;

    if (IsWifiRootPath(lpszPath)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"WiFi 管理");
        return TRUE;
    }

    std::wstring wifiName = ParseWifiName(lpszPath);
    if (wifiName.empty()) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"WiFi 管理");
        return TRUE;
    }

    StringCchCopyW(lpszDisplayName, cbDisplayNameMax, wifiName.c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
    LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;

    if (fRoot) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, WIFI_VFS_PREFIX);
        return TRUE;
    }

    if (IsWifiRootPath(lpszPath)) return FALSE;

    StringCchCopyW(lpszNewPath, cbNewPathMax, WIFI_VFS_PREFIX);
    return TRUE;
}

static VFSCONTEXTMENUITEMW g_wifiMenuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"连接到网络", L"$wifi_connect" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"编辑密码", L"$wifi_edit_password" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"忘记网络", L"$wifi_forget" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制密码", L"$wifi_copy_password" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"显示详细信息", L"$wifi_show_details" },
};

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData) {
    if (!lpMenuData) return FALSE;

    std::wstring wifiName = ParseWifiName(lpszFiles);
    if (wifiName.empty()) {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = FALSE;
    lpMenuData->lpCustomItems = g_wifiMenuItems;
    lpMenuData->iNumCustomItems = sizeof(g_wifiMenuItems) / sizeof(g_wifiMenuItems[0]);
    lpMenuData->fFreeCustomItems = FALSE;

    return TRUE;
}

static bool CopyPasswordToClipboard(const std::wstring& password) {
    if (!OpenClipboard(NULL)) return false;
    
    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_FIXED, (password.length() + 1) * sizeof(WCHAR));
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    
    wcscpy((LPWSTR)hMem, password.c_str());
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    
    return true;
}

static INT_PTR CALLBACK WifiEditPasswordProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static std::wstring* pEditData = nullptr;
    
    switch (uMsg) {
    case WM_INITDIALOG: {
        pEditData = (std::wstring*)lParam;
        if (pEditData) {
            SetDlgItemTextW(hDlg, IDC_WIFI_PASSWORD, pEditData->c_str());
            CheckDlgButton(hDlg, IDC_SHOW_PASSWORD, BST_UNCHECKED);
            SendDlgItemMessageW(hDlg, IDC_WIFI_PASSWORD, EM_SETPASSWORDCHAR, L'*', 0);
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SHOW_PASSWORD:
            if (HIWORD(wParam) == BN_CLICKED) {
                BOOL checked = IsDlgButtonChecked(hDlg, IDC_SHOW_PASSWORD);
                SendDlgItemMessageW(hDlg, IDC_WIFI_PASSWORD, EM_SETPASSWORDCHAR, 
                    checked ? 0 : L'*', 0);
                InvalidateRect(GetDlgItem(hDlg, IDC_WIFI_PASSWORD), NULL, TRUE);
            }
            return TRUE;
        case IDOK: {
            WCHAR newPassword[256] = {0};
            GetDlgItemTextW(hDlg, IDC_WIFI_PASSWORD, newPassword, 256);
            
            if (wcslen(newPassword) == 0) {
                MessageBoxW(hDlg, L"密码不能为空", L"错误", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            
            std::wstring ssid = pEditData ? *pEditData : L"";
            if (!ssid.empty()) {
                if (SetWifiPassword(ssid, newPassword)) {
                    InvalidateWifiCache();
                    EndDialog(hDlg, IDOK);
                } else {
                    MessageBoxW(hDlg, L"更新密码失败", L"错误", MB_OK | MB_ICONERROR);
                }
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK WifiDetailsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        std::wstring* pDetails = (std::wstring*)lParam;
        if (pDetails) {
            SetDlgItemTextW(hDlg, IDC_DETAILS_TEXT, pDetails->c_str());
            delete pDetails;
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_DEFAULT;

    std::wstring wifiName = ParseWifiName(lpVerbData->lpszPath);
    if (wifiName.empty()) return VFSCVRES_DEFAULT;

    WifiProfileInfo info;
    if (!FindWifiProfile(wifiName, info)) return VFSCVRES_DEFAULT;

    if (!lpVerbData->lpszVerb) return VFSCVRES_DEFAULT;

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_connect") == 0) {
        if (ConnectWifi(wifiName)) {
            MessageBoxW(lpVerbData->hwndParent, 
                (L"已成功连接到 " + wifiName).c_str(), 
                L"WiFi 连接", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(lpVerbData->hwndParent, 
                (L"连接到 " + wifiName + L" 失败").c_str(), 
                L"WiFi 连接", MB_OK | MB_ICONERROR);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_edit_password") == 0) {
        std::wstring* pSsid = new std::wstring(wifiName);
        
        INT_PTR result = DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WIFI_EDIT), 
            lpVerbData->hwndParent, WifiEditPasswordProc, (LPARAM)pSsid);
        
        if (result == IDOK) {
            MessageBoxW(lpVerbData->hwndParent, 
                L"密码已更新", L"WiFi 编辑", MB_OK | MB_ICONINFORMATION);
        }
        
        delete pSsid;
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_forget") == 0) {
        int response = MessageBoxW(lpVerbData->hwndParent,
            (L"确定要忘记网络 \"" + wifiName + L"\" 吗？\n这将删除该WiFi配置文件。").c_str(),
            L"忘记网络", MB_YESNO | MB_ICONQUESTION);
        
        if (response == IDYES) {
            if (DeleteWifiProfile(wifiName)) {
                InvalidateWifiCache();
                MessageBoxW(lpVerbData->hwndParent,
                    (L"已忘记网络 " + wifiName).c_str(),
                    L"忘记网络", MB_OK | MB_ICONINFORMATION);
                return VFSCVRES_DEFAULT;
            } else {
                MessageBoxW(lpVerbData->hwndParent,
                    (L"忘记网络 " + wifiName + L" 失败").c_str(),
                    L"忘记网络", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_copy_password") == 0) {
        WifiProfileInfo pwdInfo = GetWifiProfileDetail(wifiName);
        if (pwdInfo.hasPassword && !pwdInfo.password.empty()) {
            CopyPasswordToClipboard(pwdInfo.password);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_show_details") == 0) {
        std::wstring* pDetails = new std::wstring(FormatWifiInfo(info));
        DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WIFI_DETAILS),
            lpVerbData->hwndParent, WifiDetailsProc, (LPARAM)pDetails);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"wifi_refresh") == 0) {
        return VFSCVRES_DEFAULT;
    }

    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;

    std::wstring wifiName = ParseWifiName(lpszFiles);
    if (wifiName.empty()) return NULL;

    WifiProfileInfo info;
    if (!FindWifiProfile(wifiName, info)) return NULL;

    std::wstring propText = FormatWifiInfo(info);
    MessageBoxW(hwndParent, propText.c_str(), (L"WiFi 属性 - " + wifiName).c_str(), MB_OK | MB_ICONINFORMATION);

    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
    LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_PROPERTIES |
            VFSFUNCAVAIL_CLIPCOPY;
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
        WifiFileContext* ctx = (WifiFileContext*)hFile;
        if (piFileSize) *piFileSize = ctx->buffer.size();
        return TRUE;
    } else if (lpszPath) {
        std::wstring wifiName = ParseWifiName(lpszPath);
        if (!wifiName.empty()) {
            WifiProfileInfo info;
            if (FindWifiProfile(wifiName, info)) {
                std::wstring wifiInfo = FormatWifiInfo(info);
                if (piFileSize) *piFileSize = wifiInfo.length() * sizeof(WCHAR);
                return TRUE;
            }
        }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) {
    return GetLastError();
}

// ============================================================================
// WiFi Configuration Dialog
// ============================================================================

struct WifiConfig {
    bool showPasswordDefault = false;
    int  refreshInterval = 10;
    bool autoConnect = false;
    bool adminWarn = true;
    bool colPassword = true;
    bool colAuth = true;
    bool colCipher = true;
    bool colMode = true;
    bool colType = true;
    int  autoConnectPolicy = 0;
    int  passwordPolicy = 0;
    int  scanInterval = 30;
    bool prioritySort = false;
};

static WifiConfig g_wifiConfig;
static int g_currentPage = 0;

struct AvailableNetwork {
    std::wstring ssid;
    int signalQuality;
    std::wstring authType;
    std::wstring cipher;
    std::wstring channel;
};

static std::vector<AvailableNetwork> g_availableNetworks;

static void LoadWifiConfig() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\WIFIVFS", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    DWORD dwVal = 0, cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ShowPasswordDefault", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.showPasswordDefault = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"RefreshInterval", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.refreshInterval = (int)dwVal;

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"AutoConnect", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.autoConnect = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"AdminWarn", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.adminWarn = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ColPassword", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.colPassword = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ColAuth", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.colAuth = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ColCipher", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.colCipher = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ColMode", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.colMode = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ColType", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.colType = (dwVal != 0);

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"AutoConnectPolicy", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.autoConnectPolicy = (int)dwVal;

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"PasswordPolicy", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.passwordPolicy = (int)dwVal;

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ScanInterval", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.scanInterval = (int)dwVal;

    cbVal = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"PrioritySort", NULL, NULL, (LPBYTE)&dwVal, &cbVal) == ERROR_SUCCESS)
        g_wifiConfig.prioritySort = (dwVal != 0);

    RegCloseKey(hKey);
}

static void SaveWifiConfig() {
    HKEY hKey = NULL;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\WIFIVFS", 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS)
        return;

    DWORD dwVal = 0;
    dwVal = g_wifiConfig.showPasswordDefault ? 1 : 0;
    RegSetValueExW(hKey, L"ShowPasswordDefault", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_wifiConfig.refreshInterval;
    RegSetValueExW(hKey, L"RefreshInterval", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.autoConnect ? 1 : 0;
    RegSetValueExW(hKey, L"AutoConnect", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.adminWarn ? 1 : 0;
    RegSetValueExW(hKey, L"AdminWarn", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.colPassword ? 1 : 0;
    RegSetValueExW(hKey, L"ColPassword", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.colAuth ? 1 : 0;
    RegSetValueExW(hKey, L"ColAuth", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.colCipher ? 1 : 0;
    RegSetValueExW(hKey, L"ColCipher", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.colMode ? 1 : 0;
    RegSetValueExW(hKey, L"ColMode", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.colType ? 1 : 0;
    RegSetValueExW(hKey, L"ColType", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_wifiConfig.autoConnectPolicy;
    RegSetValueExW(hKey, L"AutoConnectPolicy", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_wifiConfig.passwordPolicy;
    RegSetValueExW(hKey, L"PasswordPolicy", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_wifiConfig.scanInterval;
    RegSetValueExW(hKey, L"ScanInterval", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    dwVal = g_wifiConfig.prioritySort ? 1 : 0;
    RegSetValueExW(hKey, L"PrioritySort", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(DWORD));

    RegCloseKey(hKey);
}

static void ShowWifiNavPage(HWND hDlg, int page);  // forward declaration

static void SetWifiChineseText(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_SHOW_PWD_DEFAULT, L"默认显示密码");
    SetDlgItemTextW(hDlg, IDC_LBL_REFRESH, L"自动刷新间隔(秒, 0=关闭):");
    SetDlgItemTextW(hDlg, IDC_CHK_AUTO_CONNECT, L"显示自动连接选项");
    SetDlgItemTextW(hDlg, IDC_CHK_ADMIN_WARN, L"需要管理员权限时警告");

    SetDlgItemTextW(hDlg, IDC_COL_PASSWORD, L"密码列");
    SetDlgItemTextW(hDlg, IDC_COL_AUTH, L"身份验证列");
    SetDlgItemTextW(hDlg, IDC_COL_CIPHER, L"加密方式列");
    SetDlgItemTextW(hDlg, IDC_COL_MODE, L"连接模式列");
    SetDlgItemTextW(hDlg, IDC_COL_TYPE, L"网络类型列");

    SetDlgItemTextW(hDlg, IDC_LBL_AUTO_POLICY, L"自动连接策略:");
    SetDlgItemTextW(hDlg, IDC_LBL_PASSWORD_POLICY, L"密码显示策略:");
    SetDlgItemTextW(hDlg, IDC_LBL_SCAN_INTERVAL, L"网络扫描间隔(秒):");
    SetDlgItemTextW(hDlg, IDC_PRIORITY_SORT, L"按优先级排序网络");

    HWND hComboPolicy = GetDlgItem(hDlg, IDC_AUTO_CONNECT_POLICY);
    if (hComboPolicy) {
        SendMessageW(hComboPolicy, CB_ADDSTRING, 0, (LPARAM)L"仅连接已知网络");
        SendMessageW(hComboPolicy, CB_ADDSTRING, 0, (LPARAM)L"优先连接信号强的网络");
        SendMessageW(hComboPolicy, CB_ADDSTRING, 0, (LPARAM)L"自动连接所有可用网络");
    }

    HWND hComboPwd = GetDlgItem(hDlg, IDC_PASSWORD_POLICY);
    if (hComboPwd) {
        SendMessageW(hComboPwd, CB_ADDSTRING, 0, (LPARAM)L"始终隐藏密码");
        SendMessageW(hComboPwd, CB_ADDSTRING, 0, (LPARAM)L"始终显示密码");
        SendMessageW(hComboPwd, CB_ADDSTRING, 0, (LPARAM)L"按需显示密码");
    }
}

static void InitWifiDialogControls(HWND hDlg) {
    HWND hNav = GetDlgItem(hDlg, IDC_WIFI_NAV_LIST);
    if (hNav) {
        SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"常规设置");
        SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"显示列");
        SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"网络扫描");
        SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"高级设置");
        SendMessageW(hNav, LB_SETCURSEL, g_currentPage, 0);
    }

    SetWifiChineseText(hDlg);

    CheckDlgButton(hDlg, IDC_SHOW_PWD_DEFAULT,
        g_wifiConfig.showPasswordDefault ? BST_CHECKED : BST_UNCHECKED);

    SetDlgItemInt(hDlg, IDC_REFRESH_INTERVAL, g_wifiConfig.refreshInterval, FALSE);

    CheckDlgButton(hDlg, IDC_CHK_AUTO_CONNECT,
        g_wifiConfig.autoConnect ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_ADMIN_WARN,
        g_wifiConfig.adminWarn ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(hDlg, IDC_COL_PASSWORD,
        g_wifiConfig.colPassword ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_AUTH,
        g_wifiConfig.colAuth ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_CIPHER,
        g_wifiConfig.colCipher ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_MODE,
        g_wifiConfig.colMode ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_TYPE,
        g_wifiConfig.colType ? BST_CHECKED : BST_UNCHECKED);

    HWND hComboPolicy = GetDlgItem(hDlg, IDC_AUTO_CONNECT_POLICY);
    if (hComboPolicy && g_wifiConfig.autoConnectPolicy >= 0 && g_wifiConfig.autoConnectPolicy < 3) {
        SendMessageW(hComboPolicy, CB_SETCURSEL, g_wifiConfig.autoConnectPolicy, 0);
    }

    HWND hComboPwd = GetDlgItem(hDlg, IDC_PASSWORD_POLICY);
    if (hComboPwd && g_wifiConfig.passwordPolicy >= 0 && g_wifiConfig.passwordPolicy < 3) {
        SendMessageW(hComboPwd, CB_SETCURSEL, g_wifiConfig.passwordPolicy, 0);
    }

    SetDlgItemInt(hDlg, IDC_SCAN_INTERVAL, g_wifiConfig.scanInterval, FALSE);

    CheckDlgButton(hDlg, IDC_PRIORITY_SORT,
        g_wifiConfig.prioritySort ? BST_CHECKED : BST_UNCHECKED);

    ShowWifiNavPage(hDlg, g_currentPage);
}

static void SaveWifiDialogControls(HWND hDlg) {
    g_wifiConfig.showPasswordDefault =
        (IsDlgButtonChecked(hDlg, IDC_SHOW_PWD_DEFAULT) == BST_CHECKED);

    g_wifiConfig.refreshInterval = GetDlgItemInt(hDlg, IDC_REFRESH_INTERVAL, NULL, FALSE);
    if (g_wifiConfig.refreshInterval < 0) g_wifiConfig.refreshInterval = 0;
    if (g_wifiConfig.refreshInterval > 3600) g_wifiConfig.refreshInterval = 3600;

    g_wifiConfig.autoConnect =
        (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_CONNECT) == BST_CHECKED);
    g_wifiConfig.adminWarn =
        (IsDlgButtonChecked(hDlg, IDC_CHK_ADMIN_WARN) == BST_CHECKED);

    g_wifiConfig.colPassword =
        (IsDlgButtonChecked(hDlg, IDC_COL_PASSWORD) == BST_CHECKED);
    g_wifiConfig.colAuth =
        (IsDlgButtonChecked(hDlg, IDC_COL_AUTH) == BST_CHECKED);
    g_wifiConfig.colCipher =
        (IsDlgButtonChecked(hDlg, IDC_COL_CIPHER) == BST_CHECKED);
    g_wifiConfig.colMode =
        (IsDlgButtonChecked(hDlg, IDC_COL_MODE) == BST_CHECKED);
    g_wifiConfig.colType =
        (IsDlgButtonChecked(hDlg, IDC_COL_TYPE) == BST_CHECKED);

    HWND hComboPolicy = GetDlgItem(hDlg, IDC_AUTO_CONNECT_POLICY);
    if (hComboPolicy) {
        int sel = (int)SendMessageW(hComboPolicy, CB_GETCURSEL, 0, 0);
        if (sel != CB_ERR) g_wifiConfig.autoConnectPolicy = sel;
    }

    HWND hComboPwd = GetDlgItem(hDlg, IDC_PASSWORD_POLICY);
    if (hComboPwd) {
        int sel = (int)SendMessageW(hComboPwd, CB_GETCURSEL, 0, 0);
        if (sel != CB_ERR) g_wifiConfig.passwordPolicy = sel;
    }

    g_wifiConfig.scanInterval = GetDlgItemInt(hDlg, IDC_SCAN_INTERVAL, NULL, FALSE);
    if (g_wifiConfig.scanInterval < 0) g_wifiConfig.scanInterval = 0;
    if (g_wifiConfig.scanInterval > 3600) g_wifiConfig.scanInterval = 3600;

    g_wifiConfig.prioritySort =
        (IsDlgButtonChecked(hDlg, IDC_PRIORITY_SORT) == BST_CHECKED);

    SaveWifiConfig();
}

static void ScanAvailableNetworks(HWND hDlg) {
    g_availableNetworks.clear();

    std::wstring output = RunNetshCommand(L"wlan show networks mode=bssid");
    if (output.empty()) {
        SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, L"扫描失败: 无法获取网络列表");
        return;
    }

    std::wistringstream stream(output);
    std::wstring line;
    AvailableNetwork currentNet;
    bool inNetwork = false;

    std::wregex ssidPattern(L"SSID\\s+\\d+\\s*:\\s*(.+)");
    std::wregex signalPattern(L"信号\\s*:\\s*(\\d+)%");
    std::wregex authPattern(L"身份验证\\s*:\\s*(.+)");
    std::wregex cipherPattern(L"加密\\s*:\\s*(.+)");
    std::wregex channelPattern(L"频道\\s*:\\s*(\\d+)");

    while (std::getline(stream, line)) {
        while (!line.empty() && line.back() == L'\r')
            line.pop_back();

        std::wsmatch match;

        if (std::regex_search(line, match, ssidPattern) && match.size() > 1) {
            if (inNetwork && !currentNet.ssid.empty()) {
                g_availableNetworks.push_back(currentNet);
            }
            currentNet = AvailableNetwork();
            currentNet.ssid = match[1].str();
            while (!currentNet.ssid.empty() && currentNet.ssid.front() == L' ')
                currentNet.ssid.erase(0, 1);
            inNetwork = true;
        }

        if (inNetwork) {
            if (std::regex_search(line, match, signalPattern) && match.size() > 1) {
                currentNet.signalQuality = std::stoi(match[1].str());
            }
            if (std::regex_search(line, match, authPattern) && match.size() > 1) {
                currentNet.authType = match[1].str();
            }
            if (std::regex_search(line, match, cipherPattern) && match.size() > 1) {
                currentNet.cipher = match[1].str();
            }
            if (std::regex_search(line, match, channelPattern) && match.size() > 1) {
                currentNet.channel = match[1].str();
            }
        }
    }

    if (inNetwork && !currentNet.ssid.empty()) {
        g_availableNetworks.push_back(currentNet);
    }

    HWND hList = GetDlgItem(hDlg, IDC_NETWORK_LIST);
    if (hList) {
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (const auto& net : g_availableNetworks) {
            std::wstring display = net.ssid + L" (" + std::to_wstring(net.signalQuality) + L"%)";
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        }
    }

    SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, 
        (L"扫描完成: 找到 " + std::to_wstring(g_availableNetworks.size()) + L" 个网络").c_str());
}

static void ConnectToSelectedNetwork(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_NETWORK_LIST);
    if (!hList) return;

    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || sel >= (int)g_availableNetworks.size()) {
        SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, L"请先选择一个网络");
        return;
    }

    const AvailableNetwork& net = g_availableNetworks[sel];
    std::wstring args = L"wlan connect name=\"" + net.ssid + L"\"";
    std::wstring output = RunNetshCommand(args);

    if (output.find(L"已成功") != std::wstring::npos || output.find(L"successfully") != std::wstring::npos) {
        SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, (L"已连接到: " + net.ssid).c_str());
    } else {
        SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, (L"连接失败: " + net.ssid).c_str());
    }
}

static void ShowWifiNavPage(HWND hDlg, int page) {
    g_currentPage = page;

    static const int page0Ids[] = {
        IDC_GRP_GENERAL,
        IDC_SHOW_PWD_DEFAULT, IDC_LBL_REFRESH, IDC_REFRESH_INTERVAL,
        IDC_CHK_AUTO_CONNECT, IDC_CHK_ADMIN_WARN
    };
    static const int page1Ids[] = {
        IDC_GRP_DISPLAY,
        IDC_COL_PASSWORD, IDC_COL_AUTH, IDC_COL_CIPHER,
        IDC_COL_MODE, IDC_COL_TYPE
    };
    static const int page2Ids[] = {
        IDC_GRP_SCAN,
        IDC_SCAN_BUTTON, IDC_NETWORK_LIST, IDC_CONNECT_BUTTON,
        IDC_REFRESH_SCAN, IDC_SCAN_STATUS
    };
    static const int page3Ids[] = {
        IDC_GRP_ADV_SETTINGS,
        IDC_LBL_AUTO_POLICY, IDC_AUTO_CONNECT_POLICY,
        IDC_LBL_PASSWORD_POLICY, IDC_PASSWORD_POLICY,
        IDC_LBL_SCAN_INTERVAL, IDC_SCAN_INTERVAL, IDC_PRIORITY_SORT
    };

    for (int id : page0Ids)
        ShowWindow(GetDlgItem(hDlg, id), (page == 0) ? SW_SHOW : SW_HIDE);
    for (int id : page1Ids)
        ShowWindow(GetDlgItem(hDlg, id), (page == 1) ? SW_SHOW : SW_HIDE);
    for (int id : page2Ids)
        ShowWindow(GetDlgItem(hDlg, id), (page == 2) ? SW_SHOW : SW_HIDE);
    for (int id : page3Ids)
        ShowWindow(GetDlgItem(hDlg, id), (page == 3) ? SW_SHOW : SW_HIDE);
}

static void ResetToDefaults(HWND hDlg) {
    g_wifiConfig = WifiConfig();  // reset to defaults
    InitWifiDialogControls(hDlg);
    SaveWifiConfig();
}

static void OpenNetworkConfig() {
    // Try modern Settings first, fall back to ncpa.cpl
    HINSTANCE hRet = ShellExecuteW(NULL, L"open",
        L"ms-settings:network", NULL, NULL, SW_SHOWNORMAL);
    if ((uintptr_t)hRet <= 32) {
        ShellExecuteW(NULL, L"open",
            L"ncpa.cpl", NULL, NULL, SW_SHOWNORMAL);
    }
}

static INT_PTR CALLBACK WifiConfigProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        LoadWifiConfig();
        InitWifiDialogControls(hDlg);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            SaveWifiDialogControls(hDlg);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        case IDC_WIFI_APPLY:
            SaveWifiDialogControls(hDlg);
            return TRUE;

        case IDC_WIFI_RESET:
            ResetToDefaults(hDlg);
            return TRUE;

        case IDC_OPEN_NETCFG:
            OpenNetworkConfig();
            return TRUE;

        case IDC_SCAN_BUTTON:
            SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, L"正在扫描...");
            ScanAvailableNetworks(hDlg);
            return TRUE;

        case IDC_CONNECT_BUTTON:
            ConnectToSelectedNetwork(hDlg);
            return TRUE;

        case IDC_REFRESH_SCAN:
            SetDlgItemTextW(hDlg, IDC_SCAN_STATUS, L"正在刷新...");
            ScanAvailableNetworks(hDlg);
            return TRUE;

        case IDC_WIFI_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                HWND hNav = GetDlgItem(hDlg, IDC_WIFI_NAV_LIST);
                int sel = (int)SendMessageW(hNav, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                    ShowWifiNavPage(hDlg, sel);
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK WifiAboutProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WIFI_CONFIG), hWndParent, WifiConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WIFI_ABOUT), hWndParent, WifiAboutProc, 0);
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
        OutputDebugStringW(L"[WIFIVFS] DllMain DLL_PROCESS_ATTACH");
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
