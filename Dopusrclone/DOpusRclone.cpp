#include <windows.h>
#include <commdlg.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include "resource.h"

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comdlg32.lib")

#define DOPUS_PLUGIN_HELPER
#include "headers/vfs plugins.h"
#include "headers/plugin support.h"
#include "RcloneClient.h"
#include "Utils.h"

static HMODULE g_hModule = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) g_hModule = hModule;
    return TRUE;
}

std::atomic<bool> g_PluginUnloading(false);
std::atomic<int> g_ActiveThreads(0);

static const GUID GUIDPlugin_Rclone = 
{ 0x8A1B2C3D, 0x4E5F, 0x6A7B, { 0x8C, 0x9D, 0x0E, 0x1F, 0x2A, 0x3B, 0x4C, 0x5D } };

struct RCLONE_VFS_DATA {
    HWND hwndMsgWindow;
};

struct VFS_FILE_CONTEXT {
    bool isWrite;
    std::wstring targetFs;
    std::wstring targetRemote;

    HINTERNET hHttpRequest; 

    HANDLE hReadPipe;
    HANDLE hProcess;
    uint64_t seekPos;
    uint64_t streamPos;
    uint64_t fileSize;

    VFS_FILE_CONTEXT() : isWrite(false), hHttpRequest(NULL), hReadPipe(NULL), hProcess(NULL), seekPos(0), streamPos(0), fileSize(0) {}

    ~VFS_FILE_CONTEXT() {
        if (hHttpRequest) WinHttpCloseHandle(hHttpRequest);
        if (hReadPipe) CloseHandle(hReadPipe);
        if (hProcess) { TerminateProcess(hProcess, 0); CloseHandle(hProcess); }
    }

    bool StartStreamProcess() {
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            CloseHandle(hReadPipe);
            hProcess = NULL;
            hReadPipe = NULL;
        }

        SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hPipeWrite = NULL;
        if (!CreatePipe(&hReadPipe, &hPipeWrite, &saAttr, 0)) return false;
        
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0); 

        STARTUPINFOW si = {sizeof(si)};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hPipeWrite; 
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE); 

        std::wstring exe = RcloneClient::GetRcloneExePath();
        std::wstring cmd = L"\"" + exe + L"\" cat --offset " + std::to_wstring(seekPos) + L" \"" + targetFs + targetRemote + L"\"";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        PROCESS_INFORMATION pi = {0};
        BOOL res = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        
        CloseHandle(hPipeWrite);

        if (res) {
            hProcess = pi.hProcess;
            CloseHandle(pi.hThread);
            streamPos = seekPos;
            return true;
        } else {
            CloseHandle(hReadPipe);
            hReadPipe = NULL;
            return false;
        }
    }
};

void NormalizePath(std::wstring& path) {
    for (auto& c : path) { if (c == L'\\') c = L'/'; }
    while (path.length() > 9 && path[9] == L'/') {
        path.erase(9, 1);
    }
    size_t pos;
    while ((pos = path.find(L"//", 9)) != std::wstring::npos) {
        path.erase(pos, 1);
    }
    while (path.length() > 9 && path.back() == L'/') path.pop_back();
}

bool ParseRclonePath(std::wstring fullPath, std::wstring& fs, std::wstring& remote) {
    NormalizePath(fullPath);
    if (fullPath.length() < 9) return false;
    if (fullPath.length() == 9) { fs = L""; remote = L""; return true; }
    std::wstring pathWithoutProto = fullPath.substr(9);
    size_t firstSlash = pathWithoutProto.find(L'/');
    if (firstSlash == std::wstring::npos) { 
        fs = pathWithoutProto + L":"; remote = L""; 
    } else { 
        fs = pathWithoutProto.substr(0, firstSlash) + L":"; 
        remote = pathWithoutProto.substr(firstSlash + 1); 
    }
    return true;
}

bool IsFileLocked(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            return true;
        }
    } else {
        CloseHandle(hFile);
    }
    return false;
}

void MonitorAndAutoSync(std::wstring localPath, std::wstring uniqueFolder, std::wstring fs, std::wstring remote) {
    g_ActiveThreads++;
    std::thread([localPath, uniqueFolder, fs, remote]() {
        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!GetFileAttributesExW(localPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
            g_ActiveThreads--;
            return;
        }
        
        FILETIME lastMod = fileInfo.ftLastWriteTime;
        
        const int CHECK_INTERVAL = 2;
        const int MAX_IDLE_SECONDS = 300;
        const int MAX_LIFESPAN_SECONDS = 12 * 3600;
        
        int maxLifespanLoops = MAX_LIFESPAN_SECONDS / CHECK_INTERVAL;
        int idleSeconds = 0;
        
        while (maxLifespanLoops-- > 0) {
            if (g_PluginUnloading.load()) break;
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
            
            if (!GetFileAttributesExW(localPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
                break;
            }

            if (CompareFileTime(&fileInfo.ftLastWriteTime, &lastMod) > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                RcloneClient::CopyLocalToRemote(localPath, fs, remote);
                
                if (GetFileAttributesExW(localPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
                    lastMod = fileInfo.ftLastWriteTime;
                }
                idleSeconds = 0;
            } else {
                idleSeconds += CHECK_INTERVAL;
                if (idleSeconds >= MAX_IDLE_SECONDS) {
                    if (!IsFileLocked(localPath)) {
                        break;
                    }
                }
            }
        }
        
        ::DeleteFileW(localPath.c_str());
        ::RemoveDirectoryW(uniqueFolder.c_str());
        g_ActiveThreads--;
    }).detach();
}

// ============================================================================
// Configuration dialog support
// ============================================================================

struct RcloneConfig {
    std::wstring rclonePath;
    std::wstring defaultRemote;
    int cacheTimeout = 60;
    int uploadChunkKB = 2048;
    int dlBufferKB = 8192;
    bool showHidden = false;
    bool confirmDelete = true;
    bool confirmOverwrite = true;
    bool verboseLog = false;
};

static RcloneConfig g_rcloneConfig;
static int g_currentPage = 0;

static const wchar_t* RCLONE_REG_KEY = L"Software\\DOpusRclone";

static void LoadRcloneConfig() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RCLONE_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    wchar_t buf[1024];
    DWORD dwSize, dwType;

    dwSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"RclonePath", NULL, &dwType, (LPBYTE)buf, &dwSize) == ERROR_SUCCESS && dwType == REG_SZ)
        g_rcloneConfig.rclonePath = buf;

    dwSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"DefaultRemote", NULL, &dwType, (LPBYTE)buf, &dwSize) == ERROR_SUCCESS && dwType == REG_SZ)
        g_rcloneConfig.defaultRemote = buf;

    DWORD dwVal;
    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"CacheTimeout", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.cacheTimeout = (int)dwVal;

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"UploadChunkKB", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.uploadChunkKB = (int)dwVal;

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"DlBufferKB", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.dlBufferKB = (int)dwVal;

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ShowHidden", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.showHidden = (dwVal != 0);

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ConfirmDelete", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.confirmDelete = (dwVal != 0);

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ConfirmOverwrite", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.confirmOverwrite = (dwVal != 0);

    dwSize = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"VerboseLog", NULL, &dwType, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_rcloneConfig.verboseLog = (dwVal != 0);

    RegCloseKey(hKey);
}

static void SaveRcloneConfig() {
    HKEY hKey;
    DWORD dwDisp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RCLONE_REG_KEY, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &hKey, &dwDisp) != ERROR_SUCCESS)
        return;

    RegSetValueExW(hKey, L"RclonePath", 0, REG_SZ,
        (const BYTE*)g_rcloneConfig.rclonePath.c_str(),
        (DWORD)((g_rcloneConfig.rclonePath.length() + 1) * sizeof(wchar_t)));

    RegSetValueExW(hKey, L"DefaultRemote", 0, REG_SZ,
        (const BYTE*)g_rcloneConfig.defaultRemote.c_str(),
        (DWORD)((g_rcloneConfig.defaultRemote.length() + 1) * sizeof(wchar_t)));

    DWORD dwVal;
    dwVal = (DWORD)g_rcloneConfig.cacheTimeout;
    RegSetValueExW(hKey, L"CacheTimeout", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_rcloneConfig.uploadChunkKB;
    RegSetValueExW(hKey, L"UploadChunkKB", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = (DWORD)g_rcloneConfig.dlBufferKB;
    RegSetValueExW(hKey, L"DlBufferKB", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = g_rcloneConfig.showHidden ? 1 : 0;
    RegSetValueExW(hKey, L"ShowHidden", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = g_rcloneConfig.confirmDelete ? 1 : 0;
    RegSetValueExW(hKey, L"ConfirmDelete", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = g_rcloneConfig.confirmOverwrite ? 1 : 0;
    RegSetValueExW(hKey, L"ConfirmOverwrite", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    dwVal = g_rcloneConfig.verboseLog ? 1 : 0;
    RegSetValueExW(hKey, L"VerboseLog", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));

    RegCloseKey(hKey);
}

static void ResetRcloneDefaults() {
    g_rcloneConfig.rclonePath = L"";
    g_rcloneConfig.defaultRemote = L"";
    g_rcloneConfig.cacheTimeout = 60;
    g_rcloneConfig.uploadChunkKB = 2048;
    g_rcloneConfig.dlBufferKB = 8192;
    g_rcloneConfig.showHidden = false;
    g_rcloneConfig.confirmDelete = true;
    g_rcloneConfig.confirmOverwrite = true;
    g_rcloneConfig.verboseLog = false;
}

static void BrowseRclonePath(HWND hDlg) {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrTitle = L"选择 rclone.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(hDlg, IDC_RCLONE_EXE_PATH, szFile);
    }
}

// IDs for page 0 controls (Paths)
static const int g_page0Ids[] = {
    IDC_RCLONE_LBL_EXE, IDC_RCLONE_EXE_PATH, IDC_RCLONE_BROWSE_EXE,
    IDC_RCLONE_LBL_REMOTE, IDC_RCLONE_DEFAULT_REMOTE, 0
};

// IDs for page 1 controls (Behavior)
static const int g_page1Ids[] = {
    IDC_RCLONE_LBL_CACHE, IDC_RCLONE_CACHE_TIMEOUT,
    IDC_RCLONE_LBL_UPLOAD, IDC_RCLONE_UPLOAD_CHUNK,
    IDC_RCLONE_LBL_DL_BUF, IDC_RCLONE_DL_BUFFER,
    IDC_RCLONE_CHK_HIDDEN, IDC_RCLONE_CHK_DEL,
    IDC_RCLONE_CHK_OVERWRITE, IDC_RCLONE_CHK_VERBOSE, 0
};

static void ShowRcloneNavPage(HWND hDlg, int page) {
    g_currentPage = page;
    const int* showIds = (page == 0) ? g_page0Ids : g_page1Ids;
    const int* hideIds = (page == 0) ? g_page1Ids : g_page0Ids;

    for (int i = 0; hideIds[i] != 0; i++)
        ShowWindow(GetDlgItem(hDlg, hideIds[i]), SW_HIDE);
    for (int i = 0; showIds[i] != 0; i++)
        ShowWindow(GetDlgItem(hDlg, showIds[i]), SW_SHOW);
}

static void SetRcloneChineseText(HWND hDlg) {
    // Page 0 labels
    SetDlgItemTextW(hDlg, IDC_RCLONE_LBL_EXE, L"Rclone 可执行文件路径:");
    SetDlgItemTextW(hDlg, IDC_RCLONE_LBL_REMOTE, L"默认远程名称:");

    // Page 1 labels
    SetDlgItemTextW(hDlg, IDC_RCLONE_LBL_CACHE, L"缓存超时 (秒):");
    SetDlgItemTextW(hDlg, IDC_RCLONE_LBL_UPLOAD, L"上传块大小 (KB):");
    SetDlgItemTextW(hDlg, IDC_RCLONE_LBL_DL_BUF, L"下载缓冲大小 (KB):");
    SetDlgItemTextW(hDlg, IDC_RCLONE_CHK_HIDDEN, L"显示隐藏文件");
    SetDlgItemTextW(hDlg, IDC_RCLONE_CHK_DEL, L"删除前确认");
    SetDlgItemTextW(hDlg, IDC_RCLONE_CHK_OVERWRITE, L"覆盖前确认");
    SetDlgItemTextW(hDlg, IDC_RCLONE_CHK_VERBOSE, L"详细日志");
}

static void InitRcloneDialogControls(HWND hDlg) {
    // If rclonePath is empty, auto-detect
    std::wstring exePath = g_rcloneConfig.rclonePath;
    if (exePath.empty())
        exePath = RcloneClient::GetRcloneExePath();

    SetDlgItemTextW(hDlg, IDC_RCLONE_EXE_PATH, exePath.c_str());
    SetDlgItemTextW(hDlg, IDC_RCLONE_DEFAULT_REMOTE, g_rcloneConfig.defaultRemote.c_str());

    SetDlgItemInt(hDlg, IDC_RCLONE_CACHE_TIMEOUT, g_rcloneConfig.cacheTimeout, FALSE);
    SetDlgItemInt(hDlg, IDC_RCLONE_UPLOAD_CHUNK, g_rcloneConfig.uploadChunkKB, FALSE);
    SetDlgItemInt(hDlg, IDC_RCLONE_DL_BUFFER, g_rcloneConfig.dlBufferKB, FALSE);

    CheckDlgButton(hDlg, IDC_RCLONE_CHK_HIDDEN, g_rcloneConfig.showHidden ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_RCLONE_CHK_DEL, g_rcloneConfig.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_RCLONE_CHK_OVERWRITE, g_rcloneConfig.confirmOverwrite ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_RCLONE_CHK_VERBOSE, g_rcloneConfig.verboseLog ? BST_CHECKED : BST_UNCHECKED);
}

static bool SaveRcloneDialogControls(HWND hDlg) {
    wchar_t buf[1024];

    GetDlgItemTextW(hDlg, IDC_RCLONE_EXE_PATH, buf, 1024);
    g_rcloneConfig.rclonePath = buf;

    GetDlgItemTextW(hDlg, IDC_RCLONE_DEFAULT_REMOTE, buf, 1024);
    g_rcloneConfig.defaultRemote = buf;

    g_rcloneConfig.cacheTimeout = GetDlgItemInt(hDlg, IDC_RCLONE_CACHE_TIMEOUT, NULL, FALSE);
    g_rcloneConfig.uploadChunkKB = GetDlgItemInt(hDlg, IDC_RCLONE_UPLOAD_CHUNK, NULL, FALSE);
    g_rcloneConfig.dlBufferKB = GetDlgItemInt(hDlg, IDC_RCLONE_DL_BUFFER, NULL, FALSE);

    g_rcloneConfig.showHidden = (IsDlgButtonChecked(hDlg, IDC_RCLONE_CHK_HIDDEN) == BST_CHECKED);
    g_rcloneConfig.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_RCLONE_CHK_DEL) == BST_CHECKED);
    g_rcloneConfig.confirmOverwrite = (IsDlgButtonChecked(hDlg, IDC_RCLONE_CHK_OVERWRITE) == BST_CHECKED);
    g_rcloneConfig.verboseLog = (IsDlgButtonChecked(hDlg, IDC_RCLONE_CHK_VERBOSE) == BST_CHECKED);

    SaveRcloneConfig();
    return true;
}

static INT_PTR CALLBACK RcloneConfigProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        LoadRcloneConfig();

        // Populate nav listbox
        HWND hList = GetDlgItem(hDlg, IDC_RCLONE_NAV_LIST);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"路径设置");
        SendMessageW(hList, LB_ADDSTRING, 1, (LPARAM)L"行为设置");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);

        SetRcloneChineseText(hDlg);
        InitRcloneDialogControls(hDlg);
        ShowRcloneNavPage(hDlg, 0);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_RCLONE_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                int sel = (int)SendDlgItemMessageW(hDlg, IDC_RCLONE_NAV_LIST, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                    ShowRcloneNavPage(hDlg, sel);
            }
            return TRUE;

        case IDC_RCLONE_BROWSE_EXE:
            BrowseRclonePath(hDlg);
            return TRUE;

        case IDC_RCLONE_RESET:
            ResetRcloneDefaults();
            InitRcloneDialogControls(hDlg);
            return TRUE;

        case IDC_RCLONE_APPLY:
            SaveRcloneDialogControls(hDlg);
            return TRUE;

        case IDOK:
            SaveRcloneDialogControls(hDlg);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK RcloneAboutProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_RCLONE_CONFIG), hWndParent, RcloneConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_RCLONE_ABOUT), hWndParent, RcloneAboutProc, 0);
    return NULL;
}

// ============================================================================
// VFS Plugin exports
// ============================================================================

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) { return TRUE; }

extern "C" __declspec(dllexport) void VFS_Uninit() { 
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    RcloneClient::StopDaemon(); 
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    if (!RcloneClient::EnsureDaemonStarted()) {
        return NULL;
    }
    RCLONE_VFS_DATA* pData = new RCLONE_VFS_DATA;
    pData->hwndMsgWindow = hwndMsgWindow;
    return (HANDLE)pData;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
    if (hVFSData) delete (RCLONE_VFS_DATA*)hVFSData;
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    lpVFSInfo->idPlugin = GUIDPlugin_Rclone;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE;
    
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_MOVEBYRENAME | VFSCAPABILITY_CASESENSITIVE | VFSCAPABILITY_RANDOMSEEK; 
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;

    if (lpVFSInfo->lpszHandlePrefix) StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"rclone://");
    if (lpVFSInfo->lpszName) StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"Rclone");

    if (lpVFSInfo->lpszDescription) {
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, 
            L"Cloud storage integration via Rclone.");
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
        case VFSPROP_FUNCAVAILABILITY: {
            unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
            *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_MOVE | VFSFUNCAVAIL_DELETE | 
                      VFSFUNCAVAIL_MAKEDIR | VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_PROPERTIES |
                      VFSFUNCAVAIL_SETATTR | VFSFUNCAVAIL_SETTIME | VFSFUNCAVAIL_SETCOMMENT |
                      VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE;
            return TRUE;
        }
        case VFSPROP_GETVALIDACTIONS: {
            return TRUE;
        }
        case VFSPROP_CANSHOWSUBFOLDERS:
        case VFSPROP_SHOWTHUMBNAILS:
        case VFSPROP_USEFULLRENAME:
            *reinterpret_cast<LPBOOL>(lpPropData) = TRUE;
            return TRUE;
        case VFSPROP_SHOWFILEINFO:
            *reinterpret_cast<LPBOOL>(lpPropData) = FALSE;
            return TRUE;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    if (!RcloneClient::EnsureDaemonStarted()) return VFSCVRES_FAIL;

    if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        return VFSCVRES_CHANGEDIR; 
    } 

    bool isDefaultOpen = (lpVerbData->lpszVerb == NULL || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0);
    
    if (isDefaultOpen) {
        std::wstring fs, remote;
        if (!ParseRclonePath(lpVerbData->lpszPath, fs, remote)) return VFSCVRES_FAIL;

        WCHAR tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        WCHAR folderName[64];
        static std::atomic<int> s_tempCounter(0);
        StringCchPrintfW(folderName, 64, L"DOpusRCL.%lu.%lu.%d\\", GetCurrentProcessId(), GetTickCount(), ++s_tempCounter);
        std::wstring uniqueFolder = std::wstring(tempPath) + folderName;
        CreateDirectoryW(uniqueFolder.c_str(), NULL);

        std::wstring remoteStr = remote;
        size_t slashPos = remoteStr.find_last_of(L"\\/");
        std::wstring fileName = (slashPos != std::wstring::npos) ? remoteStr.substr(slashPos + 1) : remoteStr;
        std::wstring localFile = uniqueFolder + fileName;

        if (!RcloneClient::CopyFileToLocal(fs, remote, localFile)) {
            RemoveDirectoryW(uniqueFolder.c_str());
            return VFSCVRES_FAIL;
        }

        MonitorAndAutoSync(localFile, uniqueFolder, fs, remote);
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, localFile.c_str());
        return VFSCVRES_CHANGE; 
    }
    return VFSCVRES_FAIL; 
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;
    std::wstring path(lpszPath); NormalizePath(path);
    if (path.length() <= 9) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Rclone"); return TRUE; }
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash != std::wstring::npos && lastSlash >= 8) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, path.substr(lastSlash + 1).c_str()); return TRUE; }
    return FALSE;
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    if (!RcloneClient::EnsureDaemonStarted()) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    std::wstring fs, remote; 
    if (!ParseRclonePath(lpszPath, fs, remote)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }
    
    RcloneFileInfo info; 
    bool isRoot = remote.empty();
    
    if (!isRoot && !RcloneClient::Stat(fs, remote, info)) { 
        SetLastError(ERROR_PATH_NOT_FOUND); 
        return NULL; 
    }

    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); 
    lpFDH->iNumItems = 1; 
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    
    WCHAR dispName[MAX_PATH] = { 0 }; 
    VFS_GetPathDisplayNameW(hData, lpszPath, dispName, MAX_PATH);
    std::wstring nameStr = dispName;
    size_t pos = nameStr.find_last_of(L"\\/");
    if (pos != std::wstring::npos) nameStr = nameStr.substr(pos + 1);
    
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, nameStr.c_str());
    lpFileData->wfdData.dwFileAttributes = (isRoot || info.isDir) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL; 
    
    ULARGE_INTEGER sz; sz.QuadPart = info.size; 
    lpFileData->wfdData.nFileSizeHigh = sz.HighPart; 
    lpFileData->wfdData.nFileSizeLow = sz.LowPart;
    lpFileData->wfdData.ftLastWriteTime = info.modTime;
    
    return lpFDH;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPDWORD lpdwAttr) {
    if (!lpszPath || !lpdwAttr) return FALSE;
    std::wstring fs, remote; 
    if (!ParseRclonePath(lpszPath, fs, remote)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    
    if (remote.empty()) { 
        *lpdwAttr = FILE_ATTRIBUTE_DIRECTORY; 
        return TRUE; 
    }
    
    RcloneFileInfo info;
    if (RcloneClient::Stat(fs, remote, info)) { 
        *lpdwAttr = info.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL; 
        return TRUE; 
    }
    
    SetLastError(ERROR_FILE_NOT_FOUND); 
    return FALSE; 
}

extern "C" __declspec(dllexport) BOOL VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || lpRDD->vfsReadOp == VFSREAD_FREEDIR) return TRUE;

    if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR || lpRDD->vfsReadOp == VFSREAD_NORMAL || lpRDD->vfsReadOp == VFSREAD_REFRESH) {
        std::wstring fs, remote;
        if (!ParseRclonePath(lpRDD->lpszPath, fs, remote)) {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return FALSE;
        }
        
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) {
            SetLastError(ERROR_CANCELLED);
            return FALSE;
        }

        if (fs.empty()) {
            if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE;
            auto remotes = RcloneClient::ListRemotes();
            int numItems = (int)remotes.size();
            size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
            LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
            if (!lpFDH) return FALSE;
            
            lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = numItems; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW); 
            if (numItems > 0) {
                LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
                for (int i = 0; i < numItems; ++i) {
                    if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
                    StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, remotes[i].c_str());
                    lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                    GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);
                }
            }
            lpRDD->lpFileData = lpFDH;
            return TRUE;
        }

        RcloneFileInfo dirInfo;
        if (!RcloneClient::Stat(fs, remote, dirInfo)) {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return FALSE;
        }
        if (!dirInfo.isDir) {
            SetLastError(ERROR_DIRECTORY);
            return FALSE;
        }
        
        if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE; 

        auto fileList = RcloneClient::ListDirectory(fs, remote);
        int numItems = (int)fileList.size();
        
        size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
        if (!lpFDH) return FALSE;
        
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); 
        lpFDH->iNumItems = numItems;
        lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

        if (numItems > 0) {
            LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
            for (int i = 0; i < numItems; ++i) {
                if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
                
                std::wstring nameStr = fileList[i].name;
                size_t pos = nameStr.find_last_of(L"\\/");
                if (pos != std::wstring::npos) nameStr = nameStr.substr(pos + 1);

                StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, nameStr.c_str());
                lpFileData[i].wfdData.dwFileAttributes = fileList[i].isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                ULARGE_INTEGER sz; sz.QuadPart = fileList[i].size;
                lpFileData[i].wfdData.nFileSizeHigh = sz.HighPart; lpFileData[i].wfdData.nFileSizeLow = sz.LowPart;
                lpFileData[i].wfdData.ftLastWriteTime = fileList[i].modTime;
            }
        }
        
        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT) {
    std::wstring fs, remote; if (!ParseRclonePath(lpszFile, fs, remote)) return NULL;
    
    bool isWrite = (dwMode & GENERIC_WRITE) != 0;
    VFS_FILE_CONTEXT* ctx = new VFS_FILE_CONTEXT();
    ctx->isWrite = isWrite;
    ctx->targetFs = fs;
    ctx->targetRemote = remote;

    if (isWrite) {
        std::wstring uploadDir = L"";
        std::wstring fileNameW = remote;
        size_t lastSlash = remote.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            uploadDir = remote.substr(0, lastSlash);
            fileNameW = remote.substr(lastSlash + 1);
        }

        std::string urlPath = "/operations/uploadfile?fs=" + UrlEncode(WideToUtf8(fs)) + "&remote=" + UrlEncode(WideToUtf8(uploadDir));
        std::wstring wUrlPath = Utf8ToWide(urlPath);

        ctx->hHttpRequest = WinHttpOpenRequest(RcloneClient::s_hConnect, L"POST", wUrlPath.c_str(), 
                                               NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!ctx->hHttpRequest) { delete ctx; return NULL; }
        
        std::wstring headers = L"Authorization: Basic " + Utf8ToWide(RcloneClient::Base64Encode("opus:" + RcloneClient::s_rcPass)) + L"\r\n"
                               L"Transfer-Encoding: chunked\r\n"
                               L"Content-Type: multipart/form-data; boundary=----DOpusRcloneBoundaryXYZ\r\n";
        
        BOOL bRes = WinHttpSendRequest(ctx->hHttpRequest, headers.c_str(), -1, 
                                       WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0);
        if (!bRes) { delete ctx; return NULL; }

        std::string utf8FileName = WideToUtf8(fileNameW);
        std::string preamble = "------DOpusRcloneBoundaryXYZ\r\n"
                               "Content-Disposition: form-data; name=\"file\"; filename=\"" + utf8FileName + "\"\r\n"
                               "Content-Type: application/octet-stream\r\n\r\n";

        char chunkHeader[32];
        DWORD bw;
        sprintf_s(chunkHeader, "%X\r\n", (DWORD)preamble.length());
        WinHttpWriteData(ctx->hHttpRequest, chunkHeader, (DWORD)strlen(chunkHeader), &bw);
        WinHttpWriteData(ctx->hHttpRequest, (LPVOID)preamble.c_str(), (DWORD)preamble.length(), &bw);
        WinHttpWriteData(ctx->hHttpRequest, "\r\n", 2, &bw);

    } else {
        RcloneFileInfo info;
        if (!RcloneClient::Stat(fs, remote, info)) {
            delete ctx;
            SetLastError(ERROR_FILE_NOT_FOUND);
            return NULL;
        }
        ctx->fileSize = info.size;
        
        if (!ctx->StartStreamProcess()) {
            delete ctx;
            return NULL;
        }
    }
    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    VFS_FILE_CONTEXT* ctx = (VFS_FILE_CONTEXT*)hFile; 
    if (!ctx || ctx->isWrite) return FALSE;

    if (ctx->seekPos != ctx->streamPos || !ctx->hReadPipe) {
        if (!ctx->StartStreamProcess()) return FALSE;
    }

    BOOL res = ::ReadFile(ctx->hReadPipe, lpData, dwSize, lpdwReadSize, NULL);
    if (res && *lpdwReadSize > 0) {
        ctx->seekPos += *lpdwReadSize;
        ctx->streamPos += *lpdwReadSize;
    }
    return res;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize) {
    VFS_FILE_CONTEXT* ctx = (VFS_FILE_CONTEXT*)hFile; 
    if (!ctx || !ctx->isWrite || !ctx->hHttpRequest) return FALSE;
    if (dwSize == 0) return TRUE;
    
    char chunkHeader[32];
    sprintf_s(chunkHeader, "%X\r\n", dwSize);
    
    DWORD bytesWritten = 0;
    
    if (!WinHttpWriteData(ctx->hHttpRequest, chunkHeader, (DWORD)strlen(chunkHeader), &bytesWritten)) return FALSE;
    if (!WinHttpWriteData(ctx->hHttpRequest, lpData, dwSize, &bytesWritten)) return FALSE;
    if (!WinHttpWriteData(ctx->hHttpRequest, "\r\n", 2, &bytesWritten)) return FALSE;

    if (lpdwWriteSize) *lpdwWriteSize = dwSize;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos) {
    VFS_FILE_CONTEXT* ctx = (VFS_FILE_CONTEXT*)hFile; 
    if (!ctx) return FALSE;

    if (ctx->isWrite) {
        return FALSE; 
    } else {
        uint64_t newPos = 0;
        if (dwMethod == FILE_BEGIN) newPos = iPos;
        else if (dwMethod == FILE_CURRENT) newPos = ctx->seekPos + iPos;
        else if (dwMethod == FILE_END) newPos = ctx->fileSize + iPos;
        
        ctx->seekPos = newPos;
        if (piNewPos) *piNewPos = newPos;
        return TRUE;
    }
}
extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize) {
    if (hFile) {
        VFS_FILE_CONTEXT* ctx = (VFS_FILE_CONTEXT*)hFile;
        if (piFileSize) *piFileSize = ctx->fileSize;
        return TRUE;
    } else if (lpszPath) {
        std::wstring fs, remote;
        if (ParseRclonePath(lpszPath, fs, remote)) {
            RcloneFileInfo info;
            if (RcloneClient::Stat(fs, remote, info)) {
                if (piFileSize) *piFileSize = info.size;
                return TRUE;
            }
        }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    VFS_FILE_CONTEXT* ctx = (VFS_FILE_CONTEXT*)hFile; 
    if (!ctx) return;
    
    if (ctx->isWrite && ctx->hHttpRequest) {
        DWORD bw;
        
        std::string epilogue = "\r\n------DOpusRcloneBoundaryXYZ--\r\n";
        char chunkHeader[32];
        sprintf_s(chunkHeader, "%X\r\n", (DWORD)epilogue.length());
        
        WinHttpWriteData(ctx->hHttpRequest, chunkHeader, (DWORD)strlen(chunkHeader), &bw);
        WinHttpWriteData(ctx->hHttpRequest, (LPVOID)epilogue.c_str(), (DWORD)epilogue.length(), &bw);
        WinHttpWriteData(ctx->hHttpRequest, "\r\n", 2, &bw);

        WinHttpWriteData(ctx->hHttpRequest, "0\r\n\r\n", 5, &bw);
        
        if (WinHttpReceiveResponse(ctx->hHttpRequest, NULL)) {
            DWORD dwSize = 0, dwDownloaded = 0;
            char buf[8192];
            do {
                WinHttpQueryDataAvailable(ctx->hHttpRequest, &dwSize);
                if (dwSize > 0) {
                    DWORD bytesToRead = (dwSize > sizeof(buf)) ? sizeof(buf) : dwSize;
                    WinHttpReadData(ctx->hHttpRequest, buf, bytesToRead, &dwDownloaded);
                }
            } while (dwSize > 0);
            
            RcloneClient::InvalidateCache();
        }
    }
    
    delete ctx; 
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    std::wstring fs, remote; if (!ParseRclonePath(lpszFile, fs, remote)) return FALSE;
    if (RcloneClient::DeleteFile(fs, remote)) {
        return TRUE;
    }
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;
    std::wstring targetPath = lpszFiles; 
    if (targetPath.empty()) return NULL;

    std::wstring fs, remote;
    if (!ParseRclonePath(targetPath, fs, remote)) return NULL;

    RcloneFileInfo info;
    if (!RcloneClient::Stat(fs, remote, info)) {
        MessageBoxW(hwndParent, 
            L"Failed to fetch remote file information.\nIt may have been deleted or the network is unreachable.", 
            L"Properties Error", 
            MB_ICONERROR | MB_OK);
        return NULL;
    }

    WCHAR msg[1024];
    if (info.isDir) {
        StringCchPrintfW(msg, 1024, 
            L"Name:\t%s\nType:\tDirectory\nPath:\t%s", 
            info.name.empty() ? L"Root" : info.name.c_str(), 
            targetPath.c_str());
    } else {
        double mbSize = (double)info.size / (1024.0 * 1024.0);
        StringCchPrintfW(msg, 1024, 
            L"Name:\t%s\nType:\tFile\nPath:\t%s\nSize:\t%llu Bytes (%.2f MB)", 
            info.name.c_str(), 
            targetPath.c_str(), 
            info.size, 
            mbSize);
    }

    MessageBoxW(hwndParent, msg, L"Rclone VFS Properties", MB_ICONINFORMATION | MB_OK);
    return NULL; 
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    std::wstring fs, remote; if (!ParseRclonePath(lpszPath, fs, remote)) return FALSE;
    if (RcloneClient::RemoveDir(fs, remote)) {
        return TRUE;
    }
    SetLastError(ERROR_DIR_NOT_EMPTY);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    std::wstring path(lpszPath); NormalizePath(path);
    if (path.length() <= 9) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, L"rclone://"); return TRUE; }
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash == std::wstring::npos || lastSlash < 9) { StringCchCopyW(lpszNewPath, cbNewPathMax, L"rclone://"); return TRUE; }
    StringCchCopyW(lpszNewPath, cbNewPathMax, path.substr(0, lastSlash).c_str()); return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFreeDiskSpaceW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, unsigned __int64* piFreeBytesAvailable, unsigned __int64* piTotalBytes, unsigned __int64* piTotalFreeBytes) {
    if (piFreeBytesAvailable) *piFreeBytesAvailable = 1000000000000ULL;
    if (piTotalBytes) *piTotalBytes = 1000000000000ULL;
    if (piTotalFreeBytes) *piTotalFreeBytes = 1000000000000ULL;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    std::wstring fs, remote; 
    if (!ParseRclonePath(lpszPath, fs, remote)) return FALSE;
    return RcloneClient::MakeDir(fs, remote) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    std::wstring srcFs, srcRemote, dstFs, dstRemote;
    if (!ParseRclonePath(lpszOldName, srcFs, srcRemote)) return FALSE;
    if (!ParseRclonePath(lpszNewName, dstFs, dstRemote)) return FALSE;
    return RcloneClient::Move(srcFs, srcRemote, dstFs, dstRemote) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return VFS_RenameFileW(hData, lpFuncData, lpszOldName, lpszNewName);
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwAttr, BOOL fForDelete) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileTimeW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime) {
    return TRUE; 
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileCommentW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszComment) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    return TRUE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData) {
    return GetLastError(); 
}
