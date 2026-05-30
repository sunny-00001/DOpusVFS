#include <windows.h>
#include <strsafe.h>
#include <atomic>
#include <shellapi.h>
#include <commctrl.h>
#define DOPUS_PLUGIN_HELPER
#include "headers/vfs plugins.h"
#include "headers/plugin support.h"
#include "WebDAVClient.h"
#include "resource.h"

#pragma comment(lib, "Shell32.lib")

static const GUID GUIDPlugin_WebDAV = 
{ 0x98A4BC21, 0x1F2A, 0x4E5D, { 0x8B, 0x1C, 0xA1, 0x9F, 0x42, 0x7D, 0x33, 0x12 } };

static HMODULE g_hModule = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) g_hModule = hModule;
    return TRUE;
}

// ---- Configuration ----

struct WebDAVConfig {
    int timeoutSec = 30;
    bool verifySSL = true;
    bool followRedirects = true;
    int uploadChunkKB = 64;
    bool showHidden = false;
    bool verboseLog = false;
};

static WebDAVConfig g_webdavConfig;
static int g_currentPage = 0;

static void LoadWebDAVConfig() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\DOpusWebDAV", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    DWORD dwVal = 0;
    if (RegQueryValueExW(hKey, L"TimeoutSec", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS && dwVal > 0)
        g_webdavConfig.timeoutSec = (int)dwVal;
    dwVal = 0;
    if (RegQueryValueExW(hKey, L"VerifySSL", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS)
        g_webdavConfig.verifySSL = (dwVal != 0);
    dwVal = 0;
    if (RegQueryValueExW(hKey, L"FollowRedirects", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS)
        g_webdavConfig.followRedirects = (dwVal != 0);
    dwVal = 0;
    if (RegQueryValueExW(hKey, L"UploadChunkKB", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS && dwVal > 0)
        g_webdavConfig.uploadChunkKB = (int)dwVal;
    dwVal = 0;
    if (RegQueryValueExW(hKey, L"ShowHidden", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS)
        g_webdavConfig.showHidden = (dwVal != 0);
    dwVal = 0;
    if (RegQueryValueExW(hKey, L"VerboseLog", NULL, NULL, (LPBYTE)&dwVal, &(DWORD){sizeof(dwVal)}) == ERROR_SUCCESS)
        g_webdavConfig.verboseLog = (dwVal != 0);
    RegCloseKey(hKey);
}

static void SaveWebDAVConfig() {
    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\DOpusWebDAV", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;
    DWORD dwVal = (DWORD)g_webdavConfig.timeoutSec;
    RegSetValueExW(hKey, L"TimeoutSec", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    dwVal = g_webdavConfig.verifySSL ? 1 : 0;
    RegSetValueExW(hKey, L"VerifySSL", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    dwVal = g_webdavConfig.followRedirects ? 1 : 0;
    RegSetValueExW(hKey, L"FollowRedirects", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    dwVal = (DWORD)g_webdavConfig.uploadChunkKB;
    RegSetValueExW(hKey, L"UploadChunkKB", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    dwVal = g_webdavConfig.showHidden ? 1 : 0;
    RegSetValueExW(hKey, L"ShowHidden", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    dwVal = g_webdavConfig.verboseLog ? 1 : 0;
    RegSetValueExW(hKey, L"VerboseLog", 0, REG_DWORD, (LPBYTE)&dwVal, sizeof(dwVal));
    RegCloseKey(hKey);
}

static void ResetWebDAVDefaults() {
    g_webdavConfig = WebDAVConfig();
}

static void SetWebDAVChineseText(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDOK, L"确定");
    SetDlgItemTextW(hDlg, IDCANCEL, L"取消");
    SetDlgItemTextW(hDlg, IDC_WEBDAV_APPLY, L"应用");
    SetDlgItemTextW(hDlg, IDC_WEBDAV_RESET, L"重置");
    SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_CHK_SSL), L"验证 SSL 证书");
    SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_CHK_REDIRECT), L"跟随重定向");
    SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_CHK_HIDDEN), L"显示隐藏文件");
    SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_CHK_VERBOSE), L"详细日志");
}

static void InitWebDAVDialogControls(HWND hDlg) {
    SetDlgItemInt(hDlg, IDC_WEBDAV_TIMEOUT, g_webdavConfig.timeoutSec, FALSE);
    CheckDlgButton(hDlg, IDC_WEBDAV_CHK_SSL, g_webdavConfig.verifySSL ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_WEBDAV_CHK_REDIRECT, g_webdavConfig.followRedirects ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_WEBDAV_CHUNK_SIZE, g_webdavConfig.uploadChunkKB, FALSE);
    CheckDlgButton(hDlg, IDC_WEBDAV_CHK_HIDDEN, g_webdavConfig.showHidden ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_WEBDAV_CHK_VERBOSE, g_webdavConfig.verboseLog ? BST_CHECKED : BST_UNCHECKED);
}

static void SaveWebDAVDialogControls(HWND hDlg) {
    BOOL bTranslated = FALSE;
    UINT nVal = GetDlgItemInt(hDlg, IDC_WEBDAV_TIMEOUT, &bTranslated, FALSE);
    if (bTranslated && nVal > 0) g_webdavConfig.timeoutSec = (int)nVal;
    g_webdavConfig.verifySSL = (IsDlgButtonChecked(hDlg, IDC_WEBDAV_CHK_SSL) == BST_CHECKED);
    g_webdavConfig.followRedirects = (IsDlgButtonChecked(hDlg, IDC_WEBDAV_CHK_REDIRECT) == BST_CHECKED);
    nVal = GetDlgItemInt(hDlg, IDC_WEBDAV_CHUNK_SIZE, &bTranslated, FALSE);
    if (bTranslated && nVal > 0) g_webdavConfig.uploadChunkKB = (int)nVal;
    g_webdavConfig.showHidden = (IsDlgButtonChecked(hDlg, IDC_WEBDAV_CHK_HIDDEN) == BST_CHECKED);
    g_webdavConfig.verboseLog = (IsDlgButtonChecked(hDlg, IDC_WEBDAV_CHK_VERBOSE) == BST_CHECKED);
    SaveWebDAVConfig();
}

static void ShowWebDAVNavPage(HWND hDlg, int page) {
    g_currentPage = page;
    BOOL showConn = (page == 0);
    BOOL showAdv  = (page == 1);

    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_LBL_TIMEOUT), showConn ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_TIMEOUT),     showConn ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_CHK_SSL),     showConn ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_CHK_REDIRECT), showConn ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_LBL_CHUNK),   showAdv ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_CHUNK_SIZE),  showAdv ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_CHK_HIDDEN),  showAdv ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_WEBDAV_CHK_VERBOSE), showAdv ? SW_SHOW : SW_HIDE);

    // Update labels for current page
    if (showConn) {
        SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_LBL_TIMEOUT), L"超时时间（秒）:");
    }
    if (showAdv) {
        SetWindowTextW(GetDlgItem(hDlg, IDC_WEBDAV_LBL_CHUNK), L"上传分块大小（KB）:");
    }
}

static INT_PTR CALLBACK WebDAVConfigProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        SetWindowTextW(hDlg, L"WebDAV VFS 配置");
        SetWebDAVChineseText(hDlg);
        // Populate nav listbox
        HWND hNav = GetDlgItem(hDlg, IDC_WEBDAV_NAV_LIST);
        SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"连接设置");
        SendMessageW(hNav, LB_ADDSTRING, 1, (LPARAM)L"高级设置");
        SendMessageW(hNav, LB_SETCURSEL, 0, 0);
        LoadWebDAVConfig();
        InitWebDAVDialogControls(hDlg);
        ShowWebDAVNavPage(hDlg, 0);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            SaveWebDAVDialogControls(hDlg);
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        case IDC_WEBDAV_APPLY:
            SaveWebDAVDialogControls(hDlg);
            return TRUE;
        case IDC_WEBDAV_RESET:
            ResetWebDAVDefaults();
            InitWebDAVDialogControls(hDlg);
            return TRUE;
        case IDC_WEBDAV_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(GetDlgItem(hDlg, IDC_WEBDAV_NAV_LIST), LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) ShowWebDAVNavPage(hDlg, sel);
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK WebDAVAboutProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) { EndDialog(hDlg, IDOK); return TRUE; }
        break;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_ConfigureW(HWND hWndParent) {
    LoadWebDAVConfig();
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WEBDAV_CONFIG), hWndParent, WebDAVConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_AboutW(HWND hWndParent) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_WEBDAV_ABOUT), hWndParent, WebDAVAboutProc, 0);
    return NULL;
}

// ---- VFS Plugin Exports ----

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) { WebDAVClient::Init(); return TRUE; }
extern "C" __declspec(dllexport) void VFS_Uninit() { WebDAVClient::Cleanup(); }
extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) { return (HANDLE)1; }
extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    lpVFSInfo->idPlugin = GUIDPlugin_WebDAV;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_MULTIPLEFORMATS;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE | VFSCAPABILITY_MOVEBYRENAME | VFSCAPABILITY_CANRESUMECOPIES;
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    if (lpVFSInfo->lpszHandlePrefix) StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"davs://");
    if (lpVFSInfo->lpszName) StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"DAV");
    if (lpVFSInfo->lpszDescription) StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, L"Native WebDAV integration.");
    ExtractIconExW(L"shell32.dll", 14, &lpVFSInfo->hIconLarge, &lpVFSInfo->hIconSmall, 1);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    LPCWSTR prefixes = L"dav://\0davs://\0";
    memcpy(lpszPrefix, prefixes, 18 * sizeof(WCHAR));
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || lpRDD->vfsReadOp == VFSREAD_FREEDIR || lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE;

    if (lpRDD->vfsReadOp == VFSREAD_NORMAL || lpRDD->vfsReadOp == VFSREAD_REFRESH) {
        if (_wcsicmp(lpRDD->lpszPath, L"dav://") == 0 || _wcsicmp(lpRDD->lpszPath, L"davs://") == 0) {
            LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER));
            if (lpFDH) {
                lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
                lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
                lpFDH->iNumItems = 0;
                lpRDD->lpFileData = lpFDH;
                return TRUE;
            }
            return FALSE;
        }

        WebDAVUrl urlInfo;
        if (!WebDAVClient::ParseUrl(lpRDD->lpszPath, urlInfo)) { SetLastError(ERROR_PATH_NOT_FOUND); return FALSE; }

        WebDAVClient::PrepareAuth(lpRDD->hwndParent, urlInfo);
        if (urlInfo.username.empty() || urlInfo.password.empty()) { SetLastError(ERROR_CANCELLED); return FALSE; }

        SetLastError(ERROR_SUCCESS);
        auto fileList = WebDAVClient::ListDirectory(urlInfo);
        DWORD err = GetLastError();

        if (fileList.empty() && err != ERROR_SUCCESS) {
            if (urlInfo.path == L"/" || urlInfo.path.empty()) {
                err = ERROR_SUCCESS; 
            } else if (err == ERROR_PATH_NOT_FOUND || err == ERROR_ACCESS_DENIED) {
                SetLastError(err);
                return FALSE;
            }
        }
        
        if (fileList.empty() && (err == ERROR_PATH_NOT_FOUND || err == ERROR_ACCESS_DENIED)) {
            SetLastError(err);
            return FALSE;
        }

        int numItems = (int)fileList.size();
        size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
        if (!lpFDH) return FALSE;
        
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

        int addedItems = 0; 
        if (numItems > 0) {
            LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
            for (int i = 0; i < numItems; ++i) {
                if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
                StringCchCopyW(lpFileData[addedItems].wfdData.cFileName, MAX_PATH, fileList[i].name.c_str());
                lpFileData[addedItems].wfdData.dwFileAttributes = fileList[i].isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                ULARGE_INTEGER sz; sz.QuadPart = fileList[i].size;
                lpFileData[addedItems].wfdData.nFileSizeHigh = sz.HighPart; lpFileData[addedItems].wfdData.nFileSizeLow = sz.LowPart;
                lpFileData[addedItems].wfdData.ftLastWriteTime = fileList[i].lastModified;
                addedItems++;
            }
        }
        lpFDH->iNumItems = addedItems; lpRDD->lpFileData = lpFDH; return TRUE;
    }
    SetLastError(ERROR_NO_MORE_FILES); return FALSE;
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszPath, urlInfo)) return NULL;
    WebDAVClient::PrepareAuth(NULL, urlInfo);
    
    WebDAVFileInfo info;
    
    if (!WebDAVClient::Stat(urlInfo, info)) {
        return NULL;
    }
    
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = 1; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    
    std::wstring nameStr = urlInfo.path;
    if (!nameStr.empty() && nameStr.back() == L'/') nameStr.pop_back();
    size_t pos = nameStr.find_last_of(L'/');
    if (pos != std::wstring::npos) nameStr = nameStr.substr(pos + 1);
    
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, WebDAVClient::Utf8ToWide(WebDAVClient::UrlDecode(WebDAVClient::WideToUtf8(nameStr))).c_str());
    lpFileData->wfdData.dwFileAttributes = info.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL; 
    
    ULARGE_INTEGER sz; sz.QuadPart = info.size; lpFileData->wfdData.nFileSizeHigh = sz.HighPart; lpFileData->wfdData.nFileSizeLow = sz.LowPart;
    lpFileData->wfdData.ftLastWriteTime = info.lastModified;
    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT) {
    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszFile, urlInfo)) return NULL;
    WebDAVClient::PrepareAuth(NULL, urlInfo);
    return (dwMode & GENERIC_WRITE) ? (HANDLE)WebDAVClient::BeginUpload(urlInfo) : (HANDLE)WebDAVClient::BeginDownload(urlInfo);
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    WebDAVClient::FileStreamContext* ctx = (WebDAVClient::FileStreamContext*)hFile; 
    if (!ctx || ctx->isWrite || !ctx->hRequest) return FALSE;
    BOOL bRet = WinHttpReadData(ctx->hRequest, lpData, dwSize, lpdwReadSize);
    if (bRet && lpdwReadSize && *lpdwReadSize == 0) {
        SetLastError(0);
        return FALSE; 
    }
    return bRet;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize) {
    WebDAVClient::FileStreamContext* ctx = (WebDAVClient::FileStreamContext*)hFile; 
    if (!ctx || !ctx->isWrite || !ctx->hRequest) return FALSE;
    if (dwSize == 0) return TRUE;
    char chunkHeader[32]; sprintf_s(chunkHeader, "%X\r\n", dwSize); DWORD bytesWritten = 0;
    if (!WinHttpWriteData(ctx->hRequest, chunkHeader, (DWORD)strlen(chunkHeader), &bytesWritten)) return FALSE;
    if (!WinHttpWriteData(ctx->hRequest, lpData, dwSize, &bytesWritten)) return FALSE;
    if (!WinHttpWriteData(ctx->hRequest, "\r\n", 2, &bytesWritten)) return FALSE;
    if (lpdwWriteSize) *lpdwWriteSize = dwSize; return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    WebDAVClient::FileStreamContext* ctx = (WebDAVClient::FileStreamContext*)hFile; 
    if (!ctx) return;
    if (ctx->isWrite && ctx->hRequest) {
        DWORD bw; WinHttpWriteData(ctx->hRequest, "0\r\n\r\n", 5, &bw);
        if (WinHttpReceiveResponse(ctx->hRequest, NULL)) {
            DWORD dwSize = 0, dwDownloaded = 0; char discardBuf[1024];
            do {
                if (!WinHttpQueryDataAvailable(ctx->hRequest, &dwSize) || dwSize == 0) break;
                if (dwSize > sizeof(discardBuf)) dwSize = sizeof(discardBuf);
                if (!WinHttpReadData(ctx->hRequest, discardBuf, dwSize, &dwDownloaded)) break;
            } while (dwSize > 0);
        }
    }
    if (ctx->hRequest) WinHttpCloseHandle(ctx->hRequest);
    if (ctx->hConnect) WinHttpCloseHandle(ctx->hConnect);
    delete ctx; 
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszFile, urlInfo)) return FALSE;
    WebDAVClient::PrepareAuth(NULL, urlInfo); if (WebDAVClient::Delete(urlInfo)) return TRUE;
    SetLastError(ERROR_ACCESS_DENIED); return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszPath, urlInfo)) return FALSE;
    WebDAVClient::PrepareAuth(NULL, urlInfo); return WebDAVClient::MakeDir(urlInfo) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    WebDAVUrl srcUrl, destUrl; if (!WebDAVClient::ParseUrl(lpszOldName, srcUrl) || !WebDAVClient::ParseUrl(lpszNewName, destUrl)) return FALSE;
    WebDAVClient::PrepareAuth(NULL, srcUrl); return WebDAVClient::Move(srcUrl, destUrl) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return VFS_RenameFileW(hData, lpFuncData, lpszOldName, lpszNewName); }

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData) { return GetLastError(); }

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;
    if (_wcsicmp(lpszPath, L"dav://") == 0 || _wcsicmp(lpszPath, L"davs://") == 0) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"DAV"); return TRUE; }
    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszPath, urlInfo)) return FALSE;
    if (urlInfo.path == L"/" || urlInfo.path.empty()) {
        std::wstring hostDisp = urlInfo.host;
        if (urlInfo.port != 80 && urlInfo.port != 443) hostDisp += L":" + std::to_wstring(urlInfo.port);
        StringCchCopyW(lpszDisplayName, cbDisplayNameMax, hostDisp.c_str()); return TRUE;
    }
    std::wstring path = urlInfo.path;
    if (!path.empty() && path.back() == L'/') path.pop_back();
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash != std::wstring::npos) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, WebDAVClient::Utf8ToWide(WebDAVClient::UrlDecode(WebDAVClient::WideToUtf8(path.substr(lastSlash + 1)))).c_str()); return TRUE; } return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, _wcsnicmp(lpszPath, L"davs://", 7) == 0 ? L"davs://" : L"dav://"); return TRUE; }
    if (_wcsicmp(lpszPath, L"dav://") == 0 || _wcsicmp(lpszPath, L"davs://") == 0) return FALSE;
    WebDAVUrl urlInfo; if (!WebDAVClient::ParseUrl(lpszPath, urlInfo)) return FALSE;
    if (urlInfo.path.empty() || urlInfo.path == L"/") { StringCchPrintfW(lpszNewPath, cbNewPathMax, L"%s://", urlInfo.scheme.c_str()); return TRUE; }
    std::wstring path = urlInfo.path;
    if (!path.empty() && path.back() == L'/') path.pop_back();
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash != std::wstring::npos) {
        std::wstring parentPath = path.substr(0, lastSlash + 1);
        if (parentPath == L"/") { StringCchPrintfW(lpszNewPath, cbNewPathMax, L"%s://%s%s", urlInfo.scheme.c_str(), urlInfo.host.c_str(), (urlInfo.port != 80 && urlInfo.port != 443) ? (L":" + std::to_wstring(urlInfo.port)).c_str() : L""); } 
        else { StringCchPrintfW(lpszNewPath, cbNewPathMax, L"%s://%s%s%s", urlInfo.scheme.c_str(), urlInfo.host.c_str(), (urlInfo.port != 80 && urlInfo.port != 443) ? (L":" + std::to_wstring(urlInfo.port)).c_str() : L"", parentPath.c_str()); }
        return TRUE;
    } return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
        case VFSPROP_FUNCAVAILABILITY: {
            unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
            *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_MOVE | VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_MAKEDIR | VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE;
            return TRUE;
        }
        case VFSPROP_GETFOLDERICON: return FALSE;
        case VFSPROP_GETVALIDACTIONS: case VFSPROP_CANSHOWSUBFOLDERS: case VFSPROP_SHOWTHUMBNAILS: case VFSPROP_USEFULLRENAME: *reinterpret_cast<LPBOOL>(lpPropData) = TRUE; return TRUE;
        case VFSPROP_SHOWFILEINFO: *reinterpret_cast<LPBOOL>(lpPropData) = FALSE; return TRUE;
    } return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize) {
    if (hFile) { WebDAVClient::FileStreamContext* ctx = (WebDAVClient::FileStreamContext*)hFile; if (piFileSize) *piFileSize = ctx->fileSize; return TRUE; } 
    else if (lpszPath) {
        WebDAVUrl urlInfo;
        if (WebDAVClient::ParseUrl(lpszPath, urlInfo)) {
            WebDAVClient::PrepareAuth(NULL, urlInfo); WebDAVFileInfo info;
            if (WebDAVClient::Stat(urlInfo, info)) { if (piFileSize) *piFileSize = info.size; return TRUE; }
        }
    } return FALSE;
}
