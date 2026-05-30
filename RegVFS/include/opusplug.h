#pragma once

#include <windows.h>

#define OPUSPLUGIN_VFS      0x00000001
#define OPUSPLUGIN_VIEWER   0x00000002
#define OPUSPLUGIN_INFO     0x00000004
#define OPUSPLUGIN_THUMB    0x00000008

#define VFS_CHECK_ARCHIVE   0x00000001
#define VFS_CHECK_FOLDER    0x00000002

#pragma pack(push, 8)

typedef struct _OpusPluginInfo
{
    DWORD cbSize;
    DWORD dwFlags;
    LPCWSTR lpszModuleName;
    LPCWSTR lpszAuthor;
    LPCWSTR lpszDescription;
    LPCWSTR lpszVersion;
    LPCWSTR lpszCopyright;
    LPCWSTR lpszURL;
    LPCWSTR lpszEmail;
    DWORD dwMinVersion;
    DWORD dwMaxVersion;
} OpusPluginInfo;

typedef struct _OpusFindData
{
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
} OpusFindData;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

OpusPluginInfo* WINAPI OpusPluginInfo(void);

BOOL WINAPI VFS_Check(LPCWSTR pszPath, DWORD dwFlags);

BOOL WINAPI VFS_FindFirst(LPCWSTR pszPath, OpusFindData* pFindData, HANDLE* phFind);

BOOL WINAPI VFS_FindNext(HANDLE hFind, OpusFindData* pFindData);

void WINAPI VFS_FindClose(HANDLE hFind);

HANDLE WINAPI VFS_Open(LPCWSTR pszPath, DWORD dwAccess, DWORD dwShareMode,
                       DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes);

DWORD WINAPI VFS_Read(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                      LPDWORD lpNumberOfBytesRead);

DWORD WINAPI VFS_Write(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                       LPDWORD lpNumberOfBytesWritten);

DWORD WINAPI VFS_Seek(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                      DWORD dwMoveMethod);

BOOL WINAPI VFS_Close(HANDLE hFile);

BOOL WINAPI VFS_GetFileInfo(LPCWSTR pszPath, OpusFindData* pFindData);

BOOL WINAPI VFS_Delete(LPCWSTR pszPath, DWORD dwFlags);

BOOL WINAPI VFS_Mkdir(LPCWSTR pszPath);

BOOL WINAPI VFS_Rmdir(LPCWSTR pszPath);

BOOL WINAPI VFS_Rename(LPCWSTR pszOldPath, LPCWSTR pszNewPath);

#ifdef __cplusplus
}
#endif
