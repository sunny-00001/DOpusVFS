#include "TrashDialog.h"
#include "RcloneClient.h"
#include "DaemonManager.h"
#include "Utils.h"
#include "PathParser.h"
#include "resource.h"
#include "json.hpp"
#include <commctrl.h>
#include <strsafe.h>

#pragma comment(lib, "Comctl32.lib")

using json = nlohmann::json;

struct TrashItem {
    std::wstring name;
    std::wstring remote;
    bool isDir;
    uint64_t size;
    std::wstring modTime;
    std::wstring id;
};

struct TrashDlgData {
    std::wstring fs;
    std::wstring remoteType;
    std::vector<TrashItem> items;
    int selectedIndex;
    bool loading;
};

static void RefreshTrashList(HWND hDlg, TrashDlgData* data) {
    HWND hList = GetDlgItem(hDlg, IDC_TRASH_LIST);
    ListView_DeleteAllItems(hList);
    data->items.clear();
    data->selectedIndex = -1;

    SetDlgItemTextW(hDlg, IDC_TRASH_STATUS, L"正在加载回收站内容...");

    std::string remoteType = WideToUtf8(data->remoteType);
    std::string result;
    bool ok = false;

    if (remoteType == "drive") {
        ok = RcloneClient::BackendCommand(data->fs, "list",
            "{\"opt\":{\"drive-trashed-only\":\"true\"}}", result);
    } else {
        json req;
        req["fs"] = WideToUtf8(data->fs);
        req["remote"] = "";
        result = RcloneClient::SendHttpPostFull("/operations/list", req.dump());
        ok = !result.empty();
        if (ok) {
            try {
                auto j = json::parse(result);
                json trashedList = json::array();
                if (j.contains("list") && j["list"].is_array()) {
                    for (const auto& item : j["list"]) {
                        if (item.contains("IsTrashed") && item["IsTrashed"].is_boolean() && item["IsTrashed"].get<bool>()) {
                            trashedList.push_back(item);
                        }
                    }
                }
                result = trashedList.dump(2);
            } catch (...) {}
        }
    }

    if (!ok || result.empty()) {
        SetDlgItemTextW(hDlg, IDC_TRASH_STATUS, L"无法获取回收站内容");
        return;
    }

    try {
        auto j = json::parse(result);
        json items = j.is_array() ? j : (j.contains("list") ? j["list"] : json::array());
        int idx = 0;
        for (const auto& item : items) {
            TrashItem ti;
            ti.name = Utf8ToWide(item.value("Name", "?"));
            ti.remote = Utf8ToWide(item.value("Path", ""));
            ti.isDir = item.value("IsDir", false);
            ti.size = item.value<uint64_t>("Size", 0);
            ti.modTime = Utf8ToWide(item.value("ModTime", ""));
            ti.id = Utf8ToWide(item.value("ID", ""));

            LVITEMW lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = idx;
            lvi.iSubItem = 0;
            lvi.pszText = (LPWSTR)ti.name.c_str();
            ListView_InsertItem(hList, &lvi);

            std::wstring typeStr = ti.isDir ? L"文件夹" : L"文件";
            ListView_SetItemText(hList, idx, 1, (LPWSTR)typeStr.c_str());

            std::wstring sizeStr = ti.isDir ? L"-" : PathParser::FormatSize(ti.size);
            ListView_SetItemText(hList, idx, 2, (LPWSTR)sizeStr.c_str());

            std::wstring timeStr;
            if (!ti.modTime.empty()) {
                timeStr = ti.modTime.substr(0, 19);
                for (auto& c : timeStr) if (c == 'T') c = ' ';
            }
            ListView_SetItemText(hList, idx, 3, (LPWSTR)timeStr.c_str());

            data->items.push_back(std::move(ti));
            idx++;
        }

        wchar_t statusBuf[128];
        StringCchPrintfW(statusBuf, _countof(statusBuf), L"共 %d 个回收站项目", idx);
        SetDlgItemTextW(hDlg, IDC_TRASH_STATUS, statusBuf);
    } catch (...) {
        SetDlgItemTextW(hDlg, IDC_TRASH_STATUS, L"解析回收站数据失败");
    }
}

static void InitTrashListView(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_TRASH_LIST);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;

    col.pszText = (LPWSTR)L"名称";
    col.cx = 200;
    ListView_InsertColumn(hList, 0, &col);

    col.pszText = (LPWSTR)L"类型";
    col.cx = 60;
    ListView_InsertColumn(hList, 1, &col);

    col.pszText = (LPWSTR)L"大小";
    col.cx = 80;
    ListView_InsertColumn(hList, 2, &col);

    col.pszText = (LPWSTR)L"修改时间";
    col.cx = 140;
    ListView_InsertColumn(hList, 3, &col);
}

INT_PTR CALLBACK TrashDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        TrashDlgData* data = (TrashDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        InitTrashListView(hDlg);
        RefreshTrashList(hDlg, data);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDC_TRASH_RESTORE: {
            TrashDlgData* data = (TrashDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (data->selectedIndex < 0 || data->selectedIndex >= (int)data->items.size()) {
                MessageBoxW(hDlg, L"请先选择要恢复的项目",
                    L"提示", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            TrashItem& item = data->items[data->selectedIndex];
            int ret = MessageBoxW(hDlg,
                (L"确定恢复 \"" + item.name + L"\" ?").c_str(),
                L"恢复确认", MB_YESNO | MB_ICONQUESTION);
            if (ret == IDYES) {
                std::string remoteType = WideToUtf8(data->remoteType);
                bool ok = false;
                if (remoteType == "drive" && !item.id.empty()) {
                    std::string cmdResult;
                    ok = RcloneClient::BackendCommand(data->fs, "untrash",
                        "{\"fileId\":\"" + WideToUtf8(item.id) + "\"}", cmdResult);
                }
                if (ok) {
                    MessageBoxW(hDlg, L"恢复成功", L"提示", MB_OK | MB_ICONINFORMATION);
                    RefreshTrashList(hDlg, data);
                } else {
                    MessageBoxW(hDlg, L"恢复失败，可能不支持该操作",
                        L"错误", MB_OK | MB_ICONERROR);
                }
            }
            return TRUE;
        }

        case IDC_TRASH_DELETE: {
            TrashDlgData* data = (TrashDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            if (data->selectedIndex < 0 || data->selectedIndex >= (int)data->items.size()) {
                MessageBoxW(hDlg, L"请先选择要删除的项目",
                    L"提示", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            TrashItem& item = data->items[data->selectedIndex];
            int ret = MessageBoxW(hDlg,
                (L"彻底删除 \"" + item.name + L"\" ?\n此操作不可撤销!").c_str(),
                L"删除确认", MB_YESNO | MB_ICONWARNING);
            if (ret == IDYES) {
                std::string remoteType = WideToUtf8(data->remoteType);
                bool ok = false;
                if (remoteType == "drive" && !item.id.empty()) {
                    std::string cmdResult;
                    ok = RcloneClient::BackendCommand(data->fs, "delete",
                        "{\"fileId\":\"" + WideToUtf8(item.id) + "\"}", cmdResult);
                }
                if (ok) {
                    MessageBoxW(hDlg, L"删除成功", L"提示", MB_OK | MB_ICONINFORMATION);
                    RefreshTrashList(hDlg, data);
                } else {
                    MessageBoxW(hDlg, L"删除失败，可能不支持该操作",
                        L"错误", MB_OK | MB_ICONERROR);
                }
            }
            return TRUE;
        }

        case IDC_TRASH_EMPTY: {
            TrashDlgData* data = (TrashDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            int ret = MessageBoxW(hDlg,
                L"确定清空回收站?\n所有回收站文件将被彻底删除，此操作不可撤销!",
                L"清空确认", MB_YESNO | MB_ICONWARNING);
            if (ret == IDYES) {
                bool ok = RcloneClient::Cleanup(data->fs);
                if (ok) {
                    MessageBoxW(hDlg, L"清空成功", L"提示", MB_OK | MB_ICONINFORMATION);
                    RefreshTrashList(hDlg, data);
                } else {
                    MessageBoxW(hDlg, L"清空失败", L"错误", MB_OK | MB_ICONERROR);
                }
            }
            return TRUE;
        }
        }
        break;

    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->idFrom == IDC_TRASH_LIST) {
            if (pnm->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                TrashDlgData* data = (TrashDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
                if (pnmv->uNewState & LVIS_SELECTED) {
                    data->selectedIndex = pnmv->iItem;
                } else if (pnmv->uOldState & LVIS_SELECTED && data->selectedIndex == pnmv->iItem) {
                    data->selectedIndex = -1;
                }
            }
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hDlg, IDOK);
        return TRUE;
    }
    return FALSE;
}

bool ShowTrashDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remoteType) {
    TrashDlgData data = {};
    data.fs = fs;
    data.remoteType = remoteType;
    data.selectedIndex = -1;
    data.loading = false;

    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&TrashDlgProc, &hModule);

    INT_PTR result = DialogBoxParamW(hModule, MAKEINTRESOURCEW(IDD_TRASH_DIALOG),
        hwndParent, TrashDlgProc, (LPARAM)&data);
    return (result == IDOK);
}
