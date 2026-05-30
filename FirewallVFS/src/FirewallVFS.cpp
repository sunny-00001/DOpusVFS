#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

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
#include <atomic>

#ifndef ERROR_INVALID_PATH
#define ERROR_INVALID_PATH 123L
#endif

#ifndef ERROR_OPEN_FAILED
#define ERROR_OPEN_FAILED 110L
#endif

#include <netfw.h>

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

static const GUID GUIDPlugin_Firewall =
{ 0xA1B2C3D4, 0xE5F6, 0x7890, { 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89 } };

#define FIREWALL_VFS_PREFIX L"firewall://"
#define FIREWALL_VFS_PREFIX_LEN 11
#define NUM_COLUMNS 12

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

struct FirewallRuleInfo
{
    std::wstring name;
    NET_FW_RULE_DIRECTION direction;
    NET_FW_ACTION action;
    std::wstring protocol;
    bool enabled;
    long profileTypes;
    std::wstring localPorts;
    std::wstring remotePorts;
    std::wstring localAddresses;
    std::wstring remoteAddresses;
    std::wstring program;
    std::wstring serviceName;
    std::wstring description;
    std::wstring grouping;
    std::wstring ruleId;

    FirewallRuleInfo()
    {
        direction = NET_FW_RULE_DIR_IN;
        action = NET_FW_ACTION_BLOCK;
        enabled = false;
        profileTypes = 0;
    }
};

struct FirewallFileContext
{
    std::vector<BYTE> buffer;
    size_t readPos;
    bool isWrite;
};

struct FirewallConfig
{
    bool showInbound;
    bool showOutbound;
    bool showEnabled;
    bool showDisabled;
    int filterProfile;
    bool colDirection;
    bool colAction;
    bool colProtocol;
    bool colEnabled;
    bool colProfile;
    bool colLocalPort;
    bool colRemotePort;
    bool colLocalAddr;
    bool colRemoteAddr;
    bool colProgram;
    bool colService;
    bool colDescription;
    int cacheTimeout;

    FirewallConfig()
    {
        showInbound = true;
        showOutbound = true;
        showEnabled = true;
        showDisabled = true;
        filterProfile = 0;
        colDirection = true;
        colAction = true;
        colProtocol = true;
        colEnabled = true;
        colProfile = true;
        colLocalPort = true;
        colRemotePort = true;
        colLocalAddr = false;
        colRemoteAddr = false;
        colProgram = true;
        colService = false;
        colDescription = true;
        cacheTimeout = 30;
    }
};

static FirewallConfig g_fwConfig;
static int g_currentPage = 0;

static CRITICAL_SECTION g_cacheLock;
static std::vector<FirewallRuleInfo> g_cachedRules;
static DWORD g_cacheTime = 0;
static bool g_cacheInitialized = false;

static void InitCacheLock()
{
    if (!g_cacheInitialized)
    {
        InitializeCriticalSection(&g_cacheLock);
        g_cacheInitialized = true;
    }
}

static bool GetCachedRules(std::vector<FirewallRuleInfo>& rules)
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    DWORD now = GetTickCount();
    DWORD timeout = (DWORD)(g_fwConfig.cacheTimeout * 1000);
    if (timeout == 0) timeout = 5000;
    if (!g_cachedRules.empty() && (now - g_cacheTime) < timeout)
    {
        rules = g_cachedRules;
        LeaveCriticalSection(&g_cacheLock);
        return true;
    }
    LeaveCriticalSection(&g_cacheLock);
    return false;
}

static void UpdateRuleCache(std::vector<FirewallRuleInfo>& rules)
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    g_cachedRules = rules;
    g_cacheTime = GetTickCount();
    LeaveCriticalSection(&g_cacheLock);
}

static void InvalidateRuleCache()
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    g_cachedRules.clear();
    g_cacheTime = 0;
    LeaveCriticalSection(&g_cacheLock);
}

static void LoadFwConfig();
static void SaveFwConfig();

static bool IsFirewallVfsPath(LPCWSTR pszPath)
{
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, FIREWALL_VFS_PREFIX, FIREWALL_VFS_PREFIX_LEN) == 0;
}

static bool IsFirewallRootPath(LPCWSTR pszPath)
{
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, FIREWALL_VFS_PREFIX) == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    if (_wcsicmp(s.c_str(), L"firewall:") == 0) return true;
    return false;
}

static std::wstring GetSubPath(LPCWSTR pszPath)
{
    if (!IsFirewallVfsPath(pszPath)) return L"";
    std::wstring path = pszPath + FIREWALL_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    return path;
}

static std::wstring GetDirectionName(NET_FW_RULE_DIRECTION dir)
{
    switch (dir)
    {
    case NET_FW_RULE_DIR_IN: return L"\u5165\u7ad9";
    case NET_FW_RULE_DIR_OUT: return L"\u51fa\u7ad9";
    default: return L"\u672a\u77e5";
    }
}

static std::wstring GetActionName(NET_FW_ACTION act)
{
    switch (act)
    {
    case NET_FW_ACTION_ALLOW: return L"\u5141\u8bb8";
    case NET_FW_ACTION_BLOCK: return L"\u963b\u6b62";
    default: return L"\u672a\u77e5";
    }
}

static std::wstring GetProtocolName(long protocol)
{
    switch (protocol)
    {
    case NET_FW_IP_PROTOCOL_TCP: return L"TCP";
    case NET_FW_IP_PROTOCOL_UDP: return L"UDP";
    case NET_FW_IP_PROTOCOL_ANY: return L"\u4efb\u610f";
    case 1: return L"ICMPv4";
    case 58: return L"ICMPv6";
    default: return std::to_wstring(protocol);
    }
}

static std::wstring GetProfileName(long profiles)
{
    if (profiles == 0x7FFFFFFF) return L"\u6240\u6709";
    std::wstring result;
    if (profiles & NET_FW_PROFILE2_DOMAIN) result += L"\u57df,";
    if (profiles & NET_FW_PROFILE2_PRIVATE) result += L"\u4e13\u7528,";
    if (profiles & NET_FW_PROFILE2_PUBLIC) result += L"\u516c\u7528,";
    if (!result.empty()) result.pop_back();
    return result.empty() ? L"\u65e0" : result;
}

static std::wstring BstrToWstr(BSTR bstr)
{
    if (!bstr) return L"";
    std::wstring result(bstr, SysStringLen(bstr));
    return result;
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str)
{
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

static bool EnumFirewallRules(std::vector<FirewallRuleInfo>& rules)
{
    rules.clear();

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    INetFwPolicy2* pNetFwPolicy2 = NULL;
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    INetFwRules* pFwRules = NULL;
    hr = pNetFwPolicy2->get_Rules(&pFwRules);
    if (FAILED(hr))
    {
        pNetFwPolicy2->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    IUnknown* pEnumerator = NULL;
    hr = pFwRules->get__NewEnum(&pEnumerator);
    if (SUCCEEDED(hr))
    {
        IEnumVARIANT* pEnumVariant = NULL;
        hr = pEnumerator->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pEnumVariant);
        if (SUCCEEDED(hr))
        {
            VARIANT var;
            ULONG fetched = 0;
            while (pEnumVariant->Next(1, &var, &fetched) == S_OK)
            {
                if (fetched > 0 && var.vt == VT_DISPATCH)
                {
                    INetFwRule* pFwRule = NULL;
                    hr = var.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&pFwRule);
                    if (SUCCEEDED(hr))
                    {
                        FirewallRuleInfo info;

                        BSTR bstrName = NULL;
                        pFwRule->get_Name(&bstrName);
                        info.name = BstrToWstr(bstrName);
                        SysFreeString(bstrName);

                        NET_FW_RULE_DIRECTION dir;
                        pFwRule->get_Direction(&dir);
                        info.direction = dir;

                        NET_FW_ACTION action;
                        pFwRule->get_Action(&action);
                        info.action = action;

                        long protocol;
                        pFwRule->get_Protocol(&protocol);
                        info.protocol = GetProtocolName(protocol);

                        VARIANT_BOOL enabled = VARIANT_FALSE;
                        pFwRule->get_Enabled(&enabled);
                        info.enabled = (enabled == VARIANT_TRUE);

                        long profiles = 0;
                        pFwRule->get_Profiles(&profiles);
                        info.profileTypes = profiles;

                        BSTR bstrLocalPorts = NULL;
                        pFwRule->get_LocalPorts(&bstrLocalPorts);
                        info.localPorts = BstrToWstr(bstrLocalPorts);
                        SysFreeString(bstrLocalPorts);

                        BSTR bstrRemotePorts = NULL;
                        pFwRule->get_RemotePorts(&bstrRemotePorts);
                        info.remotePorts = BstrToWstr(bstrRemotePorts);
                        SysFreeString(bstrRemotePorts);

                        BSTR bstrLocalAddrs = NULL;
                        pFwRule->get_LocalAddresses(&bstrLocalAddrs);
                        info.localAddresses = BstrToWstr(bstrLocalAddrs);
                        SysFreeString(bstrLocalAddrs);

                        BSTR bstrRemoteAddrs = NULL;
                        pFwRule->get_RemoteAddresses(&bstrRemoteAddrs);
                        info.remoteAddresses = BstrToWstr(bstrRemoteAddrs);
                        SysFreeString(bstrRemoteAddrs);

                        BSTR bstrApp = NULL;
                        pFwRule->get_ApplicationName(&bstrApp);
                        info.program = BstrToWstr(bstrApp);
                        SysFreeString(bstrApp);

                        BSTR bstrSvc = NULL;
                        pFwRule->get_ServiceName(&bstrSvc);
                        info.serviceName = BstrToWstr(bstrSvc);
                        SysFreeString(bstrSvc);

                        BSTR bstrDesc = NULL;
                        pFwRule->get_Description(&bstrDesc);
                        info.description = BstrToWstr(bstrDesc);
                        SysFreeString(bstrDesc);

                        BSTR bstrGroup = NULL;
                        pFwRule->get_Grouping(&bstrGroup);
                        info.grouping = BstrToWstr(bstrGroup);
                        SysFreeString(bstrGroup);

                        rules.push_back(std::move(info));
                        pFwRule->Release();
                    }
                }
                VariantClear(&var);
            }
            pEnumVariant->Release();
        }
        pEnumerator->Release();
    }

    pFwRules->Release();
    pNetFwPolicy2->Release();
    if (needUninitialize) CoUninitialize();

    std::sort(rules.begin(), rules.end(), [](const FirewallRuleInfo& a, const FirewallRuleInfo& b) {
        return a.name < b.name;
    });

    return true;
}

static bool FilterRule(const FirewallRuleInfo& rule)
{
    if (rule.direction == NET_FW_RULE_DIR_IN && !g_fwConfig.showInbound) return false;
    if (rule.direction == NET_FW_RULE_DIR_OUT && !g_fwConfig.showOutbound) return false;
    if (rule.enabled && !g_fwConfig.showEnabled) return false;
    if (!rule.enabled && !g_fwConfig.showDisabled) return false;

    if (g_fwConfig.filterProfile > 0)
    {
        long checkProfile = 0;
        switch (g_fwConfig.filterProfile)
        {
        case 1: checkProfile = NET_FW_PROFILE2_DOMAIN; break;
        case 2: checkProfile = NET_FW_PROFILE2_PRIVATE; break;
        case 3: checkProfile = NET_FW_PROFILE2_PUBLIC; break;
        }
        if (checkProfile != 0 && !(rule.profileTypes & checkProfile)) return false;
    }

    return true;
}

static std::wstring SanitizeFileName(const std::wstring& name)
{
    std::wstring result = name;
    const WCHAR invalidChars[] = L"\\/:*?\"<>|";
    for (size_t i = 0; i < result.length(); i++)
    {
        for (int j = 0; invalidChars[j] != L'\0'; j++)
        {
            if (result[i] == invalidChars[j])
            {
                result[i] = L'_';
                break;
            }
        }
    }
    return result;
}

static std::wstring RuleToFileName(const FirewallRuleInfo& rule)
{
    return SanitizeFileName(rule.name) + L".fwrule";
}

static bool FindRuleByFileName(const std::vector<FirewallRuleInfo>& rules, const std::wstring& fileName, FirewallRuleInfo& foundRule)
{
    for (const auto& rule : rules)
    {
        if (_wcsicmp(RuleToFileName(rule).c_str(), fileName.c_str()) == 0)
        {
            foundRule = rule;
            return true;
        }
    }
    return false;
}

static std::wstring FormatRuleInfo(const FirewallRuleInfo& info)
{
    std::wstring result;
    result += L"\u89c4\u5219\u540d\u79f0: " + info.name + L"\r\n";
    result += L"\u65b9\u5411: " + GetDirectionName(info.direction) + L"\r\n";
    result += L"\u64cd\u4f5c: " + GetActionName(info.action) + L"\r\n";
    result += L"\u534f\u8bae: " + info.protocol + L"\r\n";
    result += L"\u542f\u7528: " + std::wstring(info.enabled ? L"\u662f" : L"\u5426") + L"\r\n";
    result += L"\u914d\u7f6e\u6587\u4ef6: " + GetProfileName(info.profileTypes) + L"\r\n";
    if (!info.localPorts.empty()) result += L"\u672c\u5730\u7aef\u53e3: " + info.localPorts + L"\r\n";
    if (!info.remotePorts.empty()) result += L"\u8fdc\u7a0b\u7aef\u53e3: " + info.remotePorts + L"\r\n";
    if (!info.localAddresses.empty()) result += L"\u672c\u5730\u5730\u5740: " + info.localAddresses + L"\r\n";
    if (!info.remoteAddresses.empty()) result += L"\u8fdc\u7a0b\u5730\u5740: " + info.remoteAddresses + L"\r\n";
    if (!info.program.empty()) result += L"\u7a0b\u5e8f: " + info.program + L"\r\n";
    if (!info.serviceName.empty()) result += L"\u670d\u52a1\u540d: " + info.serviceName + L"\r\n";
    if (!info.description.empty()) result += L"\u63cf\u8ff0: " + info.description + L"\r\n";
    if (!info.grouping.empty()) result += L"\u5206\u7ec4: " + info.grouping + L"\r\n";
    return result;
}

static void SetCustomColumnsForRule(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const FirewallRuleInfo& info)
{
    if (!lpFileData || !hHeap) return;

    lpFileData->iNumColumns = NUM_COLUMNS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        NUM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));

    if (!lpFileData->lpvfsColumnData) return;

    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, GetDirectionName(info.direction));

    lpFileData->lpvfsColumnData[1].iColumnId = 2;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, GetActionName(info.action));

    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, info.protocol);

    lpFileData->lpvfsColumnData[3].iColumnId = 4;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap,
        info.enabled ? L"\u662f" : L"\u5426");

    lpFileData->lpvfsColumnData[4].iColumnId = 5;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, GetProfileName(info.profileTypes));

    lpFileData->lpvfsColumnData[5].iColumnId = 6;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, info.localPorts);

    lpFileData->lpvfsColumnData[6].iColumnId = 7;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, info.remotePorts);

    lpFileData->lpvfsColumnData[7].iColumnId = 8;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, info.localAddresses);

    lpFileData->lpvfsColumnData[8].iColumnId = 9;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, info.remoteAddresses);

    lpFileData->lpvfsColumnData[9].iColumnId = 10;
    lpFileData->lpvfsColumnData[9].lpszValue = AllocString(hHeap, info.program);

    lpFileData->lpvfsColumnData[10].iColumnId = 11;
    lpFileData->lpvfsColumnData[10].lpszValue = AllocString(hHeap, info.serviceName);

    lpFileData->lpvfsColumnData[11].iColumnId = 12;
    lpFileData->lpvfsColumnData[11].lpszValue = AllocString(hHeap, info.description);
}

static bool SetRuleEnabled(const std::wstring& ruleName, bool enable)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    INetFwPolicy2* pNetFwPolicy2 = NULL;
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    INetFwRules* pFwRules = NULL;
    hr = pNetFwPolicy2->get_Rules(&pFwRules);
    if (FAILED(hr))
    {
        pNetFwPolicy2->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    INetFwRule* pFwRule = NULL;
    BSTR bstrRuleName = SysAllocString(ruleName.c_str());
    if (bstrRuleName)
    {
        hr = pFwRules->Item(bstrRuleName, &pFwRule);
        SysFreeString(bstrRuleName);
    }
    if (SUCCEEDED(hr))
    {
        pFwRule->put_Enabled(enable ? VARIANT_TRUE : VARIANT_FALSE);
        pFwRule->Release();
    }

    pFwRules->Release();
    pNetFwPolicy2->Release();
    if (needUninitialize) CoUninitialize();

    InvalidateRuleCache();
    return SUCCEEDED(hr);
}

static bool DeleteFirewallRule(const std::wstring& ruleName)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool needUninitialize = SUCCEEDED(hr);

    INetFwPolicy2* pNetFwPolicy2 = NULL;
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
    if (FAILED(hr))
    {
        if (needUninitialize) CoUninitialize();
        return false;
    }

    INetFwRules* pFwRules = NULL;
    hr = pNetFwPolicy2->get_Rules(&pFwRules);
    if (FAILED(hr))
    {
        pNetFwPolicy2->Release();
        if (needUninitialize) CoUninitialize();
        return false;
    }

    BSTR bstrRemoveName = SysAllocString(ruleName.c_str());
    if (bstrRemoveName)
    {
        hr = pFwRules->Remove(bstrRemoveName);
        SysFreeString(bstrRemoveName);
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    pFwRules->Release();
    pNetFwPolicy2->Release();
    if (needUninitialize) CoUninitialize();

    InvalidateRuleCache();
    return SUCCEEDED(hr);
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData)
{
    InitCacheLock();
    LoadFwConfig();
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit()
{
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) Sleep(50);
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow)
{
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData)
{
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo)
{
    if (!lpVFSInfo) return FALSE;

    lpVFSInfo->idPlugin = GUIDPlugin_Firewall;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, FIREWALL_VFS_PREFIX);
    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"\u9632\u706b\u5899");
    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
                       L"Windows \u9632\u706b\u5899\u89c4\u5219 - \u6d4f\u89c8\u548c\u7ba1\u7406\u9632\u706b\u5899\u89c4\u5219");
    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");
    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"firewall.cpl", 0, &hIconLarge, &hIconSmall, 1);
    if (!hIconLarge)
    {
        SHFILEINFOW sfiL = {}, sfiS = {};
        if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiL, sizeof(sfiL),
            SHGFI_ICON | SHGFI_LARGEICON))
        {
            hIconLarge = CopyIcon(sfiL.hIcon);
            DestroyIcon(sfiL.hIcon);
        }
        if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiS, sizeof(sfiS),
            SHGFI_ICON | SHGFI_SMALLICON))
        {
            hIconSmall = CopyIcon(sfiS.hIcon);
            DestroyIcon(sfiS.hIcon);
        }
    }
    if (!hIconLarge)
    {
        ExtractIconExW(L"shell32.dll", 78, &hIconLarge, &hIconSmall, 1);
    }
    if (!hIconLarge)
    {
        SHFILEINFOW sfiL = {}, sfiS = {};
        if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiL, sizeof(sfiL),
            SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON))
        {
            hIconLarge = CopyIcon(sfiL.hIcon);
            DestroyIcon(sfiL.hIcon);
        }
        if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiS, sizeof(sfiS),
            SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON))
        {
            hIconSmall = CopyIcon(sfiS.hIcon);
            DestroyIcon(sfiS.hIcon);
        }
    }
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax)
{
    if (!lpszPrefix || cchPrefixMax < 13) return FALSE;
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"firewall://");
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData)
{
    static VFSCUSTOMCOLUMNW columns[NUM_COLUMNS];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\u65b9\u5411"; columns[0].lpszKey = L"fwdirection"; columns[0].dwFlags = VFSCCF_LEFTJUSTIFY; columns[0].iID = 1;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\u64cd\u4f5c"; columns[1].lpszKey = L"fwaction"; columns[1].dwFlags = VFSCCF_LEFTJUSTIFY; columns[1].iID = 2;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\u534f\u8bae"; columns[2].lpszKey = L"fwprotocol"; columns[2].dwFlags = VFSCCF_LEFTJUSTIFY; columns[2].iID = 3;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\u542f\u7528"; columns[3].lpszKey = L"fwenabled"; columns[3].dwFlags = VFSCCF_LEFTJUSTIFY; columns[3].iID = 4;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\u914d\u7f6e\u6587\u4ef6"; columns[4].lpszKey = L"fwprofile"; columns[4].dwFlags = VFSCCF_LEFTJUSTIFY; columns[4].iID = 5;

    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"\u672c\u5730\u7aef\u53e3"; columns[5].lpszKey = L"fwlocalport"; columns[5].dwFlags = VFSCCF_LEFTJUSTIFY; columns[5].iID = 6;

    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\u8fdc\u7a0b\u7aef\u53e3"; columns[6].lpszKey = L"fwremoteport"; columns[6].dwFlags = VFSCCF_LEFTJUSTIFY; columns[6].iID = 7;

    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[7].lpNext = &columns[8];
    columns[7].lpszLabel = L"\u672c\u5730\u5730\u5740"; columns[7].lpszKey = L"fwlocaladdr"; columns[7].dwFlags = VFSCCF_LEFTJUSTIFY; columns[7].iID = 8;

    columns[8].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[8].lpNext = &columns[9];
    columns[8].lpszLabel = L"\u8fdc\u7a0b\u5730\u5740"; columns[8].lpszKey = L"fwremoteaddr"; columns[8].dwFlags = VFSCCF_LEFTJUSTIFY; columns[8].iID = 9;

    columns[9].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[9].lpNext = &columns[10];
    columns[9].lpszLabel = L"\u7a0b\u5e8f"; columns[9].lpszKey = L"fwprogram"; columns[9].dwFlags = VFSCCF_LEFTJUSTIFY; columns[9].iID = 10;

    columns[10].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[10].lpNext = &columns[11];
    columns[10].lpszLabel = L"\u670d\u52a1\u540d"; columns[10].lpszKey = L"fwservice"; columns[10].dwFlags = VFSCCF_LEFTJUSTIFY; columns[10].iID = 11;

    columns[11].cbSize = sizeof(VFSCUSTOMCOLUMNW); columns[11].lpNext = NULL;
    columns[11].lpszLabel = L"\u63cf\u8ff0"; columns[11].lpszKey = L"fwdescription"; columns[11].dwFlags = VFSCCF_LEFTJUSTIFY; columns[11].iID = 12;

    return columns;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    std::vector<FirewallRuleInfo> allRules;
    if (!GetCachedRules(allRules))
    {
        if (!EnumFirewallRules(allRules)) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
        UpdateRuleCache(allRules);
    }

    std::vector<FirewallRuleInfo> filteredRules;
    for (const auto& rule : allRules)
    {
        if (FilterRule(rule)) filteredRules.push_back(rule);
    }

    int numItems = (int)filteredRules.size();
    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
    if (!lpFDH) return FALSE;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++)
    {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        std::wstring fileName = RuleToFileName(filteredRules[i]);
        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, fileName.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        if (!filteredRules[i].enabled) lpFileData[i].wfdData.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastAccessTime = lpFileData[i].wfdData.ftCreationTime;
        lpFileData[i].wfdData.ftLastWriteTime = lpFileData[i].wfdData.ftCreationTime;

        std::wstring ruleInfo = FormatRuleInfo(filteredRules[i]);
        lpFileData[i].wfdData.nFileSizeLow = (DWORD)(ruleInfo.length() * sizeof(WCHAR));

        SetCustomColumnsForRule(&lpFileData[i], lpRDD->hMemHeap, filteredRules[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD) return FALSE;
    switch (lpRDD->vfsReadOp)
    {
    case VFSREAD_FREEDIRCLOSE: case VFSREAD_FREEDIR: case VFSREAD_CHANGEDIR: return TRUE;
    case VFSREAD_NORMAL: case VFSREAD_REFRESH: case VFSREAD_PARENT:
    case VFSREAD_ROOT: case VFSREAD_BACK: case VFSREAD_FORWARD: case VFSREAD_PRINTDIR:
        return InternalReadDirectory(lpRDD);
    default: return TRUE;
    }
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags)
{
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    if (IsFirewallRootPath(lpszPath))
    {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u9632\u706b\u5899");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    std::wstring subPath = GetSubPath(lpszPath);
    if (subPath.empty()) { HeapFree(hHeap, 0, lpFDH); return NULL; }

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (FindRuleByFileName(rules, subPath, foundRule))
    {
        std::wstring fileName = RuleToFileName(foundRule);
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, fileName.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        if (!foundRule.enabled) lpFileData->wfdData.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;

        std::wstring ruleInfo = FormatRuleInfo(foundRule);
        lpFileData->wfdData.nFileSizeLow = (DWORD)(ruleInfo.length() * sizeof(WCHAR));
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
        lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;

        SetCustomColumnsForRule(lpFileData, hHeap, foundRule);
        return lpFDH;
    }

    HeapFree(hHeap, 0, lpFDH);
    return NULL;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT)
{
    if (!IsFirewallVfsPath(lpszFile)) { SetLastError(ERROR_PATH_NOT_FOUND); return NULL; }

    std::wstring subPath = GetSubPath(lpszFile);
    if (subPath.empty()) { SetLastError(ERROR_OPEN_FAILED); return NULL; }

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (!FindRuleByFileName(rules, subPath, foundRule)) { SetLastError(ERROR_OPEN_FAILED); return NULL; }

    FirewallFileContext* ctx = new FirewallFileContext();
    if (!ctx) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }

    ctx->readPos = 0;
    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;

    std::wstring ruleInfo = FormatRuleInfo(foundRule);
    ctx->buffer.resize(ruleInfo.length() * sizeof(WCHAR));
    memcpy(ctx->buffer.data(), ruleInfo.c_str(), ctx->buffer.size());

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize)
{
    FirewallFileContext* ctx = (FirewallFileContext*)hFile;
    if (!ctx || ctx->isWrite) return FALSE;
    if (lpdwReadSize) *lpdwReadSize = 0;
    if (ctx->readPos >= ctx->buffer.size()) return TRUE;
    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->buffer.size() - ctx->readPos));
    if (bytesToRead > 0 && lpData != NULL)
    {
        CopyMemory(lpData, ctx->buffer.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;
        if (lpdwReadSize) *lpdwReadSize = bytesToRead;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize)
{
    FirewallFileContext* ctx = (FirewallFileContext*)hFile;
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
    HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos)
{
    FirewallFileContext* ctx = (FirewallFileContext*)hFile;
    if (!ctx) return FALSE;
    __int64 newPos = 0;
    switch (dwMethod)
    {
    case FILE_BEGIN: newPos = iPos; break;
    case FILE_CURRENT: newPos = ctx->readPos + iPos; break;
    case FILE_END: newPos = ctx->buffer.size() + iPos; break;
    default: return FALSE;
    }
    if (newPos < 0) return FALSE;
    if (ctx->isWrite)
    {
        if ((size_t)newPos > ctx->buffer.size()) ctx->buffer.resize((size_t)newPos);
        ctx->readPos = (size_t)newPos;
    }
    else
    {
        ctx->readPos = (size_t)min(newPos, (__int64)ctx->buffer.size());
    }
    if (piNewPos) *piNewPos = ctx->readPos;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile)
{
    FirewallFileContext* ctx = (FirewallFileContext*)hFile;
    if (ctx) delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile)
{
    if (!IsFirewallVfsPath(lpszFile)) return FALSE;
    std::wstring subPath = GetSubPath(lpszFile);
    if (subPath.empty()) return FALSE;

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (!FindRuleByFileName(rules, subPath, foundRule)) return FALSE;

    int result = MessageBoxW(NULL,
        (L"\u786e\u5b9a\u8981\u5220\u9664\u9632\u706b\u5899\u89c4\u5219 \"" + foundRule.name + L"\" \u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002").c_str(),
        L"\u786e\u8ba4\u5220\u9664\u89c4\u5219", MB_YESNO | MB_ICONWARNING);
    if (result != IDYES) return FALSE;

    return DeleteFirewallRule(foundRule.name);
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return FALSE; }

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax)
{
    if (!lpszPath || !lpszDisplayName) return FALSE;
    if (IsFirewallRootPath(lpszPath)) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u9632\u706b\u5899"); return TRUE; }

    std::wstring subPath = GetSubPath(lpszPath);
    if (subPath.empty()) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u9632\u706b\u5899"); return TRUE; }

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (FindRuleByFileName(rules, subPath, foundRule))
    {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, foundRule.name.c_str());
        return TRUE;
    }

    StringCchCopyW(lpszDisplayName, cbDisplayNameMax, subPath.c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax)
{
    if (!lpszPath || !lpszNewPath) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, FIREWALL_VFS_PREFIX); return TRUE; }
    if (IsFirewallRootPath(lpszPath)) return FALSE;
    StringCchCopyW(lpszNewPath, cbNewPathMax, FIREWALL_VFS_PREFIX);
    return TRUE;
}

static bool GetFirewallIcons(HICON* phLargeIcon, HICON* phSmallIcon)
{
    if (!phLargeIcon && !phSmallIcon) return false;

    HICON hLarge = NULL, hSmall = NULL;
    ExtractIconExW(L"firewall.cpl", 0, &hLarge, &hSmall, 1);
    if (!hLarge)
    {
        SHFILEINFOW sfiL = {}, sfiS = {};
        if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiL, sizeof(sfiL),
            SHGFI_ICON | SHGFI_LARGEICON))
        {
            hLarge = CopyIcon(sfiL.hIcon);
            DestroyIcon(sfiL.hIcon);
        }
        if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiS, sizeof(sfiS),
            SHGFI_ICON | SHGFI_SMALLICON))
        {
            hSmall = CopyIcon(sfiS.hIcon);
            DestroyIcon(sfiS.hIcon);
        }
    }
    if (!hLarge)
    {
        ExtractIconExW(L"shell32.dll", 78, &hLarge, &hSmall, 1);
    }
    if (!hLarge)
    {
        SHFILEINFOW sfiL = {}, sfiS = {};
        if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiL, sizeof(sfiL),
            SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON))
        {
            hLarge = CopyIcon(sfiL.hIcon);
            DestroyIcon(sfiL.hIcon);
        }
        if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiS, sizeof(sfiS),
            SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON))
        {
            hSmall = CopyIcon(sfiS.hIcon);
            DestroyIcon(sfiS.hIcon);
        }
    }

    bool result = (hLarge != NULL || hSmall != NULL);
    if (phLargeIcon) *phLargeIcon = hLarge; else if (hLarge) DestroyIcon(hLarge);
    if (phSmallIcon) *phSmallIcon = hSmall; else if (hSmall) DestroyIcon(hSmall);
    return result;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileIconW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, LPINT lpiSysIconIndex, HICON* phLargeIcon, HICON* phSmallIcon,
    LPBOOL lpfDestroyIcons, LPWSTR lpszCacheName, int cchCacheNameMax, LPINT lpiCacheIndex)
{
    if (!lpszFile) return FALSE;

    if (IsFirewallRootPath(lpszFile))
    {
        if (lpiSysIconIndex)
        {
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON))
            {
                *lpiSysIconIndex = sfi.iIcon;
            }
        }
        if (phLargeIcon || phSmallIcon)
        {
            GetFirewallIcons(phLargeIcon, phSmallIcon);
        }
        if (lpfDestroyIcons) *lpfDestroyIcons = TRUE;
        if (lpszCacheName && cchCacheNameMax > 0)
            StringCchCopyW(lpszCacheName, cchCacheNameMax, L"FirewallVFS_RootIcon");
        if (lpiCacheIndex) *lpiCacheIndex = 0;
        return TRUE;
    }

    std::wstring subPath = GetSubPath(lpszFile);
    if (subPath.empty()) return FALSE;

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (!FindRuleByFileName(rules, subPath, foundRule)) return FALSE;

    if (foundRule.enabled)
    {
        if (lpiSysIconIndex)
        {
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(L".fwrule", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON))
            {
                *lpiSysIconIndex = sfi.iIcon;
            }
        }
        if (phLargeIcon)
        {
            SHFILEINFOW sfiL = {};
            if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiL, sizeof(sfiL),
                SHGFI_ICON | SHGFI_LARGEICON))
            {
                *phLargeIcon = CopyIcon(sfiL.hIcon);
                DestroyIcon(sfiL.hIcon);
            }
        }
        if (phSmallIcon)
        {
            SHFILEINFOW sfiS = {};
            if (SHGetFileInfoW(L"firewall.cpl", FILE_ATTRIBUTE_NORMAL, &sfiS, sizeof(sfiS),
                SHGFI_ICON | SHGFI_SMALLICON))
            {
                *phSmallIcon = CopyIcon(sfiS.hIcon);
                DestroyIcon(sfiS.hIcon);
            }
        }
        if (lpfDestroyIcons) *lpfDestroyIcons = TRUE;
        if (lpszCacheName && cchCacheNameMax > 0)
            StringCchCopyW(lpszCacheName, cchCacheNameMax, L"FirewallVFS_EnabledRule");
        if (lpiCacheIndex) *lpiCacheIndex = 0;
        return TRUE;
    }
    else
    {
        if (lpiSysIconIndex)
        {
            SHFILEINFOW sfi = {};
            if (SHGetFileInfoW(L".fwrule", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON))
            {
                *lpiSysIconIndex = sfi.iIcon;
            }
        }
        if (phLargeIcon)
        {
            ExtractIconExW(L"shell32.dll", 78, phLargeIcon, NULL, 1);
        }
        if (phSmallIcon)
        {
            ExtractIconExW(L"shell32.dll", 78, NULL, phSmallIcon, 1);
        }
        if (lpfDestroyIcons) *lpfDestroyIcons = TRUE;
        if (lpszCacheName && cchCacheNameMax > 0)
            StringCchCopyW(lpszCacheName, cchCacheNameMax, L"FirewallVFS_DisabledRule");
        if (lpiCacheIndex) *lpiCacheIndex = 0;
        return TRUE;
    }
}

static std::vector<std::wstring> ParseDoubleNullList(LPCWSTR lpszList)
{
    std::vector<std::wstring> result;
    if (!lpszList) return result;
    while (*lpszList != L'\0')
    {
        result.push_back(lpszList);
        lpszList += wcslen(lpszList) + 1;
    }
    return result;
}

static VFSCONTEXTMENUITEMW g_rootContextMenuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\u5237\u65b0(&R)", L"$fw_refresh" },
    { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"\u6253\u5f00\u9632\u706b\u5899\u8bbe\u7f6e(&W)", L"$fw_openwf" },
};

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpszFiles || !lpMenuData) return FALSE;

    if (IsFirewallRootPath(lpszFiles))
    {
        lpMenuData->fAllowContextMenu = TRUE;
        lpMenuData->fDefaultContextMenu = FALSE;
        lpMenuData->fCustomItemsBelow = FALSE;
        lpMenuData->lpCustomItems = g_rootContextMenuItems;
        lpMenuData->iNumCustomItems = 3;
        lpMenuData->fFreeCustomItems = FALSE;
        return TRUE;
    }

    std::vector<std::wstring> fileList = ParseDoubleNullList(lpszFiles);
    if (fileList.empty()) { lpMenuData->fAllowContextMenu = FALSE; return TRUE; }

    std::wstring firstSubPath = GetSubPath(fileList[0].c_str());
    if (firstSubPath.empty()) { lpMenuData->fAllowContextMenu = FALSE; return TRUE; }

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (!FindRuleByFileName(rules, firstSubPath, foundRule)) { lpMenuData->fAllowContextMenu = FALSE; return TRUE; }

    static VFSCONTEXTMENUITEMW items[10];
    int itemCount = 0;

    if (fileList.size() == 1)
    {
        if (foundRule.enabled)
        {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u7981\u7528\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_disable"; itemCount++;
        }
        else
        {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u542f\u7528\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_enable"; itemCount++;
        }

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5220\u9664\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_delete"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
        items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u590d\u5236\u89c4\u5219\u540d"; items[itemCount].lpszCommand = L"$fw_copyname"; itemCount++;

        if (!foundRule.program.empty())
        {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u6253\u5f00\u7a0b\u5e8f\u4f4d\u7f6e"; items[itemCount].lpszCommand = L"$fw_openlocation"; itemCount++;
        }

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5c5e\u6027"; items[itemCount].lpszCommand = L"$fw_properties"; itemCount++;
    }
    else
    {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u542f\u7528\u9009\u4e2d\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_batch_enable"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u7981\u7528\u9009\u4e2d\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_batch_disable"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5220\u9664\u9009\u4e2d\u89c4\u5219"; items[itemCount].lpszCommand = L"$fw_batch_delete"; itemCount++;
    }

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u6253\u5f00\u9632\u706b\u5899\u8bbe\u7f6e"; items[itemCount].lpszCommand = L"$fw_openwf"; itemCount++;

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = TRUE;
    lpMenuData->lpCustomItems = items;
    lpMenuData->iNumCustomItems = itemCount;
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData)
{
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"fw_refresh") == 0)
    {
        InvalidateRuleCache();
        if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        return VFSCVRES_CHANGE;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"fw_openwf") == 0)
    {
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = L"open"; sei.lpFile = L"wf.msc"; sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"fw_batch_enable") == 0)
    {
        std::vector<FirewallRuleInfo> rules;
        if (!GetCachedRules(rules)) { EnumFirewallRules(rules); UpdateRuleCache(rules); }
        int ok = 0, fail = 0;
        for (const auto& rule : rules)
        {
            if (!rule.enabled)
            {
                if (SetRuleEnabled(rule.name, true)) ok++; else fail++;
            }
        }
        if (ok > 0)
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        return VFSCVRES_FAIL;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"fw_batch_disable") == 0)
    {
        std::vector<FirewallRuleInfo> rules;
        if (!GetCachedRules(rules)) { EnumFirewallRules(rules); UpdateRuleCache(rules); }
        int ok = 0, fail = 0;
        for (const auto& rule : rules)
        {
            if (rule.enabled)
            {
                if (SetRuleEnabled(rule.name, false)) ok++; else fail++;
            }
        }
        if (ok > 0)
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        return VFSCVRES_FAIL;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"fw_batch_delete") == 0)
    {
        int result = MessageBoxW(lpVerbData->hwndParent,
            L"\u786e\u5b9a\u8981\u5220\u9664\u6240\u6709\u9009\u4e2d\u7684\u9632\u706b\u5899\u89c4\u5219\u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002",
            L"\u786e\u8ba4\u6279\u91cf\u5220\u9664", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return VFSCVRES_FAIL;

        std::vector<FirewallRuleInfo> rules;
        if (!GetCachedRules(rules)) { EnumFirewallRules(rules); UpdateRuleCache(rules); }
        int ok = 0, fail = 0;
        for (const auto& rule : rules)
        {
            if (DeleteFirewallRule(rule.name)) ok++; else fail++;
        }
        if (ok > 0)
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        return VFSCVRES_FAIL;
    }

    std::wstring subPath = GetSubPath(lpVerbData->lpszPath);
    if (subPath.empty()) return VFSCVRES_FAIL;

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (!FindRuleByFileName(rules, subPath, foundRule)) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0)
    {
        MessageBoxW(lpVerbData->hwndParent, FormatRuleInfo(foundRule).c_str(),
            (foundRule.name + L" - \u89c4\u5219\u4fe1\u606f").c_str(), MB_OK | MB_ICONINFORMATION);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_enable") == 0)
    {
        if (SetRuleEnabled(foundRule.name, true))
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u542f\u7528\u89c4\u5219\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u7ba1\u7406\u5458\u6743\u9650\u3002",
            L"\u64cd\u4f5c\u5931\u8d25", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_disable") == 0)
    {
        if (SetRuleEnabled(foundRule.name, false))
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u7981\u7528\u89c4\u5219\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u7ba1\u7406\u5458\u6743\u9650\u3002",
            L"\u64cd\u4f5c\u5931\u8d25", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_delete") == 0)
    {
        int result = MessageBoxW(lpVerbData->hwndParent,
            (L"\u786e\u5b9a\u8981\u5220\u9664\u9632\u706b\u5899\u89c4\u5219 \"" + foundRule.name + L"\" \u5417\uff1f\n\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002").c_str(),
            L"\u786e\u8ba4\u5220\u9664\u89c4\u5219", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return VFSCVRES_FAIL;

        if (DeleteFirewallRule(foundRule.name))
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u5220\u9664\u89c4\u5219\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u7ba1\u7406\u5458\u6743\u9650\u3002",
            L"\u64cd\u4f5c\u5931\u8d25", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_copyname") == 0)
    {
        if (OpenClipboard(lpVerbData->hwndParent))
        {
            EmptyClipboard();
            size_t size = (foundRule.name.length() + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem)
            {
                memcpy(GlobalLock(hMem), foundRule.name.c_str(), size);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_openlocation") == 0)
    {
        if (!foundRule.program.empty())
        {
            std::wstring dir = foundRule.program;
            size_t lastSlash = dir.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) dir = dir.substr(0, lastSlash);
            SHELLEXECUTEINFOW sei = {};
            sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_INVOKEIDLIST;
            sei.lpVerb = L"open"; sei.lpFile = dir.c_str(); sei.nShow = SW_SHOWNORMAL;
            ShellExecuteExW(&sei);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"fw_properties") == 0)
    {
        MessageBoxW(lpVerbData->hwndParent, FormatRuleInfo(foundRule).c_str(),
            (foundRule.name + L" - \u89c4\u5219\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
        return VFSCVRES_HANDLED;
    }

    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles)
{
    if (!lpszFiles) return NULL;

    std::wstring subPath = GetSubPath(lpszFiles);
    if (subPath.empty()) return NULL;

    std::vector<FirewallRuleInfo> rules;
    if (!GetCachedRules(rules))
    {
        EnumFirewallRules(rules);
        UpdateRuleCache(rules);
    }

    FirewallRuleInfo foundRule;
    if (FindRuleByFileName(rules, subPath, foundRule))
    {
        MessageBoxW(hwndParent, FormatRuleInfo(foundRule).c_str(),
            (foundRule.name + L" - \u89c4\u5219\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
    }
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3)
{
    switch (propId)
    {
    case VFSPROP_FUNCAVAILABILITY:
    {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_CLIPCOPY;
        return TRUE;
    }
    case VFSPROP_GETVALIDACTIONS: return TRUE;
    case VFSPROP_GETFOLDERICON:
    {
        HICON* phLargeIcon = (HICON*)lpData1;
        HICON* phSmallIcon = (HICON*)lpData2;
        LPBOOL pfDestroyIcons = (LPBOOL)lpData3;
        GetFirewallIcons(phLargeIcon, phSmallIcon);
        if (pfDestroyIcons) *pfDestroyIcons = TRUE;
        return (phLargeIcon && *phLargeIcon) || (phSmallIcon && *phSmallIcon) ? TRUE : FALSE;
    }
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

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize)
{
    if (hFile)
    {
        FirewallFileContext* ctx = (FirewallFileContext*)hFile;
        if (piFileSize) *piFileSize = ctx->buffer.size();
        return TRUE;
    }
    else if (lpszPath && IsFirewallVfsPath(lpszPath))
    {
        std::wstring subPath = GetSubPath(lpszPath);
        if (!subPath.empty())
        {
            std::vector<FirewallRuleInfo> rules;
            if (!GetCachedRules(rules))
            {
                EnumFirewallRules(rules);
                UpdateRuleCache(rules);
            }
            FirewallRuleInfo foundRule;
            if (FindRuleByFileName(rules, subPath, foundRule))
            {
                std::wstring ruleInfo = FormatRuleInfo(foundRule);
                if (piFileSize) *piFileSize = ruleInfo.length() * sizeof(WCHAR);
                return TRUE;
            }
        }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) { return GetLastError(); }

static bool RegReadValue(HKEY hKey, LPCWSTR name, DWORD& val)
{
    DWORD sz = sizeof(DWORD); DWORD type;
    return RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)&val, &sz) == ERROR_SUCCESS;
}

static void LoadFwConfig()
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\FirewallVFS", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD val;
        if (RegReadValue(hKey, L"ShowInbound", val)) g_fwConfig.showInbound = (val != 0);
        if (RegReadValue(hKey, L"ShowOutbound", val)) g_fwConfig.showOutbound = (val != 0);
        if (RegReadValue(hKey, L"ShowEnabled", val)) g_fwConfig.showEnabled = (val != 0);
        if (RegReadValue(hKey, L"ShowDisabled", val)) g_fwConfig.showDisabled = (val != 0);
        if (RegReadValue(hKey, L"FilterProfile", val)) g_fwConfig.filterProfile = (int)val;
        if (RegReadValue(hKey, L"ColDirection", val)) g_fwConfig.colDirection = (val != 0);
        if (RegReadValue(hKey, L"ColAction", val)) g_fwConfig.colAction = (val != 0);
        if (RegReadValue(hKey, L"ColProtocol", val)) g_fwConfig.colProtocol = (val != 0);
        if (RegReadValue(hKey, L"ColEnabled", val)) g_fwConfig.colEnabled = (val != 0);
        if (RegReadValue(hKey, L"ColProfile", val)) g_fwConfig.colProfile = (val != 0);
        if (RegReadValue(hKey, L"ColLocalPort", val)) g_fwConfig.colLocalPort = (val != 0);
        if (RegReadValue(hKey, L"ColRemotePort", val)) g_fwConfig.colRemotePort = (val != 0);
        if (RegReadValue(hKey, L"ColLocalAddr", val)) g_fwConfig.colLocalAddr = (val != 0);
        if (RegReadValue(hKey, L"ColRemoteAddr", val)) g_fwConfig.colRemoteAddr = (val != 0);
        if (RegReadValue(hKey, L"ColProgram", val)) g_fwConfig.colProgram = (val != 0);
        if (RegReadValue(hKey, L"ColService", val)) g_fwConfig.colService = (val != 0);
        if (RegReadValue(hKey, L"ColDescription", val)) g_fwConfig.colDescription = (val != 0);
        if (RegReadValue(hKey, L"CacheTimeout", val)) g_fwConfig.cacheTimeout = (int)val;
        RegCloseKey(hKey);
    }
}

static void SaveFwConfig()
{
    HKEY hKey; DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\FirewallVFS", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS)
    {
        DWORD val;
        val = g_fwConfig.showInbound ? 1 : 0; RegSetValueExW(hKey, L"ShowInbound", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.showOutbound ? 1 : 0; RegSetValueExW(hKey, L"ShowOutbound", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.showEnabled ? 1 : 0; RegSetValueExW(hKey, L"ShowEnabled", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.showDisabled ? 1 : 0; RegSetValueExW(hKey, L"ShowDisabled", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.filterProfile; RegSetValueExW(hKey, L"FilterProfile", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colDirection ? 1 : 0; RegSetValueExW(hKey, L"ColDirection", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colAction ? 1 : 0; RegSetValueExW(hKey, L"ColAction", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colProtocol ? 1 : 0; RegSetValueExW(hKey, L"ColProtocol", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colEnabled ? 1 : 0; RegSetValueExW(hKey, L"ColEnabled", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colProfile ? 1 : 0; RegSetValueExW(hKey, L"ColProfile", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colLocalPort ? 1 : 0; RegSetValueExW(hKey, L"ColLocalPort", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colRemotePort ? 1 : 0; RegSetValueExW(hKey, L"ColRemotePort", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colLocalAddr ? 1 : 0; RegSetValueExW(hKey, L"ColLocalAddr", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colRemoteAddr ? 1 : 0; RegSetValueExW(hKey, L"ColRemoteAddr", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colProgram ? 1 : 0; RegSetValueExW(hKey, L"ColProgram", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colService ? 1 : 0; RegSetValueExW(hKey, L"ColService", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.colDescription ? 1 : 0; RegSetValueExW(hKey, L"ColDescription", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_fwConfig.cacheTimeout; RegSetValueExW(hKey, L"CacheTimeout", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static const int PAGE_FILTER = 0;
static const int PAGE_DISPLAY = 1;
static const int PAGE_ADVANCED = 2;

static const int pageFilterCtrls[] = { IDC_SHOW_INBOUND, IDC_SHOW_OUTBOUND, IDC_SHOW_ENABLED, IDC_SHOW_DISABLED, IDC_LBL_FILTER_PROFILE, IDC_FILTER_PROFILE, 0 };
static const int pageDisplayCtrls[] = { IDC_LBL_COLUMNS, IDC_COL_DIRECTION, IDC_COL_ACTION, IDC_COL_PROTOCOL, IDC_COL_ENABLED, IDC_COL_PROFILE, IDC_COL_LOCALPORT, IDC_COL_REMOTEPORT, IDC_COL_LOCALADDR, IDC_COL_REMOTEADDR, IDC_COL_PROGRAM, IDC_COL_SERVICE, IDC_COL_DESCRIPTION, 0 };
static const int pageAdvancedCtrls[] = { IDC_LBL_CACHE_TIMEOUT, IDC_CACHE_TIMEOUT, 0 };

static const int* g_pageControls[] = { pageFilterCtrls, pageDisplayCtrls, pageAdvancedCtrls };

static void ShowNavPage(HWND hDlg, int page)
{
    g_currentPage = page;
    for (int i = 0; i < 3; i++)
    {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++)
        {
            HWND hwnd = GetDlgItem(hDlg, ctrls[j]);
            if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        }
    }
}

static void SetChineseText(HWND hDlg)
{
    SetWindowTextW(hDlg, L"\u9632\u706b\u5899 VFS \u914d\u7f6e");

    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList)
    {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 3; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88");
    SetDlgItemTextW(hDlg, IDC_OPEN_WF, L"\u9632\u706b\u5899\u8bbe\u7f6e");
    SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\u6062\u590d\u9ed8\u8ba4");

    SetDlgItemTextW(hDlg, IDC_SHOW_INBOUND, L"\u663e\u793a\u5165\u7ad9\u89c4\u5219");
    SetDlgItemTextW(hDlg, IDC_SHOW_OUTBOUND, L"\u663e\u793a\u51fa\u7ad9\u89c4\u5219");
    SetDlgItemTextW(hDlg, IDC_SHOW_ENABLED, L"\u663e\u793a\u5df2\u542f\u7528\u89c4\u5219");
    SetDlgItemTextW(hDlg, IDC_SHOW_DISABLED, L"\u663e\u793a\u5df2\u7981\u7528\u89c4\u5219");
    SetDlgItemTextW(hDlg, IDC_LBL_FILTER_PROFILE, L"\u7b5b\u9009\u914d\u7f6e\u6587\u4ef6:");

    SetDlgItemTextW(hDlg, IDC_LBL_COLUMNS, L"\u663e\u793a\u5217:");
    SetDlgItemTextW(hDlg, IDC_COL_DIRECTION, L"\u65b9\u5411");
    SetDlgItemTextW(hDlg, IDC_COL_ACTION, L"\u64cd\u4f5c");
    SetDlgItemTextW(hDlg, IDC_COL_PROTOCOL, L"\u534f\u8bae");
    SetDlgItemTextW(hDlg, IDC_COL_ENABLED, L"\u542f\u7528");
    SetDlgItemTextW(hDlg, IDC_COL_PROFILE, L"\u914d\u7f6e\u6587\u4ef6");
    SetDlgItemTextW(hDlg, IDC_COL_LOCALPORT, L"\u672c\u5730\u7aef\u53e3");
    SetDlgItemTextW(hDlg, IDC_COL_REMOTEPORT, L"\u8fdc\u7a0b\u7aef\u53e3");
    SetDlgItemTextW(hDlg, IDC_COL_LOCALADDR, L"\u672c\u5730\u5730\u5740");
    SetDlgItemTextW(hDlg, IDC_COL_REMOTEADDR, L"\u8fdc\u7a0b\u5730\u5740");
    SetDlgItemTextW(hDlg, IDC_COL_PROGRAM, L"\u7a0b\u5e8f");
    SetDlgItemTextW(hDlg, IDC_COL_SERVICE, L"\u670d\u52a1\u540d");
    SetDlgItemTextW(hDlg, IDC_COL_DESCRIPTION, L"\u63cf\u8ff0");

    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TIMEOUT, L"\u7f13\u5b58\u8d85\u65f6(\u79d2):");
}

static void InitDialogControls(HWND hDlg)
{
    SetChineseText(hDlg);

    CheckDlgButton(hDlg, IDC_SHOW_INBOUND, g_fwConfig.showInbound ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_OUTBOUND, g_fwConfig.showOutbound ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_ENABLED, g_fwConfig.showEnabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_DISABLED, g_fwConfig.showDisabled ? BST_CHECKED : BST_UNCHECKED);

    HWND hCombo = GetDlgItem(hDlg, IDC_FILTER_PROFILE);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u6240\u6709\u914d\u7f6e\u6587\u4ef6");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u57df");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u4e13\u7528");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u516c\u7528");
    SendMessageW(hCombo, CB_SETCURSEL, g_fwConfig.filterProfile, 0);

    CheckDlgButton(hDlg, IDC_COL_DIRECTION, g_fwConfig.colDirection ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_ACTION, g_fwConfig.colAction ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PROTOCOL, g_fwConfig.colProtocol ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_ENABLED, g_fwConfig.colEnabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PROFILE, g_fwConfig.colProfile ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_LOCALPORT, g_fwConfig.colLocalPort ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_REMOTEPORT, g_fwConfig.colRemotePort ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_LOCALADDR, g_fwConfig.colLocalAddr ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_REMOTEADDR, g_fwConfig.colRemoteAddr ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PROGRAM, g_fwConfig.colProgram ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_SERVICE, g_fwConfig.colService ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DESCRIPTION, g_fwConfig.colDescription ? BST_CHECKED : BST_UNCHECKED);

    WCHAR buf[32];
    swprintf_s(buf, L"%d", g_fwConfig.cacheTimeout);
    SetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf);

    ShowNavPage(hDlg, PAGE_FILTER);
}

static bool SaveDialogControls(HWND hDlg)
{
    g_fwConfig.showInbound = (IsDlgButtonChecked(hDlg, IDC_SHOW_INBOUND) == BST_CHECKED);
    g_fwConfig.showOutbound = (IsDlgButtonChecked(hDlg, IDC_SHOW_OUTBOUND) == BST_CHECKED);
    g_fwConfig.showEnabled = (IsDlgButtonChecked(hDlg, IDC_SHOW_ENABLED) == BST_CHECKED);
    g_fwConfig.showDisabled = (IsDlgButtonChecked(hDlg, IDC_SHOW_DISABLED) == BST_CHECKED);

    HWND hCombo = GetDlgItem(hDlg, IDC_FILTER_PROFILE);
    g_fwConfig.filterProfile = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);

    g_fwConfig.colDirection = (IsDlgButtonChecked(hDlg, IDC_COL_DIRECTION) == BST_CHECKED);
    g_fwConfig.colAction = (IsDlgButtonChecked(hDlg, IDC_COL_ACTION) == BST_CHECKED);
    g_fwConfig.colProtocol = (IsDlgButtonChecked(hDlg, IDC_COL_PROTOCOL) == BST_CHECKED);
    g_fwConfig.colEnabled = (IsDlgButtonChecked(hDlg, IDC_COL_ENABLED) == BST_CHECKED);
    g_fwConfig.colProfile = (IsDlgButtonChecked(hDlg, IDC_COL_PROFILE) == BST_CHECKED);
    g_fwConfig.colLocalPort = (IsDlgButtonChecked(hDlg, IDC_COL_LOCALPORT) == BST_CHECKED);
    g_fwConfig.colRemotePort = (IsDlgButtonChecked(hDlg, IDC_COL_REMOTEPORT) == BST_CHECKED);
    g_fwConfig.colLocalAddr = (IsDlgButtonChecked(hDlg, IDC_COL_LOCALADDR) == BST_CHECKED);
    g_fwConfig.colRemoteAddr = (IsDlgButtonChecked(hDlg, IDC_COL_REMOTEADDR) == BST_CHECKED);
    g_fwConfig.colProgram = (IsDlgButtonChecked(hDlg, IDC_COL_PROGRAM) == BST_CHECKED);
    g_fwConfig.colService = (IsDlgButtonChecked(hDlg, IDC_COL_SERVICE) == BST_CHECKED);
    g_fwConfig.colDescription = (IsDlgButtonChecked(hDlg, IDC_COL_DESCRIPTION) == BST_CHECKED);

    WCHAR buf[32];
    GetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf, 32);
    g_fwConfig.cacheTimeout = _wtoi(buf);
    if (g_fwConfig.cacheTimeout < 0) g_fwConfig.cacheTimeout = 0;

    SaveFwConfig();
    InvalidateRuleCache();
    return true;
}

static void ResetToDefaults(HWND hDlg)
{
    g_fwConfig = FirewallConfig();
    InitDialogControls(hDlg);
    MessageBoxW(hDlg, L"\u5df2\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
}

static void OpenWindowsFirewall(HWND hDlg)
{
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = L"open";
    sei.lpFile = L"wf.msc";
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

INT_PTR CALLBACK FirewallConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        LoadFwConfig();
        InitDialogControls(hDlg);
        return (INT_PTR)TRUE;

    case WM_MEASUREITEM:
    {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST)
            lpmis->itemHeight = 24;
    }
    return (INT_PTR)TRUE;

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST)
        {
            static const WCHAR* navLabels[] = { L"\u7b5b\u9009\u8bbe\u7f6e", L"\u663e\u793a\u8bbe\u7f6e", L"\u9ad8\u7ea7\u8bbe\u7f6e" };
            int idx = (int)lpdis->itemID;
            if (idx >= 0 && idx < 3)
            {
                BOOL selected = (lpdis->itemState & ODS_SELECTED);
                HBRUSH hBrush = selected ? CreateSolidBrush(RGB(0, 120, 215)) : GetSysColorBrush(COLOR_WINDOW);
                FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
                if (selected) DeleteObject(hBrush);
                SetTextColor(lpdis->hDC, selected ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RECT rcText = lpdis->rcItem; rcText.left += 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }
    return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (SaveDialogControls(hDlg)) EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        case IDC_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 3) ShowNavPage(hDlg, sel);
            }
            return (INT_PTR)TRUE;
        case IDC_RESET_DEFAULTS:
            ResetToDefaults(hDlg);
            return (INT_PTR)TRUE;
        case IDC_OPEN_WF:
            OpenWindowsFirewall(hDlg);
            return (INT_PTR)TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData)
{
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_FIREWALL_CONFIG), hWndParent, FirewallConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent)
{
    MessageBoxW(hWndParent,
        L"\u9632\u706b\u5899 VFS \u63d2\u4ef6 v1.0.0\n\n"
        L"(c) 2026\n\n"
        L"Windows \u9632\u706b\u5899\u89c4\u5219\u865a\u62df\u6587\u4ef6\u7cfb\u7edf\n\n"
        L"\u529f\u80fd\u7279\u6027\uff1a\n"
        L"- \u6d4f\u89c8\u6240\u6709\u9632\u706b\u5899\u89c4\u5219\n"
        L"- \u67e5\u770b\u89c4\u5219\u8be6\u7ec6\u4fe1\u606f\n"
        L"- \u542f\u7528/\u7981\u7528\u89c4\u5219\n"
        L"- \u5220\u9664\u89c4\u5219\n"
        L"- 12\u4e2a\u81ea\u5b9a\u4e49\u5217\n"
        L"- \u53f3\u952e\u83dc\u5355\u64cd\u4f5c\n"
        L"- \u53ef\u914d\u7f6e\u7684\u7b5b\u9009\u548c\u663e\u793a\u8bbe\u7f6e\n"
        L"- \u6253\u5f00\u9632\u706b\u5899\u7ba1\u7406\u63a7\u5236\u53f0",
        L"\u5173\u4e8e \u9632\u706b\u5899 VFS", MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
    if (pUSBSafeData) pUSBSafeData->pszOtherExports[0] = L'\0';
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
