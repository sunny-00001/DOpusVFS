#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>
#include <shlobj.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <atomic>
#include <algorithm>

#ifndef ERROR_INVALID_PATH
#define ERROR_INVALID_PATH 123L
#endif

#ifndef ERROR_OPEN_FAILED
#define ERROR_OPEN_FAILED 110L
#endif

typedef const BYTE* LPCBYTE;
typedef BYTE* LPBYTE;

typedef struct _PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;

typedef LONG NTSTATUS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING;

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
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

static const GUID GUIDPlugin_Process =
{ 0xB2C3D4E5, 0xF6A7, 0x8901, { 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A } };

#define PROCESS_VFS_PREFIX L"process://"
#define PROCESS_VFS_PREFIX_LEN 10
#define NUM_COLUMNS 17

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

// 自定义列数据 - 在 VFS_Init 中初始化
static VFSCUSTOMCOLUMNW g_columns[NUM_COLUMNS];
static bool g_columnsInitialized = false;

struct ProcessInfo
{
    DWORD pid;
    std::wstring name;
    std::wstring exePath;
    DWORD threadCount;
    SIZE_T workingSetSize;
    DWORD parentPid;
    bool isRunning;
    bool isCritical;
    double cpuUsage;
    FILETIME ftCreation;
    FILETIME ftKernel;
    FILETIME ftUser;
    ULONGLONG readTransferCount;
    ULONGLONG writeTransferCount;
    DWORD handleCount;
    std::wstring userName;
    DWORD priorityClass;
    std::wstring parentName;
    bool hasNetwork;
    int tcpCount;
    int udpCount;
    DWORD sessionId;
    BOOL isWow64;
    std::wstring commandLine;
    std::wstring fileDescription;

    ProcessInfo()
    {
        pid = 0;
        threadCount = 0;
        workingSetSize = 0;
        parentPid = 0;
        isRunning = true;
        isCritical = false;
        cpuUsage = 0.0;
        memset(&ftCreation, 0, sizeof(FILETIME));
        memset(&ftKernel, 0, sizeof(FILETIME));
        memset(&ftUser, 0, sizeof(FILETIME));
        readTransferCount = 0;
        writeTransferCount = 0;
        handleCount = 0;
        priorityClass = 0;
        hasNetwork = false;
        tcpCount = 0;
        udpCount = 0;
        sessionId = 0;
        isWow64 = FALSE;
    }
};

struct ProcessFileContext
{
    std::vector<BYTE> buffer;
    size_t readPos;
    bool isWrite;
};

struct ProcessConfig
{
    int defaultAction;
    bool showSystemProcesses;
    bool colPid;
    bool colStatus;
    bool colCpu;
    bool colMemory;
    bool colThreads;
    bool colPath;
    bool colDisk;
    bool colHandles;
    bool colUser;
    bool colPriority;
    bool colNetwork;
    bool colParent;
    bool colCreateTime;
    bool colCmdLine;
    bool colSessionId;
    bool colArch;
    bool colDescription;
    int killMethod;
    bool confirmKill;
    bool confirmTree;
    bool autoRefreshKill;
    bool warnCritical;
    int cacheTimeout;
    std::wstring logPath;
    std::wstring configPath;

    ProcessConfig()
    {
        defaultAction = 0;
        showSystemProcesses = false;
        colPid = true;
        colStatus = true;
        colCpu = true;
        colMemory = true;
        colThreads = true;
        colPath = false;
        colDisk = true;
        colHandles = true;
        colUser = true;
        colPriority = false;
        colNetwork = true;
        colParent = false;
        colCreateTime = false;
        colCmdLine = false;
        colSessionId = false;
        colArch = false;
        colDescription = false;
        killMethod = 0;
        confirmKill = true;
        confirmTree = true;
        autoRefreshKill = true;
        warnCritical = true;
        cacheTimeout = 30;
    }
};

static ProcessConfig g_procConfig;
static bool g_navCollapsed = false;
static int g_currentPage = 0;

static CRITICAL_SECTION g_cacheLock;
static std::vector<ProcessInfo> g_cachedProcesses;
static DWORD g_cacheTime = 0;
static bool g_cacheInitialized = false;

struct ProcessCpuSample
{
    FILETIME ftKernel;
    FILETIME ftUser;
};

static CRITICAL_SECTION g_cpuLock;
static bool g_cpuLockInitialized = false;
static std::map<DWORD, ProcessCpuSample> g_prevProcessCpu;
static FILETIME g_prevSysKernel = {};
static FILETIME g_prevSysUser = {};
static bool g_prevCpuValid = false;

static CRITICAL_SECTION g_cpuDataLock;
static bool g_cpuDataLockInitialized = false;
static std::map<DWORD, double> g_cpuUsageMap;
static HANDLE g_hCpuThread = NULL;
static std::atomic<bool> g_cpuThreadRunning(false);

static void InitCacheLock()
{
    if (!g_cacheInitialized)
    {
        InitializeCriticalSection(&g_cacheLock);
        g_cacheInitialized = true;
    }
}

static bool GetCachedProcesses(std::vector<ProcessInfo>& processes)
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    DWORD now = GetTickCount();
    DWORD timeout = (DWORD)(g_procConfig.cacheTimeout * 1000);
    if (timeout == 0) timeout = 5000;
    if (!g_cachedProcesses.empty() && (now - g_cacheTime) < timeout)
    {
        processes = g_cachedProcesses;
        LeaveCriticalSection(&g_cacheLock);
        return true;
    }
    LeaveCriticalSection(&g_cacheLock);
    return false;
}

static void UpdateProcessCache(std::vector<ProcessInfo>& processes)
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    g_cachedProcesses = processes;
    g_cacheTime = GetTickCount();
    LeaveCriticalSection(&g_cacheLock);
}

static void InvalidateProcessCache()
{
    InitCacheLock();
    EnterCriticalSection(&g_cacheLock);
    g_cachedProcesses.clear();
    g_cacheTime = 0;
    LeaveCriticalSection(&g_cacheLock);
}

static void LoadProcConfig();
static void SaveProcConfig();
static std::wstring GetProcessExePath(DWORD pid);
static void ApplyCpuUsageFromMap(std::vector<ProcessInfo>& processes);
static double GetCpuUsageForPid(DWORD pid);
static void StartCpuSamplerThread();
static void StopCpuSamplerThread();

static bool IsProcessVfsPath(LPCWSTR pszPath)
{
    if (pszPath == NULL) return false;
    return _wcsnicmp(pszPath, PROCESS_VFS_PREFIX, PROCESS_VFS_PREFIX_LEN) == 0;
}

static bool IsProcessRootPath(LPCWSTR pszPath)
{
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, PROCESS_VFS_PREFIX) == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    if (_wcsicmp(s.c_str(), L"process:") == 0) return true;
    return false;
}

static std::wstring GetSubPath(LPCWSTR pszPath)
{
    if (!IsProcessVfsPath(pszPath)) return L"";
    std::wstring path = pszPath + PROCESS_VFS_PREFIX_LEN;
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();
    return path;
}

static std::wstring GetProcessFolderName(LPCWSTR pszPath)
{
    std::wstring subPath = GetSubPath(pszPath);
    if (subPath.empty()) return L"";
    size_t slashPos = subPath.find(L'/');
    if (slashPos != std::wstring::npos) return subPath.substr(0, slashPos);
    return subPath;
}

static bool IsProcessFolderPath(LPCWSTR pszPath)
{
    if (IsProcessRootPath(pszPath)) return false;
    std::wstring subPath = GetSubPath(pszPath);
    if (subPath.empty()) return false;
    return subPath.find(L'/') == std::wstring::npos;
}

static DWORD GetPidFromPath(LPCWSTR pszPath)
{
    std::wstring subPath = GetSubPath(pszPath);
    if (subPath.empty()) return 0;
    size_t slashPos = subPath.find(L'/');
    if (slashPos == std::wstring::npos) return 0;
    std::wstring fileName = subPath.substr(slashPos + 1);
    size_t dotPos = fileName.rfind(L'.');
    if (dotPos != std::wstring::npos) fileName = fileName.substr(0, dotPos);
    DWORD pid = 0;
    for (size_t i = 0; i < fileName.length(); i++)
    {
        if (fileName[i] >= L'0' && fileName[i] <= L'9')
            pid = pid * 10 + (fileName[i] - L'0');
        else
            break;
    }
    return pid;
}

static bool IsProcessFilePath(LPCWSTR pszPath)
{
    if (IsProcessRootPath(pszPath)) return false;
    std::wstring subPath = GetSubPath(pszPath);
    if (subPath.empty()) return false;
    return subPath.find(L'/') != std::wstring::npos;
}

static std::wstring FormatSize(SIZE_T bytes)
{
    WCHAR buf[64];
    if (bytes >= 1024ULL * 1024 * 1024)
        swprintf_s(buf, L"%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024 * 1024)
        swprintf_s(buf, L"%.1f MB", (double)bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        swprintf_s(buf, L"%.1f KB", (double)bytes / 1024.0);
    else
        swprintf_s(buf, L"%llu B", (unsigned long long)bytes);
    return buf;
}

static std::wstring FormatSizeUnsigned(ULONGLONG bytes)
{
    WCHAR buf[64];
    if (bytes >= 1024ULL * 1024 * 1024)
        swprintf_s(buf, L"%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024 * 1024)
        swprintf_s(buf, L"%.1f MB", (double)bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        swprintf_s(buf, L"%.1f KB", (double)bytes / 1024.0);
    else
        swprintf_s(buf, L"%llu B", (unsigned long long)bytes);
    return buf;
}

static std::wstring GetPriorityName(DWORD priorityClass)
{
    switch (priorityClass)
    {
    case REALTIME_PRIORITY_CLASS: return L"\u5b9e\u65f6";
    case HIGH_PRIORITY_CLASS: return L"\u9ad8";
    case ABOVE_NORMAL_PRIORITY_CLASS: return L"\u9ad8\u4e8e\u6b63\u5e38";
    case NORMAL_PRIORITY_CLASS: return L"\u6b63\u5e38";
    case BELOW_NORMAL_PRIORITY_CLASS: return L"\u4f4e\u4e8e\u6b63\u5e38";
    case IDLE_PRIORITY_CLASS: return L"\u7a7a\u95f2";
    default: return L"";
    }
}

static bool IsCriticalProcess(DWORD pid)
{
    if (pid == 0 || pid == 4) return true;

    std::wstring exePath = GetProcessExePath(pid);
    if (exePath.empty()) return false;

    std::wstring path = exePath;
    std::transform(path.begin(), path.end(), path.begin(), ::towlower);
    if (path.find(L"\\windows\\system32\\") != std::wstring::npos)
    {
        if (path.find(L"csrss.exe") != std::wstring::npos ||
            path.find(L"wininit.exe") != std::wstring::npos ||
            path.find(L"services.exe") != std::wstring::npos ||
            path.find(L"lsass.exe") != std::wstring::npos ||
            path.find(L"winlogon.exe") != std::wstring::npos ||
            path.find(L"smss.exe") != std::wstring::npos)
        {
            return true;
        }
    }

    return false;
}

static ULONGLONG FileTimeToUll(const FILETIME& ft)
{
    return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static void CalculateCpuUsage(std::vector<ProcessInfo>& processes)
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
    {
        for (auto& p : processes) p.cpuUsage = 0.0;
        return;
    }

    if (!g_cpuLockInitialized)
    {
        InitializeCriticalSection(&g_cpuLock);
        g_cpuLockInitialized = true;
    }

    EnterCriticalSection(&g_cpuLock);

    if (!g_prevCpuValid)
    {
        for (auto& p : processes) p.cpuUsage = 0.0;

        g_prevProcessCpu.clear();
        for (const auto& p : processes)
        {
            ProcessCpuSample sample;
            sample.ftKernel = p.ftKernel;
            sample.ftUser = p.ftUser;
            g_prevProcessCpu[p.pid] = sample;
        }

        g_prevSysKernel = kernelTime;
        g_prevSysUser = userTime;
        g_prevCpuValid = true;

        LeaveCriticalSection(&g_cpuLock);
        return;
    }

    ULONGLONG sysDelta = (FileTimeToUll(kernelTime) - FileTimeToUll(g_prevSysKernel))
                       + (FileTimeToUll(userTime) - FileTimeToUll(g_prevSysUser));

    if (sysDelta == 0)
    {
        for (auto& p : processes) p.cpuUsage = 0.0;
    }
    else
    {
        for (auto& p : processes)
        {
            auto it = g_prevProcessCpu.find(p.pid);
            if (it != g_prevProcessCpu.end())
            {
                ULONGLONG procDelta = (FileTimeToUll(p.ftKernel) - FileTimeToUll(it->second.ftKernel))
                                    + (FileTimeToUll(p.ftUser) - FileTimeToUll(it->second.ftUser));
                p.cpuUsage = (double)procDelta / (double)sysDelta * 100.0;
                if (p.cpuUsage < 0.0) p.cpuUsage = 0.0;
                if (p.cpuUsage > 100.0) p.cpuUsage = 100.0;
            }
            else
            {
                p.cpuUsage = 0.0;
            }
        }
    }

    g_prevProcessCpu.clear();
    for (const auto& p : processes)
    {
        ProcessCpuSample sample;
        sample.ftKernel = p.ftKernel;
        sample.ftUser = p.ftUser;
        g_prevProcessCpu[p.pid] = sample;
    }

    g_prevSysKernel = kernelTime;
    g_prevSysUser = userTime;

    LeaveCriticalSection(&g_cpuLock);

    if (!g_cpuDataLockInitialized)
    {
        InitializeCriticalSection(&g_cpuDataLock);
        g_cpuDataLockInitialized = true;
    }
    EnterCriticalSection(&g_cpuDataLock);
    g_cpuUsageMap.clear();
    for (const auto& p : processes)
    {
        g_cpuUsageMap[p.pid] = p.cpuUsage;
    }
    LeaveCriticalSection(&g_cpuDataLock);
}

static bool EnableDebugPrivilege()
{
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid))
    {
        CloseHandle(hToken);
        return false;
    }
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(hToken);
    return result && (err == ERROR_SUCCESS);
}

static std::wstring FindExeInSystemPaths(const std::wstring& processName)
{
    if (processName.empty()) return L"";

    const WCHAR* searchPaths[] = {
        L"%SystemRoot%\\System32\\",
        L"%SystemRoot%\\SysWOW64\\",
        L"%SystemRoot%\\System32\\DriverStore\\FileRepository\\",
        L"%SystemRoot%\\",
        L"%ProgramFiles%\\",
        L"%ProgramFiles(x86)%\\",
        NULL
    };

    for (int i = 0; searchPaths[i] != NULL; i++)
    {
        WCHAR expanded[MAX_PATH] = {};
        ExpandEnvironmentStringsW(searchPaths[i], expanded, MAX_PATH);
        std::wstring fullPath = expanded + processName;

        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            return fullPath;
    }

    WCHAR sysDir[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (len > 0)
    {
        std::wstring fullPath = std::wstring(sysDir) + L"\\" + processName;
        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            return fullPath;
    }

    WCHAR winDir[MAX_PATH] = {};
    len = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (len > 0)
    {
        std::wstring fullPath = std::wstring(winDir) + L"\\" + processName;
        if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            return fullPath;
    }

    return L"";
}

static std::wstring GetProcessExePath(DWORD pid)
{
    if (pid == 0) return L"System Idle Process";
    if (pid == 4) return L"System";

    std::wstring result;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess)
    {
        WCHAR exePath[MAX_PATH] = {};
        DWORD exeLen = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &exeLen))
        {
            result = exePath;
        }
        CloseHandle(hProcess);
    }

    if (result.empty())
    {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProcess)
        {
            WCHAR exePath[MAX_PATH] = {};
            DWORD exeLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &exeLen))
            {
                result = exePath;
            }
            CloseHandle(hProcess);
        }
    }

    if (result.empty())
    {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess)
        {
            WCHAR exePath[MAX_PATH] = {};
            if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH))
            {
                result = exePath;
            }
            CloseHandle(hProcess);
        }
    }

    if (result.empty())
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnapshot, &pe32))
            {
                do
                {
                    if (pe32.th32ProcessID == pid)
                    {
                        result = FindExeInSystemPaths(pe32.szExeFile);
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
    }

    return result;
}

struct NetworkCounts
{
    int tcpCount = 0;
    int udpCount = 0;
};

static std::map<DWORD, NetworkCounts> GetNetworkCounts()
{
    std::map<DWORD, NetworkCounts> counts;

    DWORD size = 0;
    DWORD result = GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER && size > 0)
    {
        std::vector<BYTE> buf(size);
        if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR)
        {
            PMIB_TCPTABLE_OWNER_PID pTable = (PMIB_TCPTABLE_OWNER_PID)buf.data();
            for (DWORD i = 0; i < pTable->dwNumEntries; i++)
            {
                DWORD pid = pTable->table[i].dwOwningPid;
                if (pid != 0) counts[pid].tcpCount++;
            }
        }
    }

    size = 0;
    result = GetExtendedTcpTable(NULL, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER && size > 0)
    {
        std::vector<BYTE> buf(size);
        if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR)
        {
            PMIB_TCPTABLE_OWNER_PID pTable = (PMIB_TCPTABLE_OWNER_PID)buf.data();
            for (DWORD i = 0; i < pTable->dwNumEntries; i++)
            {
                DWORD pid = pTable->table[i].dwOwningPid;
                if (pid != 0) counts[pid].tcpCount++;
            }
        }
    }

    size = 0;
    result = GetExtendedUdpTable(NULL, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER && size > 0)
    {
        std::vector<BYTE> buf(size);
        if (GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR)
        {
            PMIB_UDPTABLE_OWNER_PID pTable = (PMIB_UDPTABLE_OWNER_PID)buf.data();
            for (DWORD i = 0; i < pTable->dwNumEntries; i++)
            {
                DWORD pid = pTable->table[i].dwOwningPid;
                if (pid != 0) counts[pid].udpCount++;
            }
        }
    }

    size = 0;
    result = GetExtendedUdpTable(NULL, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER && size > 0)
    {
        std::vector<BYTE> buf(size);
        if (GetExtendedUdpTable(buf.data(), &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR)
        {
            PMIB_UDPTABLE_OWNER_PID pTable = (PMIB_UDPTABLE_OWNER_PID)buf.data();
            for (DWORD i = 0; i < pTable->dwNumEntries; i++)
            {
                DWORD pid = pTable->table[i].dwOwningPid;
                if (pid != 0) counts[pid].udpCount++;
            }
        }
    }

    return counts;
}

static std::wstring FormatFileTime(const FILETIME& ft)
{
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return L"";
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    WCHAR buf[64];
    swprintf_s(buf, L"%04d/%02d/%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    return buf;
}

static std::wstring GetProcessCommandLine(DWORD pid)
{
    std::wstring result;

    typedef DWORD(WINAPI* pfnNtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtDll) return result;

    pfnNtQueryInformationProcess NtQIP = (pfnNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
    if (!NtQIP) return result;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess)
    {
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) return result;
    }

    ULONG_PTR pebBaseAddr = 0;
    PROCESS_BASIC_INFORMATION pbi = {};
    ULONG retLen = 0;
    NTSTATUS status = NtQIP(hProcess, 0, &pbi, sizeof(pbi), &retLen);
    if (status == 0)
    {
        pebBaseAddr = (ULONG_PTR)pbi.PebBaseAddress;
    }

    if (pebBaseAddr == 0)
    {
        CloseHandle(hProcess);
        return result;
    }

    BYTE pebData[0x300] = {};
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)pebBaseAddr, pebData, sizeof(pebData), &bytesRead))
    {
        CloseHandle(hProcess);
        return result;
    }

    ULONG_PTR rtlUserProcParams = *(ULONG_PTR*)(pebData + 0x20);
    if (rtlUserProcParams == 0)
    {
        CloseHandle(hProcess);
        return result;
    }

    BYTE paramData[0x100] = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)rtlUserProcParams, paramData, sizeof(paramData), &bytesRead))
    {
        CloseHandle(hProcess);
        return result;
    }

    UNICODE_STRING cmdLineUS = {};
    cmdLineUS.Length = *(USHORT*)(paramData + 0x60);
    cmdLineUS.MaximumLength = *(USHORT*)(paramData + 0x62);
    cmdLineUS.Buffer = *(WCHAR**)(paramData + 0x64);

    if (cmdLineUS.Length > 0 && cmdLineUS.Buffer)
    {
        std::vector<WCHAR> cmdBuf(cmdLineUS.Length / sizeof(WCHAR) + 1);
        if (ReadProcessMemory(hProcess, cmdLineUS.Buffer, cmdBuf.data(), cmdLineUS.Length, &bytesRead))
        {
            cmdBuf[cmdLineUS.Length / sizeof(WCHAR)] = L'\0';
            result = cmdBuf.data();
        }
    }

    CloseHandle(hProcess);
    return result;
}

static std::wstring GetFileDescription(const std::wstring& exePath)
{
    if (exePath.empty()) return L"";

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), &dummy);
    if (size == 0) return L"";

    std::vector<BYTE> buf(size);
    if (!GetFileVersionInfoW(exePath.c_str(), 0, size, buf.data())) return L"";

    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; };
    LANGANDCODEPAGE* lpTranslate = NULL;
    UINT cbTranslate = 0;

    if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate))
    {
        if (cbTranslate >= sizeof(LANGANDCODEPAGE))
        {
            WCHAR subBlock[64];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);

            LPWSTR pDesc = NULL;
            UINT cbDesc = 0;
            if (VerQueryValueW(buf.data(), subBlock, (LPVOID*)&pDesc, &cbDesc))
            {
                if (pDesc && cbDesc > 0) return pDesc;
            }
        }
    }

    return L"";
}

static void FillProcessDetails(ProcessInfo& info, const std::map<DWORD, NetworkCounts>& networkCounts)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
    if (!hProcess)
    {
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
    }

    if (hProcess)
    {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            info.workingSetSize = pmc.WorkingSetSize;
        }

        FILETIME ftCreate, ftExit, ftKernel, ftUser;
        if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
        {
            info.ftCreation = ftCreate;
            info.ftKernel = ftKernel;
            info.ftUser = ftUser;
        }

        IO_COUNTERS ioCounters;
        if (GetProcessIoCounters(hProcess, &ioCounters))
        {
            info.readTransferCount = ioCounters.ReadTransferCount;
            info.writeTransferCount = ioCounters.WriteTransferCount;
        }

        GetProcessHandleCount(hProcess, &info.handleCount);

        info.priorityClass = GetPriorityClass(hProcess);

        {
            WCHAR exePath[MAX_PATH] = {};
            DWORD exeLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &exeLen))
            {
                info.exePath = exePath;
            }
        }

        if (info.exePath.empty())
        {
            WCHAR exePath[MAX_PATH] = {};
            if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH))
            {
                info.exePath = exePath;
            }
        }

        {
            HANDLE hToken = NULL;
            if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
            {
                DWORD needed = 0;
                GetTokenInformation(hToken, TokenUser, NULL, 0, &needed);
                if (needed > 0)
                {
                    std::vector<BYTE> buf(needed);
                    if (GetTokenInformation(hToken, TokenUser, buf.data(), needed, &needed))
                    {
                        TOKEN_USER* pUser = (TOKEN_USER*)buf.data();
                        WCHAR name[256] = {}, domain[256] = {};
                        DWORD nameLen = 256, domainLen = 256;
                        SID_NAME_USE sidType;
                        if (LookupAccountSidW(NULL, pUser->User.Sid, name, &nameLen, domain, &domainLen, &sidType))
                        {
                            info.userName = std::wstring(domain) + L"\\" + std::wstring(name);
                        }
                    }
                }
                CloseHandle(hToken);
            }
        }

        {
            typedef BOOL(WINAPI* pfnIsWow64Process)(HANDLE, PBOOL);
            pfnIsWow64Process fnIsWow64 = (pfnIsWow64Process)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process");
            if (fnIsWow64) fnIsWow64(hProcess, &info.isWow64);
        }

        CloseHandle(hProcess);
    }

    if (info.exePath.empty())
    {
        info.exePath = GetProcessExePath(info.pid);
    }

    auto it = networkCounts.find(info.pid);
    if (it != networkCounts.end())
    {
        info.tcpCount = it->second.tcpCount;
        info.udpCount = it->second.udpCount;
        info.hasNetwork = (info.tcpCount > 0 || info.udpCount > 0);
    }
    ProcessIdToSessionId(info.pid, &info.sessionId);
}

static bool EnumProcesses(std::vector<ProcessInfo>& processes)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    std::map<DWORD, NetworkCounts> networkCounts = GetNetworkCounts();

    std::vector<PROCESSENTRY32W> entries;
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32))
    {
        do
        {
            entries.push_back(pe32);
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    std::map<DWORD, std::wstring> pidNameMap;
    for (const auto& e : entries)
        pidNameMap[e.th32ProcessID] = e.szExeFile;

    for (const auto& pe : entries)
    {
        ProcessInfo info;
        info.pid = pe.th32ProcessID;
        info.name = pe.szExeFile;
        info.threadCount = pe.cntThreads;
        info.parentPid = pe.th32ParentProcessID;
        info.isRunning = true;
        info.isCritical = IsCriticalProcess(info.pid);

        auto it = pidNameMap.find(info.parentPid);
        if (it != pidNameMap.end())
            info.parentName = it->second;

        FillProcessDetails(info, networkCounts);

        if (g_procConfig.colCmdLine)
            info.commandLine = GetProcessCommandLine(info.pid);
        if (g_procConfig.colDescription && !info.exePath.empty())
            info.fileDescription = GetFileDescription(info.exePath);

        if (!g_procConfig.showSystemProcesses && info.isCritical && info.pid != GetCurrentProcessId())
            continue;

        processes.push_back(std::move(info));
    }

    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.name < b.name;
    });

    ApplyCpuUsageFromMap(processes);

    return true;
}

static bool GetProcessInfo(DWORD pid, ProcessInfo& info)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    std::map<DWORD, NetworkCounts> networkCounts = GetNetworkCounts();

    std::map<DWORD, std::wstring> pidNameMap;
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32))
    {
        do
        {
            pidNameMap[pe32.th32ProcessID] = pe32.szExeFile;
            if (pe32.th32ProcessID == pid)
            {
                info.pid = pe32.th32ProcessID;
                info.name = pe32.szExeFile;
                info.threadCount = pe32.cntThreads;
                info.parentPid = pe32.th32ParentProcessID;
                info.isRunning = true;
                info.isCritical = IsCriticalProcess(info.pid);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (info.pid != pid) return false;

    auto it = pidNameMap.find(info.parentPid);
    if (it != pidNameMap.end()) info.parentName = it->second;

    FillProcessDetails(info, networkCounts);

    info.commandLine = GetProcessCommandLine(info.pid);
    info.fileDescription = GetFileDescription(info.exePath);

    info.cpuUsage = GetCpuUsageForPid(info.pid);

    return true;
}

static std::wstring FormatProcessInfo(const ProcessInfo& info)
{
    std::wstring result;
    result += L"\u8fdb\u7a0b\u540d\u79f0: " + info.name + L"\r\n";
    result += L"\u8fdb\u7a0b ID: " + std::to_wstring(info.pid) + L"\r\n";
    result += L"\u72b6\u6001: " + std::wstring(info.isRunning ? L"\u6b63\u5728\u8fd0\u884c" : L"\u5df2\u505c\u6b62") + L"\r\n";
    result += L"\u5185\u5b58\u4f7f\u7528: " + FormatSize(info.workingSetSize) + L"\r\n";
    result += L"\u7ebf\u7a0b\u6570: " + std::to_wstring(info.threadCount) + L"\r\n";
    result += L"\u53e5\u67c4\u6570: " + std::to_wstring(info.handleCount) + L"\r\n";
    result += L"\u4f18\u5148\u7ea7: " + GetPriorityName(info.priorityClass) + L"\r\n";
    result += L"\u7528\u6237\u540d: " + (info.userName.empty() ? L"N/A" : info.userName) + L"\r\n";
    result += L"\u7236\u8fdb\u7a0b: " + (info.parentName.empty() ? std::to_wstring(info.parentPid) : info.parentName + L" (" + std::to_wstring(info.parentPid) + L")") + L"\r\n";
    result += L"\u78c1\u76d8\u8bfb\u53d6: " + FormatSizeUnsigned(info.readTransferCount) + L"\r\n";
    result += L"\u78c1\u76d8\u5199\u5165: " + FormatSizeUnsigned(info.writeTransferCount) + L"\r\n";
    std::wstring networkDetail;
    if (info.tcpCount > 0) networkDetail += L"TCP:" + std::to_wstring(info.tcpCount);
    if (info.udpCount > 0)
    {
        if (!networkDetail.empty()) networkDetail += L", ";
        networkDetail += L"UDP:" + std::to_wstring(info.udpCount);
    }
    result += L"\u7f51\u7edc\u8fde\u63a5: " + (networkDetail.empty() ? L"\u5426" : networkDetail) + L"\r\n";
    result += L"\u53ef\u6267\u884c\u8def\u5f84: " + info.exePath + L"\r\n";
    result += L"\u521b\u5efa\u65f6\u95f4: " + FormatFileTime(info.ftCreation) + L"\r\n";
    result += L"\u4f1a\u8bddID: " + std::to_wstring(info.sessionId) + L"\r\n";
    result += L"\u67b6\u6784: " + std::wstring(info.isWow64 ? L"32\u4f4d" : L"64\u4f4d") + L"\r\n";
    if (!info.commandLine.empty()) result += L"\u547d\u4ee4\u884c: " + info.commandLine + L"\r\n";
    if (!info.fileDescription.empty()) result += L"\u63cf\u8ff0: " + info.fileDescription + L"\r\n";
    if (info.isCritical) result += L"\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b: \u662f\r\n";
    return result;
}

struct EnumWindowsData
{
    DWORD targetPid;
    bool found;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsData* pData = (EnumWindowsData*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == pData->targetPid && IsWindowVisible(hwnd))
    {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        pData->found = true;
    }
    return TRUE;
}

static bool KillProcess(DWORD pid, bool force)
{
    if (!force && g_procConfig.killMethod == 0)
    {
        EnumWindowsData ewd = { pid, false };
        EnumWindows(EnumWindowsProc, (LPARAM)&ewd);
        if (ewd.found)
        {
            HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
            if (hProcess)
            {
                DWORD waitResult = WaitForSingleObject(hProcess, 3000);
                CloseHandle(hProcess);
                if (waitResult == WAIT_OBJECT_0)
                {
                    InvalidateProcessCache();
                    return true;
                }
            }
        }
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    if (result) InvalidateProcessCache();
    return result != FALSE;
}

static bool KillProcessTreeInternal(DWORD pid, int depth)
{
    if (depth > 32) return false;
    std::vector<DWORD> children;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe32))
        {
            do
            {
                if (pe32.th32ParentProcessID == pid) children.push_back(pe32.th32ProcessID);
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    for (DWORD childPid : children) KillProcessTreeInternal(childPid, depth + 1);
    return KillProcess(pid, true);
}

static bool KillProcessTree(DWORD pid)
{
    return KillProcessTreeInternal(pid, 0);
}

static bool SetProcessPriority(DWORD pid, DWORD priorityClass)
{
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    BOOL result = SetPriorityClass(hProcess, priorityClass);
    CloseHandle(hProcess);
    if (result) InvalidateProcessCache();
    return result != FALSE;
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str)
{
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

static bool IsColumnEnabled(int columnId)
{
    switch (columnId)
    {
    case 1: return g_procConfig.colPid;
    case 2: return g_procConfig.colStatus;
    case 3: return g_procConfig.colCpu;
    case 4: return g_procConfig.colMemory;
    case 5: return g_procConfig.colThreads;
    case 6: return g_procConfig.colPath;
    case 7: return g_procConfig.colDisk;
    case 8: return g_procConfig.colHandles;
    case 9: return g_procConfig.colUser;
    case 10: return g_procConfig.colPriority;
    case 11: return g_procConfig.colNetwork;
    case 12: return g_procConfig.colParent;
    case 13: return g_procConfig.colCreateTime;
    case 14: return g_procConfig.colCmdLine;
    case 15: return g_procConfig.colSessionId;
    case 16: return g_procConfig.colArch;
    case 17: return g_procConfig.colDescription;
    default: return false;
    }
}

static int CountEnabledColumns()
{
    int count = 0;
    for (int id = 1; id <= NUM_COLUMNS; id++)
        if (IsColumnEnabled(id)) count++;
    return count;
}

static void SetCustomColumnsForFile(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const ProcessInfo& info)
{
    if (!lpFileData || !hHeap) return;

    lpFileData->iNumColumns = NUM_COLUMNS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        NUM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));

    if (!lpFileData->lpvfsColumnData) return;

    WCHAR buf[64];

    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, std::to_wstring(info.pid));

    lpFileData->lpvfsColumnData[1].iColumnId = 2;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, info.isRunning ? L"\u6b63\u5728\u8fd0\u884c" : L"\u5df2\u505c\u6b62");

    swprintf_s(buf, L"%.1f%%", info.cpuUsage);
    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, buf);

    lpFileData->lpvfsColumnData[3].iColumnId = 4;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, FormatSize(info.workingSetSize));

    lpFileData->lpvfsColumnData[4].iColumnId = 5;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, std::to_wstring(info.threadCount));

    lpFileData->lpvfsColumnData[5].iColumnId = 6;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, info.exePath);

    std::wstring diskInfo = FormatSizeUnsigned(info.readTransferCount) + L" / " + FormatSizeUnsigned(info.writeTransferCount);
    lpFileData->lpvfsColumnData[6].iColumnId = 7;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, diskInfo);

    lpFileData->lpvfsColumnData[7].iColumnId = 8;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, std::to_wstring(info.handleCount));

    lpFileData->lpvfsColumnData[8].iColumnId = 9;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, info.userName);

    lpFileData->lpvfsColumnData[9].iColumnId = 10;
    lpFileData->lpvfsColumnData[9].lpszValue = AllocString(hHeap, GetPriorityName(info.priorityClass));

    lpFileData->lpvfsColumnData[10].iColumnId = 11;
    std::wstring networkInfo;
    if (info.tcpCount > 0 || info.udpCount > 0)
    {
        if (info.tcpCount > 0) networkInfo += L"TCP:" + std::to_wstring(info.tcpCount);
        if (info.udpCount > 0)
        {
            if (!networkInfo.empty()) networkInfo += L" ";
            networkInfo += L"UDP:" + std::to_wstring(info.udpCount);
        }
    }
    lpFileData->lpvfsColumnData[10].lpszValue = AllocString(hHeap, networkInfo);

    lpFileData->lpvfsColumnData[11].iColumnId = 12;
    lpFileData->lpvfsColumnData[11].lpszValue = AllocString(hHeap, info.parentName.empty() ? std::to_wstring(info.parentPid) : info.parentName);

    lpFileData->lpvfsColumnData[12].iColumnId = 13;
    lpFileData->lpvfsColumnData[12].lpszValue = AllocString(hHeap, FormatFileTime(info.ftCreation));

    lpFileData->lpvfsColumnData[13].iColumnId = 14;
    lpFileData->lpvfsColumnData[13].lpszValue = AllocString(hHeap, info.commandLine);

    lpFileData->lpvfsColumnData[14].iColumnId = 15;
    lpFileData->lpvfsColumnData[14].lpszValue = AllocString(hHeap, std::to_wstring(info.sessionId));

    lpFileData->lpvfsColumnData[15].iColumnId = 16;
    lpFileData->lpvfsColumnData[15].lpszValue = AllocString(hHeap, info.isWow64 ? L"32\u4f4d" : L"64\u4f4d");

    lpFileData->lpvfsColumnData[16].iColumnId = 17;
    lpFileData->lpvfsColumnData[16].lpszValue = AllocString(hHeap, info.fileDescription);
}

static void SetCustomColumnsForFolder(LPVFSFILEDATAW lpFileData, HANDLE hHeap,
    const std::wstring& processName, const std::vector<ProcessInfo>& instances)
{
    if (!lpFileData || !hHeap) return;

    int count = (int)instances.size();
    SIZE_T totalMemory = 0;
    DWORD totalThreads = 0;
    ULONGLONG totalRead = 0, totalWrite = 0;
    DWORD totalHandles = 0;
    std::wstring exePath;
    bool anyCritical = false;
    int totalTcp = 0;
    int totalUdp = 0;
    double totalCpu = 0.0;
    DWORD mostCommonPriority = 0;
    FILETIME earliestCreation = {};
    std::wstring userName;
    std::wstring parentName;
    std::wstring fileDescription;
    std::map<DWORD, int> priorityCounts;
    bool anyWow64 = false;
    DWORD sessionId = 0;

    for (const auto& info : instances)
    {
        totalMemory += info.workingSetSize;
        totalThreads += info.threadCount;
        totalRead += info.readTransferCount;
        totalWrite += info.writeTransferCount;
        totalHandles += info.handleCount;
        totalCpu += info.cpuUsage;
        if (info.exePath.length() > exePath.length()) exePath = info.exePath;
        if (info.isCritical) anyCritical = true;
        totalTcp += info.tcpCount;
        totalUdp += info.udpCount;
        if (info.priorityClass != 0) priorityCounts[info.priorityClass]++;
        if (info.ftCreation.dwLowDateTime != 0 || info.ftCreation.dwHighDateTime != 0)
        {
            if (earliestCreation.dwLowDateTime == 0 && earliestCreation.dwHighDateTime == 0)
                earliestCreation = info.ftCreation;
            else
            {
                ULONGLONG existing = ((ULONGLONG)earliestCreation.dwHighDateTime << 32) | earliestCreation.dwLowDateTime;
                ULONGLONG current = ((ULONGLONG)info.ftCreation.dwHighDateTime << 32) | info.ftCreation.dwLowDateTime;
                if (current < existing) earliestCreation = info.ftCreation;
            }
        }
        if (userName.empty() && !info.userName.empty()) userName = info.userName;
        if (parentName.empty() && !info.parentName.empty()) parentName = info.parentName;
        if (fileDescription.empty() && !info.fileDescription.empty()) fileDescription = info.fileDescription;
        if (info.isWow64) anyWow64 = true;
        if (sessionId == 0 && info.sessionId != 0) sessionId = info.sessionId;
    }

    int maxPriorityCount = 0;
    for (const auto& pair : priorityCounts)
    {
        if (pair.second > maxPriorityCount) { maxPriorityCount = pair.second; mostCommonPriority = pair.first; }
    }

    lpFileData->iNumColumns = NUM_COLUMNS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
        NUM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));

    if (!lpFileData->lpvfsColumnData) return;

    WCHAR buf[64];

    swprintf_s(buf, L"%d \u4e2a\u5b9e\u4f8b", count);
    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, buf);

    lpFileData->lpvfsColumnData[1].iColumnId = 2;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, anyCritical ? L"\u542b\u7cfb\u7edf\u8fdb\u7a0b" : L"\u6b63\u5728\u8fd0\u884c");

    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    WCHAR cpuBuf[32];
    swprintf_s(cpuBuf, L"%.1f%%", totalCpu);
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, cpuBuf);

    lpFileData->lpvfsColumnData[3].iColumnId = 4;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, FormatSize(totalMemory));

    lpFileData->lpvfsColumnData[4].iColumnId = 5;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, std::to_wstring(totalThreads));

    lpFileData->lpvfsColumnData[5].iColumnId = 6;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, exePath);

    std::wstring diskInfo = FormatSizeUnsigned(totalRead) + L" / " + FormatSizeUnsigned(totalWrite);
    lpFileData->lpvfsColumnData[6].iColumnId = 7;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, diskInfo);

    lpFileData->lpvfsColumnData[7].iColumnId = 8;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, std::to_wstring(totalHandles));

    lpFileData->lpvfsColumnData[8].iColumnId = 9;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, userName);

    lpFileData->lpvfsColumnData[9].iColumnId = 10;
    lpFileData->lpvfsColumnData[9].lpszValue = AllocString(hHeap, GetPriorityName(mostCommonPriority));

    lpFileData->lpvfsColumnData[10].iColumnId = 11;
    std::wstring folderNetworkInfo;
    if (totalTcp > 0 || totalUdp > 0)
    {
        if (totalTcp > 0) folderNetworkInfo += L"TCP:" + std::to_wstring(totalTcp);
        if (totalUdp > 0)
        {
            if (!folderNetworkInfo.empty()) folderNetworkInfo += L" ";
            folderNetworkInfo += L"UDP:" + std::to_wstring(totalUdp);
        }
    }
    lpFileData->lpvfsColumnData[10].lpszValue = AllocString(hHeap, folderNetworkInfo);

    lpFileData->lpvfsColumnData[11].iColumnId = 12;
    lpFileData->lpvfsColumnData[11].lpszValue = AllocString(hHeap, parentName);

    lpFileData->lpvfsColumnData[12].iColumnId = 13;
    lpFileData->lpvfsColumnData[12].lpszValue = AllocString(hHeap, FormatFileTime(earliestCreation));

    lpFileData->lpvfsColumnData[13].iColumnId = 14;
    WCHAR cmdBuf[32];
    swprintf_s(cmdBuf, L"%d \u4e2a\u5b9e\u4f8b", count);
    lpFileData->lpvfsColumnData[13].lpszValue = AllocString(hHeap, cmdBuf);

    lpFileData->lpvfsColumnData[14].iColumnId = 15;
    lpFileData->lpvfsColumnData[14].lpszValue = AllocString(hHeap, sessionId > 0 ? std::to_wstring(sessionId) : L"");

    lpFileData->lpvfsColumnData[15].iColumnId = 16;
    lpFileData->lpvfsColumnData[15].lpszValue = AllocString(hHeap, anyWow64 ? L"32\u4f4d" : L"64\u4f4d");

    lpFileData->lpvfsColumnData[16].iColumnId = 17;
    lpFileData->lpvfsColumnData[16].lpszValue = AllocString(hHeap, fileDescription);
}

// 初始化自定义列 - 在 VFS_Init 中调用
static void InitColumns()
{
    if (g_columnsInitialized) return;

    ZeroMemory(g_columns, sizeof(g_columns));

    // 初始化列数据，lpNext 形成链表
    g_columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[0].lpNext = &g_columns[1];
    g_columns[0].lpszLabel = L"PID"; g_columns[0].lpszKey = L"procpid"; g_columns[0].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER; g_columns[0].iID = 1;

    g_columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[1].lpNext = &g_columns[2];
    g_columns[1].lpszLabel = L"\u72b6\u6001"; g_columns[1].lpszKey = L"procstatus"; g_columns[1].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[1].iID = 2;

    g_columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[2].lpNext = &g_columns[3];
    g_columns[2].lpszLabel = L"CPU"; g_columns[2].lpszKey = L"proccpu"; g_columns[2].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_PERCENT; g_columns[2].iID = 3;

    g_columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[3].lpNext = &g_columns[4];
    g_columns[3].lpszLabel = L"\u5185\u5b58"; g_columns[3].lpszKey = L"procmemory"; g_columns[3].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE; g_columns[3].iID = 4;

    g_columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[4].lpNext = &g_columns[5];
    g_columns[4].lpszLabel = L"\u7ebf\u7a0b\u6570"; g_columns[4].lpszKey = L"procthreads"; g_columns[4].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER; g_columns[4].iID = 5;

    g_columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[5].lpNext = &g_columns[6];
    g_columns[5].lpszLabel = L"\u53ef\u6267\u884c\u8def\u5f84"; g_columns[5].lpszKey = L"procpath"; g_columns[5].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[5].iID = 6;

    g_columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[6].lpNext = &g_columns[7];
    g_columns[6].lpszLabel = L"\u78c1\u76d8I/O"; g_columns[6].lpszKey = L"procdisk"; g_columns[6].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[6].iID = 7;

    g_columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[7].lpNext = &g_columns[8];
    g_columns[7].lpszLabel = L"\u53e5\u67c4\u6570"; g_columns[7].lpszKey = L"prochandles"; g_columns[7].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER; g_columns[7].iID = 8;

    g_columns[8].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[8].lpNext = &g_columns[9];
    g_columns[8].lpszLabel = L"\u7528\u6237\u540d"; g_columns[8].lpszKey = L"procuser"; g_columns[8].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[8].iID = 9;

    g_columns[9].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[9].lpNext = &g_columns[10];
    g_columns[9].lpszLabel = L"\u4f18\u5148\u7ea7"; g_columns[9].lpszKey = L"procpriority"; g_columns[9].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[9].iID = 10;

    g_columns[10].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[10].lpNext = &g_columns[11];
    g_columns[10].lpszLabel = L"\u7f51\u7edc"; g_columns[10].lpszKey = L"procnetwork"; g_columns[10].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[10].iID = 11;

    g_columns[11].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[11].lpNext = &g_columns[12];
    g_columns[11].lpszLabel = L"\u7236\u8fdb\u7a0b"; g_columns[11].lpszKey = L"procparent"; g_columns[11].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[11].iID = 12;

    g_columns[12].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[12].lpNext = &g_columns[13];
    g_columns[12].lpszLabel = L"\u521b\u5efa\u65f6\u95f4"; g_columns[12].lpszKey = L"proccreatetime"; g_columns[12].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[12].iID = 13;

    g_columns[13].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[13].lpNext = &g_columns[14];
    g_columns[13].lpszLabel = L"\u547d\u4ee4\u884c"; g_columns[13].lpszKey = L"proccmdline"; g_columns[13].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[13].iID = 14;

    g_columns[14].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[14].lpNext = &g_columns[15];
    g_columns[14].lpszLabel = L"\u4f1a\u8bddID"; g_columns[14].lpszKey = L"procsessionid"; g_columns[14].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER; g_columns[14].iID = 15;

    g_columns[15].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[15].lpNext = &g_columns[16];
    g_columns[15].lpszLabel = L"\u67b6\u6784"; g_columns[15].lpszKey = L"procarch"; g_columns[15].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[15].iID = 16;

    g_columns[16].cbSize = sizeof(VFSCUSTOMCOLUMNW); g_columns[16].lpNext = NULL;
    g_columns[16].lpszLabel = L"\u63cf\u8ff0"; g_columns[16].lpszKey = L"procdesc"; g_columns[16].dwFlags = VFSCCF_LEFTJUSTIFY; g_columns[16].iID = 17;

    g_columnsInitialized = true;
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData)
{
    EnableDebugPrivilege();
    InitCacheLock();
    InitColumns();  // 在 VFS_Init 中初始化列，确保 DOpus 查询时列已就绪
    LoadProcConfig();
    StartCpuSamplerThread();
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit()
{
    StopCpuSamplerThread();
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) Sleep(50);
    DeleteCriticalSection(&g_cacheLock);
    DeleteCriticalSection(&g_cpuLock);
    DeleteCriticalSection(&g_cpuDataLock);
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

    lpVFSInfo->idPlugin = GUIDPlugin_Process;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, PROCESS_VFS_PREFIX);
    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"\u8fdb\u7a0b");
    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
                       L"Windows \u8fdb\u7a0b - \u6d4f\u89c8\u548c\u7ba1\u7406\u8fdb\u7a0b");
    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");
    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"taskmgr.exe", 0, &hIconLarge, &hIconSmall, 1);
    if (!hIconLarge) ExtractIconExW(L"shell32.dll", 14, &hIconLarge, &hIconSmall, 1);
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax)
{
    if (!lpszPrefix || cchPrefixMax < 12) return FALSE;
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, L"process://");
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData)
{
    // 返回预初始化的列链表
    // hVFSData 参数可用于根据特定目录返回不同的列，但 ProcessVFS 对所有路径提供相同的列
    return g_columnsInitialized ? g_columns : NULL;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetAllCustomColumnsW()
{
    // 返回所有列 - 与 VFS_GetCustomColumnsW 相同，因为 ProcessVFS 对所有路径提供相同的列
    return g_columnsInitialized ? g_columns : NULL;
}

static double GetCpuUsageForPid(DWORD pid)
{
    if (!g_cpuDataLockInitialized) return 0.0;
    double usage = 0.0;
    EnterCriticalSection(&g_cpuDataLock);
    auto it = g_cpuUsageMap.find(pid);
    if (it != g_cpuUsageMap.end()) usage = it->second;
    LeaveCriticalSection(&g_cpuDataLock);
    return usage;
}

static void ApplyCpuUsageFromMap(std::vector<ProcessInfo>& processes)
{
    if (!g_cpuDataLockInitialized) return;
    EnterCriticalSection(&g_cpuDataLock);
    for (auto& p : processes)
    {
        auto it = g_cpuUsageMap.find(p.pid);
        if (it != g_cpuUsageMap.end()) p.cpuUsage = it->second;
    }
    LeaveCriticalSection(&g_cpuDataLock);
}

static DWORD WINAPI CpuSamplerThread(LPVOID lpParam)
{
    while (g_cpuThreadRunning.load())
    {
        std::vector<ProcessInfo> samples;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnapshot, &pe32))
            {
                do
                {
                    ProcessInfo info;
                    info.pid = pe32.th32ProcessID;
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
                    if (hProcess)
                    {
                        FILETIME ftCreate, ftExit, ftKernel, ftUser;
                        if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
                        {
                            info.ftKernel = ftKernel;
                            info.ftUser = ftUser;
                        }
                        CloseHandle(hProcess);
                    }
                    samples.push_back(std::move(info));
                } while (Process32NextW(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        CalculateCpuUsage(samples);
        Sleep(1000);
    }
    return 0;
}

static void StartCpuSamplerThread()
{
    if (g_hCpuThread != NULL) return;
    g_cpuThreadRunning.store(true);
    g_hCpuThread = CreateThread(NULL, 0, CpuSamplerThread, NULL, CREATE_SUSPENDED, NULL);
    if (g_hCpuThread)
    {
        SetThreadPriority(g_hCpuThread, THREAD_PRIORITY_BELOW_NORMAL);
        ResumeThread(g_hCpuThread);
    }
}

static void StopCpuSamplerThread()
{
    g_cpuThreadRunning.store(false);
    if (g_hCpuThread != NULL)
    {
        WaitForSingleObject(g_hCpuThread, 3000);
        CloseHandle(g_hCpuThread);
        g_hCpuThread = NULL;
    }
}

static void RefreshCpuSamples(std::vector<ProcessInfo>& processes)
{
    ApplyCpuUsageFromMap(processes);
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    std::vector<ProcessInfo> processes;
    if (!GetCachedProcesses(processes))
    {
        if (!EnumProcesses(processes)) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
        UpdateProcessCache(processes);
    }
    else
    {
        RefreshCpuSamples(processes);
    }

    if (IsProcessRootPath(lpRDD->lpszPath))
    {
        std::map<std::wstring, std::vector<ProcessInfo>> grouped;
        for (const auto& info : processes) grouped[info.name].push_back(info);

        int numItems = (int)grouped.size();
        size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
        if (!lpFDH) return FALSE;

        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
        lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
        lpFDH->iNumItems = numItems;

        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        int i = 0;
        for (const auto& pair : grouped)
        {
            if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

            StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, pair.first.c_str());
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
            lpFileData[i].wfdData.ftLastAccessTime = lpFileData[i].wfdData.ftCreationTime;
            lpFileData[i].wfdData.ftLastWriteTime = lpFileData[i].wfdData.ftCreationTime;

            bool anyCritical = false;
            for (const auto& info : pair.second) { if (info.isCritical) { anyCritical = true; break; } }
            if (anyCritical) lpFileData[i].wfdData.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;

            SetCustomColumnsForFolder(&lpFileData[i], lpRDD->hMemHeap, pair.first, pair.second);
            i++;
        }

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
    else
    {
        std::wstring folderName = GetProcessFolderName(lpRDD->lpszPath);
        if (folderName.empty()) return FALSE;

        std::vector<ProcessInfo> instances;
        for (const auto& info : processes)
        {
            if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0)
                instances.push_back(info);
        }

        int numItems = (int)instances.size();
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

            std::wstring fileName = std::to_wstring(instances[i].pid) + L".proc";
            StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, fileName.c_str());
            lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
            lpFileData[i].wfdData.ftLastAccessTime = lpFileData[i].wfdData.ftCreationTime;
            lpFileData[i].wfdData.ftLastWriteTime = lpFileData[i].wfdData.ftCreationTime;
            if (instances[i].isCritical) lpFileData[i].wfdData.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;

            SetCustomColumnsForFile(&lpFileData[i], lpRDD->hMemHeap, instances[i]);
        }

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }
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

    if (IsProcessRootPath(lpszPath))
    {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u8fdb\u7a0b");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (IsProcessFolderPath(lpszPath))
    {
        std::wstring folderName = GetProcessFolderName(lpszPath);
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, folderName.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);

        std::vector<ProcessInfo> processes;
        if (EnumProcesses(processes))
        {
            std::vector<ProcessInfo> instances;
            for (const auto& info : processes)
            {
                if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0) instances.push_back(info);
            }
            SetCustomColumnsForFolder(lpFileData, hHeap, folderName, instances);
        }
        return lpFDH;
    }

    DWORD pid = GetPidFromPath(lpszPath);
    if (pid == 0) { HeapFree(hHeap, 0, lpFDH); return NULL; }

    ProcessInfo info;
    if (!GetProcessInfo(pid, info)) { HeapFree(hHeap, 0, lpFDH); return NULL; }

    std::wstring processInfo = FormatProcessInfo(info);
    std::wstring fileName = std::to_wstring(pid) + L".proc";
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, fileName.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    if (info.isCritical) lpFileData->wfdData.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;
    lpFileData->wfdData.nFileSizeLow = (DWORD)((processInfo.length() + 1) * sizeof(WCHAR));
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
    lpFileData->wfdData.ftLastAccessTime = lpFileData->wfdData.ftCreationTime;
    lpFileData->wfdData.ftLastWriteTime = lpFileData->wfdData.ftCreationTime;

    SetCustomColumnsForFile(lpFileData, hHeap, info);
    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT)
{
    if (!IsProcessVfsPath(lpszFile)) { SetLastError(ERROR_PATH_NOT_FOUND); return NULL; }

    DWORD pid = GetPidFromPath(lpszFile);
    if (pid == 0) { SetLastError(ERROR_OPEN_FAILED); return NULL; }

    ProcessInfo info;
    if (!GetProcessInfo(pid, info)) { SetLastError(ERROR_OPEN_FAILED); return NULL; }

    ProcessFileContext* ctx = new ProcessFileContext();
    if (!ctx) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }

    ctx->readPos = 0;
    ctx->isWrite = (dwMode & GENERIC_WRITE) != 0;

    std::wstring processInfo = FormatProcessInfo(info);
    size_t bomOffset = sizeof(WCHAR);
    ctx->buffer.resize(bomOffset + processInfo.length() * sizeof(WCHAR));
    WCHAR bom = 0xFEFF;
    memcpy(ctx->buffer.data(), &bom, bomOffset);
    memcpy(ctx->buffer.data() + bomOffset, processInfo.c_str(), processInfo.length() * sizeof(WCHAR));

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize)
{
    ProcessFileContext* ctx = (ProcessFileContext*)hFile;
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
    ProcessFileContext* ctx = (ProcessFileContext*)hFile;
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
    ProcessFileContext* ctx = (ProcessFileContext*)hFile;
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
    ProcessFileContext* ctx = (ProcessFileContext*)hFile;
    if (ctx) delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile)
{
    if (!IsProcessVfsPath(lpszFile)) return FALSE;

    if (IsProcessFolderPath(lpszFile))
    {
        std::wstring folderName = GetProcessFolderName(lpszFile);
        if (folderName.empty()) return FALSE;

        if (g_procConfig.confirmKill)
        {
            std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f \"" + folderName + L"\" \u7684\u6240\u6709\u5b9e\u4f8b\u5417\uff1f";
            if (MessageBoxW(NULL, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u6240\u6709\u5b9e\u4f8b", MB_YESNO | MB_ICONQUESTION) != IDYES) return FALSE;
        }

        std::vector<ProcessInfo> processes;
        if (EnumProcesses(processes))
        {
            bool anyKilled = false;
            for (const auto& info : processes)
            {
                if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && info.pid != GetCurrentProcessId())
                {
                    if (g_procConfig.warnCritical && info.isCritical)
                    {
                        if (MessageBoxW(NULL, L"\u5176\u4e2d\u5305\u542b\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b\uff0c\u7ec8\u6b62\u53ef\u80fd\u5bfc\u81f4\u7cfb\u7edf\u4e0d\u7a33\u5b9a\u3002\u662f\u5426\u7ee7\u7eed\uff1f", L"\u8b66\u544a", MB_YESNO | MB_ICONWARNING) != IDYES) continue;
                    }
                    if (KillProcess(info.pid, false)) anyKilled = true;
                }
            }
            if (anyKilled && g_procConfig.autoRefreshKill) Sleep(200);
            return anyKilled;
        }
        return FALSE;
    }

    DWORD pid = GetPidFromPath(lpszFile);
    if (pid == 0) return FALSE;
    if (pid == GetCurrentProcessId())
    {
        MessageBoxW(NULL, L"\u4e0d\u80fd\u7ec8\u6b62\u5f53\u524d\u8fdb\u7a0b\uff01", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    if (g_procConfig.confirmKill)
    {
        ProcessInfo info; GetProcessInfo(pid, info);
        std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f\u8fdb\u7a0b \"" + info.name + L"\" (PID: " + std::to_wstring(pid) + L") \u5417\uff1f";
        if (MessageBoxW(NULL, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u8fdb\u7a0b", MB_YESNO | MB_ICONQUESTION) != IDYES) return FALSE;
    }
    if (g_procConfig.warnCritical && IsCriticalProcess(pid))
    {
        int result = MessageBoxW(NULL, L"\u8be5\u8fdb\u7a0b\u662f\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b\uff0c\u7ec8\u6b62\u53ef\u80fd\u5bfc\u81f4\u7cfb\u7edf\u4e0d\u7a33\u5b9a\u3002\u662f\u5426\u7ee7\u7eed\uff1f", L"\u8b66\u544a", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return FALSE;
    }
    bool killed = KillProcess(pid, false);
    if (killed && g_procConfig.autoRefreshKill) Sleep(200);
    return killed;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return FALSE; }
extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return FALSE; }

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax)
{
    if (!lpszPath || !lpszDisplayName) return FALSE;
    if (IsProcessRootPath(lpszPath)) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"\u8fdb\u7a0b"); return TRUE; }
    if (IsProcessFolderPath(lpszPath)) { std::wstring fn = GetProcessFolderName(lpszPath); StringCchCopyW(lpszDisplayName, cbDisplayNameMax, fn.c_str()); return TRUE; }
    DWORD pid = GetPidFromPath(lpszPath);
    if (pid > 0) { ProcessInfo info; if (GetProcessInfo(pid, info)) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, info.name.c_str()); return TRUE; } }
    StringCchCopyW(lpszDisplayName, cbDisplayNameMax, GetSubPath(lpszPath).c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax)
{
    if (!lpszPath || !lpszNewPath) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, PROCESS_VFS_PREFIX); return TRUE; }
    if (IsProcessRootPath(lpszPath)) return FALSE;
    if (IsProcessFolderPath(lpszPath)) { StringCchCopyW(lpszNewPath, cbNewPathMax, PROCESS_VFS_PREFIX); return TRUE; }
    if (IsProcessFilePath(lpszPath))
    {
        std::wstring parentPath = PROCESS_VFS_PREFIX + GetProcessFolderName(lpszPath) + L"/";
        StringCchCopyW(lpszNewPath, cbNewPathMax, parentPath.c_str());
        return TRUE;
    }
    StringCchCopyW(lpszNewPath, cbNewPathMax, PROCESS_VFS_PREFIX);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpszFiles || !lpMenuData) return FALSE;
    if (IsProcessRootPath(lpszFiles)) { lpMenuData->fAllowContextMenu = FALSE; return TRUE; }

    static VFSCONTEXTMENUITEMW items[32];
    int itemCount = 0;

    if (IsProcessFolderPath(lpszFiles))
    {
        std::wstring folderName = GetProcessFolderName(lpszFiles);

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u7ed3\u675f\u6240\u6709\u5b9e\u4f8b"; items[itemCount].lpszCommand = L"$killall"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u7ed3\u675f\u6240\u6709\u5b9e\u4f8b\u6811"; items[itemCount].lpszCommand = L"$killalltree"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
        items[itemCount].lpszLabel = L""; items[itemCount].lpszCommand = L""; itemCount++;

        std::vector<ProcessInfo> processes;
        if (EnumProcesses(processes))
        {
            for (const auto& info : processes)
            {
                if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && !info.exePath.empty())
                {
                    items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
                    items[itemCount].lpszLabel = L"\u6253\u5f00\u6587\u4ef6\u4f4d\u7f6e"; items[itemCount].lpszCommand = L"$openlocation"; itemCount++;
                    break;
                }
            }
        }

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5c5e\u6027"; items[itemCount].lpszCommand = L"$proc_properties"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u6253\u5f00\u4efb\u52a1\u7ba1\u7406\u5668"; items[itemCount].lpszCommand = L"$proc_taskmgr"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u590d\u5236\u8def\u5f84"; items[itemCount].lpszCommand = L"$copypath"; itemCount++;

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_BEGINSUBMENU;
        items[itemCount].lpszLabel = L"\u8bbe\u7f6e\u6240\u6709\u4f18\u5148\u7ea7"; items[itemCount].lpszCommand = L""; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5b9e\u65f6"; items[itemCount].lpszCommand = L"$allpriority_realtime"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u9ad8"; items[itemCount].lpszCommand = L"$allpriority_high"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u9ad8\u4e8e\u6807\u51c6"; items[itemCount].lpszCommand = L"$allpriority_abovenormal"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u6807\u51c6"; items[itemCount].lpszCommand = L"$allpriority_normal"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u4f4e\u4e8e\u6807\u51c6"; items[itemCount].lpszCommand = L"$allpriority_belownormal"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_ENDSUBMENU;
        items[itemCount].lpszLabel = L"\u4f4e"; items[itemCount].lpszCommand = L"$allpriority_low"; itemCount++;
    }
    else
    {
        DWORD pid = GetPidFromPath(lpszFiles);
        if (pid == 0) { lpMenuData->fAllowContextMenu = FALSE; return TRUE; }

        ProcessInfo info;
        bool found = GetProcessInfo(pid, info);

        if (found && info.isRunning && pid != GetCurrentProcessId())
        {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u7ed3\u675f\u8fdb\u7a0b"; items[itemCount].lpszCommand = L"$kill"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u7ed3\u675f\u8fdb\u7a0b\u6811"; items[itemCount].lpszCommand = L"$killtree"; itemCount++;
        }

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_SEPARATOR;
        items[itemCount].lpszLabel = L""; items[itemCount].lpszCommand = L""; itemCount++;

        if (found && !info.exePath.empty())
        {
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
            items[itemCount].lpszLabel = L"\u6253\u5f00\u6587\u4ef6\u4f4d\u7f6e"; items[itemCount].lpszCommand = L"$openlocation"; itemCount++;
        }

        if (found && info.isRunning)
        {
            DWORD currentPriority = 0;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) { currentPriority = GetPriorityClass(hProc); CloseHandle(hProc); }

            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_BEGINSUBMENU;
            items[itemCount].lpszLabel = L"\u8bbe\u7f6e\u4f18\u5148\u7ea7"; items[itemCount].lpszCommand = L""; itemCount++;

            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK | (currentPriority == REALTIME_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u5b9e\u65f6"; items[itemCount].lpszCommand = L"$priority_realtime"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK | (currentPriority == HIGH_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u9ad8"; items[itemCount].lpszCommand = L"$priority_high"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK | (currentPriority == ABOVE_NORMAL_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u9ad8\u4e8e\u6807\u51c6"; items[itemCount].lpszCommand = L"$priority_abovenormal"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK | (currentPriority == NORMAL_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u6807\u51c6"; items[itemCount].lpszCommand = L"$priority_normal"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_RADIOCHECK | (currentPriority == BELOW_NORMAL_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u4f4e\u4e8e\u6807\u51c6"; items[itemCount].lpszCommand = L"$priority_belownormal"; itemCount++;
            items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = VFSCMF_ENDSUBMENU | VFSCMF_RADIOCHECK | (currentPriority == IDLE_PRIORITY_CLASS ? VFSCMF_CHECKED : 0);
            items[itemCount].lpszLabel = L"\u4f4e"; items[itemCount].lpszCommand = L"$priority_low"; itemCount++;
        }

        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u5c5e\u6027"; items[itemCount].lpszCommand = L"$proc_properties"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u6253\u5f00\u4efb\u52a1\u7ba1\u7406\u5668"; items[itemCount].lpszCommand = L"$proc_taskmgr"; itemCount++;
        items[itemCount].cbSize = sizeof(VFSCONTEXTMENUITEMW); items[itemCount].dwFlags = 0;
        items[itemCount].lpszLabel = L"\u590d\u5236\u8def\u5f84"; items[itemCount].lpszCommand = L"$copypath"; itemCount++;
    }

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = TRUE;
    lpMenuData->lpCustomItems = items;
    lpMenuData->iNumCustomItems = itemCount;
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

static std::wstring GetExePathFromFolder(const std::wstring& folderName)
{
    std::vector<ProcessInfo> processes;
    if (EnumProcesses(processes))
    {
        for (const auto& info : processes)
        {
            if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && !info.exePath.empty())
                return info.exePath;
        }
    }
    return L"";
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData)
{
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    if (IsProcessFolderPath(lpVerbData->lpszPath))
    {
        std::wstring folderName = GetProcessFolderName(lpVerbData->lpszPath);

        if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0)
            return VFSCVRES_DEFAULT;

        if (_wcsicmp(lpVerbData->lpszVerb, L"killall") == 0)
        {
            if (g_procConfig.confirmKill)
            {
                std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f \"" + folderName + L"\" \u7684\u6240\u6709\u5b9e\u4f8b\u5417\uff1f";
                if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u6240\u6709\u5b9e\u4f8b", MB_YESNO | MB_ICONQUESTION) != IDYES) return VFSCVRES_FAIL;
            }
            std::vector<ProcessInfo> processes;
            if (EnumProcesses(processes))
            {
                bool anyKilled = false;
                for (const auto& info : processes)
                {
                    if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && info.pid != GetCurrentProcessId())
                    {
                        if (g_procConfig.warnCritical && info.isCritical)
                        {
                            if (MessageBoxW(lpVerbData->hwndParent, L"\u5176\u4e2d\u5305\u542b\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b\uff0c\u7ec8\u6b62\u53ef\u80fd\u5bfc\u81f4\u7cfb\u7edf\u4e0d\u7a33\u5b9a\u3002\u662f\u5426\u7ee7\u7eed\uff1f", L"\u8b66\u544a", MB_YESNO | MB_ICONWARNING) != IDYES) continue;
                        }
                        if (KillProcess(info.pid, true)) anyKilled = true;
                    }
                }
                if (anyKilled)
                {
                    if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                    return VFSCVRES_CHANGE;
                }
            }
            return VFSCVRES_FAIL;
        }

        if (_wcsicmp(lpVerbData->lpszVerb, L"killalltree") == 0)
        {
            if (g_procConfig.confirmTree)
            {
                std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f \"" + folderName + L"\" \u7684\u6240\u6709\u5b9e\u4f8b\u53ca\u5176\u5b50\u8fdb\u7a0b\u5417\uff1f";
                if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u6240\u6709\u5b9e\u4f8b\u6811", MB_YESNO | MB_ICONQUESTION) != IDYES) return VFSCVRES_FAIL;
            }
            std::vector<ProcessInfo> processes;
            if (EnumProcesses(processes))
            {
                bool anyKilled = false;
                for (const auto& info : processes)
                {
                    if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && info.pid != GetCurrentProcessId())
                    {
                        if (KillProcessTree(info.pid)) anyKilled = true;
                    }
                }
                if (anyKilled)
                {
                    if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                    return VFSCVRES_CHANGE;
                }
            }
            return VFSCVRES_FAIL;
        }

        if (_wcsicmp(lpVerbData->lpszVerb, L"openlocation") == 0)
        {
            std::wstring exePath = GetExePathFromFolder(folderName);
            if (!exePath.empty())
            {
                size_t lastSlash = exePath.find_last_of(L"\\/");
                if (lastSlash != std::wstring::npos)
                {
                    std::wstring dir = exePath.substr(0, lastSlash);
                    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_INVOKEIDLIST;
                    sei.lpVerb = L"open"; sei.lpFile = dir.c_str(); sei.nShow = SW_SHOWNORMAL;
                    ShellExecuteExW(&sei);
                }
            }
            return VFSCVRES_HANDLED;
        }

        if (_wcsicmp(lpVerbData->lpszVerb, L"proc_properties") == 0)
        {
            std::vector<ProcessInfo> processes;
            if (EnumProcesses(processes))
            {
                std::vector<ProcessInfo> instances;
                for (const auto& info : processes) { if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0) instances.push_back(info); }
                std::wstring result = L"\u8fdb\u7a0b\u540d\u79f0: " + folderName + L"\r\n\u5b9e\u4f8b\u6570\u91cf: " + std::to_wstring(instances.size()) + L"\r\n";
                SIZE_T totalMem = 0; DWORD totalThreads = 0; ULONGLONG totalRead = 0, totalWrite = 0; DWORD totalHandles = 0; std::wstring exePath;
                for (const auto& info : instances) { totalMem += info.workingSetSize; totalThreads += info.threadCount; totalRead += info.readTransferCount; totalWrite += info.writeTransferCount; totalHandles += info.handleCount; if (info.exePath.length() > exePath.length()) exePath = info.exePath; }
                result += L"\u603b\u5185\u5b58: " + FormatSize(totalMem) + L"\r\n\u603b\u7ebf\u7a0b\u6570: " + std::to_wstring(totalThreads) + L"\r\n\u603b\u53e5\u67c4\u6570: " + std::to_wstring(totalHandles) + L"\r\n\u603b\u78c1\u76d8\u8bfb\u53d6: " + FormatSizeUnsigned(totalRead) + L"\r\n\u603b\u78c1\u76d8\u5199\u5165: " + FormatSizeUnsigned(totalWrite) + L"\r\n\u53ef\u6267\u884c\u8def\u5f84: " + exePath + L"\r\n\r\n--- \u5404\u5b9e\u4f8b\u8be6\u60c5 ---\r\n\r\n";
                for (const auto& info : instances) result += L"PID: " + std::to_wstring(info.pid) + L"  \u5185\u5b58: " + FormatSize(info.workingSetSize) + L"  \u7ebf\u7a0b: " + std::to_wstring(info.threadCount) + L"  \u53e5\u67c4: " + std::to_wstring(info.handleCount) + L"\r\n";
                MessageBoxW(lpVerbData->hwndParent, result.c_str(), (folderName + L" - \u8fdb\u7a0b\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
            }
            return VFSCVRES_HANDLED;
        }

        if (_wcsicmp(lpVerbData->lpszVerb, L"proc_taskmgr") == 0)
        {
            SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = L"taskmgr.exe"; sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei);
            return VFSCVRES_HANDLED;
        }

        if (_wcsicmp(lpVerbData->lpszVerb, L"copypath") == 0)
        {
            std::wstring exePath = GetExePathFromFolder(folderName);
            if (!exePath.empty() && OpenClipboard(lpVerbData->hwndParent))
            {
                EmptyClipboard(); size_t size = (exePath.length() + 1) * sizeof(WCHAR);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hMem) { memcpy(GlobalLock(hMem), exePath.c_str(), size); GlobalUnlock(hMem); SetClipboardData(CF_UNICODETEXT, hMem); }
                CloseClipboard();
            }
            return VFSCVRES_HANDLED;
        }

        if (_wcsnicmp(lpVerbData->lpszVerb, L"allpriority_", 12) == 0)
        {
            DWORD priorityClass = NORMAL_PRIORITY_CLASS;
            LPCWSTR priorityName = lpVerbData->lpszVerb + 12;

            if (_wcsicmp(priorityName, L"realtime") == 0) priorityClass = REALTIME_PRIORITY_CLASS;
            else if (_wcsicmp(priorityName, L"high") == 0) priorityClass = HIGH_PRIORITY_CLASS;
            else if (_wcsicmp(priorityName, L"abovenormal") == 0) priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
            else if (_wcsicmp(priorityName, L"normal") == 0) priorityClass = NORMAL_PRIORITY_CLASS;
            else if (_wcsicmp(priorityName, L"belownormal") == 0) priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
            else if (_wcsicmp(priorityName, L"low") == 0) priorityClass = IDLE_PRIORITY_CLASS;

            std::vector<ProcessInfo> processes;
            bool anySet = false;
            if (EnumProcesses(processes))
            {
                for (const auto& info : processes)
                {
                    if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0)
                    {
                        if (SetProcessPriority(info.pid, priorityClass)) anySet = true;
                    }
                }
            }
            if (anySet)
            {
                if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                return VFSCVRES_CHANGE;
            }
            MessageBoxW(lpVerbData->hwndParent, L"\u8bbe\u7f6e\u4f18\u5148\u7ea7\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR);
            return VFSCVRES_FAIL;
        }

        return VFSCVRES_DEFAULT;
    }

    DWORD pid = GetPidFromPath(lpVerbData->lpszPath);
    if (pid == 0) return VFSCVRES_FAIL;

    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0)
    {
        ProcessInfo info;
        if (!GetProcessInfo(pid, info)) return VFSCVRES_FAIL;

        switch (g_procConfig.defaultAction)
        {
        case 1:
            if (pid == GetCurrentProcessId()) { MessageBoxW(lpVerbData->hwndParent, L"\u4e0d\u80fd\u7ed3\u675f\u5f53\u524d\u8fdb\u7a0b\uff01", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL; }
            if (g_procConfig.confirmKill) { std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f\u8fdb\u7a0b \"" + info.name + L"\" (PID: " + std::to_wstring(pid) + L") \u5417\uff1f"; if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u8fdb\u7a0b", MB_YESNO | MB_ICONQUESTION) != IDYES) return VFSCVRES_FAIL; }
            if (g_procConfig.warnCritical && IsCriticalProcess(pid)) { if (MessageBoxW(lpVerbData->hwndParent, L"\u8be5\u8fdb\u7a0b\u662f\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b\uff0c\u7ed3\u675f\u53ef\u80fd\u5bfc\u81f4\u7cfb\u7edf\u4e0d\u7a33\u5b9a\u3002\u662f\u5426\u7ee7\u7eed\uff1f", L"\u8b66\u544a", MB_YESNO | MB_ICONWARNING) != IDYES) return VFSCVRES_FAIL; }
            if (KillProcess(pid, true)) { if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath); return VFSCVRES_CHANGE; }
            MessageBoxW(lpVerbData->hwndParent, L"\u7ed3\u675f\u8fdb\u7a0b\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL;

        case 2:
            { SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = L"taskmgr.exe"; sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei); }
            return VFSCVRES_HANDLED;

        default:
            MessageBoxW(lpVerbData->hwndParent, FormatProcessInfo(info).c_str(), (info.name + L" - \u8fdb\u7a0b\u4fe1\u606f").c_str(), MB_OK | MB_ICONINFORMATION);
            return VFSCVRES_HANDLED;
        }
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"kill") == 0)
    {
        if (pid == GetCurrentProcessId()) { MessageBoxW(lpVerbData->hwndParent, L"\u4e0d\u80fd\u7ed3\u675f\u5f53\u524d\u8fdb\u7a0b\uff01", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL; }
        if (g_procConfig.confirmKill) { ProcessInfo info; GetProcessInfo(pid, info); std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f\u8fdb\u7a0b \"" + info.name + L"\" (PID: " + std::to_wstring(pid) + L") \u5417\uff1f"; if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u8fdb\u7a0b", MB_YESNO | MB_ICONQUESTION) != IDYES) return VFSCVRES_FAIL; }
        if (g_procConfig.warnCritical && IsCriticalProcess(pid)) { if (MessageBoxW(lpVerbData->hwndParent, L"\u8be5\u8fdb\u7a0b\u662f\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b\uff0c\u7ed3\u675f\u53ef\u80fd\u5bfc\u81f4\u7cfb\u7edf\u4e0d\u7a33\u5b9a\u3002\u662f\u5426\u7ee7\u7eed\uff1f", L"\u8b66\u544a", MB_YESNO | MB_ICONWARNING) != IDYES) return VFSCVRES_FAIL; }
        if (KillProcess(pid, true)) { if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath); return VFSCVRES_CHANGE; }
        MessageBoxW(lpVerbData->hwndParent, L"\u7ed3\u675f\u8fdb\u7a0b\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"killtree") == 0)
    {
        if (pid == GetCurrentProcessId()) { MessageBoxW(lpVerbData->hwndParent, L"\u4e0d\u80fd\u7ed3\u675f\u5f53\u524d\u8fdb\u7a0b\uff01", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL; }
        if (g_procConfig.confirmTree) { ProcessInfo info; GetProcessInfo(pid, info); std::wstring msg = L"\u786e\u5b9a\u8981\u7ed3\u675f\u8fdb\u7a0b\u6811 \"" + info.name + L"\" (PID: " + std::to_wstring(pid) + L") \u5417\uff1f\n\u8fd9\u5c06\u7ed3\u675f\u8be5\u8fdb\u7a0b\u53ca\u5176\u6240\u6709\u5b50\u8fdb\u7a0b\u3002"; if (MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u786e\u8ba4\u7ed3\u675f\u8fdb\u7a0b\u6811", MB_YESNO | MB_ICONQUESTION) != IDYES) return VFSCVRES_FAIL; }
        if (KillProcessTree(pid)) { if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath); return VFSCVRES_CHANGE; }
        MessageBoxW(lpVerbData->hwndParent, L"\u7ed3\u675f\u8fdb\u7a0b\u6811\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR); return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"openlocation") == 0)
    {
        ProcessInfo info;
        if (GetProcessInfo(pid, info) && !info.exePath.empty())
        {
            std::wstring dir = info.exePath;
            size_t lastSlash = dir.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) dir = dir.substr(0, lastSlash);
            SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_INVOKEIDLIST;
            sei.lpVerb = L"open"; sei.lpFile = dir.c_str(); sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"proc_properties") == 0)
    {
        ProcessInfo info;
        if (GetProcessInfo(pid, info)) MessageBoxW(lpVerbData->hwndParent, FormatProcessInfo(info).c_str(), (info.name + L" - \u8fdb\u7a0b\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"proc_taskmgr") == 0)
    {
        SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = L"taskmgr.exe"; sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copypath") == 0)
    {
        ProcessInfo info;
        if (GetProcessInfo(pid, info) && !info.exePath.empty() && OpenClipboard(lpVerbData->hwndParent))
        {
            EmptyClipboard(); size_t size = (info.exePath.length() + 1) * sizeof(WCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) { memcpy(GlobalLock(hMem), info.exePath.c_str(), size); GlobalUnlock(hMem); SetClipboardData(CF_UNICODETEXT, hMem); }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsnicmp(lpVerbData->lpszVerb, L"priority_", 9) == 0)
    {
        DWORD priorityClass = NORMAL_PRIORITY_CLASS;
        LPCWSTR priorityName = lpVerbData->lpszVerb + 9;

        if (_wcsicmp(priorityName, L"realtime") == 0) priorityClass = REALTIME_PRIORITY_CLASS;
        else if (_wcsicmp(priorityName, L"high") == 0) priorityClass = HIGH_PRIORITY_CLASS;
        else if (_wcsicmp(priorityName, L"abovenormal") == 0) priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
        else if (_wcsicmp(priorityName, L"normal") == 0) priorityClass = NORMAL_PRIORITY_CLASS;
        else if (_wcsicmp(priorityName, L"belownormal") == 0) priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
        else if (_wcsicmp(priorityName, L"low") == 0) priorityClass = IDLE_PRIORITY_CLASS;

        if (SetProcessPriority(pid, priorityClass))
        {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0) StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u8bbe\u7f6e\u4f18\u5148\u7ea7\u5931\u8d25\uff0c\u8bf7\u786e\u4fdd\u6709\u8db3\u591f\u6743\u9650\u3002", L"\u8fdb\u7a0b\u63a7\u5236\u9519\u8bef", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles)
{
    if (!lpszFiles) return NULL;

    if (IsProcessFolderPath(lpszFiles))
    {
        std::wstring folderName = GetProcessFolderName(lpszFiles);
        std::vector<ProcessInfo> processes;
        if (EnumProcesses(processes))
        {
            std::vector<ProcessInfo> instances;
            for (const auto& info : processes) { if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0) instances.push_back(info); }
            std::wstring result = L"\u8fdb\u7a0b\u540d\u79f0: " + folderName + L"\r\n\u5b9e\u4f8b\u6570\u91cf: " + std::to_wstring(instances.size()) + L"\r\n";
            SIZE_T totalMem = 0; DWORD totalThreads = 0; ULONGLONG totalRead = 0, totalWrite = 0; DWORD totalHandles = 0; std::wstring exePath;
            for (const auto& info : instances) { totalMem += info.workingSetSize; totalThreads += info.threadCount; totalRead += info.readTransferCount; totalWrite += info.writeTransferCount; totalHandles += info.handleCount; if (info.exePath.length() > exePath.length()) exePath = info.exePath; }
            result += L"\u603b\u5185\u5b58: " + FormatSize(totalMem) + L"\r\n\u603b\u7ebf\u7a0b\u6570: " + std::to_wstring(totalThreads) + L"\r\n\u603b\u53e5\u67c4\u6570: " + std::to_wstring(totalHandles) + L"\r\n\u603b\u78c1\u76d8\u8bfb\u53d6: " + FormatSizeUnsigned(totalRead) + L"\r\n\u603b\u78c1\u76d8\u5199\u5165: " + FormatSizeUnsigned(totalWrite) + L"\r\n\u53ef\u6267\u884c\u8def\u5f84: " + exePath + L"\r\n\r\n--- \u5404\u5b9e\u4f8b\u8be6\u60c5 ---\r\n\r\n";
            for (const auto& info : instances) result += L"PID: " + std::to_wstring(info.pid) + L"  \u5185\u5b58: " + FormatSize(info.workingSetSize) + L"  \u7ebf\u7a0b: " + std::to_wstring(info.threadCount) + L"  \u53e5\u67c4: " + std::to_wstring(info.handleCount) + L"\r\n";
            MessageBoxW(hwndParent, result.c_str(), (folderName + L" - \u8fdb\u7a0b\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
        }
        return NULL;
    }

    DWORD pid = GetPidFromPath(lpszFiles);
    if (pid == 0) return NULL;
    ProcessInfo info;
    if (GetProcessInfo(pid, info)) MessageBoxW(hwndParent, FormatProcessInfo(info).c_str(), (info.name + L" - \u8fdb\u7a0b\u5c5e\u6027").c_str(), MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3)
{
    switch (propId)
    {
    case VFSPROP_FUNCAVAILABILITY: { unsigned __int64* pAvail = (unsigned __int64*)lpPropData; *pAvail = VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT; return TRUE; }
    case VFSPROP_GETVALIDACTIONS: return TRUE;
    case VFSPROP_GETFOLDERICON: return FALSE;
    case VFSPROP_SHOWTHUMBNAILS: case VFSPROP_USEFULLRENAME: case VFSPROP_CANSHOWSUBFOLDERS: case VFSPROP_SUPPORTPATHCOMPLETION: *reinterpret_cast<LPBOOL>(lpPropData) = TRUE; return TRUE;
    case VFSPROP_SHOWFILEINFO: *reinterpret_cast<LPBOOL>(lpPropData) = FALSE; return TRUE;
    case VFSPROP_ISEXTRACTABLE: *reinterpret_cast<LPBOOL>(lpPropData) = FALSE; return TRUE;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize)
{
    if (hFile) { ProcessFileContext* ctx = (ProcessFileContext*)hFile; if (piFileSize) *piFileSize = ctx->buffer.size(); return TRUE; }
    else if (lpszPath && IsProcessFilePath(lpszPath))
    {
        DWORD pid = GetPidFromPath(lpszPath);
        if (pid > 0) { ProcessInfo info; if (GetProcessInfo(pid, info)) { std::wstring pi = FormatProcessInfo(info); if (piFileSize) *piFileSize = (pi.length() + 1) * sizeof(WCHAR); return TRUE; } }
    }
    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) { return GetLastError(); }

static std::wstring GetConfigFilePath()
{
    WCHAR appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::wstring dir = std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\ProcessVFS";
        CreateDirectoryW((std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data").c_str(), NULL);
        CreateDirectoryW((std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin").c_str(), NULL);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\ProcessVFS.ini";
    }
    WCHAR modulePath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);
    std::wstring dir(modulePath);
    size_t lastSlash = dir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) dir = dir.substr(0, lastSlash);
    return dir + L"\\ProcessVFS.ini";
}

static std::wstring GetDefaultLogPath()
{
    WCHAR appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::wstring dir = std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\ProcessVFS";
        CreateDirectoryW((std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data").c_str(), NULL);
        CreateDirectoryW((std::wstring(appData) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin").c_str(), NULL);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\ProcessVFS.log";
    }
    WCHAR modulePath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);
    std::wstring dir(modulePath);
    size_t lastSlash = dir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) dir = dir.substr(0, lastSlash);
    return dir + L"\\ProcessVFS.log";
}

static void LoadProcConfig()
{
    std::wstring iniPath = GetConfigFilePath();
    LPCWSTR section = L"ProcessVFS";

    g_procConfig.defaultAction = GetPrivateProfileIntW(section, L"DefaultAction", g_procConfig.defaultAction, iniPath.c_str());
    g_procConfig.showSystemProcesses = GetPrivateProfileIntW(section, L"ShowSystemProcesses", g_procConfig.showSystemProcesses ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colPid = GetPrivateProfileIntW(section, L"ColPid", g_procConfig.colPid ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colStatus = GetPrivateProfileIntW(section, L"ColStatus", g_procConfig.colStatus ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colCpu = GetPrivateProfileIntW(section, L"ColCpu", g_procConfig.colCpu ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colMemory = GetPrivateProfileIntW(section, L"ColMemory", g_procConfig.colMemory ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colThreads = GetPrivateProfileIntW(section, L"ColThreads", g_procConfig.colThreads ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colPath = GetPrivateProfileIntW(section, L"ColPath", g_procConfig.colPath ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colDisk = GetPrivateProfileIntW(section, L"ColDisk", g_procConfig.colDisk ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colHandles = GetPrivateProfileIntW(section, L"ColHandles", g_procConfig.colHandles ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colUser = GetPrivateProfileIntW(section, L"ColUser", g_procConfig.colUser ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colPriority = GetPrivateProfileIntW(section, L"ColPriority", g_procConfig.colPriority ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colNetwork = GetPrivateProfileIntW(section, L"ColNetwork", g_procConfig.colNetwork ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colParent = GetPrivateProfileIntW(section, L"ColParent", g_procConfig.colParent ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colCreateTime = GetPrivateProfileIntW(section, L"ColCreateTime", g_procConfig.colCreateTime ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colCmdLine = GetPrivateProfileIntW(section, L"ColCmdLine", g_procConfig.colCmdLine ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colSessionId = GetPrivateProfileIntW(section, L"ColSessionId", g_procConfig.colSessionId ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colArch = GetPrivateProfileIntW(section, L"ColArch", g_procConfig.colArch ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.colDescription = GetPrivateProfileIntW(section, L"ColDescription", g_procConfig.colDescription ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.killMethod = GetPrivateProfileIntW(section, L"KillMethod", g_procConfig.killMethod, iniPath.c_str());
    g_procConfig.confirmKill = GetPrivateProfileIntW(section, L"ConfirmKill", g_procConfig.confirmKill ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.confirmTree = GetPrivateProfileIntW(section, L"ConfirmTree", g_procConfig.confirmTree ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.autoRefreshKill = GetPrivateProfileIntW(section, L"AutoRefreshKill", g_procConfig.autoRefreshKill ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.warnCritical = GetPrivateProfileIntW(section, L"WarnCritical", g_procConfig.warnCritical ? 1 : 0, iniPath.c_str()) != 0;
    g_procConfig.cacheTimeout = GetPrivateProfileIntW(section, L"CacheTimeout", g_procConfig.cacheTimeout, iniPath.c_str());

    WCHAR pathBuf[MAX_PATH] = {};
    GetPrivateProfileStringW(section, L"LogPath", L"", pathBuf, MAX_PATH, iniPath.c_str());
    g_procConfig.logPath = pathBuf[0] ? pathBuf : GetDefaultLogPath();
    GetPrivateProfileStringW(section, L"ConfigPath", L"", pathBuf, MAX_PATH, iniPath.c_str());
    g_procConfig.configPath = pathBuf[0] ? pathBuf : iniPath;
}

static void SaveProcConfig()
{
    std::wstring iniPath = GetConfigFilePath();
    LPCWSTR section = L"ProcessVFS";

    WCHAR buf[32];

    swprintf_s(buf, L"%d", g_procConfig.defaultAction); WritePrivateProfileStringW(section, L"DefaultAction", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.showSystemProcesses ? 1 : 0); WritePrivateProfileStringW(section, L"ShowSystemProcesses", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colPid ? 1 : 0); WritePrivateProfileStringW(section, L"ColPid", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colStatus ? 1 : 0); WritePrivateProfileStringW(section, L"ColStatus", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colCpu ? 1 : 0); WritePrivateProfileStringW(section, L"ColCpu", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colMemory ? 1 : 0); WritePrivateProfileStringW(section, L"ColMemory", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colThreads ? 1 : 0); WritePrivateProfileStringW(section, L"ColThreads", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colPath ? 1 : 0); WritePrivateProfileStringW(section, L"ColPath", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colDisk ? 1 : 0); WritePrivateProfileStringW(section, L"ColDisk", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colHandles ? 1 : 0); WritePrivateProfileStringW(section, L"ColHandles", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colUser ? 1 : 0); WritePrivateProfileStringW(section, L"ColUser", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colPriority ? 1 : 0); WritePrivateProfileStringW(section, L"ColPriority", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colNetwork ? 1 : 0); WritePrivateProfileStringW(section, L"ColNetwork", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colParent ? 1 : 0); WritePrivateProfileStringW(section, L"ColParent", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colCreateTime ? 1 : 0); WritePrivateProfileStringW(section, L"ColCreateTime", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colCmdLine ? 1 : 0); WritePrivateProfileStringW(section, L"ColCmdLine", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colSessionId ? 1 : 0); WritePrivateProfileStringW(section, L"ColSessionId", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colArch ? 1 : 0); WritePrivateProfileStringW(section, L"ColArch", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.colDescription ? 1 : 0); WritePrivateProfileStringW(section, L"ColDescription", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.killMethod); WritePrivateProfileStringW(section, L"KillMethod", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.confirmKill ? 1 : 0); WritePrivateProfileStringW(section, L"ConfirmKill", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.confirmTree ? 1 : 0); WritePrivateProfileStringW(section, L"ConfirmTree", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.autoRefreshKill ? 1 : 0); WritePrivateProfileStringW(section, L"AutoRefreshKill", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.warnCritical ? 1 : 0); WritePrivateProfileStringW(section, L"WarnCritical", buf, iniPath.c_str());
    swprintf_s(buf, L"%d", g_procConfig.cacheTimeout); WritePrivateProfileStringW(section, L"CacheTimeout", buf, iniPath.c_str());
    WritePrivateProfileStringW(section, L"LogPath", g_procConfig.logPath.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(section, L"ConfigPath", g_procConfig.configPath.c_str(), iniPath.c_str());
}

static const int PAGE_GENERAL = 0;
static const int PAGE_DISPLAY = 1;
static const int PAGE_ACTIONS = 2;
static const int PAGE_ADVANCED = 3;

static const int pageGeneralCtrls[] = { IDC_LBL_DEFAULT_ACTION, IDC_DEFAULT_ACTION, IDC_SHOW_SYSTEM_PROCESSES, 0 };
static const int pageDisplayCtrls[] = { IDC_LBL_COLUMNS, IDC_COL_PID, IDC_COL_STATUS, IDC_COL_CPU, IDC_COL_MEMORY, IDC_COL_THREADS, IDC_COL_PATH, IDC_COL_CREATETIME, IDC_COL_SESSIONID, IDC_COL_DESCRIPTION, IDC_COL_DISK, IDC_COL_HANDLES, IDC_COL_USER, IDC_COL_PRIORITY, IDC_COL_NETWORK, IDC_COL_PARENT, IDC_COL_CMDLINE, IDC_COL_ARCH, 0 };
static const int pageActionsCtrls[] = { IDC_LBL_KILL_METHOD, IDC_KILL_METHOD, IDC_CHK_CONFIRM_KILL, IDC_CHK_CONFIRM_TREE, IDC_CHK_AUTO_REFRESH_KILL, IDC_CHK_WARN_CRITICAL, 0 };
static const int pageAdvancedCtrls[] = { IDC_LBL_CACHE_TIMEOUT, IDC_CACHE_TIMEOUT, IDC_LBL_LOG_PATH, IDC_LOG_PATH, IDC_BROWSE_LOG_PATH, IDC_LBL_CONFIG_PATH, IDC_CONFIG_PATH, IDC_BROWSE_CONFIG_PATH, 0 };

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
    SetWindowTextW(hDlg, L"\u8fdb\u7a0b VFS \u914d\u7f6e");
    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList) { SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24); SendMessageW(hList, LB_RESETCONTENT, 0, 0); for (int i = 0; i < 4; i++) SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L""); SendMessageW(hList, LB_SETCURSEL, 0, 0); }

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a"); SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88"); SetDlgItemTextW(hDlg, IDC_APPLY, L"\u5e94\u7528");
    SetDlgItemTextW(hDlg, IDC_OPEN_TASKMGR, L"\u6253\u5f00\u4efb\u52a1\u7ba1\u7406\u5668"); SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\u6062\u590d\u9ed8\u8ba4");

    SetDlgItemTextW(hDlg, IDC_LBL_DEFAULT_ACTION, L"\u9ed8\u8ba4\u64cd\u4f5c:");
    SetDlgItemTextW(hDlg, IDC_SHOW_SYSTEM_PROCESSES, L"\u663e\u793a\u7cfb\u7edf\u8fdb\u7a0b");

    SetDlgItemTextW(hDlg, IDC_LBL_COLUMNS, L"\u663e\u793a\u5217:");
    SetDlgItemTextW(hDlg, IDC_COL_PID, L"PID");
    SetDlgItemTextW(hDlg, IDC_COL_STATUS, L"\u72b6\u6001");
    SetDlgItemTextW(hDlg, IDC_COL_CPU, L"CPU");
    SetDlgItemTextW(hDlg, IDC_COL_MEMORY, L"\u5185\u5b58");
    SetDlgItemTextW(hDlg, IDC_COL_THREADS, L"\u7ebf\u7a0b\u6570");
    SetDlgItemTextW(hDlg, IDC_COL_PATH, L"\u53ef\u6267\u884c\u8def\u5f84");
    SetDlgItemTextW(hDlg, IDC_COL_DISK, L"\u78c1\u76d8I/O");
    SetDlgItemTextW(hDlg, IDC_COL_HANDLES, L"\u53e5\u67c4\u6570");
    SetDlgItemTextW(hDlg, IDC_COL_USER, L"\u7528\u6237\u540d");
    SetDlgItemTextW(hDlg, IDC_COL_PRIORITY, L"\u4f18\u5148\u7ea7");
    SetDlgItemTextW(hDlg, IDC_COL_NETWORK, L"\u7f51\u7edc");
    SetDlgItemTextW(hDlg, IDC_COL_PARENT, L"\u7236\u8fdb\u7a0b");
    SetDlgItemTextW(hDlg, IDC_COL_CREATETIME, L"\u521b\u5efa\u65f6\u95f4");
    SetDlgItemTextW(hDlg, IDC_COL_CMDLINE, L"\u547d\u4ee4\u884c");
    SetDlgItemTextW(hDlg, IDC_COL_SESSIONID, L"\u4f1a\u8bddID");
    SetDlgItemTextW(hDlg, IDC_COL_ARCH, L"\u67b6\u6784");
    SetDlgItemTextW(hDlg, IDC_COL_DESCRIPTION, L"\u63cf\u8ff0");

    SetDlgItemTextW(hDlg, IDC_LBL_KILL_METHOD, L"\u7ed3\u675f\u65b9\u5f0f:");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_KILL, L"\u7ed3\u675f\u8fdb\u7a0b\u524d\u786e\u8ba4");
    SetDlgItemTextW(hDlg, IDC_CHK_CONFIRM_TREE, L"\u7ed3\u675f\u8fdb\u7a0b\u6811\u524d\u786e\u8ba4");
    SetDlgItemTextW(hDlg, IDC_CHK_AUTO_REFRESH_KILL, L"\u7ed3\u675f\u540e\u81ea\u52a8\u5237\u65b0");
    SetDlgItemTextW(hDlg, IDC_CHK_WARN_CRITICAL, L"\u8b66\u544a\u7cfb\u7edf\u5173\u952e\u8fdb\u7a0b");

    SetDlgItemTextW(hDlg, IDC_LBL_CACHE_TIMEOUT, L"\u7f13\u5b58\u8d85\u65f6(\u79d2):");
    SetDlgItemTextW(hDlg, IDC_LBL_LOG_PATH, L"\u65e5\u5fd7\u6587\u4ef6:");
    SetDlgItemTextW(hDlg, IDC_BROWSE_LOG_PATH, L"...");
    SetDlgItemTextW(hDlg, IDC_LBL_CONFIG_PATH, L"\u914d\u7f6e\u6587\u4ef6:");
    SetDlgItemTextW(hDlg, IDC_BROWSE_CONFIG_PATH, L"...");
}

static void InitDialogControls(HWND hDlg)
{
    SetChineseText(hDlg);
    WCHAR buf[32];
    HWND hCombo = GetDlgItem(hDlg, IDC_DEFAULT_ACTION);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u67e5\u770b\u5c5e\u6027"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u7ed3\u675f\u8fdb\u7a0b"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u6253\u5f00\u4efb\u52a1\u7ba1\u7406\u5668"); SendMessageW(hCombo, CB_SETCURSEL, g_procConfig.defaultAction, 0);
    CheckDlgButton(hDlg, IDC_SHOW_SYSTEM_PROCESSES, g_procConfig.showSystemProcesses ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PID, g_procConfig.colPid ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_STATUS, g_procConfig.colStatus ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_CPU, g_procConfig.colCpu ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_MEMORY, g_procConfig.colMemory ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_THREADS, g_procConfig.colThreads ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PATH, g_procConfig.colPath ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DISK, g_procConfig.colDisk ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_HANDLES, g_procConfig.colHandles ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_USER, g_procConfig.colUser ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PRIORITY, g_procConfig.colPriority ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_NETWORK, g_procConfig.colNetwork ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_PARENT, g_procConfig.colParent ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_CREATETIME, g_procConfig.colCreateTime ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_CMDLINE, g_procConfig.colCmdLine ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_SESSIONID, g_procConfig.colSessionId ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_ARCH, g_procConfig.colArch ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_COL_DESCRIPTION, g_procConfig.colDescription ? BST_CHECKED : BST_UNCHECKED);
    hCombo = GetDlgItem(hDlg, IDC_KILL_METHOD);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u6e29\u548c\u7ed3\u675f"); SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u5f3a\u5236\u7ed3\u675f"); SendMessageW(hCombo, CB_SETCURSEL, g_procConfig.killMethod, 0);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_KILL, g_procConfig.confirmKill ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_CONFIRM_TREE, g_procConfig.confirmTree ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_AUTO_REFRESH_KILL, g_procConfig.autoRefreshKill ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_WARN_CRITICAL, g_procConfig.warnCritical ? BST_CHECKED : BST_UNCHECKED);
    swprintf_s(buf, L"%d", g_procConfig.cacheTimeout); SetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf);
    SetDlgItemTextW(hDlg, IDC_LOG_PATH, g_procConfig.logPath.c_str());
    SetDlgItemTextW(hDlg, IDC_CONFIG_PATH, g_procConfig.configPath.c_str());
    ShowNavPage(hDlg, PAGE_GENERAL);
}

static bool SaveDialogControls(HWND hDlg)
{
    HWND hCombo = GetDlgItem(hDlg, IDC_DEFAULT_ACTION); g_procConfig.defaultAction = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    g_procConfig.showSystemProcesses = (IsDlgButtonChecked(hDlg, IDC_SHOW_SYSTEM_PROCESSES) == BST_CHECKED);
    g_procConfig.colPid = (IsDlgButtonChecked(hDlg, IDC_COL_PID) == BST_CHECKED);
    g_procConfig.colStatus = (IsDlgButtonChecked(hDlg, IDC_COL_STATUS) == BST_CHECKED);
    g_procConfig.colCpu = (IsDlgButtonChecked(hDlg, IDC_COL_CPU) == BST_CHECKED);
    g_procConfig.colMemory = (IsDlgButtonChecked(hDlg, IDC_COL_MEMORY) == BST_CHECKED);
    g_procConfig.colThreads = (IsDlgButtonChecked(hDlg, IDC_COL_THREADS) == BST_CHECKED);
    g_procConfig.colPath = (IsDlgButtonChecked(hDlg, IDC_COL_PATH) == BST_CHECKED);
    g_procConfig.colDisk = (IsDlgButtonChecked(hDlg, IDC_COL_DISK) == BST_CHECKED);
    g_procConfig.colHandles = (IsDlgButtonChecked(hDlg, IDC_COL_HANDLES) == BST_CHECKED);
    g_procConfig.colUser = (IsDlgButtonChecked(hDlg, IDC_COL_USER) == BST_CHECKED);
    g_procConfig.colPriority = (IsDlgButtonChecked(hDlg, IDC_COL_PRIORITY) == BST_CHECKED);
    g_procConfig.colNetwork = (IsDlgButtonChecked(hDlg, IDC_COL_NETWORK) == BST_CHECKED);
    g_procConfig.colParent = (IsDlgButtonChecked(hDlg, IDC_COL_PARENT) == BST_CHECKED);
    g_procConfig.colCreateTime = (IsDlgButtonChecked(hDlg, IDC_COL_CREATETIME) == BST_CHECKED);
    g_procConfig.colCmdLine = (IsDlgButtonChecked(hDlg, IDC_COL_CMDLINE) == BST_CHECKED);
    g_procConfig.colSessionId = (IsDlgButtonChecked(hDlg, IDC_COL_SESSIONID) == BST_CHECKED);
    g_procConfig.colArch = (IsDlgButtonChecked(hDlg, IDC_COL_ARCH) == BST_CHECKED);
    g_procConfig.colDescription = (IsDlgButtonChecked(hDlg, IDC_COL_DESCRIPTION) == BST_CHECKED);
    hCombo = GetDlgItem(hDlg, IDC_KILL_METHOD); g_procConfig.killMethod = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    g_procConfig.confirmKill = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_KILL) == BST_CHECKED);
    g_procConfig.confirmTree = (IsDlgButtonChecked(hDlg, IDC_CHK_CONFIRM_TREE) == BST_CHECKED);
    g_procConfig.autoRefreshKill = (IsDlgButtonChecked(hDlg, IDC_CHK_AUTO_REFRESH_KILL) == BST_CHECKED);
    g_procConfig.warnCritical = (IsDlgButtonChecked(hDlg, IDC_CHK_WARN_CRITICAL) == BST_CHECKED);
    WCHAR buf[32]; GetDlgItemTextW(hDlg, IDC_CACHE_TIMEOUT, buf, 32); g_procConfig.cacheTimeout = _wtoi(buf); if (g_procConfig.cacheTimeout < 0) g_procConfig.cacheTimeout = 0;
    WCHAR pathBuf[MAX_PATH] = {};
    GetDlgItemTextW(hDlg, IDC_LOG_PATH, pathBuf, MAX_PATH); g_procConfig.logPath = pathBuf;
    GetDlgItemTextW(hDlg, IDC_CONFIG_PATH, pathBuf, MAX_PATH); g_procConfig.configPath = pathBuf;
    SaveProcConfig();
    return true;
}

static void ResetToDefaults(HWND hDlg) { g_procConfig = ProcessConfig(); InitDialogControls(hDlg); MessageBoxW(hDlg, L"\u5df2\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e\u3002", L"\u63d0\u793a", MB_OK | MB_ICONINFORMATION); }
static void OpenTaskManager(HWND hDlg) { SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = L"taskmgr.exe"; sei.nShow = SW_SHOWNORMAL; ShellExecuteExW(&sei); }

static void BrowseForFolder(HWND hDlg, int editId)
{
    WCHAR szDir[MAX_PATH] = L"";
    BROWSEINFOW bi = {};
    bi.hwndOwner = hDlg;
    bi.lpszTitle = L"\u9009\u62e9\u76ee\u5f55";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        if (SHGetPathFromIDListW(pidl, szDir))
        {
            std::wstring currentText;
            WCHAR curBuf[MAX_PATH] = {};
            GetDlgItemTextW(hDlg, editId, curBuf, MAX_PATH);
            currentText = curBuf;
            std::wstring fileName;
            if (editId == IDC_LOG_PATH) fileName = L"ProcessVFS.log";
            else if (editId == IDC_CONFIG_PATH) fileName = L"ProcessVFS.ini";
            if (!fileName.empty()) SetDlgItemTextW(hDlg, editId, (std::wstring(szDir) + L"\\" + fileName).c_str());
            else SetDlgItemTextW(hDlg, editId, szDir);
        }
        CoTaskMemFree(pidl);
    }
}

INT_PTR CALLBACK ProcessConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG: LoadProcConfig(); InitDialogControls(hDlg); return (INT_PTR)TRUE;
    case WM_MEASUREITEM: { LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam; if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST) lpmis->itemHeight = 24; } return (INT_PTR)TRUE;
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST)
        {
            static const WCHAR* navLabels[] = { L"\u5e38\u89c4\u8bbe\u7f6e", L"\u663e\u793a\u8bbe\u7f6e", L"\u8fdb\u7a0b\u64cd\u4f5c", L"\u9ad8\u7ea7\u8bbe\u7f6e" };
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
        case IDC_OPEN_TASKMGR: OpenTaskManager(hDlg); return (INT_PTR)TRUE;
        case IDC_BROWSE_LOG_PATH: BrowseForFolder(hDlg, IDC_LOG_PATH); return (INT_PTR)TRUE;
        case IDC_BROWSE_CONFIG_PATH: BrowseForFolder(hDlg, IDC_CONFIG_PATH); return (INT_PTR)TRUE;
        }
        break;
    case WM_CLOSE: EndDialog(hDlg, IDCANCEL); return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData)
{
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_PROCESS_CONFIG), hWndParent, ProcessConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent)
{
    MessageBoxW(hWndParent,
        L"\u8fdb\u7a0b VFS \u63d2\u4ef6 v1.2.0\n\n"
        L"(c) 2026\n\n"
        L"Windows \u8fdb\u7a0b\u865a\u62df\u6587\u4ef6\u7cfb\u7edf\n\n"
        L"\u529f\u80fd\u7279\u6027\uff1a\n"
        L"- \u6309\u8fdb\u7a0b\u540d\u5206\u7ec4\u6d4f\u89c8\n"
        L"- \u67e5\u770b\u8fdb\u7a0b\u8be6\u7ec6\u4fe1\u606f\n"
        L"- \u7ed3\u675f\u8fdb\u7a0b/\u8fdb\u7a0b\u6811/\u6240\u6709\u5b9e\u4f8b\n"
        L"- \u78c1\u76d8I/O\u3001\u53e5\u67c4\u3001\u7528\u6237\u540d\u3001\u4f18\u5148\u7ea7\n"
        L"- \u7f51\u7edc\u8fde\u63a5\u68c0\u6d4b(TCP/UDP)\n"
        L"- \u7236\u8fdb\u7a0b\u663e\u793a\n"
        L"- \u521b\u5efa\u65f6\u95f4\u3001\u547d\u4ee4\u884c\u3001\u4f1a\u8bddID\n"
        L"- \u67b6\u6784\u68c0\u6d4b(32/64\u4f4d)\u3001\u6587\u4ef6\u63cf\u8ff0\n"
        L"- \u8fdb\u7a0b\u4f18\u5148\u7ea7\u8bbe\u7f6e\n"
        L"- \u81ea\u5b9a\u4e49\u5217\u663e\u793a\n"
        L"- \u53ef\u914d\u7f6e\u7684\u64cd\u4f5c\u8bbe\u7f6e",
        L"\u5173\u4e8e \u8fdb\u7a0b VFS", MB_OK | MB_ICONINFORMATION);
    return NULL;
}

static std::wstring GetExePathForIcon(LPCWSTR lpszFile)
{
    if (!lpszFile) return L"";

    if (IsProcessRootPath(lpszFile)) return L"";

    if (IsProcessFilePath(lpszFile))
    {
        DWORD pid = GetPidFromPath(lpszFile);
        if (pid != 0)
        {
            std::wstring exePath = GetProcessExePath(pid);
            if (!exePath.empty()) return exePath;
        }
        return L"";
    }

    if (IsProcessFolderPath(lpszFile))
    {
        std::wstring folderName = GetProcessFolderName(lpszFile);
        if (folderName.empty()) return L"";

        std::vector<ProcessInfo> processes;
        if (GetCachedProcesses(processes))
        {
            for (const auto& info : processes)
            {
                if (_wcsicmp(info.name.c_str(), folderName.c_str()) == 0 && !info.exePath.empty())
                    return info.exePath;
            }
        }

        std::wstring searchName = folderName;
        if (searchName.find(L'.') == std::wstring::npos)
            searchName += L".exe";

        std::wstring found = FindExeInSystemPaths(searchName);
        if (!found.empty()) return found;

        return L"";
    }

    return L"";
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileIconW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFile, LPINT lpiSysIconIndex, HICON* phLargeIcon, HICON* phSmallIcon,
    LPBOOL lpfDestroyIcons, LPWSTR lspzCacheName, int cchCacheNameMax, LPINT lpiCacheIndex)
{
    if (!lpszFile) return FALSE;

    if (IsProcessRootPath(lpszFile)) return FALSE;

    std::wstring exePath = GetExePathForIcon(lpszFile);

    if (lpiSysIconIndex)
        *lpiSysIconIndex = -1;

    if (lpfDestroyIcons)
        *lpfDestroyIcons = TRUE;

    if (!exePath.empty())
    {
        HICON hLarge = NULL, hSmall = NULL;
        UINT extracted = ExtractIconExW(exePath.c_str(), 0, &hLarge, &hSmall, 1);

        if (extracted > 0 && (hLarge || hSmall))
        {
            if (phLargeIcon) *phLargeIcon = hLarge; else if (hLarge) DestroyIcon(hLarge);
            if (phSmallIcon) *phSmallIcon = hSmall; else if (hSmall) DestroyIcon(hSmall);

            if (lspzCacheName && cchCacheNameMax > 0)
            {
                DWORD pid = GetPidFromPath(lpszFile);
                if (pid != 0)
                    StringCchPrintfW(lspzCacheName, cchCacheNameMax, L"ProcessVFS_%u", pid);
                else
                {
                    std::wstring folderName = GetProcessFolderName(lpszFile);
                    StringCchPrintfW(lspzCacheName, cchCacheNameMax, L"ProcessVFS_%s", folderName.c_str());
                }
            }
            if (lpiCacheIndex) *lpiCacheIndex = 0;

            return TRUE;
        }
        if (hLarge) DestroyIcon(hLarge);
        if (hSmall) DestroyIcon(hSmall);
    }

    SHFILEINFOW sfi = {};
    if (!exePath.empty() && SHGetFileInfoW(exePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON))
    {
        if (phLargeIcon) *phLargeIcon = sfi.hIcon; else DestroyIcon(sfi.hIcon);
    }

    SHFILEINFOW sfiSmall = {};
    if (!exePath.empty() && SHGetFileInfoW(exePath.c_str(), 0, &sfiSmall, sizeof(sfiSmall), SHGFI_ICON | SHGFI_SMALLICON))
    {
        if (phSmallIcon) *phSmallIcon = sfiSmall.hIcon; else DestroyIcon(sfiSmall.hIcon);
    }

    if (phLargeIcon && *phLargeIcon == NULL)
        *phLargeIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (phSmallIcon && *phSmallIcon == NULL)
        *phSmallIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (lspzCacheName && cchCacheNameMax > 0)
        StringCchCopyW(lspzCacheName, cchCacheNameMax, L"ProcessVFS_Default");
    if (lpiCacheIndex) *lpiCacheIndex = 0;

    return TRUE;
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
    case DLL_PROCESS_ATTACH: g_hModule = hModule; DisableThreadLibraryCalls(hModule); break;
    case DLL_THREAD_ATTACH: case DLL_THREAD_DETACH: case DLL_PROCESS_DETACH: break;
    }
    return TRUE;
}
