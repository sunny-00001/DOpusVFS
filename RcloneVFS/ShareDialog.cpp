#include "ShareDialog.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "Utils.h"
#include "PathParser.h"
#include "resource.h"
#include <shellapi.h>
#include <strsafe.h>
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

struct ShareDlgData {
    std::wstring fs;
    std::wstring remote;
    std::wstring url;
    std::wstring expire;
    bool generating;
    bool success;
};

static void SetStatus(HWND hDlg, const WCHAR* status) {
    SetDlgItemTextW(hDlg, IDC_SHARE_STATUS, status);
}

static void CopyToClipboard(HWND hDlg, const std::wstring& text) {
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
}

static DWORD WINAPI GenerateShareLinkThread(LPVOID param) {
    ShareDlgData* data = (ShareDlgData*)param;

    HWND hDlg = FindWindowW(NULL, L"分享链接");
    if (!hDlg) {
        hDlg = FindWindowW(NULL, L"Share Link");
    }

    if (hDlg) {
        SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETMARQUEE, (WPARAM)TRUE, 0);
        SetStatus(hDlg, L"正在生成链接...");
    }

    if (RcloneClient::PublicLink(data->fs, data->remote, data->url, data->expire)) {
        data->success = true;
        if (hDlg) {
            SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETMARQUEE, (WPARAM)FALSE, 0);
            SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETPOS, 100, 0);
            SetDlgItemTextW(hDlg, IDC_SHARE_URL, data->url.c_str());
            SetStatus(hDlg, L"链接已生成");
        }
    } else {
        data->success = false;
        if (hDlg) {
            SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETMARQUEE, (WPARAM)FALSE, 0);
            SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETPOS, 0, 0);
            std::wstring errMsg = data->url.empty() ? L"生成分享链接失败" : data->url;
            SetStatus(hDlg, errMsg.c_str());
        }
    }

    data->generating = false;
    return 0;
}

static void InitExpireCombo(HWND hDlg) {
    HWND hCombo = GetDlgItem(hDlg, IDC_SHARE_EXPIRE);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"永久有效 (默认)");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"1 小时");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"24 小时");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"7 天");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"30 天");
    SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
}

static std::wstring GetExpireValue(HWND hDlg) {
    HWND hCombo = GetDlgItem(hDlg, IDC_SHARE_EXPIRE);
    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    switch (sel) {
        case 1: return L"1h";
        case 2: return L"24h";
        case 3: return L"168h";
        case 4: return L"720h";
        default: return L"";
    }
}

static INT_PTR CALLBACK ShareDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ShareDlgData* s_data = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        s_data = (ShareDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)s_data);

        SetWindowTextW(hDlg, L"分享链接");
        SetDlgItemTextW(hDlg, IDC_SHARE_URL, L"");
        InitExpireCombo(hDlg);
        SetStatus(hDlg, L"点击下方按钮生成分享链接");
        SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(GetDlgItem(hDlg, IDC_SHARE_PROGRESS), PBM_SETPOS, 0, 0);

        EnableWindow(GetDlgItem(hDlg, IDC_SHARE_COPY_LINK), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_SHARE_OPEN_BROWSER), FALSE);

        std::wstring title = L"分享: " + PathParser::ExtractFsName(s_data->fs) + L":" + s_data->remote;
        SetWindowTextW(hDlg, title.c_str());
        return TRUE;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_SHARE_GENERATE) {
            if (s_data->generating) {
                SetStatus(hDlg, L"正在生成中...");
                return TRUE;
            }

            s_data->expire = GetExpireValue(hDlg);
            s_data->generating = true;
            s_data->success = false;
            s_data->url.clear();

            HANDLE hThread = CreateThread(nullptr, 0, GenerateShareLinkThread, s_data, 0, nullptr);
            if (hThread) CloseHandle(hThread);
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_SHARE_COPY_LINK) {
            if (s_data && !s_data->url.empty()) {
                CopyToClipboard(hDlg, s_data->url);
                SetStatus(hDlg, L"链接已复制到剪贴板");
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_SHARE_OPEN_BROWSER) {
            if (s_data && !s_data->url.empty()) {
                ShellExecuteW(NULL, L"open", s_data->url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDOK) {
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_USER + 100: {
        if (s_data && s_data->success && !s_data->url.empty()) {
            EnableWindow(GetDlgItem(hDlg, IDC_SHARE_COPY_LINK), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_SHARE_OPEN_BROWSER), TRUE);
        }
        return TRUE;
    }

    case WM_CLOSE: {
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}

bool ShowShareDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote) {
    ShareDlgData data;
    data.fs = fs;
    data.remote = remote;
    data.generating = false;
    data.success = false;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&ShareDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_SHARE_DIALOG), hwndParent, ShareDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
