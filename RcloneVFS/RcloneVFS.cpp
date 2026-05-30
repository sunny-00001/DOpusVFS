#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#ifndef LPCBYTE
typedef const BYTE* LPCBYTE;
#endif

static void DebugLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")

#include "headers/vfs plugins.h"
#include "headers/plugin support.h"
#include "resource.h"
#include "RcloneClient.h"
#include "DaemonManager.h"
#include "RcloneCache.h"
#include "RcloneFeatures.h"
#include "ColumnManager.h"
#include "MenuManager.h"
#include "PathParser.h"
#include "Utils.h"
#include "PropertyDialog.h"
#include "ShareDialog.h"
#include "VersionsDialog.h"
#include "SyncDialog.h"
#include "CheckDialog.h"
#include "TrashDialog.h"
#include "ConfigManager.h"
#include "ThreadPool.h"
#include "DebounceRefresh.h"
#include "BatchOperationManager.h"

std::atomic<bool> g_PluginUnloading(false);
std::atomic<int> g_ActiveThreads(0);

static ThreadPool* g_threadPool = nullptr;

HMODULE g_hModule = NULL;
static HWND g_hwndMsgWindow = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

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

    static std::wstring EscapePathArg(const std::wstring& path) {
        std::wstring escaped;
        for (wchar_t c : path) {
            if (c == L'"') escaped += L"\\\"";
            else if (c == L'\\') escaped += L"\\\\";
            else escaped += c;
        }
        return escaped;
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

        std::wstring exe = DaemonManager::GetRcloneExePath();
        std::wstring escapedPath = EscapePathArg(targetFs + targetRemote);
        std::wstring cmd = L"\"" + EscapePathArg(exe) + L"\" cat --offset " + std::to_wstring(seekPos) + L" \"" + escapedPath + L"\"";
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

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) { 
    ConfigLoad(); 
    g_threadPool = new ThreadPool(4);
    return TRUE; 
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    g_PluginUnloading.store(true);
    
    DebounceRefresh::Shutdown();
    
    if (g_threadPool) {
        g_threadPool->waitAll();
        delete g_threadPool;
        g_threadPool = nullptr;
    }
    
    while (g_ActiveThreads.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    DaemonManager::Shutdown();
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    if (!RcloneClient::EnsureDaemonStarted()) {
        if (DaemonManager::IsRestartFailed()) {
            MessageBoxW(hwndMsgWindow,
                L"Rclone 守护进程多次启动失败，请检查：\n"
                L"1. rclone 是否已正确安装\n"
                L"2. 端口 8657 是否被占用\n"
                L"3. 网络连接是否正常\n\n"
                L"点击确定重试。",
                L"Rclone VFS - 连接失败",
                MB_ICONERROR | MB_OK);
            DaemonManager::ResetRestartState();
        }
        return NULL;
    }
    RCLONE_VFS_DATA* pData = new RCLONE_VFS_DATA;
    pData->hwndMsgWindow = hwndMsgWindow;
    g_hwndMsgWindow = hwndMsgWindow;
    return (HANDLE)pData;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
    if (hVFSData) delete (RCLONE_VFS_DATA*)hVFSData;
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    DebugLog("VFS_IdentifyW called: cbSize=%u\n", lpVFSInfo->cbSize);
    lpVFSInfo->idPlugin = GUIDPlugin_Rclone;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE;

    lpVFSInfo->dwCapabilities = VFSCAPABILITY_MOVEBYRENAME | VFSCAPABILITY_CASESENSITIVE | VFSCAPABILITY_RANDOMSEEK | VFSCAPABILITY_ALLOWIMAGECOLUMNS | VFSCAPABILITY_ALLOWMUSICCOLUMNS | VFSCAPABILITY_ALLOWEXTRADATECOLUMNS | VFSCAPABILITY_COMBINEDPROPERTIES;
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;

    if (lpVFSInfo->lpszHandlePrefix) StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"rclone://");
    if (lpVFSInfo->lpszName) StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"Rclone");

    if (lpVFSInfo->lpszDescription) {
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
            L"Cloud storage integration via Rclone.");
    }

    HICON hLarge = NULL, hSmall = NULL;
    SHFILEINFOW sfiL = {}, sfiS = {};
    if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiL, sizeof(sfiL),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON)) {
        hLarge = CopyIcon(sfiL.hIcon);
        DestroyIcon(sfiL.hIcon);
    }
    if (SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfiS, sizeof(sfiS),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON)) {
        hSmall = CopyIcon(sfiS.hIcon);
        DestroyIcon(sfiS.hIcon);
    }
    lpVFSInfo->hIconLarge = hLarge;
    lpVFSInfo->hIconSmall = hSmall;

    return TRUE;
}

static int g_currentPage = 0;

static const int PAGE_BASIC = 0;
static const int PAGE_CONNECTION = 1;
static const int PAGE_CACHE = 2;
static const int PAGE_PERFORMANCE = 3;
static const int PAGE_SYNC = 4;
static const int PAGE_LOG = 5;
static const int PAGE_ADVANCED = 6;

static const int pageBasicCtrls[] = {
    IDC_LBL_RCLONE_PATH, IDC_RCLONE_PATH, IDC_BROWSE_RCLONE,
    IDC_LBL_CONFIG_PATH, IDC_CONFIG_PATH, IDC_BROWSE_CONFIG,
    IDC_AUTO_START_DAEMON, 0
};

static const int pageConnectionCtrls[] = {
    IDC_LBL_RC_PORT, IDC_RC_PORT, IDC_TEST_CONN,
    IDC_LBL_CONN_TIMEOUT, IDC_CONN_TIMEOUT,
    IDC_LBL_RC_USER, IDC_RC_USER,
    IDC_LBL_RC_PASS, IDC_RC_PASS, 0
};

static const int pageCacheCtrls[] = {
    IDC_CACHE_ENABLED,
    IDC_LBL_VFS_CACHE_MODE, IDC_VFS_CACHE_MODE,
    IDC_LBL_CACHE_SIZE, IDC_CACHE_SIZE,
    IDC_LBL_CACHE_MAX_AGE, IDC_CACHE_MAX_AGE,
    IDC_LBL_CACHE_TTL, IDC_CACHE_TTL,
    IDC_LBL_DIR_CACHE_TIME, IDC_DIR_CACHE_TIME,
    IDC_LBL_POLL_INTERVAL, IDC_POLL_INTERVAL,
    IDC_LBL_CACHE_DIR, IDC_CACHE_DIR, IDC_BROWSE_CACHE_DIR, 0
};

static const int pagePerformanceCtrls[] = {
    IDC_LBL_MAX_CONN, IDC_MAX_CONNECTIONS,
    IDC_LBL_TRANSFERS, IDC_TRANSFERS,
    IDC_LBL_CHECKERS, IDC_CHECKERS,
    IDC_LBL_BANDWIDTH, IDC_BANDWIDTH_LIMIT,
    IDC_LBL_BUFFER_SIZE, IDC_BUFFER_SIZE,
    IDC_LBL_READ_CHUNK_SIZE, IDC_READ_CHUNK_SIZE,
    IDC_LBL_READ_AHEAD, IDC_READ_AHEAD,
    IDC_LBL_LOW_LEVEL_RETRIES, IDC_LOW_LEVEL_RETRIES, 0
};

static const int pageSyncCtrls[] = {
    IDC_AUTO_SYNC,
    IDC_LBL_SYNC_INTERVAL, IDC_SYNC_INTERVAL,
    IDC_LBL_WRITE_BACK_DELAY, IDC_WRITE_BACK_DELAY,
    IDC_LBL_MAX_IDLE, IDC_MAX_IDLE,
    IDC_LBL_STATS_INTERVAL, IDC_STATS_INTERVAL,
    IDC_CACHE_WRITE_BACK, 0
};

static const int pageLogCtrls[] = {
    IDC_LOG_ENABLED,
    IDC_LBL_LOG_PATH, IDC_LOG_PATH, IDC_BROWSE_LOG,
    IDC_LBL_LOG_LEVEL, IDC_LOG_LEVEL,
    IDC_LBL_LOG_MAX_SIZE, IDC_LOG_MAX_SIZE,
    IDC_LBL_LOG_MAX_FILES, IDC_LOG_MAX_FILES,
    IDC_OPEN_LOG, IDC_CLEAR_LOG, 0
};

static const int pageAdvancedCtrls[] = {
    IDC_VERBOSE_LOGGING, IDC_CONFIRM_DELETE, IDC_SHOW_HIDDEN,
    IDC_CASE_SENSITIVE, IDC_NO_MODTIME, IDC_NO_CHECKSUM,
    IDC_FAST_LIST, IDC_USE_MMAP, IDC_USE_RCLONE_COPY, 0
};

static const int* g_pageControls[] = {
    pageBasicCtrls, pageConnectionCtrls, pageCacheCtrls,
    pagePerformanceCtrls, pageSyncCtrls, pageLogCtrls, pageAdvancedCtrls
};

static void ShowNavPage(HWND hDlg, int page)
{
    g_currentPage = page;
    for (int i = 0; i < 7; i++)
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
    SetWindowTextW(hDlg, L"Rclone VFS \u914d\u7f6e");

    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList)
    {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < 7; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88");
    SetDlgItemTextW(hDlg, IDC_OPEN_CONFIG, L"\u6253\u5f00\u914d\u7f6e");
    SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\u6062\u590d\u9ed8\u8ba4");

    SetDlgItemTextW(hDlg, IDC_LBL_RCLONE_PATH, L"rclone \u8def\u5f84:");
    SetDlgItemTextW(hDlg, IDC_LBL_CONFIG_PATH, L"\u914d\u7f6e\u6587\u4ef6:");
    SetDlgItemTextW(hDlg, IDC_AUTO_START_DAEMON, L"\u81ea\u52a8\u542f\u52a8\u5b88\u62a4\u8fdb\u7a0b");

    SetDlgItemTextW(hDlg, IDC_LBL_RC_PORT, L"RC \u7aef\u53e3:");
    SetDlgItemTextW(hDlg, IDC_LBL_CONN_TIMEOUT, L"\u8fde\u63a5\u8d85\u65f6(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_RC_USER, L"\u7528\u6237\u540d:");
    SetDlgItemTextW(hDlg, IDC_LBL_RC_PASS, L"\u5bc6\u7801:");
    SetDlgItemTextW(hDlg, IDC_TEST_CONN, L"\u6d4b\u8bd5");

    SetDlgItemTextW(hDlg, IDC_CACHE_ENABLED, L"\u542f\u7528 VFS \u7f13\u5b58");
    SetDlgItemTextW(hDlg, IDC_LBL_VFS_CACHE_MODE, L"\u7f13\u5b58\u6a21\u5f0f:");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_SIZE, L"\u7f13\u5b58\u5927\u5c0f(MB):");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_MAX_AGE, L"\u6700\u5927\u5b58\u6d3b\u65f6\u95f4(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TTL, L"\u7f13\u5b58 TTL(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_DIR_CACHE_TIME, L"\u76ee\u5f55\u7f13\u5b58\u65f6\u95f4(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_POLL_INTERVAL, L"\u8f6e\u8be2\u95f4\u9694(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_DIR, L"\u7f13\u5b58\u76ee\u5f55:");

    SetDlgItemTextW(hDlg, IDC_LBL_MAX_CONN, L"\u6700\u5927\u8fde\u63a5\u6570:");
    SetDlgItemTextW(hDlg, IDC_LBL_TRANSFERS, L"\u4f20\u8f93\u6570:");
    SetDlgItemTextW(hDlg, IDC_LBL_CHECKERS, L"\u6821\u9a8c\u6570:");
    SetDlgItemTextW(hDlg, IDC_LBL_BANDWIDTH, L"\u5e26\u5bbd\u9650\u5236(MB/s):");
    SetDlgItemTextW(hDlg, IDC_LBL_BUFFER_SIZE, L"\u7f13\u51b2\u533a\u5927\u5c0f(KB):");
    SetDlgItemTextW(hDlg, IDC_LBL_READ_CHUNK_SIZE, L"\u8bfb\u5757\u5927\u5c0f(KB):");
    SetDlgItemTextW(hDlg, IDC_LBL_READ_AHEAD, L"\u9884\u8bfb\u5927\u5c0f(KB):");
    SetDlgItemTextW(hDlg, IDC_LBL_LOW_LEVEL_RETRIES, L"\u4f4e\u7ea7\u91cd\u8bd5\u6b21\u6570:");

    SetDlgItemTextW(hDlg, IDC_AUTO_SYNC, L"\u542f\u7528\u81ea\u52a8\u540c\u6b65");
    SetDlgItemTextW(hDlg, IDC_LBL_SYNC_INTERVAL, L"\u540c\u6b65\u95f4\u9694(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_WRITE_BACK_DELAY, L"\u5199\u56de\u5ef6\u8fdf(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_MAX_IDLE, L"\u6700\u5927\u7a7a\u95f2\u65f6\u95f4(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_STATS_INTERVAL, L"\u7edf\u8ba1\u95f4\u9694(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_CACHE_WRITE_BACK, L"\u542f\u7528\u5199\u56de\u7f13\u5b58");

    SetDlgItemTextW(hDlg, IDC_LOG_ENABLED, L"\u542f\u7528\u65e5\u5fd7");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_PATH, L"\u65e5\u5fd7\u6587\u4ef6:");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_LEVEL, L"\u65e5\u5fd7\u7ea7\u522b:");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_MAX_SIZE, L"\u6700\u5927\u65e5\u5fd7\u5927\u5c0f(MB):");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_MAX_FILES, L"\u6700\u5927\u65e5\u5fd7\u6587\u4ef6\u6570:");
    SetDlgItemTextW(hDlg, IDC_OPEN_LOG, L"\u6253\u5f00");
    SetDlgItemTextW(hDlg, IDC_CLEAR_LOG, L"\u6e05\u7a7a");

    SetDlgItemTextW(hDlg, IDC_VERBOSE_LOGGING, L"\u8be6\u7ec6\u65e5\u5fd7");
    SetDlgItemTextW(hDlg, IDC_CONFIRM_DELETE, L"\u5220\u9664\u524d\u786e\u8ba4");
    SetDlgItemTextW(hDlg, IDC_SHOW_HIDDEN, L"\u663e\u793a\u9690\u85cf\u6587\u4ef6");
    SetDlgItemTextW(hDlg, IDC_CASE_SENSITIVE, L"\u533a\u5206\u5927\u5c0f\u5199");
    SetDlgItemTextW(hDlg, IDC_NO_MODTIME, L"\u7981\u7528\u4fee\u6539\u65f6\u95f4");
    SetDlgItemTextW(hDlg, IDC_NO_CHECKSUM, L"\u7981\u7528\u6821\u9a8c\u548c");
    SetDlgItemTextW(hDlg, IDC_FAST_LIST, L"\u4f7f\u7528\u5feb\u901f\u5217\u8868");
    SetDlgItemTextW(hDlg, IDC_USE_MMAP, L"\u4f7f\u7528 mmap");
    SetDlgItemTextW(hDlg, IDC_USE_RCLONE_COPY, L"\u4f7f\u7528 rclone \u539f\u751f\u547d\u4ee4\u590d\u5236/\u79fb\u52a8");
}

static void InitDialogControls(HWND hDlg)
{
    SetChineseText(hDlg);

    CheckDlgButton(hDlg, IDC_AUTO_START_DAEMON, g_config.autoStartDaemon ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CACHE_ENABLED, g_config.cacheEnabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_AUTO_SYNC, g_config.autoSync ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CACHE_WRITE_BACK, g_config.cacheWriteBack ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_LOG_ENABLED, g_config.logEnabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_VERBOSE_LOGGING, g_config.verboseLogging ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CONFIRM_DELETE, g_config.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_HIDDEN, g_config.showHidden ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CASE_SENSITIVE, g_config.caseSensitive ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_NO_MODTIME, g_config.noModtime ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_NO_CHECKSUM, g_config.noChecksum ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_FAST_LIST, g_config.fastList ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_USE_MMAP, g_config.useMmap ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_USE_RCLONE_COPY, g_config.useRcloneCopy ? BST_CHECKED : BST_UNCHECKED);

    SetDlgItemTextW(hDlg, IDC_RCLONE_PATH, g_config.rclonePath.c_str());
    SetDlgItemTextW(hDlg, IDC_CONFIG_PATH, g_config.configPath.c_str());
    SetDlgItemTextW(hDlg, IDC_RC_PORT, std::to_wstring(g_config.rcPort).c_str());
    SetDlgItemTextW(hDlg, IDC_CONN_TIMEOUT, std::to_wstring(g_config.connTimeout).c_str());
    SetDlgItemTextW(hDlg, IDC_RC_USER, g_config.rcUser.c_str());
    SetDlgItemTextW(hDlg, IDC_RC_PASS, g_config.rcPass.c_str());
    SetDlgItemTextW(hDlg, IDC_CACHE_SIZE, std::to_wstring(g_config.cacheSize).c_str());
    SetDlgItemTextW(hDlg, IDC_CACHE_MAX_AGE, std::to_wstring(g_config.cacheMaxAge).c_str());
    SetDlgItemTextW(hDlg, IDC_CACHE_TTL, std::to_wstring(g_config.cacheTTL).c_str());
    SetDlgItemTextW(hDlg, IDC_DIR_CACHE_TIME, std::to_wstring(g_config.dirCacheTime).c_str());
    SetDlgItemTextW(hDlg, IDC_POLL_INTERVAL, std::to_wstring(g_config.pollInterval).c_str());
    SetDlgItemTextW(hDlg, IDC_CACHE_DIR, g_config.cacheDir.c_str());
    SetDlgItemTextW(hDlg, IDC_MAX_CONNECTIONS, std::to_wstring(g_config.maxConnections).c_str());
    SetDlgItemTextW(hDlg, IDC_TRANSFERS, std::to_wstring(g_config.transfers).c_str());
    SetDlgItemTextW(hDlg, IDC_CHECKERS, std::to_wstring(g_config.checkers).c_str());
    SetDlgItemTextW(hDlg, IDC_BANDWIDTH_LIMIT, g_config.bandwidthLimit.c_str());
    SetDlgItemTextW(hDlg, IDC_BUFFER_SIZE, std::to_wstring(g_config.bufferSize).c_str());
    SetDlgItemTextW(hDlg, IDC_READ_CHUNK_SIZE, std::to_wstring(g_config.readChunkSize).c_str());
    SetDlgItemTextW(hDlg, IDC_READ_AHEAD, std::to_wstring(g_config.readAhead).c_str());
    SetDlgItemTextW(hDlg, IDC_LOW_LEVEL_RETRIES, std::to_wstring(g_config.lowLevelRetries).c_str());
    SetDlgItemTextW(hDlg, IDC_SYNC_INTERVAL, std::to_wstring(g_config.syncInterval).c_str());
    SetDlgItemTextW(hDlg, IDC_WRITE_BACK_DELAY, std::to_wstring(g_config.writeBackDelay).c_str());
    SetDlgItemTextW(hDlg, IDC_MAX_IDLE, std::to_wstring(g_config.maxIdle).c_str());
    SetDlgItemTextW(hDlg, IDC_STATS_INTERVAL, std::to_wstring(g_config.statsInterval).c_str());
    SetDlgItemTextW(hDlg, IDC_LOG_PATH, g_config.logPath.c_str());
    SetDlgItemTextW(hDlg, IDC_LOG_MAX_SIZE, std::to_wstring(g_config.logMaxSize).c_str());
    SetDlgItemTextW(hDlg, IDC_LOG_MAX_FILES, std::to_wstring(g_config.logMaxFiles).c_str());

    HWND hCombo = GetDlgItem(hDlg, IDC_VFS_CACHE_MODE);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u5173\u95ed");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u5199\u5165");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u5b8c\u6574");
    int cacheModeIdx = 0;
    if (g_config.vfsCacheMode == L"writes") cacheModeIdx = 1;
    else if (g_config.vfsCacheMode == L"full") cacheModeIdx = 2;
    SendMessageW(hCombo, CB_SETCURSEL, cacheModeIdx, 0);

    hCombo = GetDlgItem(hDlg, IDC_LOG_LEVEL);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"ERROR");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"NOTICE");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"INFO");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"DEBUG");
    int logLevelIdx = 1;
    if (g_config.logLevel == L"ERROR") logLevelIdx = 0;
    else if (g_config.logLevel == L"NOTICE") logLevelIdx = 1;
    else if (g_config.logLevel == L"INFO") logLevelIdx = 2;
    else if (g_config.logLevel == L"DEBUG") logLevelIdx = 3;
    SendMessageW(hCombo, CB_SETCURSEL, logLevelIdx, 0);

    ShowNavPage(hDlg, PAGE_BASIC);
}

static bool SaveDialogControls(HWND hDlg)
{
    g_config.autoStartDaemon = (IsDlgButtonChecked(hDlg, IDC_AUTO_START_DAEMON) == BST_CHECKED);
    g_config.cacheEnabled = (IsDlgButtonChecked(hDlg, IDC_CACHE_ENABLED) == BST_CHECKED);
    g_config.autoSync = (IsDlgButtonChecked(hDlg, IDC_AUTO_SYNC) == BST_CHECKED);
    g_config.cacheWriteBack = (IsDlgButtonChecked(hDlg, IDC_CACHE_WRITE_BACK) == BST_CHECKED);
    g_config.logEnabled = (IsDlgButtonChecked(hDlg, IDC_LOG_ENABLED) == BST_CHECKED);
    g_config.verboseLogging = (IsDlgButtonChecked(hDlg, IDC_VERBOSE_LOGGING) == BST_CHECKED);
    g_config.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_CONFIRM_DELETE) == BST_CHECKED);
    g_config.showHidden = (IsDlgButtonChecked(hDlg, IDC_SHOW_HIDDEN) == BST_CHECKED);
    g_config.caseSensitive = (IsDlgButtonChecked(hDlg, IDC_CASE_SENSITIVE) == BST_CHECKED);
    g_config.noModtime = (IsDlgButtonChecked(hDlg, IDC_NO_MODTIME) == BST_CHECKED);
    g_config.noChecksum = (IsDlgButtonChecked(hDlg, IDC_NO_CHECKSUM) == BST_CHECKED);
    g_config.fastList = (IsDlgButtonChecked(hDlg, IDC_FAST_LIST) == BST_CHECKED);
    g_config.useMmap = (IsDlgButtonChecked(hDlg, IDC_USE_MMAP) == BST_CHECKED);
    g_config.useRcloneCopy = (IsDlgButtonChecked(hDlg, IDC_USE_RCLONE_COPY) == BST_CHECKED);

    wchar_t buf[1024];

    GetDlgItemTextW(hDlg, IDC_RCLONE_PATH, buf, _countof(buf));
    g_config.rclonePath = buf;
    GetDlgItemTextW(hDlg, IDC_CONFIG_PATH, buf, _countof(buf));
    g_config.configPath = buf;

    GetDlgItemTextW(hDlg, IDC_RC_PORT, buf, _countof(buf));
    g_config.rcPort = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_CONN_TIMEOUT, buf, _countof(buf));
    g_config.connTimeout = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_RC_USER, buf, _countof(buf));
    g_config.rcUser = buf;
    GetDlgItemTextW(hDlg, IDC_RC_PASS, buf, _countof(buf));
    g_config.rcPass = buf;

    HWND hCombo = GetDlgItem(hDlg, IDC_VFS_CACHE_MODE);
    int cacheModeIdx = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    switch (cacheModeIdx) {
        case 1: g_config.vfsCacheMode = L"writes"; break;
        case 2: g_config.vfsCacheMode = L"full"; break;
        default: g_config.vfsCacheMode = L"off"; break;
    }

    GetDlgItemTextW(hDlg, IDC_CACHE_SIZE, buf, _countof(buf));
    g_config.cacheSize = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_CACHE_MAX_AGE, buf, _countof(buf));
    g_config.cacheMaxAge = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_CACHE_TTL, buf, _countof(buf));
    g_config.cacheTTL = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_DIR_CACHE_TIME, buf, _countof(buf));
    g_config.dirCacheTime = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_POLL_INTERVAL, buf, _countof(buf));
    g_config.pollInterval = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_CACHE_DIR, buf, _countof(buf));
    g_config.cacheDir = buf;

    GetDlgItemTextW(hDlg, IDC_MAX_CONNECTIONS, buf, _countof(buf));
    g_config.maxConnections = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_TRANSFERS, buf, _countof(buf));
    g_config.transfers = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_CHECKERS, buf, _countof(buf));
    g_config.checkers = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_BANDWIDTH_LIMIT, buf, _countof(buf));
    g_config.bandwidthLimit = buf;
    GetDlgItemTextW(hDlg, IDC_BUFFER_SIZE, buf, _countof(buf));
    g_config.bufferSize = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_READ_CHUNK_SIZE, buf, _countof(buf));
    g_config.readChunkSize = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_READ_AHEAD, buf, _countof(buf));
    g_config.readAhead = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_LOW_LEVEL_RETRIES, buf, _countof(buf));
    g_config.lowLevelRetries = _wtoi(buf);

    GetDlgItemTextW(hDlg, IDC_SYNC_INTERVAL, buf, _countof(buf));
    g_config.syncInterval = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_WRITE_BACK_DELAY, buf, _countof(buf));
    g_config.writeBackDelay = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_MAX_IDLE, buf, _countof(buf));
    g_config.maxIdle = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_STATS_INTERVAL, buf, _countof(buf));
    g_config.statsInterval = _wtoi(buf);

    hCombo = GetDlgItem(hDlg, IDC_LOG_LEVEL);
    int logLevelIdx = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    switch (logLevelIdx) {
        case 0: g_config.logLevel = L"ERROR"; break;
        case 2: g_config.logLevel = L"INFO"; break;
        case 3: g_config.logLevel = L"DEBUG"; break;
        default: g_config.logLevel = L"NOTICE"; break;
    }

    GetDlgItemTextW(hDlg, IDC_LOG_PATH, buf, _countof(buf));
    g_config.logPath = buf;
    GetDlgItemTextW(hDlg, IDC_LOG_MAX_SIZE, buf, _countof(buf));
    g_config.logMaxSize = _wtoi(buf);
    GetDlgItemTextW(hDlg, IDC_LOG_MAX_FILES, buf, _countof(buf));
    g_config.logMaxFiles = _wtoi(buf);

    ConfigSave();
    return true;
}

static void ResetToDefaults(HWND hDlg)
{
    g_config = RcloneVFSConfig();
    InitDialogControls(hDlg);
    MessageBoxW(hDlg, L"\u5df2\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION);
}

static void OpenRcloneConfig(HWND hDlg)
{
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = L"open";
    sei.lpFile = L"rclone";
    sei.lpParameters = L"config";
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            InitDialogControls(hDlg);
            return TRUE;

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
                static const WCHAR* navLabels[] = {
                    L"\u57fa\u672c\u8bbe\u7f6e", L"\u8fde\u63a5\u8bbe\u7f6e", L"\u7f13\u5b58\u8bbe\u7f6e",
                    L"\u6027\u80fd\u8bbe\u7f6e", L"\u540c\u6b65\u8bbe\u7f6e", L"\u65e5\u5fd7\u8bbe\u7f6e", L"\u9ad8\u7ea7\u8bbe\u7f6e"
                };
                int idx = (int)lpdis->itemID;
                if (idx >= 0 && idx < 7)
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
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (SaveDialogControls(hDlg)) EndDialog(hDlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                case IDC_NAV_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                        int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < 7) ShowNavPage(hDlg, sel);
                    }
                    return TRUE;
                case IDC_RESET_DEFAULTS:
                    ResetToDefaults(hDlg);
                    return TRUE;
                case IDC_OPEN_CONFIG:
                    OpenRcloneConfig(hDlg);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hwndParent, HWND hwndNotify, DWORD dwNotifyData) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_RCLONE_CONFIG), hwndParent, ConfigDlgProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hData) {
    return ColumnManager::GetColumns();
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_MOVE | VFSFUNCAVAIL_DELETE |
                  VFSFUNCAVAIL_MAKEDIR | VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_PROPERTIES |
                  VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE;
        return TRUE;
    }
    case VFSPROP_GETFOLDERICON:
        return FALSE;
    case VFSPROP_SHOWTHUMBNAILS:
    case VFSPROP_USEFULLRENAME:
    case VFSPROP_CANSHOWSUBFOLDERS:
    case VFSPROP_SUPPORTPATHCOMPLETION:
    case VFSPROP_SHOWFILEINFO:
    case VFSPROP_ALLOWTOOLTIPGETSIZES:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    case VFSPROP_ISEXTRACTABLE:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    case VFSPROP_CANDELETETOTRASH:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    case VFSPROP_CANDELETESECURE:
        *(LPBOOL)lpPropData = FALSE;
        return TRUE;
    case VFSPROP_SUPPORTFILEHASH:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    case VFSPROP_DRAGEFFECTS:
        *(LPDWORD)lpPropData = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
        return TRUE;
    case VFSPROP_COPYBUFFERSIZE:
        *(LPDWORD)lpPropData = 64 * 1024;
        return TRUE;
    case VFSPROP_SHOWFULLPROGRESSBAR:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    case VFSPROP_SHOWPICTURESDIRECTLY:
        *(LPBOOL)lpPropData = TRUE;
        return TRUE;
    default:
        return FALSE;
    }
}

static MenuContext BuildMenuContext(LPCWSTR lpszPath) {
    MenuContext ctx = {};
    if (!lpszPath) return ctx;

    DebugLog("[MenuCtx] path=%ls\n", lpszPath);

    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) {
        DebugLog("[MenuCtx] Parse failed\n");
        return ctx;
    }

    ctx.isRoot = pathInfo.isRoot;
    ctx.isRemoteRoot = pathInfo.isRemoteRoot;
    ctx.fs = pathInfo.fs;
    ctx.remote = pathInfo.remotePath;

    DebugLog("[MenuCtx] isRoot=%d isRemoteRoot=%d fs=%ls remote=%ls\n",
        ctx.isRoot, ctx.isRemoteRoot, ctx.fs.c_str(), ctx.remote.c_str());

    if (!pathInfo.fs.empty()) {
        std::vector<RcloneRemoteInfo> remotes;
        if (RcloneCache::GetRemotes(remotes)) {
            for (auto& r : remotes) {
                if (PathParser::ExtractFsName(pathInfo.fs) == r.name) {
                    ctx.remoteType = r.type;
                    break;
                }
            }
        }
        ctx.features = RcloneFeatures::Get(pathInfo.fs, ctx.remoteType);
        DebugLog("[MenuCtx] remoteType=%ls about=%d pubLink=%d trash=%d\n",
            ctx.remoteType.c_str(), ctx.features.supportsAbout,
            ctx.features.supportsPublicLink, ctx.features.supportsTrash);
    }

    if (!pathInfo.remotePath.empty()) {
        RcloneFileInfo info;
        if (RcloneCache::GetStat(pathInfo.fs, pathInfo.remotePath, info)) {
            ctx.isDir = info.isDir;
            ctx.isSelectedDir = info.isDir;
            ctx.hasId = !info.id.empty();
            DebugLog("[MenuCtx] Cache hit: isDir=%d hasId=%d\n", ctx.isDir, ctx.hasId);
        } else {
            ctx.isDir = false;
            ctx.isSelectedDir = false;
            ctx.hasId = true;
            DebugLog("[MenuCtx] No cache, using defaults\n");
        }
    } else {
        DebugLog("[MenuCtx] remotePath empty, skipping Stat\n");
    }

    return ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData) {
    if (!lpszFiles || !lpMenuData) return FALSE;

    DebugLog("[GetContextMenu] lpszFiles=%ls\n", lpszFiles);

    MenuContext ctx = BuildMenuContext(lpszFiles);
    if (ctx.fs.empty() && !ctx.isRoot) return FALSE;

    return MenuManager::BuildContextMenu(ctx, lpMenuData);
}

static VFSCONTEXTMENUITEMW g_DropMenuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制到本地", L"rclone_drop_copy_local" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"同步到远程", L"rclone_drop_sync_remote" },
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"移动到远程", L"rclone_drop_move_remote" },
};

extern "C" __declspec(dllexport) BOOL VFS_GetDropMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData, DWORD dwEffects) {
    if (!lpszFiles || !lpMenuData) return FALSE;

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = TRUE;
    lpMenuData->fCustomItemsBelow = FALSE;
    lpMenuData->lpCustomItems = g_DropMenuItems;
    lpMenuData->iNumCustomItems = sizeof(g_DropMenuItems) / sizeof(g_DropMenuItems[0]);
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    MenuContext ctx = BuildMenuContext(lpVerbData->lpszPath);

    int result = MenuManager::HandleVerb(ctx, lpFuncData, lpVerbData);
    if (result >= 0) return result;

    if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
        return VFSCVRES_CHANGEDIR;
    }

    bool isDefaultOpen = (lpVerbData->lpszVerb == NULL || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0);

    if (isDefaultOpen) {
        RclonePathInfo pathInfo;
        if (!PathParser::Parse(lpVerbData->lpszPath, pathInfo)) return VFSCVRES_FAIL;

        WCHAR tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        WCHAR folderName[64];
        static std::atomic<int> s_tempCounter(0);
        StringCchPrintfW(folderName, 64, L"RcloneVFS.%lu.%lu.%d\\", GetCurrentProcessId(), GetTickCount(), ++s_tempCounter);
        std::wstring uniqueFolder = std::wstring(tempPath) + folderName;
        CreateDirectoryW(uniqueFolder.c_str(), NULL);

        std::wstring remoteStr = pathInfo.remotePath;
        size_t slashPos = remoteStr.find_last_of(L"\\/");
        std::wstring fileName = (slashPos != std::wstring::npos) ? remoteStr.substr(slashPos + 1) : remoteStr;
        std::wstring localFile = uniqueFolder + fileName;

        if (!RcloneClient::CopyFileToLocal(pathInfo.fs, pathInfo.remotePath, localFile)) {
            RemoveDirectoryW(uniqueFolder.c_str());
            return VFSCVRES_FAIL;
        }

        MonitorAndAutoSync(localFile, uniqueFolder, pathInfo.fs, pathInfo.remotePath);
        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, localFile.c_str());
        return VFSCVRES_CHANGE;
    }
    return VFSCVRES_FAIL;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;
    std::wstring path(lpszPath); PathParser::NormalizePath(path);
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

    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    RcloneFileInfo info;
    bool isRoot = pathInfo.remotePath.empty();

    std::wstring remoteType;
    if (!pathInfo.fs.empty()) {
        auto remotes = RcloneClient::ListRemotesWithType();
        std::wstring fsName = PathParser::ExtractFsName(pathInfo.fs);
        for (auto& r : remotes) {
            if (r.name == fsName) {
                remoteType = r.type;
                break;
            }
        }
    }

    if (!isRoot && !RcloneClient::Stat(pathInfo.fs, pathInfo.remotePath, info)) {
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

    if (isRoot || pathInfo.isRemoteRoot) {
        RcloneRemoteInfo remoteInfo;
        remoteInfo.name = PathParser::ExtractFsName(pathInfo.fs);
        remoteInfo.type = remoteType;

        RcloneAboutInfo aboutInfo;
        if (!pathInfo.fs.empty()) {
            if (!RcloneCache::GetAbout(pathInfo.fs, aboutInfo)) {
                if (!RcloneCache::IsAboutRequestPending(pathInfo.fs)) {
                    RcloneCache::MarkAboutRequestPending(pathInfo.fs);
                    std::wstring fsBg = pathInfo.fs;
                    std::wstring typeBg = remoteType;
                    if (g_threadPool) {
                        g_threadPool->enqueue([fsBg, typeBg]() {
                            RcloneAboutInfo ai;
                            RcloneBackendFeatures feat = RcloneFeatures::Get(fsBg, typeBg);
                            if (feat.supportsAbout) {
                                RcloneClient::About(fsBg, ai);
                            } else {
                                RcloneClient::CalcAboutRecursive(fsBg, ai);
                            }
                            RcloneCache::ClearAboutRequestPending(fsBg);
                        });
                    }
                }
            }
        }

        ColumnManager::FillDataForRemote(lpFileData, hHeap, remoteInfo, aboutInfo);
    } else {
        RcloneBackendFeatures features = RcloneFeatures::Get(pathInfo.fs, remoteType);
        ColumnManager::FillDataForFile(lpFileData, hHeap, pathInfo.fs, pathInfo.remotePath, info, features.remoteType);
    }

    return lpFDH;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPDWORD lpdwAttr) {
    if (!lpszPath || !lpdwAttr) return FALSE;
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }

    if (pathInfo.remotePath.empty()) {
        *lpdwAttr = FILE_ATTRIBUTE_DIRECTORY;
        return TRUE;
    }

    RcloneFileInfo info;
    if (RcloneClient::Stat(pathInfo.fs, pathInfo.remotePath, info)) {
        *lpdwAttr = info.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        return TRUE;
    }

    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || lpRDD->vfsReadOp == VFSREAD_FREEDIR) return TRUE;

    if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) {
        return TRUE;
    }

    if (lpRDD->vfsReadOp == VFSREAD_NORMAL || lpRDD->vfsReadOp == VFSREAD_REFRESH ||
        lpRDD->vfsReadOp == VFSREAD_PARENT || lpRDD->vfsReadOp == VFSREAD_ROOT ||
        lpRDD->vfsReadOp == VFSREAD_BACK || lpRDD->vfsReadOp == VFSREAD_FORWARD ||
        lpRDD->vfsReadOp == VFSREAD_PRINTDIR) {
        RclonePathInfo pathInfo;
        if (!PathParser::Parse(lpRDD->lpszPath, pathInfo)) {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return FALSE;
        }

        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) {
            SetLastError(ERROR_CANCELLED);
            return FALSE;
        }

        if (pathInfo.fs.empty()) {
            if (lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE;

            if (!DaemonManager::IsRunning()) {
                if (!RcloneClient::EnsureDaemonStarted()) {
                    size_t allocSize = sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW);
                    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
                    if (!lpFDH) return FALSE;
                    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
                    lpFDH->iNumItems = 0;
                    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
                    lpRDD->lpFileData = lpFDH;
                    return TRUE;
                }
            }

            auto remotesWithType = RcloneClient::ListRemotesWithType();
            int numItems = (int)remotesWithType.size();
            size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
            LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
            if (!lpFDH) return FALSE;

            lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = numItems; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
            if (numItems > 0) {
                LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
                for (int i = 0; i < numItems; ++i) {
                    if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
                    StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, remotesWithType[i].name.c_str());
                    lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                    lpFileData[i].wfdData.nFileSizeHigh = 0;
                    lpFileData[i].wfdData.nFileSizeLow = 0;
                    GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

                    RcloneAboutInfo aboutInfo;
                    std::wstring fs = remotesWithType[i].name + L":";
                    RcloneCache::GetAbout(fs, aboutInfo);

                    ColumnManager::FillDataForRemote(&lpFileData[i], lpRDD->hMemHeap, remotesWithType[i], aboutInfo);
                }
            }
            lpRDD->lpFileData = lpFDH;

            if (g_threadPool) {
                g_threadPool->enqueue([remotesWithType]() {
                    bool anyLoaded = false;
                    for (auto& r : remotesWithType) {
                        if (g_PluginUnloading.load()) break;
                        std::wstring fs = r.name + L":";
                        RcloneBackendFeatures feat = RcloneFeatures::Get(fs, r.type);
                        if (feat.supportsAbout) {
                            if (!RcloneCache::IsAboutRequestPending(fs)) {
                                RcloneCache::MarkAboutRequestPending(fs);
                                RcloneAboutInfo aboutInfo;
                                if (RcloneClient::About(fs, aboutInfo)) {
                                    anyLoaded = true;
                                }
                                RcloneCache::ClearAboutRequestPending(fs);
                            }
                        } else {
                            // No About API support — calculate used space recursively
                            if (!RcloneCache::IsAboutRequestPending(fs)) {
                                RcloneAboutInfo cached;
                                if (!RcloneCache::GetAbout(fs, cached)) {
                                    RcloneCache::MarkAboutRequestPending(fs);
                                    RcloneAboutInfo aboutInfo;
                                    if (RcloneClient::CalcAboutRecursive(fs, aboutInfo)) {
                                        anyLoaded = true;
                                    }
                                    RcloneCache::ClearAboutRequestPending(fs);
                                }
                            }
                        }
                    }
                    if (anyLoaded && !g_PluginUnloading.load()) {
                        DebounceRefresh::RequestRefresh();
                    }
                });
            }

            return TRUE;
        }

        if (!DaemonManager::IsRunning()) {
            if (!RcloneClient::EnsureDaemonStarted()) {
                SetLastError(ERROR_PATH_NOT_FOUND);
                return FALSE;
            }
        }
        auto fileList = RcloneClient::ListDirectory(pathInfo.fs, pathInfo.remotePath);
        int numItems = (int)fileList.size();

        std::wstring remoteType;
        {
            auto remotes = RcloneClient::ListRemotesWithType();
            std::wstring fsName = PathParser::ExtractFsName(pathInfo.fs);
            for (auto& r : remotes) {
                if (r.name == fsName) {
                    remoteType = r.type;
                    break;
                }
            }
        }
        RcloneBackendFeatures features = RcloneFeatures::Get(pathInfo.fs, remoteType);

        // Preload About asynchronously for this remote if not cached
        if (pathInfo.isRemoteRoot) {
            std::wstring preFs = pathInfo.fs;
            std::wstring preRemoteType = remoteType;
            if (g_threadPool) {
                g_threadPool->enqueue([preFs, preRemoteType]() {
                    if (g_PluginUnloading.load()) return;
                    RcloneAboutInfo aboutInfo;
                    if (!RcloneCache::GetAbout(preFs, aboutInfo)) {
                        if (!RcloneCache::IsAboutRequestPending(preFs)) {
                            RcloneCache::MarkAboutRequestPending(preFs);
                            RcloneBackendFeatures feat = RcloneFeatures::Get(preFs, preRemoteType);
                            if (feat.supportsAbout) {
                                RcloneClient::About(preFs, aboutInfo);
                            } else {
                                RcloneClient::CalcAboutRecursive(preFs, aboutInfo);
                            }
                            RcloneCache::ClearAboutRequestPending(preFs);
                        }
                    }
                });
            }
        }

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

                ColumnManager::FillDataForFile(&lpFileData[i], lpRDD->hMemHeap, pathInfo.fs, pathInfo.remotePath, fileList[i], features.remoteType);
            }
        }

        lpRDD->lpFileData = lpFDH;

        // Preload Stat for subdirectories (max 10)
        {
            std::vector<std::pair<std::wstring, std::wstring>> subdirs;
            for (int i = 0; i < numItems && (int)subdirs.size() < 10; ++i) {
                if (fileList[i].isDir) {
                    std::wstring subRemote = pathInfo.remotePath.empty() ? fileList[i].name : pathInfo.remotePath + L"/" + fileList[i].name;
                    subdirs.push_back({pathInfo.fs, subRemote});
                }
            }
            if (!subdirs.empty()) {
                if (g_threadPool) {
                    g_threadPool->enqueue([subdirs]() {
                        for (auto& sd : subdirs) {
                            if (g_PluginUnloading.load()) break;
                            RcloneFileInfo info;
                            if (!RcloneCache::GetStat(sd.first, sd.second, info)) {
                                RcloneClient::Stat(sd.first, sd.second, info);
                            }
                        }
                    });
                }
            }
        }

        return TRUE;
    }
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT) {
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszFile, pathInfo)) return NULL;

    if (!RcloneClient::EnsureDaemonStarted()) {
        if (!DaemonManager::IsRunning() && DaemonManager::Reconnect()) {
            if (!RcloneClient::EnsureDaemonStarted()) return NULL;
        } else {
            return NULL;
        }
    }

    bool isWrite = (dwMode & GENERIC_WRITE) != 0;
    VFS_FILE_CONTEXT* ctx = new VFS_FILE_CONTEXT();
    ctx->isWrite = isWrite;
    ctx->targetFs = pathInfo.fs;
    ctx->targetRemote = pathInfo.remotePath;

    if (isWrite) {
        std::wstring uploadDir = L"";
        std::wstring fileNameW = pathInfo.remotePath;
        size_t lastSlash = pathInfo.remotePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            uploadDir = pathInfo.remotePath.substr(0, lastSlash);
            fileNameW = pathInfo.remotePath.substr(lastSlash + 1);
        }

        std::string urlPath = "/operations/uploadfile?fs=" + UrlEncode(WideToUtf8(pathInfo.fs)) + "&remote=" + UrlEncode(WideToUtf8(uploadDir));
        std::wstring wUrlPath = Utf8ToWide(urlPath);

        ctx->hHttpRequest = WinHttpOpenRequest(DaemonManager::GetConnect(), L"POST", wUrlPath.c_str(),
                                               NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!ctx->hHttpRequest) { delete ctx; return NULL; }

        std::wstring headers = DaemonManager::GetAuthHeader()
                               + L"Transfer-Encoding: chunked\r\n"
                               + L"Content-Type: multipart/form-data; boundary=----RcloneVFSBoundaryXYZ\r\n";

        BOOL bRes = WinHttpSendRequest(ctx->hHttpRequest, headers.c_str(), -1,
                                       WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0);
        if (!bRes) { delete ctx; return NULL; }

        std::string utf8FileName = WideToUtf8(fileNameW);
        std::string preamble = "------RcloneVFSBoundaryXYZ\r\n"
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
        if (!RcloneClient::Stat(pathInfo.fs, pathInfo.remotePath, info)) {
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
        RclonePathInfo pathInfo;
        if (PathParser::Parse(lpszPath, pathInfo)) {
            RcloneFileInfo info;
            if (RcloneClient::Stat(pathInfo.fs, pathInfo.remotePath, info)) {
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

        std::string epilogue = "\r\n------RcloneVFSBoundaryXYZ--\r\n";
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

            RcloneCache::InvalidateAll();
        }
    }

    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags, int iSecurePasses) {
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) return FALSE;
    
    if (dwFlags & VFSDELETEF_RECYCLE) {
        std::wstring trashPath = L".trash/" + pathInfo.remotePath;
        if (RcloneClient::Move(pathInfo.fs, pathInfo.remotePath, pathInfo.fs, trashPath)) {
            return TRUE;
        }
    } else {
        if (RcloneClient::DeleteFile(pathInfo.fs, pathInfo.remotePath)) {
            return TRUE;
        }
    }
    
    if (!DaemonManager::IsRunning() && DaemonManager::Reconnect()) {
        if (dwFlags & VFSDELETEF_RECYCLE) {
            std::wstring trashPath = L".trash/" + pathInfo.remotePath;
            if (RcloneClient::Move(pathInfo.fs, pathInfo.remotePath, pathInfo.fs, trashPath)) {
                return TRUE;
            }
        } else {
            if (RcloneClient::DeleteFile(pathInfo.fs, pathInfo.remotePath)) {
                return TRUE;
            }
        }
    }
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

struct PropDlgData {
    RcloneFileInfo info;
    RcloneAboutInfo aboutInfo;
    std::wstring fs;
    std::wstring remote;
    std::wstring displayPath;
    bool hasAbout;
};

static std::wstring BuildPropCopyText(const PropDlgData* data) {
    std::wstring text;
    text += L"名称:\t" + (data->info.name.empty() ? L"-" : data->info.name) + L"\r\n";
    text += L"类型:\t" + std::wstring(data->info.isDir ? L"文件夹" : L"文件") + L"\r\n";
    if (!data->info.isDir && data->info.size != (uint64_t)-1) {
        text += L"大小:\t" + PathParser::FormatSize(data->info.size) + L"\r\n";
    }
    text += L"修改时间:\t" + PathParser::FormatTime(data->info.modTime) + L"\r\n";
    if (!data->info.isDir) {
        text += L"MIME类型:\t" + (data->info.mimeType.empty() ? L"-" : data->info.mimeType) + L"\r\n";
        text += L"文件 ID:\t" + (data->info.id.empty() ? L"-" : data->info.id) + L"\r\n";
    }
    text += L"远程路径:\t" + data->displayPath + L"\r\n";
    if (!data->info.isDir && !data->info.hash.empty()) {
        text += L"哈希:\t" + data->info.hash + L" (" + Utf8ToWide(data->info.hashType) + L")\r\n";
    }
    if (data->hasAbout) {
        text += L"\r\n--- 存储配额 ---\r\n";
        if (data->aboutInfo.hasTotal && data->aboutInfo.total > 0) text += L"总配额:\t" + PathParser::FormatSize(data->aboutInfo.total) + L"\r\n";
        if (data->aboutInfo.hasUsed && data->aboutInfo.used > 0) text += L"已用:\t" + PathParser::FormatSize(data->aboutInfo.used) + L"\r\n";
        if (data->aboutInfo.hasFree && data->aboutInfo.free > 0) text += L"可用:\t" + PathParser::FormatSize(data->aboutInfo.free) + L"\r\n";
    }
    return text;
}

static INT_PTR CALLBACK PropDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    PropDlgData* data = (PropDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (PropDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        SetWindowTextW(hDlg, L"属性");

        SetDlgItemTextW(hDlg, IDC_PROP_NAME, data->info.name.empty() ? L"-" : data->info.name.c_str());
        SetDlgItemTextW(hDlg, IDC_PROP_TYPE, data->info.isDir ? L"文件夹" : L"文件");

        if (data->info.isDir || data->info.size == (uint64_t)-1) {
            SetDlgItemTextW(hDlg, IDC_PROP_SIZE, L"-");
        } else {
            WCHAR sizeText[128];
            StringCchPrintfW(sizeText, 128, L"%s (%llu 字节)",
                PathParser::FormatSize(data->info.size).c_str(), data->info.size);
            SetDlgItemTextW(hDlg, IDC_PROP_SIZE, sizeText);
        }

        SetDlgItemTextW(hDlg, IDC_PROP_MTIME, PathParser::FormatTime(data->info.modTime).c_str());

        if (data->info.isDir) {
            SetDlgItemTextW(hDlg, IDC_PROP_MIME, L"-");
            SetDlgItemTextW(hDlg, IDC_PROP_FID, L"-");
            SetDlgItemTextW(hDlg, IDC_PROP_HASH, L"-");
            SetDlgItemTextW(hDlg, IDC_PROP_HASHTYPE, L"");
        } else {
            SetDlgItemTextW(hDlg, IDC_PROP_MIME, data->info.mimeType.empty() ? L"-" : data->info.mimeType.c_str());
            SetDlgItemTextW(hDlg, IDC_PROP_FID, data->info.id.empty() ? L"-" : data->info.id.c_str());
            SetDlgItemTextW(hDlg, IDC_PROP_HASH, data->info.hash.empty() ? L"-" : data->info.hash.c_str());
            SetDlgItemTextW(hDlg, IDC_PROP_HASHTYPE, data->info.hash.empty() ? L"" : Utf8ToWide(data->info.hashType).c_str());
        }

        SetDlgItemTextW(hDlg, IDC_PROP_RPATH, data->displayPath.c_str());

        if (data->hasAbout) {
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_TOTAL,
                (data->aboutInfo.hasTotal && data->aboutInfo.total > 0) ? PathParser::FormatSize(data->aboutInfo.total).c_str() : L"...");
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_USED,
                (data->aboutInfo.hasUsed && data->aboutInfo.used > 0) ? PathParser::FormatSize(data->aboutInfo.used).c_str() : L"...");
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_FREE,
                (data->aboutInfo.hasFree && data->aboutInfo.free > 0) ? PathParser::FormatSize(data->aboutInfo.free).c_str() : L"...");
        } else {
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_TOTAL, L"...");
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_USED, L"...");
            SetDlgItemTextW(hDlg, IDC_PROP_QUOTA_FREE, L"...");
        }

        SetDlgItemTextW(hDlg, IDC_PROP_COPY, L"复制信息");

        return TRUE;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_PROP_COPY && data) {
            std::wstring text = BuildPropCopyText(data);
            if (OpenClipboard(hDlg)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(WCHAR));
                if (hMem) {
                    LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                    StringCchCopyW(pMem, text.length() + 1, text.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        EndDialog(hDlg, IDOK);
        return TRUE;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles) return NULL;
    std::wstring targetPath = lpszFiles;
    if (targetPath.empty()) return NULL;

    RclonePathInfo pathInfo;
    if (!PathParser::Parse(targetPath, pathInfo)) return NULL;

    if (ShowPropertyDialog(hwndParent, pathInfo.fs, pathInfo.remotePath)) {
        return (HWND)TRUE;
    }
    return NULL;
}

bool ShowPropertyDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote) {
    RcloneFileInfo info;
    if (!RcloneClient::Stat(fs, remote, info)) {
        if (!DaemonManager::IsRunning()) {
            if (DaemonManager::Reconnect()) {
                if (!RcloneClient::Stat(fs, remote, info)) {
                    MessageBoxW(hwndParent,
                        L"无法获取远程文件信息。\n文件可能已被删除或网络不可达。",
                        L"属性错误",
                        MB_ICONERROR | MB_OK);
                    return false;
                }
            } else {
                MessageBoxW(hwndParent,
                    L"无法获取远程文件信息。\n文件可能已被删除或网络不可达。",
                    L"属性错误",
                    MB_ICONERROR | MB_OK);
                return false;
            }
        } else {
            MessageBoxW(hwndParent,
                L"无法获取远程文件信息。\n文件可能已被删除或网络不可达。",
                L"属性错误",
                MB_ICONERROR | MB_OK);
            return false;
        }
    }

    PropDlgData dlgData = {};
    dlgData.info = info;
    dlgData.fs = fs;
    dlgData.remote = remote;
    dlgData.displayPath = PathParser::ExtractFsName(fs) + L":" + remote;
    dlgData.hasAbout = RcloneClient::About(fs, dlgData.aboutInfo);

    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_FILE_PROPERTIES), hwndParent, PropDlgProc, (LPARAM)&dlgData);
    return true;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) return FALSE;
    
    if (dwFlags & VFSDELETEF_RECYCLE) {
        std::wstring trashPath = L".trash/" + pathInfo.remotePath;
        if (RcloneClient::Move(pathInfo.fs, pathInfo.remotePath, pathInfo.fs, trashPath)) {
            return TRUE;
        }
    } else {
        if (RcloneClient::RemoveDir(pathInfo.fs, pathInfo.remotePath)) {
            return TRUE;
        }
    }
    
    if (!DaemonManager::IsRunning() && DaemonManager::Reconnect()) {
        if (dwFlags & VFSDELETEF_RECYCLE) {
            std::wstring trashPath = L".trash/" + pathInfo.remotePath;
            if (RcloneClient::Move(pathInfo.fs, pathInfo.remotePath, pathInfo.fs, trashPath)) {
                return TRUE;
            }
        } else {
            if (RcloneClient::RemoveDir(pathInfo.fs, pathInfo.remotePath)) {
                return TRUE;
            }
        }
    }
    SetLastError(ERROR_DIR_NOT_EMPTY);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, L"rclone://"); return TRUE; }
    if (PathParser::IsRootPath(lpszPath)) return FALSE;
    
    std::wstring path(lpszPath); PathParser::NormalizePath(path);
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash == std::wstring::npos || lastSlash < 9) { 
        StringCchCopyW(lpszNewPath, cbNewPathMax, L"rclone://"); 
        return TRUE; 
    }
    StringCchCopyW(lpszNewPath, cbNewPathMax, path.substr(0, lastSlash).c_str()); 
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFreeDiskSpaceW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, unsigned __int64* piFreeBytesAvailable, unsigned __int64* piTotalBytes, unsigned __int64* piTotalFreeBytes) {
    const uint64_t FALLBACK = 1000000000000ULL;

    RclonePathInfo pathInfo;
    if (!lpszPath || !PathParser::Parse(lpszPath, pathInfo) || pathInfo.fs.empty()) {
        if (piFreeBytesAvailable) *piFreeBytesAvailable = FALLBACK;
        if (piTotalBytes) *piTotalBytes = FALLBACK;
        if (piTotalFreeBytes) *piTotalFreeBytes = FALLBACK;
        return TRUE;
    }

    RcloneAboutInfo aboutInfo;
    bool hasAbout = RcloneCache::GetAbout(pathInfo.fs, aboutInfo);
    if (!hasAbout) {
        hasAbout = RcloneClient::About(pathInfo.fs, aboutInfo);
    }

    if (hasAbout && aboutInfo.hasTotal && aboutInfo.hasFree) {
        if (piTotalBytes) *piTotalBytes = aboutInfo.total;
        if (piFreeBytesAvailable) *piFreeBytesAvailable = aboutInfo.free;
        if (piTotalFreeBytes) *piTotalFreeBytes = aboutInfo.free;
    } else {
        if (piFreeBytesAvailable) *piFreeBytesAvailable = FALLBACK;
        if (piTotalBytes) *piTotalBytes = FALLBACK;
        if (piTotalFreeBytes) *piTotalFreeBytes = FALLBACK;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) return FALSE;
    bool ok = RcloneClient::MakeDir(pathInfo.fs, pathInfo.remotePath);
    if (ok) RcloneCache::InvalidateForWrite(pathInfo.fs, pathInfo.remotePath);
    return ok ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    RclonePathInfo srcPathInfo, dstPathInfo;
    if (!PathParser::Parse(lpszOldName, srcPathInfo)) return FALSE;
    if (!PathParser::Parse(lpszNewName, dstPathInfo)) return FALSE;
    return RcloneClient::Move(srcPathInfo.fs, srcPathInfo.remotePath, dstPathInfo.fs, dstPathInfo.remotePath) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return VFS_RenameFileW(hData, lpFuncData, lpszOldName, lpszNewName);
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileAttrW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwAttr, BOOL fForDelete) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileTimeW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_SetFileCommentW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszComment) {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    return TRUE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData) {
    return GetLastError();
}

extern "C" __declspec(dllexport) UINT VFS_BatchOperationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPVFSBATCHDATAW lpBatchData) {
    if (!lpBatchData) return VFSBATCHRES_DODEFAULT;
    
    std::wstring currentPath = lpszPath ? lpszPath : L"";
    return BatchOperationManager::ProcessBatchOperation(lpBatchData, currentPath);
}

struct VFS_FIND_CONTEXT {
    std::vector<RcloneFileInfo> files;
    size_t currentIndex;
    std::wstring pattern;
    std::wstring basePath;
    std::wstring fs;
};

static bool MatchPattern(const std::wstring& name, const std::wstring& pattern) {
    if (pattern.empty() || pattern == L"*" || pattern == L"*.*") return true;
    
    std::wstring lowerName = name;
    std::wstring lowerPattern = pattern;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::towlower);
    
    if (lowerPattern.find(L'*') == std::wstring::npos && lowerPattern.find(L'?') == std::wstring::npos) {
        return lowerName == lowerPattern;
    }
    
    size_t pIdx = 0, nIdx = 0;
    size_t pStar = std::wstring::npos, nStar = 0;
    
    while (nIdx < lowerName.size()) {
        if (pIdx < lowerPattern.size() && (lowerPattern[pIdx] == lowerName[nIdx] || lowerPattern[pIdx] == L'?')) {
            pIdx++;
            nIdx++;
        } else if (pIdx < lowerPattern.size() && lowerPattern[pIdx] == L'*') {
            pStar = pIdx++;
            nStar = nIdx;
        } else if (pStar != std::wstring::npos) {
            pIdx = pStar + 1;
            nIdx = ++nStar;
        } else {
            return false;
        }
    }
    
    while (pIdx < lowerPattern.size() && lowerPattern[pIdx] == L'*') pIdx++;
    return pIdx == lowerPattern.size();
}

extern "C" __declspec(dllexport) HANDLE VFS_FindFirstFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWIN32_FIND_DATAW lpwfdData, HANDLE hAbortEvent) {
    if (!lpszPath || !lpwfdData) return INVALID_HANDLE_VALUE;
    
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszPath, pathInfo)) return INVALID_HANDLE_VALUE;
    
    if (pathInfo.isRoot) return INVALID_HANDLE_VALUE;
    
    VFS_FIND_CONTEXT* ctx = new VFS_FIND_CONTEXT();
    ctx->currentIndex = 0;
    ctx->fs = pathInfo.fs;
    
    std::wstring fullPath = pathInfo.remotePath;
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        ctx->pattern = fullPath.substr(lastSlash + 1);
        ctx->basePath = fullPath.substr(0, lastSlash);
    } else {
        ctx->pattern = fullPath;
        ctx->basePath = L"";
    }
    
    if (ctx->pattern.empty()) ctx->pattern = L"*";
    
    std::vector<RcloneFileInfo> allFiles = RcloneClient::ListDirectory(pathInfo.fs, ctx->basePath);
    if (allFiles.empty()) {
        delete ctx;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    
    for (const auto& file : allFiles) {
        if (MatchPattern(file.name, ctx->pattern)) {
            ctx->files.push_back(file);
        }
    }
    
    if (ctx->files.empty()) {
        delete ctx;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    
    const auto& firstFile = ctx->files[0];
    ZeroMemory(lpwfdData, sizeof(WIN32_FIND_DATAW));
    StringCchCopyW(lpwfdData->cFileName, MAX_PATH, firstFile.name.c_str());
    lpwfdData->dwFileAttributes = firstFile.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    lpwfdData->nFileSizeHigh = (DWORD)(firstFile.size >> 32);
    lpwfdData->nFileSizeLow = (DWORD)(firstFile.size & 0xFFFFFFFF);
    if (firstFile.modTime.dwLowDateTime != 0 || firstFile.modTime.dwHighDateTime != 0) {
        lpwfdData->ftLastWriteTime = firstFile.modTime;
    }
    
    ctx->currentIndex = 1;
    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_FindNextFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, HANDLE hFind, LPWIN32_FIND_DATAW lpwfdData) {
    VFS_FIND_CONTEXT* ctx = (VFS_FIND_CONTEXT*)hFind;
    if (!ctx || !lpwfdData) return FALSE;
    
    if (ctx->currentIndex >= ctx->files.size()) {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    
    const auto& file = ctx->files[ctx->currentIndex];
    ZeroMemory(lpwfdData, sizeof(WIN32_FIND_DATAW));
    StringCchCopyW(lpwfdData->cFileName, MAX_PATH, file.name.c_str());
    lpwfdData->dwFileAttributes = file.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    lpwfdData->nFileSizeHigh = (DWORD)(file.size >> 32);
    lpwfdData->nFileSizeLow = (DWORD)(file.size & 0xFFFFFFFF);
    if (file.modTime.dwLowDateTime != 0 || file.modTime.dwHighDateTime != 0) {
        lpwfdData->ftLastWriteTime = file.modTime;
    }
    
    ctx->currentIndex++;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_FindClose(HANDLE hData, HANDLE hFind) {
    VFS_FIND_CONTEXT* ctx = (VFS_FIND_CONTEXT*)hFind;
    if (ctx) delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ExtractFilesW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSEXTRACTFILESDATAW lpExtractData) {
    if (!lpExtractData) return FALSE;
    
    std::wstring destPath = lpExtractData->lpszDestPath ? lpExtractData->lpszDestPath : L"";
    if (destPath.empty()) return FALSE;
    
    std::vector<std::wstring> files;
    LPWSTR p = lpExtractData->lpszFiles;
    while (p && *p) {
        files.push_back(p);
        p += wcslen(p) + 1;
    }
    
    if (files.empty()) return FALSE;
    
    bool allSuccess = true;
    for (const auto& file : files) {
        RclonePathInfo pathInfo;
        if (!PathParser::Parse(file, pathInfo)) {
            allSuccess = false;
            continue;
        }
        
        std::wstring localDest = destPath;
        if (localDest.back() != L'\\' && localDest.back() != L'/') {
            localDest += L"\\";
        }
        
        size_t lastSlash = pathInfo.remotePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            localDest += pathInfo.remotePath.substr(lastSlash + 1);
        } else {
            localDest += pathInfo.remotePath;
        }
        
        if (!RcloneClient::CopyFileToLocal(pathInfo.fs, pathInfo.remotePath, localDest)) {
            allSuccess = false;
        }
    }
    
    return allSuccess ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_QueryPathW(LPWSTR lpszPath, BOOL fPrefix, LPGUID pGUID) {
    if (!lpszPath) return FALSE;
    
    std::wstring path = lpszPath;
    if (path.length() >= 9 && _wcsnicmp(path.c_str(), L"rclone://", 9) == 0) {
        if (pGUID) *pGUID = GUIDPlugin_Rclone;
        return TRUE;
    }
    
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cbPrefixMax) {
    if (!lpszPrefix || cbPrefixMax < 10) return FALSE;
    
    StringCchCopyW(lpszPrefix, cbPrefixMax, L"rclone://\0");
    return TRUE;
}

extern "C" __declspec(dllexport) HANDLE VFS_Clone(HANDLE hVFSData) {
    RCLONE_VFS_DATA* pOldData = (RCLONE_VFS_DATA*)hVFSData;
    if (!pOldData) return NULL;
    
    RCLONE_VFS_DATA* pNewData = new RCLONE_VFS_DATA;
    pNewData->hwndMsgWindow = pOldData->hwndMsgWindow;
    return (HANDLE)pNewData;
}

extern "C" __declspec(dllexport) DWORD VFS_GetCapabilities(HANDLE hVFSData) {
    return VFSCAPABILITY_MOVEBYRENAME | VFSCAPABILITY_CASESENSITIVE | 
           VFSCAPABILITY_RANDOMSEEK | VFSCAPABILITY_ALLOWIMAGECOLUMNS | 
           VFSCAPABILITY_ALLOWMUSICCOLUMNS | VFSCAPABILITY_ALLOWEXTRADATECOLUMNS | 
           VFSCAPABILITY_COMBINEDPROPERTIES;
}

extern "C" __declspec(dllexport) int VFS_GetFileDescriptionW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, LPWSTR lpszDescription, int cchDescriptionMax) {
    if (!lpszFile || !lpszDescription || cchDescriptionMax <= 0) return 0;
    
    RclonePathInfo pathInfo;
    if (!PathParser::Parse(lpszFile, pathInfo)) return 0;
    
    RcloneFileInfo info;
    if (!RcloneCache::GetStat(pathInfo.fs, pathInfo.remotePath, info)) {
        if (!RcloneClient::Stat(pathInfo.fs, pathInfo.remotePath, info)) {
            return 0;
        }
    }
    
    std::wstring desc;
    if (info.isDir) {
        desc = L"文件夹";
    } else {
        desc = L"文件 - " + FormatFileSize(info.size);
        if (!info.mimeType.empty()) {
            desc += L" (" + info.mimeType + L")";
        }
    }
    
    StringCchCopyW(lpszDescription, cchDescriptionMax, desc.c_str());
    return (int)desc.length();
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileCommentW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszComment, int cchCommentMax) {
    if (!lpszPath || !lpszComment || cchCommentMax <= 0) return FALSE;
    
    lpszComment[0] = L'\0';
    return TRUE;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    std::wstring aboutText = 
        L"Rclone VFS Plugin for Directory Opus\n\n"
        L"版本: 1.0.0\n"
        L"作者: RcloneVFS Team\n\n"
        L"通过 Rclone 集成云存储服务\n"
        L"支持 Google Drive、OneDrive、Dropbox 等\n\n"
        L"© 2024 RcloneVFS Project";
    
    MessageBoxW(hWndParent, aboutText.c_str(), L"关于 Rclone VFS", MB_OK | MB_ICONINFORMATION);
    return NULL;
}
