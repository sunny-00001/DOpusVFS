#include "SyncDialog.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "Utils.h"
#include "PathParser.h"
#include "resource.h"
#include <strsafe.h>
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

enum SyncMode { SYNC_UPDATE = 0, SYNC_COPY = 1, SYNC_MOVE = 2 };

struct SyncDlgData {
    std::wstring srcFs;
    std::wstring srcRemote;
    std::wstring dstFs;
    std::wstring dstRemote;
    SyncMode mode;
    bool running;
    int jobId;
};

static void SetStatus(HWND hDlg, const WCHAR* status) {
    SetDlgItemTextW(hDlg, IDC_SYNC_STATUS, status);
}

static DWORD WINAPI SyncThreadProc(LPVOID param) {
    SyncDlgData* data = (SyncDlgData*)param;

    HWND hDlg = FindWindowW(NULL, L"同步操作");
    if (!hDlg) hDlg = FindWindowW(NULL, L"Sync");

    if (hDlg) {
        SendMessageW(GetDlgItem(hDlg, IDC_SYNC_PROGRESS), PBM_SETMARQUEE, (WPARAM)TRUE, 0);
        SetStatus(hDlg, L"正在同步...");
    }

    bool success = false;
    std::string reqJson;
    std::string res;

    switch (data->mode) {
    case SYNC_COPY:
        reqJson = "{\"srcFs\":\"" + WideToUtf8(data->srcFs + data->srcRemote) +
                  "\",\"dstFs\":\"" + WideToUtf8(data->dstFs + data->dstRemote) +
                  "\",\"createEmptySrcDirs\":true}";
        res = RcloneClient::SendHttpPost("/sync/copy", reqJson);
        success = !res.empty();
        break;

    case SYNC_MOVE:
        reqJson = "{\"srcFs\":\"" + WideToUtf8(data->srcFs + data->srcRemote) +
                  "\",\"dstFs\":\"" + WideToUtf8(data->dstFs + data->dstRemote) +
                  "\",\"deleteEmptySrcDirs\":true}";
        res = RcloneClient::SendHttpPost("/sync/move", reqJson);
        success = !res.empty();
        break;

    case SYNC_UPDATE:
    default:
        reqJson = "{\"srcFs\":\"" + WideToUtf8(data->srcFs + data->srcRemote) +
                  "\",\"dstFs\":\"" + WideToUtf8(data->dstFs + data->dstRemote) +
                  "\",\"deleteEmptySrcDirs\":true}";
        res = RcloneClient::SendHttpPost("/sync/copy", reqJson);
        success = !res.empty();
        break;
    }

    if (hDlg) {
        SendMessageW(GetDlgItem(hDlg, IDC_SYNC_PROGRESS), PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        SendMessageW(GetDlgItem(hDlg, IDC_SYNC_PROGRESS), PBM_SETPOS, success ? 100 : 0, 0);

        if (success) {
            SetStatus(hDlg, L"同步完成");
            EnableWindow(GetDlgItem(hDlg, IDC_SYNC_START), FALSE);
            RcloneCache::InvalidateAll();
        } else {
            SetStatus(hDlg, res.empty() ? L"同步失败" : Utf8ToWide(res).c_str());
        }
    }

    data->running = false;
    return success ? 0 : 1;
}

static INT_PTR CALLBACK SyncDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static SyncDlgData* s_data = nullptr;
    static HANDLE s_hThread = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        s_data = (SyncDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)s_data);

        SetWindowTextW(hDlg, L"同步操作");

        std::wstring srcPath = PathParser::ExtractFsName(s_data->srcFs) + L":" + s_data->srcRemote;
        std::wstring dstPath = PathParser::ExtractFsName(s_data->dstFs) + L":" + s_data->dstRemote;
        SetDlgItemTextW(hDlg, IDC_SYNC_SRC_PATH, srcPath.c_str());
        SetDlgItemTextW(hDlg, IDC_SYNC_DST_PATH, dstPath.c_str());

        HWND hCombo = GetDlgItem(hDlg, IDC_SYNC_MODE);
        SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"更新文件 (默认)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"复制");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"移动");
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

        SendMessageW(GetDlgItem(hDlg, IDC_SYNC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(GetDlgItem(hDlg, IDC_SYNC_PROGRESS), PBM_SETPOS, 0, 0);
        SetStatus(hDlg, L"请设置目标路径并点击开始");

        return TRUE;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_SYNC_START) {
            if (s_data->running) {
                SetStatus(hDlg, L"操作进行中...");
                return TRUE;
            }

            WCHAR dstPath[512] = { 0 };
            GetDlgItemTextW(hDlg, IDC_SYNC_DST_PATH, dstPath, 512);

            if (wcslen(dstPath) == 0) {
                SetStatus(hDlg, L"请输入目标路径");
                return TRUE;
            }

            HWND hCombo = GetDlgItem(hDlg, IDC_SYNC_MODE);
            s_data->mode = (SyncMode)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);

            s_data->running = true;
            s_hThread = CreateThread(nullptr, 0, SyncThreadProc, s_data, 0, nullptr);
            if (s_hThread) {
                CloseHandle(s_hThread);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDOK) {
            if (s_data->running) {
                SetStatus(hDlg, L"请先等待操作完成");
                return TRUE;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL) {
            if (s_data->running) {
                if (MessageBoxW(hDlg, L"确定要取消同步吗?",
                    L"取消", MB_YESNO | MB_ICONWARNING) != IDYES) {
                    return TRUE;
                }
            }
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE: {
        if (s_data->running) {
            SetStatus(hDlg, L"请先等待操作完成");
            return TRUE;
        }
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}

bool ShowSyncDialog(HWND hwndParent, const std::wstring& srcFs, const std::wstring& srcRemote,
                    const std::wstring& dstFs, const std::wstring& dstRemote) {
    SyncDlgData data = {};
    data.srcFs = srcFs;
    data.srcRemote = srcRemote;
    data.dstFs = dstFs;
    data.dstRemote = dstRemote;
    data.mode = SYNC_UPDATE;
    data.running = false;
    data.jobId = -1;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&SyncDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_SYNC_DIALOG), hwndParent, SyncDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
