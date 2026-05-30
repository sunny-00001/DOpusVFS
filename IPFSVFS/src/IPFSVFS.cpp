#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <chrono>
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
#include "IPFSClient.h"
#include "Utils.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Winhttp.lib")

static const GUID GUIDPlugin_IPFS =
{ 0xF1E2D3C4, 0xB5A6, 0x9786, { 0x54, 0x32, 0x10, 0xAB, 0xCD, 0xEF, 0x01, 0x23 } };

#define IPFS_VFS_PREFIX L"ipfs://"
#define IPFS_VFS_PREFIX_LEN 7

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

static std::wstring g_configDaemonHost = L"127.0.0.1";
static int g_configDaemonPort = 5001;

static bool IsIpfsVfsPath(LPCWSTR pszPath) {
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, IPFS_VFS_PREFIX, IPFS_VFS_PREFIX_LEN) == 0;
}

static bool IsIpfsRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, IPFS_VFS_PREFIX) == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    if (_wcsicmp(s.c_str(), L"ipfs:") == 0) return true;
    return false;
}

static void NormalizeIpfsPath(std::wstring& path) {
    for (auto& c : path) { if (c == L'\\') c = L'/'; }
    while (path.length() > IPFS_VFS_PREFIX_LEN && path[IPFS_VFS_PREFIX_LEN] == L'/') {
        path.erase(IPFS_VFS_PREFIX_LEN, 1);
    }
    size_t pos;
    while ((pos = path.find(L"//", IPFS_VFS_PREFIX_LEN)) != std::wstring::npos) {
        path.erase(pos, 1);
    }
    while (path.length() > IPFS_VFS_PREFIX_LEN && path.back() == L'/') path.pop_back();
}

static bool ParseIpfsPath(const std::wstring& fullPath, std::wstring& cid, std::wstring& subPath) {
    std::wstring path = fullPath;
    NormalizeIpfsPath(path);

    if (path.length() <= IPFS_VFS_PREFIX_LEN) {
        cid = L"";
        subPath = L"";
        return true;
    }

    std::wstring pathWithoutProto = path.substr(IPFS_VFS_PREFIX_LEN);
    size_t firstSlash = pathWithoutProto.find(L'/');

    if (firstSlash == std::wstring::npos) {
        cid = pathWithoutProto;
        subPath = L"";
    } else {
        cid = pathWithoutProto.substr(0, firstSlash);
        subPath = pathWithoutProto.substr(firstSlash + 1);
    }

    while (!subPath.empty() && subPath.front() == L'/') subPath.erase(0, 1);
    while (!subPath.empty() && subPath.back() == L'/') subPath.pop_back();

    return !cid.empty();
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str) {
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

static void LoadConfig() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\IPFSVFS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR buf[256] = { 0 };
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hKey, L"DaemonHost", NULL, NULL, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            g_configDaemonHost = buf;
        }
        DWORD port = 0;
        bufSize = sizeof(port);
        if (RegQueryValueExW(hKey, L"DaemonPort", NULL, NULL, (LPBYTE)&port, &bufSize) == ERROR_SUCCESS) {
            if (port > 0 && port < 65536) g_configDaemonPort = (int)port;
        }
        RegCloseKey(hKey);
    }
}

static void SaveConfig() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\IPFSVFS", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"DaemonHost", 0, REG_SZ, (const BYTE*)g_configDaemonHost.c_str(),
            (DWORD)((g_configDaemonHost.length() + 1) * sizeof(WCHAR)));
        RegSetValueExW(hKey, L"DaemonPort", 0, REG_DWORD, (const BYTE*)&g_configDaemonPort, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        SetWindowTextW(hDlg, L"IPFS VFS 配置");
        SetDlgItemTextW(hDlg, IDC_LBL_DAEMON_HOST, L"IPFS 守护进程地址:");
        SetDlgItemTextW(hDlg, IDC_LBL_DAEMON_PORT, L"IPFS 守护进程端口:");
        SetDlgItemTextW(hDlg, IDC_DAEMON_HOST, g_configDaemonHost.c_str());
        SetDlgItemInt(hDlg, IDC_DAEMON_PORT, g_configDaemonPort, FALSE);
        SetDlgItemTextW(hDlg, IDC_CHK_AUTO_CONNECT, L"自动连接守护进程");
        CheckDlgButton(hDlg, IDC_CHK_AUTO_CONNECT, BST_CHECKED);
        SetDlgItemTextW(hDlg, IDOK, L"确定");
        SetDlgItemTextW(hDlg, IDCANCEL, L"取消");
        SetDlgItemTextW(hDlg, IDC_BTN_TEST, L"测试连接");
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_TEST: {
            WCHAR host[256] = { 0 };
            GetDlgItemTextW(hDlg, IDC_DAEMON_HOST, host, 256);
            int port = GetDlgItemInt(hDlg, IDC_DAEMON_PORT, NULL, FALSE);
            IPFSClient::SetDaemonAddress(host);
            IPFSClient::SetDaemonPort(port);
            if (IPFSClient::EnsureDaemonStarted()) {
                MessageBoxW(hDlg, L"连接成功！IPFS 守护进程正在运行。", L"测试连接", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hDlg, L"连接失败！请确认 IPFS 守护进程已启动。\n\n"
                    L"您可以通过以下命令启动:\nipfs daemon",
                    L"测试连接", MB_OK | MB_ICONERROR);
            }
            IPFSClient::SetDaemonAddress(g_configDaemonHost);
            IPFSClient::SetDaemonPort(g_configDaemonPort);
            return TRUE;
        }
        case IDOK: {
            WCHAR host[256] = { 0 };
            GetDlgItemTextW(hDlg, IDC_DAEMON_HOST, host, 256);
            int port = GetDlgItemInt(hDlg, IDC_DAEMON_PORT, NULL, FALSE);
            if (port <= 0 || port > 65535) {
                MessageBoxW(hDlg, L"端口号无效，请输入 1-65535 之间的数值。", L"错误", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            g_configDaemonHost = host;
            g_configDaemonPort = port;
            SaveConfig();
            IPFSClient::SetDaemonAddress(g_configDaemonHost);
            IPFSClient::SetDaemonPort(g_configDaemonPort);
            EndDialog(hDlg, IDOK);
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

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) {
    LoadConfig();
    IPFSClient::SetDaemonAddress(g_configDaemonHost);
    IPFSClient::SetDaemonPort(g_configDaemonPort);
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) {
        Sleep(50);
    }
    IPFSClient::Cleanup();
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    if (!lpVFSInfo) return FALSE;

    lpVFSInfo->idPlugin = GUIDPlugin_IPFS;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK | VFSCAPABILITY_CASESENSITIVE;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, IPFS_VFS_PREFIX);

    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"IPFS");

    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
            L"IPFS 分布式文件系统 - 浏览和访问 IPFS 网络");

    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");

    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"https://ipfs.io");

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
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"ipfs://");
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData) {
    static VFSCUSTOMCOLUMNW columns[5];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"CID";
    columns[0].lpszKey = L"ipfs_cid";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"类型";
    columns[1].lpszKey = L"ipfs_type";
    columns[1].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[1].iID = 2;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"区块数";
    columns[2].lpszKey = L"ipfs_blocks";
    columns[2].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[2].iID = 3;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"大小";
    columns[3].lpszKey = L"ipfs_size";
    columns[3].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[3].iID = 4;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = NULL;
    columns[4].lpszLabel = L"固定状态";
    columns[4].lpszKey = L"ipfs_pin";
    columns[4].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[4].iID = 5;

    return columns;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpRDD->lpszPath, cid, subPath)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    if (!IPFSClient::EnsureDaemonStarted()) {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }

    if (cid.empty()) {
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
            sizeof(VFSFILEDATAHEADER));
        if (!lpFDH) return FALSE;
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
        lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
        lpFDH->iNumItems = 0;
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    IPFSFileInfo dirInfo;
    if (!IPFSClient::Stat(cid, subPath, dirInfo)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    if (!dirInfo.isDir) {
        SetLastError(ERROR_DIRECTORY);
        return FALSE;
    }

    auto fileList = IPFSClient::ListDirectory(cid, subPath);
    int numItems = (int)fileList.size();

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

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, fileList[i].name.c_str());
        lpFileData[i].wfdData.dwFileAttributes = fileList[i].isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        ULARGE_INTEGER sz; sz.QuadPart = fileList[i].size;
        lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart;
        lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;
        lpFileData[i].wfdData.ftLastWriteTime = fileList[i].modTime;

        lpFileData[i].iNumColumns = 5;
        lpFileData[i].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
            5 * sizeof(VFSFILEDATACOLUMNW));

        if (lpFileData[i].lpvfsColumnData) {
            std::wstring shortCid = fileList[i].cid;
            if (shortCid.length() > 20) shortCid = shortCid.substr(0, 12) + L"..." + shortCid.substr(shortCid.length() - 8);

            lpFileData[i].lpvfsColumnData[0].iColumnId = 1;
            lpFileData[i].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, shortCid);

            lpFileData[i].lpvfsColumnData[1].iColumnId = 2;
            lpFileData[i].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap, fileList[i].type);

            lpFileData[i].lpvfsColumnData[2].iColumnId = 3;
            lpFileData[i].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap, std::to_wstring(fileList[i].blocks));

            lpFileData[i].lpvfsColumnData[3].iColumnId = 4;
            lpFileData[i].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap, FormatFileSize(fileList[i].size));

            bool isPinned = false;
            IPFSClient::PinLs(fileList[i].cid, isPinned);
            lpFileData[i].lpvfsColumnData[4].iColumnId = 5;
            lpFileData[i].lpvfsColumnData[4].lpszValue = AllocString(lpRDD->hMemHeap, isPinned ? L"已固定" : L"未固定");
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

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszPath, cid, subPath)) return NULL;

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;

    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->iNumItems = 1;
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    if (cid.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"IPFS");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (!IPFSClient::EnsureDaemonStarted()) {
        HeapFree(hHeap, 0, lpFDH);
        return NULL;
    }

    IPFSFileInfo info;
    if (!IPFSClient::Stat(cid, subPath, info)) {
        HeapFree(hHeap, 0, lpFDH);
        return NULL;
    }

    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, info.name.c_str());
    lpFileData->wfdData.dwFileAttributes = info.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    ULARGE_INTEGER sz; sz.QuadPart = info.size;
    lpFileData->wfdData.nFileSizeHigh = sz.HighPart;
    lpFileData->wfdData.nFileSizeLow = sz.LowPart;
    lpFileData->wfdData.ftLastWriteTime = info.modTime;

    lpFileData->iNumColumns = 5;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        5 * sizeof(VFSFILEDATACOLUMNW));

    if (lpFileData->lpvfsColumnData) {
        std::wstring shortCid = info.cid;
        if (shortCid.length() > 20) shortCid = shortCid.substr(0, 12) + L"..." + shortCid.substr(shortCid.length() - 8);

        lpFileData->lpvfsColumnData[0].iColumnId = 1;
        lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, shortCid);

        lpFileData->lpvfsColumnData[1].iColumnId = 2;
        lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, info.type);

        lpFileData->lpvfsColumnData[2].iColumnId = 3;
        lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, std::to_wstring(info.blocks));

        lpFileData->lpvfsColumnData[3].iColumnId = 4;
        lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, FormatFileSize(info.size));

        bool isPinned = false;
        IPFSClient::PinLs(info.cid, isPinned);
        lpFileData->lpvfsColumnData[4].iColumnId = 5;
        lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, isPinned ? L"已固定" : L"未固定");
    }

    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT) {
    if (!IsIpfsVfsPath(lpszFile)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszFile, cid, subPath)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    if (cid.empty()) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    if (!IPFSClient::EnsureDaemonStarted()) {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return NULL;
    }

    IPFSFileInfo info;
    if (!IPFSClient::Stat(cid, subPath, info)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return NULL;
    }

    IPFSFileStreamContext* ctx = new IPFSFileStreamContext();
    if (!ctx) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;
    ctx->cid = info.cid;
    ctx->path = subPath;
    ctx->fileSize = info.size;

    if (!ctx->isWrite) {
        std::string ipfsPath = "/ipfs/" + WideToUtf8(info.cid);
        std::string urlPath = "/api/v0/cat?arg=" + UrlEncode(ipfsPath);

        ctx->hConnect = WinHttpConnect(IPFSClient::s_hSession, IPFSClient::GetDaemonAddress().c_str(),
            (INTERNET_PORT)IPFSClient::GetDaemonPort(), 0);
        if (!ctx->hConnect) { delete ctx; return NULL; }

        ctx->hRequest = WinHttpOpenRequest(ctx->hConnect, L"POST", Utf8ToWide(urlPath).c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!ctx->hRequest) { delete ctx; return NULL; }

        BOOL bRes = WinHttpSendRequest(ctx->hRequest, L"Content-Type: application/json\r\n",
            (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!bRes) { delete ctx; return NULL; }

        if (!WinHttpReceiveResponse(ctx->hRequest, NULL)) { delete ctx; return NULL; }
    }

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    IPFSFileStreamContext* ctx = (IPFSFileStreamContext*)hFile;
    if (!ctx || ctx->isWrite) return FALSE;

    if (lpdwReadSize) *lpdwReadSize = 0;

    if (!ctx->hRequest) return FALSE;

    DWORD dwAvailable = 0;
    if (!WinHttpQueryDataAvailable(ctx->hRequest, &dwAvailable)) return FALSE;
    if (dwAvailable == 0) return TRUE;

    DWORD bytesToRead = min(dwSize, dwAvailable);
    return WinHttpReadData(ctx->hRequest, lpData, bytesToRead, lpdwReadSize);
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize) {
    IPFSFileStreamContext* ctx = (IPFSFileStreamContext*)hFile;
    if (!ctx || !ctx->isWrite) return FALSE;
    if (lpdwWriteSize) *lpdwWriteSize = 0;
    if (dwSize == 0 || lpData == NULL) return TRUE;

    if (lpdwWriteSize) *lpdwWriteSize = dwSize;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos) {
    IPFSFileStreamContext* ctx = (IPFSFileStreamContext*)hFile;
    if (!ctx) return FALSE;

    uint64_t newPos = 0;
    if (dwMethod == FILE_BEGIN) newPos = iPos;
    else if (dwMethod == FILE_CURRENT) newPos = ctx->seekPos + iPos;
    else if (dwMethod == FILE_END) newPos = ctx->fileSize + iPos;
    else return FALSE;

    ctx->seekPos = newPos;
    if (piNewPos) *piNewPos = newPos;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    IPFSFileStreamContext* ctx = (IPFSFileStreamContext*)hFile;
    if (!ctx) return;
    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    if (!IsIpfsVfsPath(lpszFile)) return FALSE;
    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszFile, cid, subPath)) return FALSE;
    if (cid.empty()) return FALSE;

    if (!IPFSClient::EnsureDaemonStarted()) return FALSE;

    int result = MessageBoxW(NULL,
        (L"确定要从本地节点取消固定此文件吗？\n\nCID: " + cid + L"\n\n"
            L"注意：这不会从 IPFS 网络中删除文件，仅取消本地固定。").c_str(),
        L"确认取消固定", MB_YESNO | MB_ICONQUESTION);

    if (result != IDYES) return FALSE;

    if (IPFSClient::PinRm(cid)) {
        IPFSClient::InvalidateCache();
        return TRUE;
    }

    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    if (!IsIpfsVfsPath(lpszPath)) return FALSE;
    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszPath, cid, subPath)) return FALSE;
    if (cid.empty()) return FALSE;

    if (!IPFSClient::EnsureDaemonStarted()) return FALSE;

    int result = MessageBoxW(NULL,
        (L"确定要取消固定此目录吗？\n\nCID: " + cid + L"\n\n"
            L"注意：这不会从 IPFS 网络中删除目录，仅取消本地固定。").c_str(),
        L"确认取消固定", MB_YESNO | MB_ICONQUESTION);

    if (result != IDYES) return FALSE;

    if (IPFSClient::PinRm(cid)) {
        IPFSClient::InvalidateCache();
        return TRUE;
    }

    SetLastError(ERROR_DIR_NOT_EMPTY);
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

    if (IsIpfsRootPath(lpszPath)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"IPFS");
        return TRUE;
    }

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszPath, cid, subPath)) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"IPFS");
        return TRUE;
    }

    if (subPath.empty()) {
        std::wstring shortCid = cid;
        if (shortCid.length() > 16) shortCid = shortCid.substr(0, 8) + L"..." + shortCid.substr(shortCid.length() - 4);
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, shortCid.c_str());
        return TRUE;
    }

    size_t lastSlash = subPath.find_last_of(L"/\\");
    if (lastSlash != std::wstring::npos) {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, subPath.substr(lastSlash + 1).c_str());
    } else {
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, subPath.c_str());
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
    LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;

    if (fRoot) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, IPFS_VFS_PREFIX);
        return TRUE;
    }

    if (IsIpfsRootPath(lpszPath)) return FALSE;

    std::wstring path(lpszPath);
    NormalizeIpfsPath(path);

    std::wstring cid, subPath;
    if (!ParseIpfsPath(path, cid, subPath)) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, IPFS_VFS_PREFIX);
        return TRUE;
    }

    if (subPath.empty()) {
        StringCchCopyW(lpszNewPath, cbNewPathMax, IPFS_VFS_PREFIX);
        return TRUE;
    }

    size_t lastSlash = subPath.find_last_of(L'/');
    if (lastSlash != std::wstring::npos) {
        std::wstring parentSubPath = subPath.substr(0, lastSlash);
        StringCchPrintfW(lpszNewPath, cbNewPathMax, L"ipfs://%s/%s", cid.c_str(), parentSubPath.c_str());
    } else {
        StringCchPrintfW(lpszNewPath, cbNewPathMax, L"ipfs://%s", cid.c_str());
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData) {
    if (!lpszFiles || !lpMenuData) return FALSE;

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszFiles, cid, subPath) || cid.empty()) {
        lpMenuData->fAllowContextMenu = FALSE;
        return TRUE;
    }

    static VFSCONTEXTMENUITEMW items[12];
    int itemCount = 0;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"复制 CID";
    items[itemCount].lpszCommand = L"copy_cid";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"复制 IPFS 路径";
    items[itemCount].lpszCommand = L"copy_ipfs_path";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"复制网关链接";
    items[itemCount].lpszCommand = L"copy_gateway_url";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = L"";
    items[itemCount].lpszCommand = L"";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"固定到本地";
    items[itemCount].lpszCommand = L"pin_add";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"取消固定";
    items[itemCount].lpszCommand = L"pin_rm";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = L"";
    items[itemCount].lpszCommand = L"";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"下载到本地";
    items[itemCount].lpszCommand = L"download";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"在浏览器中打开";
    items[itemCount].lpszCommand = L"open_browser";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = VFSCMF_SEPARATOR;
    items[itemCount].lpszLabel = L"";
    items[itemCount].lpszCommand = L"";
    itemCount++;

    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    items[itemCount].dwFlags = 0;
    items[itemCount].lpszLabel = L"查看详情";
    items[itemCount].lpszCommand = L"view_details";
    itemCount++;

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = TRUE;
    lpMenuData->fCustomItemsBelow = TRUE;
    lpMenuData->lpCustomItems = items;
    lpMenuData->iNumCustomItems = itemCount;
    lpMenuData->fFreeCustomItems = FALSE;

    return TRUE;
}

static void DownloadToLocal(HWND hwndParent, const std::wstring& cid, const std::wstring& name) {
    if (!IPFSClient::EnsureDaemonStarted()) {
        MessageBoxW(hwndParent, L"无法连接到 IPFS 守护进程。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hwndParent;
    bi.lpszTitle = L"选择保存位置";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;

    WCHAR destPath[MAX_PATH] = { 0 };
    if (!SHGetPathFromIDListW(pidl, destPath)) {
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);

    std::wstring destFile = std::wstring(destPath) + L"\\" + name;

    std::string ipfsPath = "/ipfs/" + WideToUtf8(cid);
    std::string urlPath = "/api/v0/get?arg=" + UrlEncode(ipfsPath) + "&archive=false&compress=false";

    HINTERNET hConnect = WinHttpConnect(IPFSClient::s_hSession, IPFSClient::GetDaemonAddress().c_str(),
        (INTERNET_PORT)IPFSClient::GetDaemonPort(), 0);
    if (!hConnect) {
        MessageBoxW(hwndParent, L"下载失败：无法连接到守护进程。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", Utf8ToWide(urlPath).c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        MessageBoxW(hwndParent, L"下载失败：无法创建请求。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    BOOL bRes = WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n",
        (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bRes || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        MessageBoxW(hwndParent, L"下载失败：请求发送失败。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    HANDLE hFile = CreateFileW(destFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        MessageBoxW(hwndParent, L"下载失败：无法创建本地文件。", L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD dwSize = 0, dwDownloaded = 0;
    char buf[65536];
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize > 0) {
            DWORD bytesToRead = min(dwSize, (DWORD)sizeof(buf));
            if (WinHttpReadData(hRequest, buf, bytesToRead, &dwDownloaded) && dwDownloaded > 0) {
                DWORD written;
                WriteFile(hFile, buf, dwDownloaded, &written, NULL);
            }
        }
    } while (dwSize > 0);

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    MessageBoxW(hwndParent, (L"文件已下载到:\n" + destFile).c_str(), L"下载完成", MB_OK | MB_ICONINFORMATION);
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpVerbData->lpszPath, cid, subPath) || cid.empty()) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"open") == 0) {

        if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }

        if (!IPFSClient::EnsureDaemonStarted()) return VFSCVRES_FAIL;

        IPFSFileInfo info;
        if (!IPFSClient::Stat(cid, subPath, info)) return VFSCVRES_FAIL;

        DownloadToLocal(lpVerbData->hwndParent, info.cid, info.name);

        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_cid") == 0) {
        IPFSFileInfo info;
        std::wstring copyCid = cid;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            copyCid = info.cid;
        }
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            size_t len = (copyCid.length() + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hMem) {
                LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                if (pMem) {
                    wcscpy_s(pMem, len / sizeof(WCHAR), copyCid.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_ipfs_path") == 0) {
        IPFSFileInfo info;
        std::wstring ipfsPath;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            ipfsPath = L"/ipfs/" + info.cid;
            if (!subPath.empty()) ipfsPath += L"/" + subPath;
        } else {
            ipfsPath = L"/ipfs/" + cid;
            if (!subPath.empty()) ipfsPath += L"/" + subPath;
        }
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            size_t len = (ipfsPath.length() + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hMem) {
                LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                if (pMem) {
                    wcscpy_s(pMem, len / sizeof(WCHAR), ipfsPath.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_gateway_url") == 0) {
        IPFSFileInfo info;
        std::wstring gatewayCid = cid;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            gatewayCid = info.cid;
        }
        std::wstring gatewayUrl = L"https://ipfs.io/ipfs/" + gatewayCid;
        if (!subPath.empty()) gatewayUrl += L"/" + subPath;
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            size_t len = (gatewayUrl.length() + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hMem) {
                LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                if (pMem) {
                    wcscpy_s(pMem, len / sizeof(WCHAR), gatewayUrl.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"pin_add") == 0) {
        IPFSFileInfo info;
        std::wstring pinCid = cid;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            pinCid = info.cid;
        }
        if (IPFSClient::PinAdd(pinCid)) {
            IPFSClient::InvalidateCache();
            MessageBoxW(lpVerbData->hwndParent, L"已成功固定到本地节点。", L"固定成功", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(lpVerbData->hwndParent, L"固定失败，请检查 IPFS 守护进程状态。", L"错误", MB_OK | MB_ICONERROR);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"pin_rm") == 0) {
        IPFSFileInfo info;
        std::wstring pinCid = cid;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            pinCid = info.cid;
        }
        int result = MessageBoxW(lpVerbData->hwndParent,
            L"确定要取消固定吗？\n\n取消固定后，垃圾回收可能会删除此内容。",
            L"确认取消固定", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            if (IPFSClient::PinRm(pinCid)) {
                IPFSClient::InvalidateCache();
                MessageBoxW(lpVerbData->hwndParent, L"已取消固定。", L"操作成功", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(lpVerbData->hwndParent, L"取消固定失败。", L"错误", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"download") == 0) {
        if (!IPFSClient::EnsureDaemonStarted()) return VFSCVRES_FAIL;
        IPFSFileInfo info;
        if (!IPFSClient::Stat(cid, subPath, info)) return VFSCVRES_FAIL;
        DownloadToLocal(lpVerbData->hwndParent, info.cid, info.name);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"open_browser") == 0) {
        IPFSFileInfo info;
        std::wstring browserCid = cid;
        if (IPFSClient::EnsureDaemonStarted() && IPFSClient::Stat(cid, subPath, info)) {
            browserCid = info.cid;
        }
        std::wstring url = L"https://ipfs.io/ipfs/" + browserCid;
        if (!subPath.empty()) url += L"/" + subPath;
        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"view_details") == 0) {
        if (!IPFSClient::EnsureDaemonStarted()) {
            MessageBoxW(lpVerbData->hwndParent, L"无法连接到 IPFS 守护进程。", L"错误", MB_OK | MB_ICONERROR);
            return VFSCVRES_HANDLED;
        }

        IPFSFileInfo info;
        if (!IPFSClient::Stat(cid, subPath, info)) {
            MessageBoxW(lpVerbData->hwndParent, L"无法获取文件信息。", L"错误", MB_OK | MB_ICONERROR);
            return VFSCVRES_HANDLED;
        }

        bool isPinned = false;
        IPFSClient::PinLs(info.cid, isPinned);

        std::wstring details;
        details += L"名称: " + info.name + L"\r\n";
        details += L"CID: " + info.cid + L"\r\n";
        details += L"类型: " + info.type + L"\r\n";
        details += L"大小: " + FormatFileSize(info.size) + L" (" + std::to_wstring(info.size) + L" 字节)\r\n";
        details += L"区块数: " + std::to_wstring(info.blocks) + L"\r\n";
        details += L"固定状态: " + std::wstring(isPinned ? L"已固定" : L"未固定") + L"\r\n";
        details += L"\r\nIPFS 路径: /ipfs/" + info.cid;
        if (!subPath.empty()) details += L"/" + subPath;
        details += L"\r\n";
        details += L"网关链接: https://ipfs.io/ipfs/" + info.cid;
        if (!subPath.empty()) details += L"/" + subPath;

        MessageBoxW(lpVerbData->hwndParent, details.c_str(), (L"IPFS 详情 - " + info.name).c_str(),
            MB_OK | MB_ICONINFORMATION);
        return VFSCVRES_HANDLED;
    }

    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;

    std::wstring cid, subPath;
    if (!ParseIpfsPath(lpszFiles, cid, subPath) || cid.empty()) return NULL;

    if (!IPFSClient::EnsureDaemonStarted()) {
        MessageBoxW(hwndParent, L"无法连接到 IPFS 守护进程。", L"属性错误", MB_ICONERROR | MB_OK);
        return NULL;
    }

    IPFSFileInfo info;
    if (!IPFSClient::Stat(cid, subPath, info)) {
        MessageBoxW(hwndParent, L"无法获取文件信息。", L"属性错误", MB_ICONERROR | MB_OK);
        return NULL;
    }

    bool isPinned = false;
    IPFSClient::PinLs(info.cid, isPinned);

    WCHAR msg[2048];
    if (info.isDir) {
        StringCchPrintfW(msg, 2048,
            L"名称:\t%s\n类型:\t目录\nCID:\t%s\n大小:\t%s\n区块数:\t%llu\n固定:\t%s",
            info.name.c_str(), info.cid.c_str(), FormatFileSize(info.size).c_str(),
            info.blocks, isPinned ? L"是" : L"否");
    } else {
        StringCchPrintfW(msg, 2048,
            L"名称:\t%s\n类型:\t文件\nCID:\t%s\n大小:\t%s (%llu 字节)\n区块数:\t%llu\n固定:\t%s",
            info.name.c_str(), info.cid.c_str(), FormatFileSize(info.size).c_str(),
            info.size, info.blocks, isPinned ? L"是" : L"否");
    }

    MessageBoxW(hwndParent, msg, L"IPFS 属性", MB_ICONINFORMATION | MB_OK);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
    LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
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
    LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize) {
    if (hFile) {
        IPFSFileStreamContext* ctx = (IPFSFileStreamContext*)hFile;
        if (piFileSize) *piFileSize = ctx->fileSize;
        return TRUE;
    } else if (lpszPath) {
        std::wstring cid, subPath;
        if (ParseIpfsPath(lpszPath, cid, subPath) && !cid.empty()) {
            if (IPFSClient::EnsureDaemonStarted()) {
                IPFSFileInfo info;
                if (IPFSClient::Stat(cid, subPath, info)) {
                    if (piFileSize) *piFileSize = info.size;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) {
    return GetLastError();
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_IPFS_CONFIG), hWndParent, ConfigDlgProc, 0);

    if (hWndNotify) {
        PostMessageW(hWndNotify, DVFSPLUGINMSG_REINITIALIZE, 0, dwNotifyData);
    }
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    MessageBoxW(hWndParent,
        L"IPFS VFS 插件 v1.0.0\n\n"
        L"(c) 2026\n\n"
        L"IPFS 分布式文件系统虚拟文件系统\n\n"
        L"功能特性:\n"
        L"- 浏览 IPFS 网络内容\n"
        L"- 通过 CID 访问文件和目录\n"
        L"- 固定/取消固定内容\n"
        L"- 下载文件到本地\n"
        L"- 复制 CID 和网关链接\n"
        L"- 自定义列显示\n"
        L"- 右键菜单操作",
        L"关于 IPFS VFS",
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
