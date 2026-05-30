#include "ColumnManager.h"
#include "PathParser.h"
#include "DaemonManager.h"
#include "RcloneFeatures.h"
#include <strsafe.h>

LPWSTR ColumnManager::AllocHeapString(HANDLE hHeap, const std::wstring& str) {
    if (str.empty()) return NULL;
    LPWSTR p = (LPWSTR)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, (str.length() + 1) * sizeof(WCHAR));
    if (p) StringCchCopyW(p, str.length() + 1, str.c_str());
    return p;
}

LPVFSCUSTOMCOLUMNW ColumnManager::GetColumns() {
    static VFSCUSTOMCOLUMNW allCols[NUM_ALL_COLUMNS];

    int i = 0;

    // --- Root context columns (1-8) ---
    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"远程存储";
    allCols[i].lpszKey = L"rcRemote";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_REMOTE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"存储类型";
    allCols[i].lpszKey = L"rcStorageType";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_STYPE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"总配额";
    allCols[i].lpszKey = L"rcQuotaTotal";
    allCols[i].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    allCols[i].iID = COL_RC_QUOTA_TOTAL;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"已用空间";
    allCols[i].lpszKey = L"rcQuotaUsed";
    allCols[i].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    allCols[i].iID = COL_RC_QUOTA_USED;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"可用空间";
    allCols[i].lpszKey = L"rcQuotaFree";
    allCols[i].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    allCols[i].iID = COL_RC_QUOTA_FREE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"使用率";
    allCols[i].lpszKey = L"rcQuotaUsage";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_QUOTA_USAGE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"回收站";
    allCols[i].lpszKey = L"rcTrashedSize";
    allCols[i].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    allCols[i].iID = COL_RC_TRASHED_SIZE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"状态";
    allCols[i].lpszKey = L"rcStatus";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_STATUS;
    i++;

    // --- File context columns (101-109) ---
    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"远程存储";
    allCols[i].lpszKey = L"rcRemote2";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_REMOTE2;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"项目类型";
    allCols[i].lpszKey = L"rcItemType";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_ITYPE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"大小";
    allCols[i].lpszKey = L"rcFormattedSize";
    allCols[i].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    allCols[i].iID = COL_RC_FSIZE;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"修改时间";
    allCols[i].lpszKey = L"rcModTime";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_MTIME;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"MIME类型";
    allCols[i].lpszKey = L"rcMimeType";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_MIME;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"文件 ID";
    allCols[i].lpszKey = L"rcFileID";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_FID;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"远程路径";
    allCols[i].lpszKey = L"rcRemotePath";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_RPATH;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = &allCols[i + 1];
    allCols[i].lpszLabel = L"哈希";
    allCols[i].lpszKey = L"rcHash";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_HASH;
    i++;

    allCols[i].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    allCols[i].lpNext = NULL;
    allCols[i].lpszLabel = L"分享状态";
    allCols[i].lpszKey = L"rcShared";
    allCols[i].dwFlags = VFSCCF_LEFTJUSTIFY;
    allCols[i].iID = COL_RC_SHARED;

    return allCols;
}

int ColumnManager::GetTotalColumnCount() {
    return NUM_ALL_COLUMNS;
}

void ColumnManager::FillDataForRemote(LPVFSFILEDATAW pFileData, HANDLE hHeap,
    const RcloneRemoteInfo& remoteInfo, const RcloneAboutInfo& aboutInfo) {
    // Root context: fill all 17 columns (root cols = data, file cols = "-")
    pFileData->iNumColumns = NUM_ALL_COLUMNS;
    pFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, NUM_ALL_COLUMNS * sizeof(VFSFILEDATACOLUMNW));
    if (!pFileData->lpvfsColumnData) return;

    // Root context columns (1-8) — with real data
    pFileData->lpvfsColumnData[0].iColumnId = COL_RC_REMOTE;
    pFileData->lpvfsColumnData[0].lpszValue = AllocHeapString(hHeap, remoteInfo.name);

    pFileData->lpvfsColumnData[1].iColumnId = COL_RC_STYPE;
    pFileData->lpvfsColumnData[1].lpszValue = AllocHeapString(hHeap, PathParser::GetRemoteTypeCN(remoteInfo.type));

    pFileData->lpvfsColumnData[2].iColumnId = COL_RC_QUOTA_TOTAL;
    pFileData->lpvfsColumnData[2].lpszValue = AllocHeapString(hHeap,
        (aboutInfo.hasTotal && aboutInfo.total > 0) ? PathParser::FormatSize(aboutInfo.total) : L"...");

    pFileData->lpvfsColumnData[3].iColumnId = COL_RC_QUOTA_USED;
    pFileData->lpvfsColumnData[3].lpszValue = AllocHeapString(hHeap,
        (aboutInfo.hasUsed && aboutInfo.used > 0) ? PathParser::FormatSize(aboutInfo.used) : L"...");

    pFileData->lpvfsColumnData[4].iColumnId = COL_RC_QUOTA_FREE;
    pFileData->lpvfsColumnData[4].lpszValue = AllocHeapString(hHeap,
        (aboutInfo.hasFree && aboutInfo.free > 0) ? PathParser::FormatSize(aboutInfo.free) : L"...");

    pFileData->lpvfsColumnData[5].iColumnId = COL_RC_QUOTA_USAGE;
    if (aboutInfo.hasTotal && aboutInfo.total > 0 && aboutInfo.hasUsed && aboutInfo.used > 0) {
        WCHAR buf[32];
        double pct = (double)aboutInfo.used / (double)aboutInfo.total * 100.0;
        StringCchPrintfW(buf, 32, L"%.1f%%", pct);
        pFileData->lpvfsColumnData[5].lpszValue = AllocHeapString(hHeap, buf);
    } else {
        pFileData->lpvfsColumnData[5].lpszValue = AllocHeapString(hHeap, L"...");
    }

    pFileData->lpvfsColumnData[6].iColumnId = COL_RC_TRASHED_SIZE;
    pFileData->lpvfsColumnData[6].lpszValue = AllocHeapString(hHeap,
        (aboutInfo.hasTrashed && aboutInfo.trashed > 0) ? PathParser::FormatSize(aboutInfo.trashed) : L"...");

    pFileData->lpvfsColumnData[7].iColumnId = COL_RC_STATUS;
    pFileData->lpvfsColumnData[7].lpszValue = AllocHeapString(hHeap,
        DaemonManager::IsRunning() ? L"在线" : L"离线");

    // File context columns (101-109) — fill with "-" for root items
    pFileData->lpvfsColumnData[8].iColumnId = COL_RC_REMOTE2;
    pFileData->lpvfsColumnData[8].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[9].iColumnId = COL_RC_ITYPE;
    pFileData->lpvfsColumnData[9].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[10].iColumnId = COL_RC_FSIZE;
    pFileData->lpvfsColumnData[10].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[11].iColumnId = COL_RC_MTIME;
    pFileData->lpvfsColumnData[11].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[12].iColumnId = COL_RC_MIME;
    pFileData->lpvfsColumnData[12].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[13].iColumnId = COL_RC_FID;
    pFileData->lpvfsColumnData[13].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[14].iColumnId = COL_RC_RPATH;
    pFileData->lpvfsColumnData[14].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[15].iColumnId = COL_RC_HASH;
    pFileData->lpvfsColumnData[15].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[16].iColumnId = COL_RC_SHARED;
    pFileData->lpvfsColumnData[16].lpszValue = AllocHeapString(hHeap, L"-");
}

void ColumnManager::FillDataForFile(LPVFSFILEDATAW pFileData, HANDLE hHeap,
    const std::wstring& fs, const std::wstring& remote,
    const RcloneFileInfo& fileInfo, const std::wstring& remoteType) {
    // File context: fill all 17 columns (root cols = "-", file cols = data)
    pFileData->iNumColumns = NUM_ALL_COLUMNS;
    pFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, NUM_ALL_COLUMNS * sizeof(VFSFILEDATACOLUMNW));
    if (!pFileData->lpvfsColumnData) return;

    // Root context columns (1-8) — fill with "-" for file items
    pFileData->lpvfsColumnData[0].iColumnId = COL_RC_REMOTE;
    pFileData->lpvfsColumnData[0].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[1].iColumnId = COL_RC_STYPE;
    pFileData->lpvfsColumnData[1].lpszValue = AllocHeapString(hHeap, PathParser::GetRemoteTypeCN(remoteType));

    pFileData->lpvfsColumnData[2].iColumnId = COL_RC_QUOTA_TOTAL;
    pFileData->lpvfsColumnData[2].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[3].iColumnId = COL_RC_QUOTA_USED;
    pFileData->lpvfsColumnData[3].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[4].iColumnId = COL_RC_QUOTA_FREE;
    pFileData->lpvfsColumnData[4].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[5].iColumnId = COL_RC_QUOTA_USAGE;
    pFileData->lpvfsColumnData[5].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[6].iColumnId = COL_RC_TRASHED_SIZE;
    pFileData->lpvfsColumnData[6].lpszValue = AllocHeapString(hHeap, L"-");

    pFileData->lpvfsColumnData[7].iColumnId = COL_RC_STATUS;
    pFileData->lpvfsColumnData[7].lpszValue = AllocHeapString(hHeap,
        DaemonManager::IsRunning() ? L"在线" : L"离线");

    // File context columns (101-109) — with real data
    pFileData->lpvfsColumnData[8].iColumnId = COL_RC_REMOTE2;
    pFileData->lpvfsColumnData[8].lpszValue = AllocHeapString(hHeap, PathParser::ExtractFsName(fs));

    pFileData->lpvfsColumnData[9].iColumnId = COL_RC_ITYPE;
    pFileData->lpvfsColumnData[9].lpszValue = AllocHeapString(hHeap,
        fileInfo.isDir ? L"文件夹" : L"文件");

    pFileData->lpvfsColumnData[10].iColumnId = COL_RC_FSIZE;
    pFileData->lpvfsColumnData[10].lpszValue = AllocHeapString(hHeap,
        fileInfo.isDir ? L"-" : PathParser::FormatSize(fileInfo.size));

    pFileData->lpvfsColumnData[11].iColumnId = COL_RC_MTIME;
    pFileData->lpvfsColumnData[11].lpszValue = AllocHeapString(hHeap, PathParser::FormatTime(fileInfo.modTime));

    pFileData->lpvfsColumnData[12].iColumnId = COL_RC_MIME;
    pFileData->lpvfsColumnData[12].lpszValue = AllocHeapString(hHeap,
        fileInfo.mimeType.empty() ? L"-" : fileInfo.mimeType);

    pFileData->lpvfsColumnData[13].iColumnId = COL_RC_FID;
    pFileData->lpvfsColumnData[13].lpszValue = AllocHeapString(hHeap,
        fileInfo.id.empty() ? L"-" : fileInfo.id);

    std::wstring fullPath = remote.empty() ? fileInfo.name : (remote + L"/" + fileInfo.name);
    pFileData->lpvfsColumnData[14].iColumnId = COL_RC_RPATH;
    pFileData->lpvfsColumnData[14].lpszValue = AllocHeapString(hHeap, fullPath);

    pFileData->lpvfsColumnData[15].iColumnId = COL_RC_HASH;
    pFileData->lpvfsColumnData[15].lpszValue = AllocHeapString(hHeap,
        fileInfo.hash.empty() ? L"-" : fileInfo.hash);

    pFileData->lpvfsColumnData[16].iColumnId = COL_RC_SHARED;
    {
        RcloneBackendFeatures feat = RcloneFeatures::Get(fs, remoteType);
        if (feat.supportsPublicLink && !fileInfo.isDir) {
            pFileData->lpvfsColumnData[16].lpszValue = AllocHeapString(hHeap, L"可分享");
        } else {
            pFileData->lpvfsColumnData[16].lpszValue = AllocHeapString(hHeap, L"-");
        }
    }
}
