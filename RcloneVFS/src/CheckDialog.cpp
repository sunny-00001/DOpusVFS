#include "CheckDialog.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "Utils.h"
#include "PathParser.h"
#include "resource.h"
#include <strsafe.h>
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

struct CheckDlgData {
    std::wstring srcFs;
    std::wstring srcRemote;
    std::wstring dstFs;
    std::wstring dstRemote;
    bool oneWay;
    bool running;
    int jobId;
};

static void SetStatus(HWND hDlg, const WCHAR* status) {
    SetDlgItemTextW(hDlg, IDC_CHECK_STATUS, status);
}

static void AppendReport(HWND hDlg, const WCHAR* text) {
    HWND hReport = GetDlgItem(hDlg, IDC_CHECK_REPORT);
    int len = GetWindowTextLengthW(hReport);
    SendMessageW(hReport, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hReport, EM_REPLACESEL, FALSE, (LPARAM)text);
}

static void ClearReport(HWND hDlg) {
    SetDlgItemTextW(hDlg, IDC_CHECK_REPORT, L"");
}

static DWORD WINAPI CheckThreadProc(LPVOID param) {
    CheckDlgData* data = (CheckDlgData*)param;

    HWND hDlg = FindWindowW(NULL, L"检验操作");
    if (!hDlg) hDlg = FindWindowW(NULL, L"Check");

    if (hDlg) {
        ClearReport(hDlg);
        AppendReport(hDlg, L"开始检验...\r\n");
        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_PROGRESS), PBM_SETMARQUEE, (WPARAM)TRUE, 0);
        SetStatus(hDlg, L"正在检验...");
    }

    std::string reqJson = "{\"srcFs\":\"" + WideToUtf8(data->srcFs + data->srcRemote) +
                          "\",\"dstFs\":\"" + WideToUtf8(data->dstFs + data->dstRemote) +
                          "\",\"oneWay\":" + (data->oneWay ? "true" : "false") + "}";

    std::string res = RcloneClient::SendHttpPost("/operations/check", reqJson);

    if (hDlg) {
        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_PROGRESS), PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_PROGRESS), PBM_SETPOS, 100, 0);

        if (!res.empty()) {
            AppendReport(hDlg, L"检验完成\r\n");
            AppendReport(hDlg, Utf8ToWide(res).c_str());
            SetStatus(hDlg, L"检验完成");
        } else {
            AppendReport(hDlg, L"检验失败\r\n");
            AppendReport(hDlg, res.empty() ? L"无法获取检验结果" : Utf8ToWide(res).c_str());
            SetStatus(hDlg, L"检验失败");
        }

        EnableWindow(GetDlgItem(hDlg, IDC_CHECK_START), FALSE);
    }

    data->running = false;
    return 0;
}

static INT_PTR CALLBACK CheckDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static CheckDlgData* s_data = nullptr;
    static HANDLE s_hThread = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        s_data = (CheckDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)s_data);

        SetWindowTextW(hDlg, L"检验操作");

        std::wstring srcPath = PathParser::ExtractFsName(s_data->srcFs) + L":" + s_data->srcRemote;
        std::wstring dstPath = PathParser::ExtractFsName(s_data->dstFs) + L":" + s_data->dstRemote;
        SetDlgItemTextW(hDlg, IDC_CHECK_SRC_PATH, srcPath.c_str());
        SetDlgItemTextW(hDlg, IDC_CHECK_DST_PATH, dstPath.c_str());

        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_ONEWAY), BM_SETCHECK, BST_CHECKED, 0);

        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(GetDlgItem(hDlg, IDC_CHECK_PROGRESS), PBM_SETPOS, 0, 0);
        SetStatus(hDlg, L"请点击开始进行检验");
        ClearReport(hDlg);
        AppendReport(hDlg, L"源路径: ");
        AppendReport(hDlg, srcPath.c_str());
        AppendReport(hDlg, L"\r\n");
        AppendReport(hDlg, L"目标路径: ");
        AppendReport(hDlg, dstPath.c_str());
        AppendReport(hDlg, L"\r\n");

        return TRUE;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_CHECK_START) {
            if (s_data->running) {
                SetStatus(hDlg, L"检验进行中...");
                return TRUE;
            }

            s_data->oneWay = (SendMessageW(GetDlgItem(hDlg, IDC_CHECK_ONEWAY), BM_GETCHECK, 0, 0) == BST_CHECKED);

            s_data->running = true;
            s_hThread = CreateThread(nullptr, 0, CheckThreadProc, s_data, 0, nullptr);
            if (s_hThread) {
                CloseHandle(s_hThread);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDOK) {
            if (s_data->running) {
                SetStatus(hDlg, L"请先等待检验完成");
                return TRUE;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL) {
            if (s_data->running) {
                if (MessageBoxW(hDlg, L"确定要取消检验吗?",
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
            SetStatus(hDlg, L"请先等待检验完成");
            return TRUE;
        }
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}

bool ShowCheckDialog(HWND hwndParent, const std::wstring& srcFs, const std::wstring& srcRemote,
                    const std::wstring& dstFs, const std::wstring& dstRemote) {
    CheckDlgData data = {};
    data.srcFs = srcFs;
    data.srcRemote = srcRemote;
    data.dstFs = dstFs;
    data.dstRemote = dstRemote;
    data.oneWay = true;
    data.running = false;
    data.jobId = -1;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&CheckDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_CHECK_DIALOG), hwndParent, CheckDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
