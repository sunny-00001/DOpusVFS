#include "Logger.h"
#include <shlobj.h>
#include <time.h>
#include <algorithm>
#include <vector>

static std::wstring s_logDir;
static std::wstring s_currentLogFile;
static HANDLE s_logMutex = NULL;
static LogLevel s_logLevel = LOG_INFO;

std::wstring GetLogDir() {
    if (!s_logDir.empty()) return s_logDir;

    WCHAR appDataPath[MAX_PATH] = { 0 };
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath) != S_OK) {
        WCHAR tempPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tempPath);
        s_logDir = std::wstring(tempPath) + L"RcloneVFS\\logs\\";
        return s_logDir;
    }

    s_logDir = std::wstring(appDataPath) + L"\\GPSoftware\\Directory Opus\\User Data\\VFSPlugin\\RcloneVFS\\logs\\";
    return s_logDir;
}

void EnsureLogDir() {
    std::wstring dir = GetLogDir();
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }
}

std::wstring GetLogFileName() {
    if (!s_currentLogFile.empty()) return s_currentLogFile;

    EnsureLogDir();
    std::wstring dir = GetLogDir();
    s_currentLogFile = dir + L"rclonevfs.log";
    return s_currentLogFile;
}

void RotateLogsIfNeeded(DWORD maxSizeMB, DWORD maxFiles) {
    std::wstring dir = GetLogDir();
    std::wstring currentFile = GetLogFileName();

    // Check current file size
    HANDLE hFile = CreateFileW(currentFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile, &fileSize)) {
            DWORD sizeMB = (DWORD)(fileSize.QuadPart / (1024 * 1024));
            if (sizeMB >= maxSizeMB) {
                // Need to rotate
                SYSTEMTIME st;
                GetLocalTime(&st);
                
                WCHAR dateStr[32];
                swprintf_s(dateStr, L"rclonevfs-%04d-%02d-%02d.log", 
                    st.wYear, st.wMonth, st.wDay);
                
                std::wstring backupFile = dir + dateStr;
                CloseHandle(hFile);
                
                // Try to rename
                MoveFileExW(currentFile.c_str(), backupFile.c_str(), MOVEFILE_REPLACE_EXISTING);
                
                // Delete oldest files if we exceed maxFiles
                WIN32_FIND_DATAW findData;
                std::wstring searchPattern = dir + L"rclonevfs-*.log";
                HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
                
                if (hFind != INVALID_HANDLE_VALUE) {
                    std::vector<std::wstring> logFiles;
                    do {
                        logFiles.push_back(dir + findData.cFileName);
                    } while (FindNextFileW(hFind, &findData));
                    FindClose(hFind);
                    
                    std::sort(logFiles.begin(), logFiles.end());
                    while (logFiles.size() >= maxFiles) {
                        DeleteFileW(logFiles.front().c_str());
                        logFiles.erase(logFiles.begin());
                    }
                }
                
                s_currentLogFile.clear(); // Reset to create new file
            } else {
                CloseHandle(hFile);
            }
        } else {
            CloseHandle(hFile);
        }
    }
}

void InitLogger(LogLevel level) {
    s_logLevel = level;
    if (s_logMutex == NULL) {
        s_logMutex = CreateMutexW(NULL, FALSE, NULL);
    }
    EnsureLogDir();
}

void SetLogLevel(LogLevel level) {
    s_logLevel = level;
}

void Log(LogLevel level, const wchar_t* format, ...) {
    if (level > s_logLevel) return;

    if (s_logMutex == NULL) {
        s_logMutex = CreateMutexW(NULL, FALSE, NULL);
    }

    WaitForSingleObject(s_logMutex, INFINITE);

    try {
        SYSTEMTIME st;
        GetLocalTime(&st);

        WCHAR timeStr[64];
        swprintf_s(timeStr, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        const wchar_t* levelStr = L"INFO";
        switch (level) {
            case LOG_ERROR: levelStr = L"ERROR"; break;
            case LOG_WARN:  levelStr = L"WARN";  break;
            case LOG_INFO:  levelStr = L"INFO";  break;
            case LOG_DEBUG: levelStr = L"DEBUG"; break;
        }

        va_list args;
        va_start(args, format);
        
        int bufSize = _vscwprintf(format, args) + 1;
        std::wstring message(bufSize, L'\0');
        vswprintf_s(&message[0], bufSize, format, args);
        
        va_end(args);

        std::wstring logLine = timeStr + std::wstring(levelStr) + L": " + message + L"\n";

        std::wstring logFile = GetLogFileName();
        HANDLE hFile = CreateFileW(logFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            SetFilePointer(hFile, 0, NULL, FILE_END);
            DWORD written;
            WriteFile(hFile, logLine.c_str(), (DWORD)(logLine.size() * sizeof(wchar_t)), &written, NULL);
            CloseHandle(hFile);
        }
    }
    catch (...) {
        // Ignore logging errors
    }

    ReleaseMutex(s_logMutex);
}

static void LogWithLevel(LogLevel level, const wchar_t* format, va_list args) {
    int bufSize = _vscwprintf(format, args) + 1;
    std::wstring message(bufSize, L'\0');
    vswprintf_s(&message[0], bufSize, format, args);
    Log(level, message.c_str());
}

void LogError(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    LogWithLevel(LOG_ERROR, format, args);
    va_end(args);
}

void LogWarn(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    LogWithLevel(LOG_WARN, format, args);
    va_end(args);
}

void LogInfo(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    LogWithLevel(LOG_INFO, format, args);
    va_end(args);
}

void LogDebug(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    LogWithLevel(LOG_DEBUG, format, args);
    va_end(args);
}

void CleanOldLogs(DWORD maxDays) {
    std::wstring dir = GetLogDir();
    WIN32_FIND_DATAW findData;
    std::wstring searchPattern = dir + L"rclonevfs-*.log";
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME now;
    GetLocalTime(&now);

    do {
        SYSTEMTIME st;
        if (FileTimeToSystemTime(&findData.ftLastWriteTime, &st)) {
            int daysDiff = (now.wYear - st.wYear) * 365 +
                (now.wMonth - st.wMonth) * 30 +
                (now.wDay - st.wDay);

            if (daysDiff > (int)maxDays) {
                DeleteFileW((dir + findData.cFileName).c_str());
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}