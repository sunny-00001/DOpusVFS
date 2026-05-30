#include "MenuManager.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "PathParser.h"
#include "Utils.h"
#include "PropertyDialog.h"
#include "ShareDialog.h"
#include "SyncDialog.h"
#include "CheckDialog.h"
#include "TrashDialog.h"
#include <shellapi.h>
#include <strsafe.h>
#include "JobDialog.h"
#include "json.hpp"
#include <map>

using json = nlohmann::json;

static void MenuDebugLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

// ============================================================
// In-memory dialog template builder for CreateFolder input box
// ============================================================

struct CreateFolderDlgData {
    WCHAR folderName[256];
};

static INT_PTR CALLBACK CreateFolderDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* pData = (CreateFolderDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)pData);
        SetWindowTextW(hDlg, L"新建文件夹");
        SetDlgItemTextW(hDlg, 101, L"新文件夹");
        SendDlgItemMessageW(hDlg, 101, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hDlg, 101));
        return FALSE; // we set focus ourselves
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            auto* pData = (CreateFolderDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            GetDlgItemTextW(hDlg, 101, pData->folderName, 256);
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

// Build a dialog template in memory with: Edit control (ID 101) + OK + Cancel buttons
static HGLOBAL BuildCreateFolderDlgTemplate() {
    const size_t bufSize = 4096;
    HGLOBAL hMem = GlobalAlloc(GMEM_FIXED, bufSize);
    if (!hMem) return NULL;
    BYTE* p = (BYTE*)hMem;

    auto AlignToDword = [&p]() {
        p = (BYTE*)(((ULONG_PTR)p + 3) & ~3);
    };

    // --- DLGTEMPLATE ---
    DLGTEMPLATE dlg = {};
    dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
    dlg.dwExtendedStyle = 0;
    dlg.cdit = 3; // 3 items: edit + OK + Cancel
    dlg.x = 0; dlg.y = 0;
    dlg.cx = 200; dlg.cy = 70;
    memcpy(p, &dlg, sizeof(dlg));
    p += sizeof(dlg);

    // menu = 0
    *(WORD*)p = 0; p += 2;
    // class = 0 (default)
    *(WORD*)p = 0; p += 2;
    // title: L"\x65B0\x5EFA\x6587\x4EF6\x5939"
    { const WCHAR t[] = L"新建文件夹"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
    // font size
    *(WORD*)p = 9; p += 2;
    // font name: L"MS Shell Dlg"
    { const WCHAR f[] = L"MS Shell Dlg"; size_t n = wcslen(f); memcpy(p, f, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }

    AlignToDword();

    // --- Item 1: Edit control ---
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
        item.dwExtendedStyle = 0;
        item.x = 7; item.y = 8;
        item.cx = 186; item.cy = 14;
        item.id = 101;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2; // atom indicator
        *(WORD*)p = 0x0085; p += 2; // "Edit" atom
        *(WORD*)p = 0; p += 2;     // empty text
        *(WORD*)p = 0; p += 2;     // no creation data
    }

    AlignToDword();

    // --- Item 2: OK button ---
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
        item.dwExtendedStyle = 0;
        item.x = 60; item.y = 30;
        item.cx = 60; item.cy = 14;
        item.id = IDOK;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0080; p += 2; // "Button" atom
        { const WCHAR t[] = L"确定"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2; // no creation data
    }

    AlignToDword();

    // --- Item 3: Cancel button ---
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        item.dwExtendedStyle = 0;
        item.x = 130; item.y = 30;
        item.cx = 60; item.cy = 14;
        item.id = IDCANCEL;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0080; p += 2; // "Button" atom
        { const WCHAR t[] = L"取消"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2; // no creation data
    }

    return hMem;
}

// ============================================================
// In-memory dialog template for Provider selection (ComboBox + Edit + OK + Cancel)
// ============================================================

struct AddRemoteDlgData {
    WCHAR remoteName[256];
    WCHAR providerType[64];
    const std::vector<std::string>* providers;  // pointer to provider list
};

static INT_PTR CALLBACK AddRemoteDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    AddRemoteDlgData* pData = (AddRemoteDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        pData = (AddRemoteDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)pData);
        SetWindowTextW(hDlg, L"添加远程存储");
        SetDlgItemTextW(hDlg, 102, L"存储类型:");

        // Populate combo box with providers
        HWND hCombo = GetDlgItem(hDlg, 103);
        if (pData->providers) {
            for (const auto& p : *pData->providers) {
                SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)Utf8ToWide(p).c_str());
            }
        }
        // Select first item
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

        SetFocus(GetDlgItem(hDlg, 101));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            GetDlgItemTextW(hDlg, 101, pData->remoteName, 256);
            GetDlgItemTextW(hDlg, 103, pData->providerType, 64);
            if (pData->remoteName[0] == L'\0') {
                MessageBoxW(hDlg, L"请输入名称", L"错误", MB_ICONWARNING | MB_OK);
                return TRUE;
            }
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

static HGLOBAL BuildAddRemoteDlgTemplate() {
    const size_t bufSize = 8192;
    HGLOBAL hMem = GlobalAlloc(GMEM_FIXED, bufSize);
    if (!hMem) return NULL;
    BYTE* p = (BYTE*)hMem;

    auto AlignToDword = [&p]() {
        p = (BYTE*)(((ULONG_PTR)p + 3) & ~3);
    };

    // --- DLGTEMPLATE ---
    DLGTEMPLATE dlg = {};
    dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
    dlg.dwExtendedStyle = 0;
    dlg.cdit = 5; // name label + name edit + type label + type combo + OK + Cancel
    dlg.x = 0; dlg.y = 0;
    dlg.cx = 220; dlg.cy = 110;
    memcpy(p, &dlg, sizeof(dlg)); p += sizeof(dlg);

    // menu = 0
    *(WORD*)p = 0; p += 2;
    // class = 0 (default)
    *(WORD*)p = 0; p += 2;
    // title
    { const WCHAR t[] = L"添加远程存储"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
    // font size
    *(WORD*)p = 9; p += 2;
    // font name
    { const WCHAR f[] = L"MS Shell Dlg"; size_t n = wcslen(f); memcpy(p, f, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }

    // --- Item 1: "Name" label ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        item.x = 7; item.y = 7; item.cx = 50; item.cy = 10;
        item.id = 102;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0082; p += 2; // "Static" atom
        { const WCHAR t[] = L"名称:"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2;
    }

    // --- Item 2: Name Edit control ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
        item.x = 60; item.y = 5; item.cx = 150; item.cy = 14;
        item.id = 101;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0085; p += 2; // "Edit" atom
        *(WORD*)p = 0; p += 2;
        *(WORD*)p = 0; p += 2;
    }

    // --- Item 3: Type label ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | SS_LEFT;
        item.x = 7; item.y = 25; item.cx = 50; item.cy = 10;
        item.id = 104;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0082; p += 2; // "Static" atom
        { const WCHAR t[] = L"类型:"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2;
    }

    // --- Item 4: Type ComboBox (dropdown list) ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT;
        item.x = 60; item.y = 23; item.cx = 150; item.cy = 200;  // tall for dropdown
        item.id = 103;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0085; p += 2; // "ComboBox" atom
        *(WORD*)p = 0; p += 2;
        *(WORD*)p = 0; p += 2;
    }

    // --- Item 5: OK button ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
        item.x = 55; item.y = 50; item.cx = 60; item.cy = 14;
        item.id = IDOK;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0080; p += 2; // "Button" atom
        { const WCHAR t[] = L"确定"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2;
    }

    // --- Item 6: Cancel button ---
    AlignToDword();
    {
        DLGITEMTEMPLATE item = {};
        item.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        item.x = 125; item.y = 50; item.cx = 60; item.cy = 14;
        item.id = IDCANCEL;
        memcpy(p, &item, sizeof(item)); p += sizeof(item);
        *(WORD*)p = 0xFFFF; p += 2;
        *(WORD*)p = 0x0080; p += 2;
        { const WCHAR t[] = L"取消"; size_t n = wcslen(t); memcpy(p, t, n*sizeof(WCHAR)); p += n*sizeof(WCHAR); *(WORD*)p = 0; p += 2; }
        *(WORD*)p = 0; p += 2;
    }

    return hMem;
}

// ============================================================
// BuildMenuItems
// ============================================================

std::vector<VFSCONTEXTMENUITEMW> MenuManager::BuildMenuItems(const MenuContext& ctx) {
    std::vector<VFSCONTEXTMENUITEMW> items;

    MenuDebugLog("[Menu] isRoot=%d isRemoteRoot=%d isDir=%d isSelDir=%d pubLink=%d trash=%d about=%d\n",
        ctx.isRoot, ctx.isRemoteRoot, ctx.isDir, ctx.isSelectedDir,
        ctx.features.supportsPublicLink, ctx.features.supportsTrash, ctx.features.supportsAbout);

    VFSCONTEXTMENUITEMW item;

    // ===== Root context: browser and remote management =====
    if (ctx.isRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"在浏览器中管理(&B)", L"$rc_open_browser" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"添加远程存储...(&A)", L"$rc_add_remote" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"删除远程配置(&X)", L"$rc_delete_remote" };
        items.push_back(item);

        return items;
    }

    // ===== Common operations: Open / Copy / Cut / Paste / Delete / Rename =====

    // Open — custom verb, routed to VFS_ContextVerbW default-open handler
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"打开(&O)", L"$rc_open" };
    items.push_back(item);

    item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
    items.push_back(item);

    // Copy — DOpus built-in command
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制(&C)", L"Clipboard COPY" };
    items.push_back(item);

    // Cut — DOpus built-in command
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"剪切(&T)", L"Clipboard CUT" };
    items.push_back(item);

    // Paste — DOpus built-in command
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"粘贴(&P)", L"Clipboard PASTE" };
    items.push_back(item);

    item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
    items.push_back(item);

    // Delete — DOpus built-in command (triggers VFS_DeleteFilesW)
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"删除(&D)", L"Delete" };
    items.push_back(item);

    // Rename — DOpus built-in command (triggers in-place rename in file list)
    item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"重命名(&M)", L"Rename" };
    items.push_back(item);

    item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
    items.push_back(item);

    // ===== Directory-specific operations =====
    if (ctx.isSelectedDir || ctx.isDir) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"新建文件夹(&N)", L"$rc_create_folder" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"递归删除(&P)", L"$rc_purge" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);
    }

    // ===== Cloud-specific operations =====

    if (ctx.features.supportsPublicLink && !ctx.isDir && !ctx.isRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"分享链接(&S)", L"$rc_share_link" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制分享链接(&L)", L"$rc_copy_link" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);
    }

    if (ctx.features.supportsTrash && ctx.isRemoteRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"查看回收站(&T)", L"$rc_trash" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"清空回收站(&L)", L"$rc_cleanup" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);
    }

    if (ctx.features.supportsAbout && ctx.isRemoteRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"存储信息(&A)", L"$rc_about" };
        items.push_back(item);
    }

    if (!ctx.isRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制远程路径(&Y)", L"$rc_copy_path" };
        items.push_back(item);
    }

    if (ctx.hasId && !ctx.isRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"复制文件 ID(&I)", L"$rc_copy_id" };
        items.push_back(item);
    }

    if (!ctx.isRoot && !ctx.isDir) {
        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"同步到...(&Y)", L"$rc_sync" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"校验(&K)", L"$rc_check" };
        items.push_back(item);
    }

    if (ctx.isRemoteRoot) {
        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"带宽限制(&W)", L"$rc_bwlimit" };
        items.push_back(item);
    }

    // Properties & Refresh & Jobs
    {
        item = { sizeof(VFSCONTEXTMENUITEMW), VFSCMF_SEPARATOR, NULL, NULL };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"任务管理(&J)", L"$rc_jobs" };
        items.push_back(item);

        item = { sizeof(VFSCONTEXTMENUITEMW), 0, L"属性(&O)", L"$rc_properties" };
        items.push_back(item);
    }

    return items;
}

// ============================================================
// BuildContextMenu
// ============================================================

bool MenuManager::BuildContextMenu(const MenuContext& ctx, LPVFSCONTEXTMENUDATAW lpMenuData) {
    if (!lpMenuData) return false;

    auto items = BuildMenuItems(ctx);
    MenuDebugLog("[BuildContextMenu] items count=%d\n", (int)items.size());
    if (items.empty()) return false;

    static std::vector<VFSCONTEXTMENUITEMW> s_menuItems;
    s_menuItems = std::move(items);

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = FALSE;
    lpMenuData->lpCustomItems = s_menuItems.data();
    lpMenuData->iNumCustomItems = (int)s_menuItems.size();
    lpMenuData->fFreeCustomItems = FALSE;
    return TRUE;
}

// ============================================================
// HandleVerb
// ============================================================

int MenuManager::HandleVerb(const MenuContext& ctx, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    if (!RcloneClient::EnsureDaemonStarted()) return VFSCVRES_FAIL;

    // ===== Open: download to temp + ShellExecute + auto-sync =====
    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_open") == 0) {
        RclonePathInfo pathInfo;
        if (!PathParser::Parse(lpVerbData->lpszPath, pathInfo)) return VFSCVRES_FAIL;

        // If it's a directory, navigate into it
        if (lpVerbData->dwFlags & DOPUSCVF_ISDIR) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }

        // Download file to temp directory
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

        // Open with associated application
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOASYNC;
        sei.lpFile = localFile.c_str();
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&sei)) {
            // No association — show "open with" dialog
            sei.fMask = SEE_MASK_NOASYNC;
            sei.lpVerb = L"openas";
            sei.lpFile = localFile.c_str();
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteExW(&sei);
        }

        return VFSCVRES_HANDLED;
    }

    // ===== CreateFolder: input dialog → rclone mkdir → refresh =====
    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_create_folder") == 0) {
        HGLOBAL hTmpl = BuildCreateFolderDlgTemplate();
        if (!hTmpl) return VFSCVRES_FAIL;

        CreateFolderDlgData dlgData = {};

        INT_PTR result = DialogBoxIndirectParamW(
            GetModuleHandleW(NULL),
            (LPCDLGTEMPLATEW)hTmpl,
            lpVerbData->hwndParent,
            CreateFolderDlgProc,
            (LPARAM)&dlgData
        );

        GlobalFree(hTmpl);

        if (result != IDOK || dlgData.folderName[0] == L'\0') {
            return VFSCVRES_HANDLED; // cancelled or empty name
        }

        // Build remote path: ctx.remote + "/" + folderName
        std::wstring newRemote = ctx.remote;
        if (!newRemote.empty() && newRemote.back() != L'/') newRemote += L'/';
        newRemote += dlgData.folderName;

        if (!RcloneClient::MakeDir(ctx.fs, newRemote)) {
            MessageBoxW(lpVerbData->hwndParent,
                L"创建文件夹失败",
                L"错误", MB_ICONERROR | MB_OK);
            return VFSCVRES_HANDLED;
        }

        RcloneCache::InvalidateForWrite(ctx.fs, ctx.remote);
        return VFSCVRES_CHANGE;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_properties") == 0) {
        if (ctx.fs.empty()) return VFSCVRES_FAIL;
        ShowPropertyDialog(lpVerbData->hwndParent, ctx.fs, ctx.remote);
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_about") == 0) {
        RcloneAboutInfo aboutInfo;
        if (RcloneClient::About(ctx.fs, aboutInfo)) {
            WCHAR msg[1024];
            StringCchPrintfW(msg, 1024,
                L"总配额:\t%s\n已用:\t%s\n可用:\t%s\n回收站:\t%s",
                aboutInfo.hasTotal ? PathParser::FormatSize(aboutInfo.total).c_str() : L"-",
                aboutInfo.hasUsed ? PathParser::FormatSize(aboutInfo.used).c_str() : L"-",
                aboutInfo.hasFree ? PathParser::FormatSize(aboutInfo.free).c_str() : L"-",
                aboutInfo.hasTrashed ? PathParser::FormatSize(aboutInfo.trashed).c_str() : L"-");
            MessageBoxW(lpVerbData->hwndParent, msg,
                L"存储信息", MB_ICONINFORMATION | MB_OK);
        } else {
            MessageBoxW(lpVerbData->hwndParent,
                L"无法获取存储信息",
                L"存储信息", MB_ICONWARNING | MB_OK);
        }
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_copy_path") == 0) {
        std::wstring pathStr = PathParser::ExtractFsName(ctx.fs) + L":" + ctx.remote;
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (pathStr.length() + 1) * sizeof(WCHAR));
            if (hMem) {
                LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                StringCchCopyW(pMem, pathStr.length() + 1, pathStr.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_copy_id") == 0) {
        RcloneFileInfo info;
        if (RcloneClient::Stat(ctx.fs, ctx.remote, info) && !info.id.empty()) {
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (info.id.length() + 1) * sizeof(WCHAR));
                if (hMem) {
                    LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                    StringCchCopyW(pMem, info.id.length() + 1, info.id.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_open_browser") == 0) {
        ShellExecuteW(NULL, L"open", L"http://127.0.0.57:8657/", NULL, NULL, SW_SHOWNORMAL);
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_share_link") == 0) {
        ShowShareDialog(lpVerbData->hwndParent, ctx.fs, ctx.remote);
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_trash") == 0) {
        ShowTrashDialog(lpVerbData->hwndParent, ctx.fs, ctx.remoteType);
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_sync") == 0) {
        ShowSyncDialog(lpVerbData->hwndParent, ctx.fs, ctx.remote, ctx.fs, L"");
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_check") == 0) {
        ShowCheckDialog(lpVerbData->hwndParent, ctx.fs, ctx.remote, ctx.fs, L"");
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_copy_link") == 0) {
        std::wstring url;
        if (RcloneClient::PublicLink(ctx.fs, ctx.remote, url)) {
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (url.length() + 1) * sizeof(WCHAR));
                if (hMem) {
                    LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
                    StringCchCopyW(pMem, url.length() + 1, url.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            MessageBoxW(lpVerbData->hwndParent, L"分享链接已复制到剪贴板",
                L"复制成功", MB_ICONINFORMATION | MB_OK);
        } else {
            std::wstring errMsg = url.empty() ? L"无法生成分享链接" : url;
            MessageBoxW(lpVerbData->hwndParent, errMsg.c_str(),
                L"复制失败", MB_ICONWARNING | MB_OK);
        }
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_cleanup") == 0) {
        if (MessageBoxW(lpVerbData->hwndParent,
                L"确认要清空回收站吗? 此操作不可撤销。",
                L"清空回收站", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return VFSCVRES_HANDLED;
        }
        if (RcloneClient::Cleanup(ctx.fs)) {
            MessageBoxW(lpVerbData->hwndParent, L"回收站已清空",
                L"清空成功", MB_ICONINFORMATION | MB_OK);
            return VFSCVRES_CHANGE;
        } else {
            MessageBoxW(lpVerbData->hwndParent, L"清空回收站失败",
                L"清空失败", MB_ICONWARNING | MB_OK);
            return VFSCVRES_HANDLED;
        }
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_purge") == 0) {
        if (MessageBoxW(lpVerbData->hwndParent,
                L"确认要递归删除此目录吗? 此操作不可撤销。",
                L"递归删除", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return VFSCVRES_HANDLED;
        }
        if (RcloneClient::Purge(ctx.fs, ctx.remote)) {
            return VFSCVRES_CHANGE;
        } else {
            MessageBoxW(lpVerbData->hwndParent, L"递归删除失败",
                L"删除失败", MB_ICONWARNING | MB_OK);
            return VFSCVRES_HANDLED;
        }
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"RcloneCopy") == 0) {
        RcloneFileInfo info;
        if (!RcloneClient::Stat(ctx.fs, ctx.remote, info)) return VFSCVRES_FAIL;

        bool ok = info.isDir ? RcloneClient::CopyDir(ctx.fs, ctx.remote, ctx.fs, ctx.remote)
                             : RcloneClient::CopyFileRemote(ctx.fs, ctx.remote, ctx.fs, ctx.remote);
        if (!ok) return VFSCVRES_FAIL;

        RcloneCache::InvalidateForWrite(ctx.fs, ctx.remote);
        return VFSCVRES_CHANGE;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"RcloneMove") == 0) {
        RcloneFileInfo info;
        if (!RcloneClient::Stat(ctx.fs, ctx.remote, info)) return VFSCVRES_FAIL;

        bool ok = info.isDir ? RcloneClient::MoveDir(ctx.fs, ctx.remote, ctx.fs, ctx.remote)
                             : RcloneClient::MoveFileRemote(ctx.fs, ctx.remote, ctx.fs, ctx.remote);
        if (!ok) return VFSCVRES_FAIL;

        RcloneCache::InvalidateForWrite(ctx.fs, ctx.remote);
        return VFSCVRES_CHANGE;
    }

    // ===== Phase 3 verbs =====

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_add_remote") == 0) {
        // Get available providers
        std::vector<std::string> providers;
        if (!RcloneClient::ConfigProviders(providers) || providers.empty()) {
            MessageBoxW(lpVerbData->hwndParent,
                L"无法获取存储类型列表",
                L"错误", MB_ICONWARNING | MB_OK);
            return VFSCVRES_HANDLED;
        }

        // Show add-remote dialog with name edit + provider dropdown
        AddRemoteDlgData dlgData = {};
        dlgData.providers = &providers;
        HGLOBAL hTmpl = BuildAddRemoteDlgTemplate();
        if (!hTmpl) return VFSCVRES_FAIL;

        INT_PTR result = DialogBoxIndirectParamW(
            GetModuleHandleW(NULL), (LPCDLGTEMPLATEW)hTmpl,
            lpVerbData->hwndParent, AddRemoteDlgProc, (LPARAM)&dlgData);
        GlobalFree(hTmpl);

        if (result != IDOK || dlgData.remoteName[0] == L'\0' || dlgData.providerType[0] == L'\0')
            return VFSCVRES_HANDLED;

        std::string typeStr = WideToUtf8(dlgData.providerType);
        std::wstring wName = dlgData.remoteName;

        // Create config with _async=true (OAuth types need browser authorization)
        std::map<std::string, std::string> params;
        std::string error;
        if (RcloneClient::ConfigCreate(wName, typeStr, params, lpVerbData->hwndParent, &error)) {
            MessageBoxW(lpVerbData->hwndParent,
                L"远程存储已添加成功",
                L"添加成功", MB_ICONINFORMATION | MB_OK);
            return VFSCVRES_CHANGE;
        } else {
            std::wstring errMsg = L"添加远程存储失败";
            if (!error.empty()) errMsg += L"\n" + Utf8ToWide(error);
            MessageBoxW(lpVerbData->hwndParent, errMsg.c_str(),
                L"错误", MB_ICONWARNING | MB_OK);
            return VFSCVRES_HANDLED;
        }
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_delete_remote") == 0) {
        if (ctx.fs.empty()) return VFSCVRES_FAIL;
        std::wstring remoteName = PathParser::ExtractFsName(ctx.fs);
        if (remoteName.empty()) return VFSCVRES_FAIL;

        WCHAR msg[512];
        StringCchPrintfW(msg, 512,
            L"确认要删除远程配置 \"%s\" 吗?\n此操作不可撤销。",
            remoteName.c_str());
        if (MessageBoxW(lpVerbData->hwndParent, msg,
                L"删除远程配置", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return VFSCVRES_HANDLED;
        }

        std::string error;
        if (RcloneClient::ConfigDelete(remoteName, &error)) {
            return VFSCVRES_CHANGE;
        } else {
            std::wstring errMsg = L"删除远程配置失败";
            if (!error.empty()) errMsg += L"\n" + Utf8ToWide(error);
            MessageBoxW(lpVerbData->hwndParent, errMsg.c_str(),
                L"错误", MB_ICONWARNING | MB_OK);
            return VFSCVRES_HANDLED;
        }
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_bwlimit") == 0) {
        // Show current bandwidth limit and let user change it
        std::string currentLimit;
        bool hasCurrent = RcloneClient::GetBwlimit(currentLimit);

        // Show current value and ask for new
        WCHAR prompt[512];
        if (hasCurrent) {
            StringCchPrintfW(prompt, 512,
                L"当前带宽限制: %s\n\n"
                L"输入新的限制值(如: 1M, 10M, off):",
                Utf8ToWide(currentLimit).c_str());
        } else {
            StringCchPrintfW(prompt, 512,
                L"输入带宽限制值(如: 1M, 10M, off):");
        }
        MessageBoxW(lpVerbData->hwndParent, prompt,
            L"带宽限制", MB_ICONINFORMATION | MB_OK);

        // Use input dialog for new limit
        CreateFolderDlgData limitData = {};
        if (hasCurrent) {
            StringCchCopyW(limitData.folderName, 256, Utf8ToWide(currentLimit).c_str());
        }
        HGLOBAL hTmpl = BuildCreateFolderDlgTemplate();
        if (!hTmpl) return VFSCVRES_FAIL;

        INT_PTR result = DialogBoxIndirectParamW(
            GetModuleHandleW(NULL), (LPCDLGTEMPLATEW)hTmpl,
            lpVerbData->hwndParent, CreateFolderDlgProc, (LPARAM)&limitData);
        GlobalFree(hTmpl);

        if (result != IDOK || limitData.folderName[0] == L'\0')
            return VFSCVRES_HANDLED;

        std::string newLimit = WideToUtf8(limitData.folderName);
        std::string error;
        if (RcloneClient::SetBwlimit(newLimit, &error)) {
            WCHAR confirm[256];
            StringCchPrintfW(confirm, 256,
                L"带宽限制已设置为: %s",
                Utf8ToWide(newLimit).c_str());
            MessageBoxW(lpVerbData->hwndParent, confirm,
                L"设置成功", MB_ICONINFORMATION | MB_OK);
        } else {
            std::wstring errMsg = L"设置带宽限制失败";
            if (!error.empty()) errMsg += L"\n" + Utf8ToWide(error);
            MessageBoxW(lpVerbData->hwndParent, errMsg.c_str(),
                L"错误", MB_ICONWARNING | MB_OK);
        }
        return VFSCVRES_HANDLED;
    }

    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"rc_jobs") == 0) {
        ShowJobDialog(lpVerbData->hwndParent);
        return VFSCVRES_HANDLED;
    }

    return -1;
}
