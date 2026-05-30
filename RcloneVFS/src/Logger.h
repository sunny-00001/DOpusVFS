#pragma once
#include <windows.h>
#include <string>

typedef enum {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
} LogLevel;

void InitLogger(LogLevel level);
void SetLogLevel(LogLevel level);

void Log(LogLevel level, const wchar_t* format, ...);
void LogError(const wchar_t* format, ...);
void LogWarn(const wchar_t* format, ...);
void LogInfo(const wchar_t* format, ...);
void LogDebug(const wchar_t* format, ...);

void CleanOldLogs(DWORD maxDays);
void RotateLogsIfNeeded(DWORD maxSizeMB, DWORD maxFiles);