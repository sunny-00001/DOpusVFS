#include "JobDialog.h"
#include "RcloneClient.h"
#include "PathParser.h"
#include "Utils.h"
#include "resource.h"
#include <strsafe.h>
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

struct JobDlgData {
    bool refreshPending;
};

static void RefreshJobList(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_JOB_LIST);
    ListView_DeleteAllItems(hList);

    std::vector<std::pair<int, RcloneJobStatus>> jobs;
    if (!RcloneClient::ListJobs(jobs)) {
        SetDlgItemTextW(hDlg, IDC_JOB_STATUS,
            L"无法获取任务列表");
        return;
    }

    if (jobs.empty()) {
        SetDlgItemTextW(hDlg, IDC_JOB_STATUS,
            L"当前无运行中任务");
        return;
    }

    int runningCount = 0;
    for (size_t i = 0; i < jobs.size(); i++) {
        int jobid = jobs[i].first;
        const RcloneJobStatus& st = jobs[i].second;

        WCHAR idText[32];
        StringCchPrintfW(idText, 32, L"%d", jobid);

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.iSubItem = 0;
        item.pszText = idText;
        ListView_InsertItem(hList, &item);

        // Status
        const WCHAR* statusText;
        if (st.finished) {
            statusText = st.success ? L"完成" : L"失败";
        } else {
            statusText = L"运行中";
            runningCount++;
        }
        ListView_SetItemText(hList, (int)i, 1, (LPWSTR)statusText);

        // Progress
        WCHAR progressText[32];
        if (st.finished) {
            StringCchPrintfW(progressText, 32, L"-");
        } else {
            int pct = (int)(st.progress * 100.0);
            StringCchPrintfW(progressText, 32, L"%d%%", pct);
        }
        ListView_SetItemText(hList, (int)i, 2, progressText);

        // Transferred / Total
        WCHAR sizeText[128];
        if (st.total > 0) {
            StringCchPrintfW(sizeText, 128, L"%s / %s",
                PathParser::FormatSize(st.transferred).c_str(),
                PathParser::FormatSize(st.total).c_str());
        } else if (st.transferred > 0) {
            StringCchPrintfW(sizeText, 128, L"%s",
                PathParser::FormatSize(st.transferred).c_str());
        } else {
            StringCchPrintfW(sizeText, 128, L"-");
        }
        ListView_SetItemText(hList, (int)i, 3, sizeText);

        // Speed
        WCHAR speedText[64];
        if (st.speed > 0) {
            StringCchPrintfW(speedText, 64, L"%s/s",
                PathParser::FormatSize(st.speed).c_str());
        } else {
            StringCchPrintfW(speedText, 64, L"-");
        }
        ListView_SetItemText(hList, (int)i, 4, speedText);

        // Error
        if (!st.error.empty()) {
            std::wstring wErr = Utf8ToWide(st.error);
            if (wErr.length() > 60) wErr = wErr.substr(0, 57) + L"...";
            ListView_SetItemText(hList, (int)i, 5, (LPWSTR)wErr.c_str());
        } else {
            ListView_SetItemText(hList, (int)i, 5, (LPWSTR)L"-");
        }
    }

    WCHAR statusLine[128];
    StringCchPrintfW(statusLine, 128,
        L"共 %d 个任务，%d 个运行中",
        (int)jobs.size(), runningCount);
    SetDlgItemTextW(hDlg, IDC_JOB_STATUS, statusLine);
}

static int GetSelectedJobId(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_JOB_LIST);
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return -1;

    WCHAR idText[32];
    ListView_GetItemText(hList, sel, 0, idText, 32);
    return _wtoi(idText);
}

static INT_PTR CALLBACK JobDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static JobDlgData* s_data = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        s_data = (JobDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)s_data);

        // Set up list view columns
        HWND hList = GetDlgItem(hDlg, IDC_JOB_LIST);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_LEFT;

        col.pszText = (LPWSTR)L"ID";
        col.cx = 50;
        ListView_InsertColumn(hList, 0, &col);

        col.pszText = (LPWSTR)L"状态";
        col.cx = 60;
        ListView_InsertColumn(hList, 1, &col);

        col.pszText = (LPWSTR)L"进度";
        col.cx = 60;
        ListView_InsertColumn(hList, 2, &col);

        col.pszText = (LPWSTR)L"已传输 / 总计";
        col.cx = 140;
        ListView_InsertColumn(hList, 3, &col);

        col.pszText = (LPWSTR)L"速度";
        col.cx = 80;
        ListView_InsertColumn(hList, 4, &col);

        col.pszText = (LPWSTR)L"错误";
        col.cx = 96;
        ListView_InsertColumn(hList, 5, &col);

        // Initial load
        RefreshJobList(hDlg);

        // Auto-refresh timer (3 seconds)
        SetTimer(hDlg, 1, 3000, NULL);
        return TRUE;
    }

    case WM_TIMER: {
        if (wParam == 1) {
            RefreshJobList(hDlg);
        }
        return TRUE;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_JOB_REFRESH:
            RefreshJobList(hDlg);
            return TRUE;

        case IDC_JOB_STOP: {
            int jobid = GetSelectedJobId(hDlg);
            if (jobid < 0) {
                MessageBoxW(hDlg, L"请先选择一个任务",
                    L"提示", MB_ICONINFORMATION | MB_OK);
                return TRUE;
            }
            WCHAR msg[128];
            StringCchPrintfW(msg, 128,
                L"确定要停止任务 #%d 吗?", jobid);
            if (MessageBoxW(hDlg, msg,
                    L"停止任务", MB_YESNO | MB_ICONWARNING) != IDYES) {
                return TRUE;
            }
            if (RcloneClient::StopJob(jobid)) {
                Sleep(500);
                RefreshJobList(hDlg);
            } else {
                MessageBoxW(hDlg, L"停止任务失败",
                    L"错误", MB_ICONWARNING | MB_OK);
            }
            return TRUE;
        }

        case IDC_JOB_STOP_ALL: {
            if (MessageBoxW(hDlg,
                    L"确定要停止所有运行中任务吗?",
                    L"全部停止", MB_YESNO | MB_ICONWARNING) != IDYES) {
                return TRUE;
            }
            std::vector<std::pair<int, RcloneJobStatus>> jobs;
            if (RcloneClient::ListJobs(jobs)) {
                for (const auto& j : jobs) {
                    if (!j.second.finished) {
                        RcloneClient::StopJob(j.first);
                    }
                }
                Sleep(500);
                RefreshJobList(hDlg);
            }
            return TRUE;
        }

        case IDOK:
            KillTimer(hDlg, 1);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            KillTimer(hDlg, 1);
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        KillTimer(hDlg, 1);
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

bool ShowJobDialog(HWND hwndParent) {
    JobDlgData data = {};
    data.refreshPending = false;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&JobDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_JOB_DIALOG),
        hwndParent, JobDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
