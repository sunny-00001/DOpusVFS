#pragma once
#include <windows.h>
#include <string>
#include "DataStructs.h"
#include "vfs_plugins.h"

class ColumnManager {
public:
    static LPVFSCUSTOMCOLUMNW GetColumns();
    static int GetTotalColumnCount();

    static void FillDataForRemote(LPVFSFILEDATAW pFileData, HANDLE hHeap,
        const RcloneRemoteInfo& remoteInfo, const RcloneAboutInfo& aboutInfo);
    static void FillDataForFile(LPVFSFILEDATAW pFileData, HANDLE hHeap,
        const std::wstring& fs, const std::wstring& remote,
        const RcloneFileInfo& fileInfo, const std::wstring& remoteType);

private:
    static LPWSTR AllocHeapString(HANDLE hHeap, const std::wstring& str);
};
