#include "VersionsDialog.h"
#include "RcloneClient.h"
#include "RcloneCache.h"
#include "Utils.h"
#include "PathParser.h"
#include "resource.h"
#include "json.hpp"
#include <commctrl.h>
#include <strsafe.h>

#pragma comment(lib, "Comctl32.lib")

using json = nlohmann::json;

struct VersionsDlgData {
    std::wstring fs;
    std::wstring remote;
    std::vector<RcloneVersionInfo> versions;
    int selectedIndex;
    bool loading;
};

static void SetStatus(HWND hDlg, const WCHAR* status) {
    SetDlgItemTextW(hDlg, IDC_VERSION_STATUS, status);
}

static void InitListView(HWND hList) {
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = (LPWSTR)L"版本 ID";
    lvc.cx = 180;
    ListView_InsertColumn(hList, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = (LPWSTR)L"修改时间";
    lvc.cx = 160;
    ListView_InsertColumn(hList, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = (LPWSTR)L"大小";
    lvc.cx = 100;
    ListView_InsertColumn(hList, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.pszText = (LPWSTR)L"状态";
    lvc.cx = 60;
    ListView_InsertColumn(hList, 3, &lvc);

    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

static void PopulateListView(HWND hList, const std::vector<RcloneVersionInfo>& versions) {
    ListView_DeleteAllItems(hList);

    LVITEMW lvi = {};
    for (size_t i = 0; i < versions.size(); i++) {
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_TEXT;
        std::wstring idDisplay = versions[i].id.empty() ? L"-" : versions[i].id;
        lvi.pszText = (LPWSTR)idDisplay.c_str();
        ListView_InsertItem(hList, &lvi);

        lvi.iSubItem = 1;
        std::wstring timeDisplay = versions[i].modTime;
        if (timeDisplay.length() > 19) {
            timeDisplay = timeDisplay.substr(0, 19);
            for (auto& c : timeDisplay) if (c == L'T') c = L' ';
        }
        lvi.pszText = (LPWSTR)timeDisplay.c_str();
        ListView_SetItem(hList, &lvi);

        lvi.iSubItem = 2;
        std::wstring sizeStr = PathParser::FormatSize(versions[i].size);
        lvi.pszText = (LPWSTR)sizeStr.c_str();
        ListView_SetItem(hList, &lvi);

        lvi.iSubItem = 3;
        lvi.pszText = versions[i].isCurrent ? (LPWSTR)L"当前" : (LPWSTR)L"";
        ListView_SetItem(hList, &lvi);
    }
}

static DWORD WINAPI LoadVersionsThread(LPVOID param) {
    VersionsDlgData* data = (VersionsDlgData*)param;

    HWND hDlg = FindWindowW(NULL, L"版本历史");
    if (!hDlg) hDlg = FindWindowW(NULL, L"Versions");

    if (hDlg) {
        SetStatus(hDlg, L"正在获取版本列表...");
    }

    std::vector<RcloneVersionInfo> versions;
    bool success = RcloneClient::GetVersions(data->fs, data->remote, versions);

    if (hDlg) {
        if (success && !versions.empty()) {
            data->versions = versions;
            PopulateListView(GetDlgItem(hDlg, IDC_VERSION_LIST), versions);
            std::wstring status = L"共找到 " + std::to_wstring(versions.size()) + L" 个版本";
            SetStatus(hDlg, status.c_str());
        } else {
            SetStatus(hDlg, L"此文件暂无版本历史或不支持版本管理");
        }
        data->loading = false;
    }

    return 0;
}

static INT_PTR CALLBACK VersionsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static VersionsDlgData* s_data = nullptr;
    static HWND s_hList = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        s_data = (VersionsDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)s_data);

        s_hList = GetDlgItem(hDlg, IDC_VERSION_LIST);
        InitListView(s_hList);

        std::wstring title = L"版本历史: " + PathParser::ExtractFsName(s_data->fs) + L":" + s_data->remote;
        SetWindowTextW(hDlg, title.c_str());

        EnableWindow(GetDlgItem(hDlg, IDC_VERSION_PREVIEW), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_VERSION_RESTORE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_VERSION_DELETE), FALSE);

        s_data->loading = true;
        s_data->selectedIndex = -1;
        HANDLE hThread = CreateThread(nullptr, 0, LoadVersionsThread, s_data, 0, nullptr);
        if (hThread) CloseHandle(hThread);

        return TRUE;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_VERSION_PREVIEW) {
            if (s_data && s_data->selectedIndex >= 0 && s_data->selectedIndex < (int)s_data->versions.size()) {
                const RcloneVersionInfo& vi = s_data->versions[s_data->selectedIndex];
                std::wstring msg = L"版本 ID: " + vi.id + L"\n";
                msg += L"修改时间: " + vi.modTime + L"\n";
                msg += L"大小: " + PathParser::FormatSize(vi.size) + L"\n";
                msg += vi.isCurrent ? L"状态: 当前版本" : L"状态: 历史版本";
                MessageBoxW(hDlg, msg.c_str(), L"版本详情", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_VERSION_RESTORE) {
            if (s_data && s_data->selectedIndex >= 0 && s_data->selectedIndex < (int)s_data->versions.size()) {
                const RcloneVersionInfo& vi = s_data->versions[s_data->selectedIndex];
                if (vi.isCurrent) {
                    MessageBoxW(hDlg, L"无法恢复当前版本", L"提示", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }

                std::wstring msg = L"确定要恢复此版本吗?\n\n";
                msg += L"版本 ID: " + vi.id + L"\n";
                msg += L"修改时间: " + vi.modTime + L"\n";
                msg += L"大小: " + PathParser::FormatSize(vi.size);

                if (MessageBoxW(hDlg, msg.c_str(), L"恢复版本", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    SetStatus(hDlg, L"正在恢复版本...");
                    if (RcloneClient::RestoreVersion(s_data->fs, s_data->remote, vi.id)) {
                        MessageBoxW(hDlg, L"版本恢复成功", L"成功", MB_OK | MB_ICONINFORMATION);
                        s_data->loading = true;
                        HANDLE hThread = CreateThread(nullptr, 0, LoadVersionsThread, s_data, 0, nullptr);
                        if (hThread) CloseHandle(hThread);
                    } else {
                        MessageBoxW(hDlg, L"版本恢复失败，请检查网络连接和 rclone 状态",
                            L"错误", MB_OK | MB_ICONERROR);
                        SetStatus(hDlg, L"恢复失败");
                    }
                }
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_VERSION_DELETE) {
            if (s_data && s_data->selectedIndex >= 0 && s_data->selectedIndex < (int)s_data->versions.size()) {
                const RcloneVersionInfo& vi = s_data->versions[s_data->selectedIndex];
                if (vi.isCurrent) {
                    MessageBoxW(hDlg, L"无法删除当前版本", L"提示", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }

                std::wstring msg = L"确定要删除此版本吗? 此操作不可撤销!\n\n";
                msg += L"版本 ID: " + vi.id + L"\n";
                msg += L"修改时间: " + vi.modTime + L"\n";
                msg += L"大小: " + PathParser::FormatSize(vi.size);

                if (MessageBoxW(hDlg, msg.c_str(), L"删除版本", MB_YESNO | MB_ICONWARNING) == IDYES) {
                    SetStatus(hDlg, L"正在删除版本...");
                    if (RcloneClient::DeleteVersion(s_data->fs, s_data->remote, vi.id)) {
                        MessageBoxW(hDlg, L"版本删除成功", L"成功", MB_OK | MB_ICONINFORMATION);
                        s_data->loading = true;
                        HANDLE hThread = CreateThread(nullptr, 0, LoadVersionsThread, s_data, 0, nullptr);
                        if (hThread) CloseHandle(hThread);
                    } else {
                        MessageBoxW(hDlg, L"版本删除失败，请检查网络连接和 rclone 状态",
                            L"错误", MB_OK | MB_ICONERROR);
                        SetStatus(hDlg, L"删除失败");
                    }
                }
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

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == IDC_VERSION_LIST && pnmh->code == LVN_ITEMCHANGED) {
            LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
            if (pnmv->uNewState & LVIS_SELECTED) {
                s_data->selectedIndex = pnmv->iItem;
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_PREVIEW), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_RESTORE), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_DELETE), TRUE);
            } else if (pnmv->uOldState & LVIS_SELECTED && s_data->selectedIndex == pnmv->iItem) {
                s_data->selectedIndex = -1;
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_PREVIEW), FALSE);
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_RESTORE), FALSE);
                EnableWindow(GetDlgItem(hDlg, IDC_VERSION_DELETE), FALSE);
            }
        }
        break;
    }

    case WM_CLOSE: {
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}

bool ShowVersionsDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote) {
    VersionsDlgData data;
    data.fs = fs;
    data.remote = remote;
    data.selectedIndex = -1;
    data.loading = false;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&VersionsDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_VERSIONS_DIALOG), hwndParent, VersionsDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
