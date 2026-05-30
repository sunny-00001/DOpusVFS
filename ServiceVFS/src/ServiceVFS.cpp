#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <algorithm>

#ifndef LPCBYTE
typedef const BYTE* LPCBYTE;
#endif

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Gdi32.lib")

#define DOPUS_PLUGIN_HELPER
#include "vfs_plugins.h"
#include "plugin_support.h"
#include "resource.h"

static HMODULE g_hModule = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) { g_hModule = hModule; DisableThreadLibraryCalls(hModule); }
    return TRUE;
}

static const GUID GUIDPlugin_Service =
{ 0x9A2B3C4D, 0x5E6F, 0x7A8B, { 0x9C, 0x0D, 0x1E, 0x2F, 0x3A, 0x4B, 0x5C, 0x6D } };

struct SERVICE_VFS_DATA {
    HWND hwndMsgWindow;
};

struct ServiceInfo {
    std::wstring name;
    std::wstring displayName;
    std::wstring status;
    std::wstring startType;
    std::wstring description;
    DWORD dwServiceType;
    DWORD dwCurrentState;
    DWORD dwControlsAccepted;
    DWORD dwWin32ExitCode;
    DWORD dwStartTypeRaw;
    DWORD dwProcessId;
};

enum {
    COL_SVC_NAME = 1,
    COL_SVC_DISPLAYNAME,
    COL_SVC_STATUS,
    COL_SVC_STARTTYPE,
    COL_SVC_TYPE,
    COL_SVC_DESCRIPTION,
    COL_SVC_PROCESSID,
    COL_SVC_EXITCODE,
};

#define NUM_CUSTOM_COLUMNS 8

static LPWSTR AllocHeapString(HANDLE hHeap, const std::wstring& str) {
    if (str.empty()) return NULL;
    LPWSTR p = (LPWSTR)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, (str.length() + 1) * sizeof(WCHAR));
    if (p) StringCchCopyW(p, str.length() + 1, str.c_str());
    return p;
}

static std::wstring GetServiceStatusCN(DWORD dwState) {
    switch (dwState) {
        case SERVICE_RUNNING: return L"\x8FD0\x884C\x4E2D";
        case SERVICE_STOPPED: return L"\x5DF2\x505C\x6B62";
        case SERVICE_PAUSED: return L"\x5DF2\x6682\x505C";
        case SERVICE_START_PENDING: return L"\x6B63\x5728\x542F\x52A8";
        case SERVICE_STOP_PENDING: return L"\x6B63\x5728\x505C\x6B62";
        case SERVICE_PAUSE_PENDING: return L"\x6B63\x5728\x6682\x505C";
        case SERVICE_CONTINUE_PENDING: return L"\x6B63\x5728\x6062\x590D";
        default: return L"\x672A\x77E5";
    }
}

static std::wstring GetServiceStartTypeCN(DWORD dwStartType) {
    switch (dwStartType) {
        case SERVICE_AUTO_START: return L"\x81EA\x52A8";
        case SERVICE_DEMAND_START: return L"\x624B\x52A8";
        case SERVICE_DISABLED: return L"\x5DF2\x7981\x7528";
        case SERVICE_BOOT_START: return L"\x7CFB\x7EDF\x542F\x52A8";
        case SERVICE_SYSTEM_START: return L"\x7CFB\x7EDF\x8BBE\x5907\x542F\x52A8";
        default: return L"\x672A\x77E5";
    }
}

static std::wstring GetServiceTypeCN(DWORD dwType) {
    switch (dwType) {
        case SERVICE_WIN32_OWN_PROCESS: return L"\x72EC\x7ACB\x8FDB\x7A0B";
        case SERVICE_WIN32_SHARE_PROCESS: return L"\x5171\x4EAB\x8FDB\x7A0B";
        case SERVICE_KERNEL_DRIVER: return L"\x5185\x6838\x9A71\x52A8";
        case SERVICE_FILE_SYSTEM_DRIVER: return L"\x6587\x4EF6\x7CFB\x7EDF\x9A71\x52A8";
        case SERVICE_INTERACTIVE_PROCESS | SERVICE_WIN32_OWN_PROCESS: return L"\x4EA4\x4E92\x5F0F\x72EC\x7ACB\x8FDB\x7A0B";
        case SERVICE_INTERACTIVE_PROCESS | SERVICE_WIN32_SHARE_PROCESS: return L"\x4EA4\x4E92\x5F0F\x5171\x4EAB\x8FDB\x7A0B";
        default: return L"\x5176\x4ED6";
    }
}

static bool EnumServices(std::vector<ServiceInfo>& services) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCManager) return false;

    DWORD dwBytesNeeded = 0, dwServicesReturned = 0, dwResumeHandle = 0;
    EnumServicesStatusExW(hSCManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                          SERVICE_STATE_ALL, NULL, 0, &dwBytesNeeded, &dwServicesReturned, &dwResumeHandle, NULL);

    if (GetLastError() != ERROR_MORE_DATA) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    LPBYTE pBuffer = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, dwBytesNeeded);
    if (!pBuffer) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    if (!EnumServicesStatusExW(hSCManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                               SERVICE_STATE_ALL, pBuffer, dwBytesNeeded, &dwBytesNeeded,
                               &dwServicesReturned, &dwResumeHandle, NULL)) {
        HeapFree(GetProcessHeap(), 0, pBuffer);
        CloseServiceHandle(hSCManager);
        return false;
    }

    LPENUM_SERVICE_STATUS_PROCESSW pServices = (LPENUM_SERVICE_STATUS_PROCESSW)pBuffer;

    for (DWORD i = 0; i < dwServicesReturned; i++) {
        ServiceInfo info;
        info.name = pServices[i].lpServiceName;
        info.displayName = pServices[i].lpDisplayName;
        info.dwCurrentState = pServices[i].ServiceStatusProcess.dwCurrentState;
        info.dwServiceType = pServices[i].ServiceStatusProcess.dwServiceType;
        info.dwControlsAccepted = pServices[i].ServiceStatusProcess.dwControlsAccepted;
        info.dwWin32ExitCode = pServices[i].ServiceStatusProcess.dwWin32ExitCode;
        info.dwProcessId = pServices[i].ServiceStatusProcess.dwProcessId;
        info.status = GetServiceStatusCN(info.dwCurrentState);

        SC_HANDLE hService = OpenServiceW(hSCManager, pServices[i].lpServiceName, SERVICE_QUERY_CONFIG);
        if (hService) {
            DWORD dwBytesNeeded = 0;
            QueryServiceConfigW(hService, NULL, 0, &dwBytesNeeded);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                LPQUERY_SERVICE_CONFIGW pConfig = (LPQUERY_SERVICE_CONFIGW)HeapAlloc(GetProcessHeap(), 0, dwBytesNeeded);
                if (pConfig && QueryServiceConfigW(hService, pConfig, dwBytesNeeded, &dwBytesNeeded)) {
                    info.dwStartTypeRaw = pConfig->dwStartType;
                    info.startType = GetServiceStartTypeCN(pConfig->dwStartType);
                }
                if (pConfig) HeapFree(GetProcessHeap(), 0, pConfig);
            }

            QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &dwBytesNeeded);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                LPSERVICE_DESCRIPTION pDesc = (LPSERVICE_DESCRIPTION)HeapAlloc(GetProcessHeap(), 0, dwBytesNeeded);
                if (pDesc && QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, (LPBYTE)pDesc, dwBytesNeeded, &dwBytesNeeded)) {
                    if (pDesc->lpDescription) {
                        info.description = pDesc->lpDescription;
                    }
                }
                if (pDesc) HeapFree(GetProcessHeap(), 0, pDesc);
            }

            CloseServiceHandle(hService);
        }

        services.push_back(info);
    }

    HeapFree(GetProcessHeap(), 0, pBuffer);
    CloseServiceHandle(hSCManager);
    return true;
}

static bool GetServiceInfo(const std::wstring& serviceName, ServiceInfo& info) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) return false;

    SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(),
                                       SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwBytesNeeded = 0;
    if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &dwBytesNeeded)) {
        info.name = serviceName;
        info.dwCurrentState = ssp.dwCurrentState;
        info.dwServiceType = ssp.dwServiceType;
        info.dwControlsAccepted = ssp.dwControlsAccepted;
        info.dwWin32ExitCode = ssp.dwWin32ExitCode;
        info.dwProcessId = ssp.dwProcessId;
        info.status = GetServiceStatusCN(ssp.dwCurrentState);
    }

    QueryServiceConfigW(hService, NULL, 0, &dwBytesNeeded);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        LPQUERY_SERVICE_CONFIGW pConfig = (LPQUERY_SERVICE_CONFIGW)HeapAlloc(GetProcessHeap(), 0, dwBytesNeeded);
        if (pConfig && QueryServiceConfigW(hService, pConfig, dwBytesNeeded, &dwBytesNeeded)) {
            info.displayName = pConfig->lpDisplayName;
            info.dwStartTypeRaw = pConfig->dwStartType;
            info.startType = GetServiceStartTypeCN(pConfig->dwStartType);
        }
        if (pConfig) HeapFree(GetProcessHeap(), 0, pConfig);
    }

    QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &dwBytesNeeded);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        LPSERVICE_DESCRIPTION pDesc = (LPSERVICE_DESCRIPTION)HeapAlloc(GetProcessHeap(), 0, dwBytesNeeded);
        if (pDesc && QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, (LPBYTE)pDesc, dwBytesNeeded, &dwBytesNeeded)) {
            if (pDesc->lpDescription) {
                info.description = pDesc->lpDescription;
            }
        }
        if (pDesc) HeapFree(GetProcessHeap(), 0, pDesc);
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

static bool ControlServiceByName(const std::wstring& serviceName, DWORD dwControl, bool isStart = false) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) return false;

    SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_START | SERVICE_STOP | SERVICE_PAUSE_CONTINUE);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS status;
    BOOL result = FALSE;

    if (isStart) {
        result = StartServiceW(hService, 0, NULL);
    } else {
        result = ControlService(hService, dwControl, &status);
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return result != FALSE;
}

static void FillColumnData(LPVFSFILEDATAW pFileData, HANDLE hHeap, const ServiceInfo& info) {
    pFileData->iNumColumns = NUM_CUSTOM_COLUMNS;
    pFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, NUM_CUSTOM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));
    if (!pFileData->lpvfsColumnData) return;

    pFileData->lpvfsColumnData[0].iColumnId = COL_SVC_NAME;
    pFileData->lpvfsColumnData[0].lpszValue = AllocHeapString(hHeap, info.name);

    pFileData->lpvfsColumnData[1].iColumnId = COL_SVC_DISPLAYNAME;
    pFileData->lpvfsColumnData[1].lpszValue = AllocHeapString(hHeap, info.displayName);

    pFileData->lpvfsColumnData[2].iColumnId = COL_SVC_STATUS;
    pFileData->lpvfsColumnData[2].lpszValue = AllocHeapString(hHeap, info.status);

    pFileData->lpvfsColumnData[3].iColumnId = COL_SVC_STARTTYPE;
    pFileData->lpvfsColumnData[3].lpszValue = AllocHeapString(hHeap, info.startType);

    pFileData->lpvfsColumnData[4].iColumnId = COL_SVC_TYPE;
    pFileData->lpvfsColumnData[4].lpszValue = AllocHeapString(hHeap, GetServiceTypeCN(info.dwServiceType));

    pFileData->lpvfsColumnData[5].iColumnId = COL_SVC_DESCRIPTION;
    pFileData->lpvfsColumnData[5].lpszValue = AllocHeapString(hHeap, info.description);

    WCHAR buf[32];
    StringCchPrintfW(buf, 32, L"%u", info.dwProcessId);
    pFileData->lpvfsColumnData[6].iColumnId = COL_SVC_PROCESSID;
    pFileData->lpvfsColumnData[6].lpszValue = AllocHeapString(hHeap, buf);

    StringCchPrintfW(buf, 32, L"%u", info.dwWin32ExitCode);
    pFileData->lpvfsColumnData[7].iColumnId = COL_SVC_EXITCODE;
    pFileData->lpvfsColumnData[7].lpszValue = AllocHeapString(hHeap, buf);
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) { return TRUE; }

extern "C" __declspec(dllexport) void VFS_Uninit() {}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    SERVICE_VFS_DATA* pData = new SERVICE_VFS_DATA;
    pData->hwndMsgWindow = hwndMsgWindow;
    return (HANDLE)pData;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
    if (hVFSData) delete (SERVICE_VFS_DATA*)hVFSData;
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    lpVFSInfo->idPlugin = GUIDPlugin_Service;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;

    lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE;
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;

    if (lpVFSInfo->lpszHandlePrefix) StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"service://");
    if (lpVFSInfo->lpszName) StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"ServiceVFS");

    if (lpVFSInfo->lpszDescription) {
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
            L"Windows Service Management Plugin");
    }

    HICON hLarge = NULL, hSmall = NULL;
    ExtractIconExW(L"shell32.dll", 15, &hLarge, &hSmall, 1);
    lpVFSInfo->hIconLarge = hLarge;
    lpVFSInfo->hIconSmall = hSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hData) {
    static VFSCUSTOMCOLUMNW columns[NUM_CUSTOM_COLUMNS];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\x670D\x52A1\x540D\x79F0";
    columns[0].lpszKey = L"svcName";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = COL_SVC_NAME;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\x663E\x793A\x540D\x79F0";
    columns[1].lpszKey = L"svcDisplayName";
    columns[1].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[1].iID = COL_SVC_DISPLAYNAME;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\x72B6\x6001";
    columns[2].lpszKey = L"svcStatus";
    columns[2].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[2].iID = COL_SVC_STATUS;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\x542F\x52A8\x7C7B\x578B";
    columns[3].lpszKey = L"svcStartType";
    columns[3].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[3].iID = COL_SVC_STARTTYPE;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\x7C7B\x578B";
    columns[4].lpszKey = L"svcType";
    columns[4].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[4].iID = COL_SVC_TYPE;

    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"\x63CF\x8FF0";
    columns[5].lpszKey = L"svcDescription";
    columns[5].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[5].iID = COL_SVC_DESCRIPTION;

    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\x8FDB\x7A0B ID";
    columns[6].lpszKey = L"svcProcessId";
    columns[6].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[6].iID = COL_SVC_PROCESSID;

    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[7].lpNext = NULL;
    columns[7].lpszLabel = L"\x9000\x51FA\x4EE3\x7801";
    columns[7].lpszKey = L"svcExitCode";
    columns[7].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[7].iID = COL_SVC_EXITCODE;

    return columns;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
        case VFSPROP_FUNCAVAILABILITY: {
            unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
            *pAvail = VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_CLIPCOPY;
            return TRUE;
        }
        case VFSPROP_GETVALIDACTIONS: {
            return TRUE;
        }
        case VFSPROP_CANSHOWSUBFOLDERS:
            *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
            return TRUE;
        case VFSPROP_SHOWTHUMBNAILS:
            *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
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

static std::wstring GetServiceNameFromPath(LPCWSTR lpszPath) {
    if (!lpszPath) return L"";
    std::wstring path(lpszPath);
    if (path.length() <= 9) return L"";
    std::wstring serviceName = path.substr(9);
    while (!serviceName.empty() && serviceName.front() == L'/') serviceName.erase(0, 1);
    while (!serviceName.empty() && serviceName.back() == L'/') serviceName.pop_back();
    return serviceName;
}

static bool SetServiceStartType(const std::wstring& serviceName, DWORD dwStartType) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) return false;

    SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_CHANGE_CONFIG);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    BOOL result = ChangeServiceConfigW(hService,
        SERVICE_NO_CHANGE, dwStartType, SERVICE_NO_CHANGE,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return result != FALSE;
}

static bool DeleteServiceByName(const std::wstring& serviceName) {
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) return false;

    SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), DELETE);
    if (!hService) {
        CloseServiceHandle(hSCManager);
        return false;
    }

    BOOL result = DeleteService(hService);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return result != FALSE;
}

static std::wstring FormatServiceInfo(const ServiceInfo& info) {
    std::wstring result;
    result += L"\u670D\u52A1\u540D\u79F0: " + info.name + L"\r\n";
    result += L"\u663E\u793A\u540D\u79F0: " + info.displayName + L"\r\n";
    result += L"\u72B6\u6001: " + info.status + L"\r\n";
    result += L"\u542F\u52A8\u7C7B\u578B: " + info.startType + L"\r\n";
    result += L"\u7C7B\u578B: " + GetServiceTypeCN(info.dwServiceType) + L"\r\n";
    if (!info.description.empty())
        result += L"\u63CF\u8FF0: " + info.description + L"\r\n";
    WCHAR buf[32];
    StringCchPrintfW(buf, 32, L"%u", info.dwWin32ExitCode);
    result += L"\u9000\u51FA\u4EE3\u7801: " + std::wstring(buf) + L"\r\n";
    return result;
}

static BOOL RunOpusCommand(LPCWSTR lpszCommand) {
    WCHAR opusDir[MAX_PATH] = {};
    HMODULE hOpus = GetModuleHandleW(NULL);
    if (hOpus) {
        GetModuleFileNameW(hOpus, opusDir, MAX_PATH);
        WCHAR* lastSlash = wcsrchr(opusDir, L'\\');
        if (lastSlash) *lastSlash = L'\0';
    }
    if (opusDir[0] == L'\0') {
        DWORD size = MAX_PATH * sizeof(WCHAR);
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\GPSoftware\\Directory Opus", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
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

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    BOOL result = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return result;
}

static void CopyToClipboard(HWND hwndOwner, const std::wstring& text) {
    if (OpenClipboard(hwndOwner)) {
        EmptyClipboard();
        size_t size = (text.length() + 1) * sizeof(WCHAR);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), size);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpszFiles || !lpMenuData) return FALSE;

    std::wstring serviceName = GetServiceNameFromPath(lpszFiles);
    if (serviceName.empty()) {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    static VFSCONTEXTMENUITEMW items[24];
    SecureZeroMemory(items, sizeof(items));
    int itemCount = 0;

    ServiceInfo info;
    bool found = GetServiceInfo(serviceName, info);

    if (found) {
        DWORD state = info.dwCurrentState;

        if (state == SERVICE_STOPPED) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u542F\u52A8\u670D\u52A1"; items[itemCount].lpszCommand = L"$start"; itemCount++;
        }

        if (state == SERVICE_RUNNING) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u505C\u6B62\u670D\u52A1"; items[itemCount].lpszCommand = L"$stop"; itemCount++;

            if (info.dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) {
                items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
                items[itemCount].lpszLabel = L"\u6682\u505C\u670D\u52A1"; items[itemCount].lpszCommand = L"$pause"; itemCount++;
            }

            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u91CD\u65B0\u542F\u52A8"; items[itemCount].lpszCommand = L"$restart"; itemCount++;
        }

        if (state == SERVICE_PAUSED) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u7EE7\u7EED\u670D\u52A1"; items[itemCount].lpszCommand = L"$continue"; itemCount++;

            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u505C\u6B62\u670D\u52A1"; items[itemCount].lpszCommand = L"$stop"; itemCount++;

            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u91CD\u65B0\u542F\u52A8"; items[itemCount].lpszCommand = L"$restart"; itemCount++;
        }

        if (state == SERVICE_STOP_PENDING || state == SERVICE_START_PENDING ||
            state == SERVICE_PAUSE_PENDING || state == SERVICE_CONTINUE_PENDING) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_DISABLED;
            items[itemCount].lpszLabel = L"\u670D\u52A1\u6B63\u5728\u8FC7\u6E21\u4E2D..."; items[itemCount].lpszCommand = L""; itemCount++;
        }
    }

    if (found) {
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
        items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_BEGINSUBMENU;
        items[itemCount].lpszLabel = L"\u542F\u52A8\u7C7B\u578B"; items[itemCount].lpszCommand = L"starttype_submenu"; itemCount++;

        if (info.dwStartTypeRaw == SERVICE_AUTO_START) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_CHECKED | VFSCMF_RADIOCHECK;
        } else {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK;
        }
        items[itemCount].lpszLabel = L"\u81EA\u52A8"; items[itemCount].lpszCommand = L"$setauto"; itemCount++;

        if (info.dwStartTypeRaw == SERVICE_DEMAND_START) {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_CHECKED | VFSCMF_RADIOCHECK;
        } else {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK;
        }
        items[itemCount].lpszLabel = L"\u624B\u52A8"; items[itemCount].lpszCommand = L"$setmanual"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_ENDSUBMENU;
        items[itemCount].lpszLabel = L"\u7981\u7528"; items[itemCount].lpszCommand = L"$setdisabled"; itemCount++;
    }

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_BEGINSUBMENU;
    items[itemCount].lpszLabel = L"\u590D\u5236\u670D\u52A1"; items[itemCount].lpszCommand = L"copy_submenu"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u670D\u52A1\u540D\u79F0"; items[itemCount].lpszCommand = L"$copy_name"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u663E\u793A\u540D\u79F0"; items[itemCount].lpszCommand = L"$copy_displayname"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u72B6\u6001"; items[itemCount].lpszCommand = L"$copy_status"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u542F\u52A8\u7C7B\u578B"; items[itemCount].lpszCommand = L"$copy_starttype"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u7C7B\u578B"; items[itemCount].lpszCommand = L"$copy_type"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u63CF\u8FF0"; items[itemCount].lpszCommand = L"$copy_description"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u8FDB\u7A0B ID"; items[itemCount].lpszCommand = L"$copy_pid"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_ENDSUBMENU;
    items[itemCount].lpszLabel = L"\u9000\u51FA\u4EE3\u7801"; items[itemCount].lpszCommand = L"$copy_exitcode"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u5C5E\u6027"; items[itemCount].lpszCommand = L"$svc_properties"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u5220\u9664\u670D\u52A1"; items[itemCount].lpszCommand = L"$delete"; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = NULL; items[itemCount].lpszCommand = NULL; itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"\u6253\u5F00\u670D\u52A1\u7BA1\u7406\u5668"; items[itemCount].lpszCommand = L"$svc_msc"; itemCount++;

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = TRUE;
    lpMenuData->lpCustomItems = items;
    lpMenuData->iNumCustomItems = itemCount;
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    std::wstring serviceName = GetServiceNameFromPath(lpVerbData->lpszPath);
    if (serviceName.empty()) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb == NULL || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) {
            MessageBoxW(lpVerbData->hwndParent, FormatServiceInfo(info).c_str(),
                (info.displayName + L" - \u670D\u52A1\u4FE1\u606F").c_str(), MB_OK | MB_ICONINFORMATION);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"start") == 0) {
        if (ControlServiceByName(serviceName, 0, true)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u542F\u52A8\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"stop") == 0) {
        ServiceInfo info;
        GetServiceInfo(serviceName, info);
        std::wstring msg = L"\u786E\u5B9A\u8981\u505C\u6B62\u670D\u52A1 \"" + info.displayName + L"\" \u5417\uFF1F";
        if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786E\u8BA4\u505C\u6B62\u670D\u52A1", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return VFSCVRES_FAIL;
        if (ControlServiceByName(serviceName, SERVICE_CONTROL_STOP)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u505C\u6B62\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"pause") == 0) {
        if (ControlServiceByName(serviceName, SERVICE_CONTROL_PAUSE)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u6682\u505C\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"continue") == 0) {
        if (ControlServiceByName(serviceName, SERVICE_CONTROL_CONTINUE)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u7EE7\u7EED\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"restart") == 0) {
        ServiceInfo info;
        GetServiceInfo(serviceName, info);
        std::wstring msg = L"\u786E\u5B9A\u8981\u91CD\u65B0\u542F\u52A8\u670D\u52A1 \"" + info.displayName + L"\" \u5417\uFF1F";
        if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786E\u8BA4\u91CD\u65B0\u542F\u52A8", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return VFSCVRES_FAIL;
        ControlServiceByName(serviceName, SERVICE_CONTROL_STOP);
        Sleep(1500);
        if (ControlServiceByName(serviceName, 0, true)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u91CD\u65B0\u542F\u52A8\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"setauto") == 0) {
        if (SetServiceStartType(serviceName, SERVICE_AUTO_START)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u4FEE\u6539\u542F\u52A8\u7C7B\u578B\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"setmanual") == 0) {
        if (SetServiceStartType(serviceName, SERVICE_DEMAND_START)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u4FEE\u6539\u542F\u52A8\u7C7B\u578B\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"setdisabled") == 0) {
        ServiceInfo info;
        GetServiceInfo(serviceName, info);
        std::wstring msg = L"\u786E\u5B9A\u8981\u7981\u7528\u670D\u52A1 \"" + info.displayName + L"\" \u5417\uFF1F\n\u7981\u7528\u540E\u670D\u52A1\u5C06\u65E0\u6CD5\u542F\u52A8\u3002";
        if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786E\u8BA4\u7981\u7528\u670D\u52A1", MB_YESNO | MB_ICONWARNING) != IDYES)
            return VFSCVRES_FAIL;
        if (SetServiceStartType(serviceName, SERVICE_DISABLED)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u4FEE\u6539\u542F\u52A8\u7C7B\u578B\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"delete") == 0) {
        ServiceInfo info;
        GetServiceInfo(serviceName, info);
        std::wstring msg = L"\u786E\u5B9A\u8981\u5220\u9664\u670D\u52A1 \"" + info.displayName + L"\" (" + serviceName + L") \u5417\uFF1F\n\u6B64\u64CD\u4F5C\u4E0D\u53EF\u64A4\u9500\uFF01";
        if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786E\u8BA4\u5220\u9664\u670D\u52A1", MB_YESNO | MB_ICONWARNING) != IDYES)
            return VFSCVRES_FAIL;
        if (DeleteServiceByName(serviceName)) {
            RunOpusCommand(L"Go REFRESH");
            return VFSCVRES_HANDLED;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u5220\u9664\u670D\u52A1\u5931\u8D25\uFF0C\u8BF7\u786E\u4FDD\u6709\u8DB3\u591F\u6743\u9650\u3002", L"\u670D\u52A1\u63A7\u5236\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"svc_properties") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) {
            MessageBoxW(lpVerbData->hwndParent, FormatServiceInfo(info).c_str(),
                (info.displayName + L" - \u670D\u52A1\u5C5E\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"svc_msc") == 0) {
        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"open";
        sei.lpFile = L"services.msc";
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_name") == 0) {
        CopyToClipboard(lpVerbData->hwndParent, serviceName);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_displayname") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) CopyToClipboard(lpVerbData->hwndParent, info.displayName);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_status") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) CopyToClipboard(lpVerbData->hwndParent, info.status);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_starttype") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) CopyToClipboard(lpVerbData->hwndParent, info.startType);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_type") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) CopyToClipboard(lpVerbData->hwndParent, GetServiceTypeCN(info.dwServiceType));
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_description") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) CopyToClipboard(lpVerbData->hwndParent, info.description);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_pid") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) {
            WCHAR buf[32];
            StringCchPrintfW(buf, 32, L"%u", info.dwProcessId);
            CopyToClipboard(lpVerbData->hwndParent, buf);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_exitcode") == 0) {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info)) {
            WCHAR buf[32];
            StringCchPrintfW(buf, 32, L"%u", info.dwWin32ExitCode);
            CopyToClipboard(lpVerbData->hwndParent, buf);
        }
        return VFSCVRES_HANDLED;
    }

    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName || cbDisplayNameMax <= 0) return FALSE;
    
    std::wstring path(lpszPath);
    
    if (path.empty() || path == L"service://" || path.length() <= 9) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Services");
        return TRUE;
    }
    
    std::wstring serviceName = path.substr(9);
    while (!serviceName.empty() && serviceName.front() == L'/') {
        serviceName.erase(0, 1);
    }
    
    if (serviceName.empty()) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Services");
    } else {
        ServiceInfo info;
        if (GetServiceInfo(serviceName, info) && !info.displayName.empty()) {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, info.displayName.c_str());
        } else {
            StringCchCopyW(lpszDisplayName, cbDisplayNameMax, serviceName.c_str());
        }
    }
    
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    std::wstring path(lpszPath);
    if (path.length() <= 9) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    std::wstring serviceName = path.substr(9);
    ServiceInfo info;
    if (!GetServiceInfo(serviceName, info)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, serviceName.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

    FillColumnData(lpFileData, hHeap, info);

    return lpFDH;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPDWORD lpdwAttr) {
    if (!lpszPath || !lpdwAttr) return FALSE;
    std::wstring path(lpszPath);
    if (path.length() <= 9) {
        *lpdwAttr = FILE_ATTRIBUTE_DIRECTORY;
        return TRUE;
    }
    *lpdwAttr = FILE_ATTRIBUTE_NORMAL;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || lpRDD->vfsReadOp == VFSREAD_FREEDIR) return TRUE;

    if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR || lpRDD->vfsReadOp == VFSREAD_NORMAL || lpRDD->vfsReadOp == VFSREAD_REFRESH) {
        std::wstring path(lpRDD->lpszPath ? lpRDD->lpszPath : L"");
        
        bool isRoot = (path.length() <= 9 || _wcsicmp(path.c_str(), L"service://") == 0);
        
        if (!isRoot) {
            SetLastError(ERROR_DIRECTORY);
            return FALSE;
        }

        if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE;

        std::vector<ServiceInfo> services;
        if (!EnumServices(services)) {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return FALSE;
        }

        int numItems = (int)services.size();
        size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
        if (!lpFDH) return FALSE;

        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
        lpFDH->iNumItems = numItems;
        lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

        if (numItems > 0) {
            LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
            for (int i = 0; i < numItems; ++i) {
                StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, services[i].name.c_str());
                lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

                FillColumnData(&lpFileData[i], lpRDD->hMemHeap, services[i]);
            }
        }

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;

    std::wstring serviceName = GetServiceNameFromPath(lpszFiles);
    if (serviceName.empty()) return NULL;

    ServiceInfo info;
    if (!GetServiceInfo(serviceName, info)) {
        MessageBoxW(hwndParent, L"\u83B7\u53D6\u670D\u52A1\u4FE1\u606F\u5931\u8D25", L"\u9519\u8BEF", MB_ICONERROR | MB_OK);
        return NULL;
    }

    MessageBoxW(hwndParent, FormatServiceInfo(info).c_str(),
        (info.displayName + L" - \u670D\u52A1\u5C5E\u6027").c_str(), MB_OK | MB_ICONINFORMATION);

    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    StringCchCopyW(lpszNewPath, cbNewPathMax, L"service://");
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFreeDiskSpaceW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, unsigned __int64* piFreeBytesAvailable, unsigned __int64* piTotalBytes, unsigned __int64* piTotalFreeBytes) {
    if (piFreeBytesAvailable) *piFreeBytesAvailable = 0;
    if (piTotalBytes) *piTotalBytes = 0;
    if (piTotalFreeBytes) *piTotalFreeBytes = 0;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwAttr, BOOL fForDelete) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileTimeW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileCommentW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszComment) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    return TRUE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData) {
    return GetLastError();
}

// ============================================================================
// Configuration Dialog
// ============================================================================

struct ServiceConfig {
    bool autoRefresh = true;
    int refreshInterval = 5;
    bool showSystemServices = false;
    bool colSvcName = true;
    bool colDisplayName = true;
    bool colStatus = true;
    bool colStartType = true;
    bool colSvcType = true;
    bool colDescription = true;
    bool colProcessId = true;
    bool colExitCode = false;
    bool confirmStop = true;
    bool confirmRestart = true;
    bool confirmDelete = true;
    bool warnCritical = true;
    bool verboseLog = false;
    int cacheTimeout = 10;
};

static ServiceConfig g_svcConfig;
static int g_currentPage = 0;

static bool RegReadValue(HKEY hKey, LPCWSTR name, DWORD& val) {
    DWORD sz = sizeof(DWORD); DWORD type;
    return RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)&val, &sz) == ERROR_SUCCESS;
}

static void LoadSvcConfig() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ServiceVFS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val;
        if (RegReadValue(hKey, L"AutoRefresh", val)) g_svcConfig.autoRefresh = (val != 0);
        if (RegReadValue(hKey, L"RefreshInterval", val)) g_svcConfig.refreshInterval = (int)val;
        if (RegReadValue(hKey, L"ShowSystemServices", val)) g_svcConfig.showSystemServices = (val != 0);
        if (RegReadValue(hKey, L"ColSvcName", val)) g_svcConfig.colSvcName = (val != 0);
        if (RegReadValue(hKey, L"ColDisplayName", val)) g_svcConfig.colDisplayName = (val != 0);
        if (RegReadValue(hKey, L"ColStatus", val)) g_svcConfig.colStatus = (val != 0);
        if (RegReadValue(hKey, L"ColStartType", val)) g_svcConfig.colStartType = (val != 0);
        if (RegReadValue(hKey, L"ColSvcType", val)) g_svcConfig.colSvcType = (val != 0);
        if (RegReadValue(hKey, L"ColDescription", val)) g_svcConfig.colDescription = (val != 0);
        if (RegReadValue(hKey, L"ColProcessId", val)) g_svcConfig.colProcessId = (val != 0);
        if (RegReadValue(hKey, L"ColExitCode", val)) g_svcConfig.colExitCode = (val != 0);
        if (RegReadValue(hKey, L"ConfirmStop", val)) g_svcConfig.confirmStop = (val != 0);
        if (RegReadValue(hKey, L"ConfirmRestart", val)) g_svcConfig.confirmRestart = (val != 0);
        if (RegReadValue(hKey, L"ConfirmDelete", val)) g_svcConfig.confirmDelete = (val != 0);
        if (RegReadValue(hKey, L"WarnCritical", val)) g_svcConfig.warnCritical = (val != 0);
        if (RegReadValue(hKey, L"CacheTimeout", val)) g_svcConfig.cacheTimeout = (int)val;
        if (RegReadValue(hKey, L"VerboseLog", val)) g_svcConfig.verboseLog = (val != 0);
        RegCloseKey(hKey);
    }
}

static void SaveSvcConfig() {
    HKEY hKey; DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ServiceVFS", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS) {
        DWORD val;
        val = g_svcConfig.autoRefresh ? 1 : 0; RegSetValueExW(hKey, L"AutoRefresh", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.refreshInterval; RegSetValueExW(hKey, L"RefreshInterval", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.showSystemServices ? 1 : 0; RegSetValueExW(hKey, L"ShowSystemServices", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colSvcName ? 1 : 0; RegSetValueExW(hKey, L"ColSvcName", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colDisplayName ? 1 : 0; RegSetValueExW(hKey, L"ColDisplayName", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colStatus ? 1 : 0; RegSetValueExW(hKey, L"ColStatus", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colStartType ? 1 : 0; RegSetValueExW(hKey, L"ColStartType", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colSvcType ? 1 : 0; RegSetValueExW(hKey, L"ColSvcType", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colDescription ? 1 : 0; RegSetValueExW(hKey, L"ColDescription", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colProcessId ? 1 : 0; RegSetValueExW(hKey, L"ColProcessId", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.colExitCode ? 1 : 0; RegSetValueExW(hKey, L"ColExitCode", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.confirmStop ? 1 : 0; RegSetValueExW(hKey, L"ConfirmStop", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.confirmRestart ? 1 : 0; RegSetValueExW(hKey, L"ConfirmRestart", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.confirmDelete ? 1 : 0; RegSetValueExW(hKey, L"ConfirmDelete", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.warnCritical ? 1 : 0; RegSetValueExW(hKey, L"WarnCritical", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.cacheTimeout; RegSetValueExW(hKey, L"CacheTimeout", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_svcConfig.verboseLog ? 1 : 0; RegSetValueExW(hKey, L"VerboseLog", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static const int PAGE_GENERAL = 0;
static const int PAGE_DISPLAY = 1;
static const int PAGE_CONFIRM = 2;

static const int pageGeneralCtrls[] = { IDC_AUTO_REFRESH, IDC_SHOW_SYSTEM_SERVICES, IDC_LBL_REFRESH_INTERVAL, IDC_REFRESH_INTERVAL, IDC_LBL_CACHE_TIMEOUT, IDC_CACHE_TIMEOUT, IDC_CHK_VERBOSE_LOG, 0 };
static const int pageDisplayCtrls[] = { IDC_LBL_COLUMNS, IDC_COL_SVCNAME, IDC_COL_DISPLAYNAME, IDC_COL_STATUS, IDC_COL_STARTTYPE, IDC_COL_SVCTYPE, IDC_COL_DESCRIPTION, IDC_COL_PROCESSID, IDC_COL_EXITCODE, 0 };
static const int pageConfirmCtrls[] = { IDC_CHK_CONFIRM_STOP, IDC_CHK_CONFIRM_RESTART, IDC_CHK_CONFIRM_DELETE, IDC_CHK_WARN_CRITICAL, 0 };

static const int* g_pageControls[] = { pageGeneralCtrls, pageDisplayCtrls, pageConfirmCtrls };

static void ShowNavPage(HWND hDlg, int page) {
    g_currentPage = page;
    for (int i = 0; i < 3; i++) {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++) {
            HWND hwnd = GetDlgItem(hDlg, ctrls[j]);
            if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        }
    }
}

static void SetChineseText(HWND hDlg) {
    SetWindowTextW(hDlg, L"\x670D\x52A1 VFS \x914D\x7F6E");

    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList) {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 3; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }

    SetDlgItemTextW(hDlg, IDOK, L"\x786E\x5B9A");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\x53D6\x6D88");
    SetDlgItemTextW(hDlg, IDC_OPEN_SVCMGR, L"\x670D\x52A1\x7BA1\x7406\x5668");
    SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\x6062\x590D\x9ED8\x8BA4");

    SetDlgItemTextW(hDlg, IDC_AUTO_REFRESH, L"\x542F\x7528\x81EA\x52A8\x5237\x65B0");
    SetDlgItemTextW(hDlg, IDC_SHOW_SYSTEM_SERVICES, L"\x663E\x793A\x7CFB\x7EDF\x670D\x52A1");
    SetDlgItemTextW(hDlg, IDC_LBL_REFRESH_INTERVAL, L"\x5237\x65B0\x95F4\x9694(\x79D2):");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TIMEOUT, L"\x7F13\x5B58\x8D85\x65F6(\x79D2):");
    SetDlgItemTextW(hDlg, IDC_CHK_VERBOSE_LOG, L"\x8BE6\x7EC6\x65E5\x5FD7");

    SetDlgItemTextW(hDlg, IDC_LBL_COLUMNS, L"\x663E\x793A\x5217:");
    SetDlgItemTextW(hDlg, IDC_COL_SVCNAME, L"\x670D\x52A1\x540D\x79F0");
    SetDlgItemTextW(hDlg, IDC_COL_DISPLAYNAME, L"\x663E\x793A\x540D\x79F0");
    SetDlgItemTextW(hDlg, IDC_COL_STATUS, L"\x72B6\x6001");
    SetDlgItemTextW(hDlg, IDC_COL_STARTTYPE, L"\x542F\x52A8\x7C7B\x578B");
    SetDlgItemTextW(hDlg, IDC_COL_SVCTYPE, L"\x7C7B\x578B");
    SetDlgItemTextW(hDlg, IDC_COL_DESCRIPTION, L"\x63CF\x8FF0");
    SetDlgItemTextW(hDlg, IDC_COL_PROCESSID, L"\x8FDB\x7A0B ID");
    SetDlgItemTextW(hDlg, IDC_COL_EXITCODE, L"\x9000\x51FA\x4EE3\x7801");

    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_STOP, L"\x505C\x6B62\x670D\x52A1\x524D\x786E\x8BA4");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_RESTART, L"\x91CD\x65B0\x542F\x52A8\x670D\x52A1\x524D\x786E\x8BA4");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_DELETE, L"\x5220\x9664\x670D\x52A1\x524D\x786E\x8BA4");
    SetDlgItemTextW(hDlg, IDC_CHK_WARN_CRITICAL, L"\x8B66\x544A\x7CFB\x7EDF\x5173\x952E\x670D\x52A1");
}

static void InitDialogControls(HWND hDlg) {
    SetChineseText(hDlg);

    CheckDlgButton(hDlg, IDC_AUTO_REFRESH, g_svcConfig.autoRefresh ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_SYSTEM_SERVICES, g_svcConfig.showSystemServices ? BST_CHECKED : BST_UNCHECKED);
    WCHAR buf[32];
    swprintf_s(buf, L"%d", g_svcConfig.refreshInterval);
    SetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL, buf);
    swprintf_s(buf, L"%d", g_svcConfig.cacheTimeout);
    SetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf);
    CheckDlgButton(hDlg, IDC_CHK_VERBOSE_LOG, g_svcConfig.verboseLog ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(hDlg, IDC_COL_SVCNAME, g_svcConfig.colSvcName ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DISPLAYNAME, g_svcConfig.colDisplayName ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_STATUS, g_svcConfig.colStatus ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_STARTTYPE, g_svcConfig.colStartType ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_SVCTYPE, g_svcConfig.colSvcType ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DESCRIPTION, g_svcConfig.colDescription ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PROCESSID, g_svcConfig.colProcessId ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_EXITCODE, g_svcConfig.colExitCode ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_STOP, g_svcConfig.confirmStop ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_RESTART, g_svcConfig.confirmRestart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_DELETE, g_svcConfig.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_WARN_CRITICAL, g_svcConfig.warnCritical ? BST_CHECKED : BST_UNCHECKED);

    ShowNavPage(hDlg, PAGE_GENERAL);
}

static bool SaveDialogControls(HWND hDlg) {
    g_svcConfig.autoRefresh = (IsDlgButtonChecked(hDlg, IDC_AUTO_REFRESH) == BST_CHECKED);
    g_svcConfig.showSystemServices = (IsDlgButtonChecked(hDlg, IDC_SHOW_SYSTEM_SERVICES) == BST_CHECKED);
    WCHAR buf[32];
    GetDlgItemTextW(hDlg, IDC_REFRESH_INTERVAL, buf, 32);
    g_svcConfig.refreshInterval = _wtoi(buf);
    if (g_svcConfig.refreshInterval < 1) g_svcConfig.refreshInterval = 1;
    GetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf, 32);
    g_svcConfig.cacheTimeout = _wtoi(buf);
    if (g_svcConfig.cacheTimeout < 0) g_svcConfig.cacheTimeout = 0;
    g_svcConfig.verboseLog = (IsDlgButtonChecked(hDlg, IDC_CHK_VERBOSE_LOG) == BST_CHECKED);

    g_svcConfig.colSvcName = (IsDlgButtonChecked(hDlg, IDC_COL_SVCNAME) == BST_CHECKED);
    g_svcConfig.colDisplayName = (IsDlgButtonChecked(hDlg, IDC_COL_DISPLAYNAME) == BST_CHECKED);
    g_svcConfig.colStatus = (IsDlgButtonChecked(hDlg, IDC_COL_STATUS) == BST_CHECKED);
    g_svcConfig.colStartType = (IsDlgButtonChecked(hDlg, IDC_COL_STARTTYPE) == BST_CHECKED);
    g_svcConfig.colSvcType = (IsDlgButtonChecked(hDlg, IDC_COL_SVCTYPE) == BST_CHECKED);
    g_svcConfig.colDescription = (IsDlgButtonChecked(hDlg, IDC_COL_DESCRIPTION) == BST_CHECKED);
    g_svcConfig.colProcessId = (IsDlgButtonChecked(hDlg, IDC_COL_PROCESSID) == BST_CHECKED);
    g_svcConfig.colExitCode = (IsDlgButtonChecked(hDlg, IDC_COL_EXITCODE) == BST_CHECKED);

    g_svcConfig.confirmStop = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_STOP) == BST_CHECKED);
    g_svcConfig.confirmRestart = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_RESTART) == BST_CHECKED);
    g_svcConfig.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_DELETE) == BST_CHECKED);
    g_svcConfig.warnCritical = (IsDlgButtonChecked(hDlg, IDC_CHK_WARN_CRITICAL) == BST_CHECKED);

    SaveSvcConfig();
    return true;
}

static void ResetToDefaults(HWND hDlg) {
    g_svcConfig = ServiceConfig();
    InitDialogControls(hDlg);
    MessageBoxW(hDlg, L"\x5DF2\x6062\x590D\x9ED8\x8BA4\x8BBE\x7F6E\x3002", L"\x63D0\x793A", MB_OK | MB_ICONINFORMATION);
}

static void OpenServiceManager(HWND hDlg) {
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = L"open";
    sei.lpFile = L"services.msc";
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

INT_PTR CALLBACK ServiceConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        LoadSvcConfig();
        InitDialogControls(hDlg);
        return (INT_PTR)TRUE;

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST)
            lpmis->itemHeight = 24;
    }
    return (INT_PTR)TRUE;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST) {
            static const WCHAR* navLabels[] = { L"\x5E38\x89C4\x8BBE\x7F6E", L"\x663E\x793A\x8BBE\x7F6E", L"\x64CD\x4F5C\x786E\x8BA4" };
            int idx = (int)lpdis->itemID;
            if (idx >= 0 && idx < 3) {
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
        switch (LOWORD(wParam)) {
        case IDOK:
            if (SaveDialogControls(hDlg)) EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        case IDC_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 3) ShowNavPage(hDlg, sel);
            }
            return (INT_PTR)TRUE;
        case IDC_RESET_DEFAULTS:
            ResetToDefaults(hDlg);
            return (INT_PTR)TRUE;
        case IDC_OPEN_SVCMGR:
            OpenServiceManager(hDlg);
            return (INT_PTR)TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hwndParent) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_SERVICE_CONFIG), hwndParent, ServiceConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hwndParent) {
    MessageBoxW(hwndParent,
        L"ServiceVFS v1.0.0\n\n"
        L"(c) 2026\n\n"
        L"Windows \x670D\x52A1\x7BA1\x7406\x865A\x62DF\x6587\x4EF6\x7CFB\x7EDF\n\n"
        L"\x529F\x80FD\x7279\x6027:\n"
        L"- \x6D4F\x89C8\x6240\x6709 Windows \x670D\x52A1\n"
        L"- \x7BA1\x7406\x670D\x52A1(\x542F\x52A8/\x505C\x6B62/\x6682\x505C/\x6062\x590D)\n"
        L"- \x67E5\x770B\x670D\x52A1\x8BE6\x7EC6\x4FE1\x606F\n"
        L"- 8\x4E2A\x81EA\x5B9A\x4E49\x5217\n",
        L"\x5173\x4E8E ServiceVFS",
        MB_ICONINFORMATION | MB_OK);
    return NULL;
}
