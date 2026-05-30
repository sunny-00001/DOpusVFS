#include <windows.h>
#include <strsafe.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <aclapi.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#define DOPUS_PLUGIN_HELPER
#include "vfs_plugins.h"
#include "plugin_support.h"
#include "resource.h"

#pragma comment(lib, "Shell32.lib")

static HMODULE g_hModule = NULL;

static void NormalizePath(std::wstring& path) {
    for (auto& c : path) { if (c == L'\\') c = L'/'; }
    while (path.length() > 6 && path[6] == L'/') {
        path.erase(6, 1);
    }
    size_t pos;
    while ((pos = path.find(L"//", 6)) != std::wstring::npos) {
        path.erase(pos, 1);
    }
    while (path.length() > 6 && path.back() == L'/') path.pop_back();
}

static void ToRegPath(std::wstring& path) {
    for (auto& c : path) { if (c == L'/') c = L'\\'; }
}

static bool IsRegVfsPath(LPCWSTR path) {
    if (!path) return false;
    return _wcsnicmp(path, L"reg://", 6) == 0;
}

struct RegVFSConfig {
    bool showDefaultValues;
    bool readOnlyMode;
    bool confirmDelete;
    bool showHiddenKeys;
    int valueDisplayFormat;
    DWORD maxValueSize;
    bool autoRefresh;
    int refreshInterval;
    bool expandEnvVars;
    bool showBinaryAsHex;
    bool wow64Redirection;
    bool showKeyIcons;
    bool autoExpandKeys;
    int doubleClickAction;
    int sortOrder;
    int maxDepth;
    bool caseSensitive;
    bool showSizeColumn;
    bool showTypeColumn;
    int logLevel;
    bool cacheEnabled;
    std::wstring cacheDir;
    std::wstring tempDir;
    bool autoCleanTemp;
    int tempFileAge;
    int exportFormat;
    bool remoteRegistry;
    std::wstring remoteServer;
    bool showInFolderTree;

    RegVFSConfig() {
        showDefaultValues = true;
        readOnlyMode = false;
        confirmDelete = true;
        showHiddenKeys = true;
        valueDisplayFormat = 0;
        maxValueSize = 1048576;
        autoRefresh = false;
        refreshInterval = 5;
        expandEnvVars = true;
        showBinaryAsHex = true;
        wow64Redirection = false;
        showKeyIcons = true;
        autoExpandKeys = false;
        doubleClickAction = 0;
        sortOrder = 0;
        maxDepth = 0;
        caseSensitive = false;
        showSizeColumn = true;
        showTypeColumn = true;
        logLevel = 1;
        cacheEnabled = false;
        autoCleanTemp = true;
        tempFileAge = 60;
        exportFormat = 0;
        remoteRegistry = false;
        showInFolderTree = true;
    }
};

static RegVFSConfig g_config;
static std::set<std::wstring> g_favorites;
static std::wstring g_searchResults;
static bool g_searchCancelled = false;

enum ClipboardOp { CLIP_NONE, CLIP_COPY, CLIP_CUT };
struct ClipboardEntry
{
    std::wstring vfsPath;
    bool isKey;
};
static ClipboardOp g_clipOp = CLIP_NONE;
static std::vector<ClipboardEntry> g_clipEntries;

struct SearchParams
{
    std::wstring searchTerm;
    bool searchKeyNames;
    bool searchValueNames;
    bool searchValueData;
    bool caseSensitive;
    HWND hWndParent;
    HWND hProgressDlg;
};

static void LoadConfig() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegVFS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0, size = sizeof(DWORD);
        WCHAR strVal[MAX_PATH];
        DWORD strSize = MAX_PATH * sizeof(WCHAR);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowDefaultValues", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showDefaultValues = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ReadOnlyMode", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.readOnlyMode = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ConfirmDelete", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.confirmDelete = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowHiddenKeys", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showHiddenKeys = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ValueDisplayFormat", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.valueDisplayFormat = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"MaxValueSize", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.maxValueSize = val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AutoRefresh", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.autoRefresh = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"RefreshInterval", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.refreshInterval = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ExpandEnvVars", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.expandEnvVars = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowBinaryAsHex", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showBinaryAsHex = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"WOW64Redirection", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.wow64Redirection = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowKeyIcons", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showKeyIcons = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AutoExpandKeys", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.autoExpandKeys = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"DoubleClickAction", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.doubleClickAction = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"SortOrder", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.sortOrder = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"MaxDepth", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.maxDepth = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"CaseSensitive", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.caseSensitive = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowSizeColumn", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showSizeColumn = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowTypeColumn", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showTypeColumn = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"LogLevel", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.logLevel = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"CacheEnabled", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.cacheEnabled = (val != 0);

        strSize = MAX_PATH * sizeof(WCHAR);
        if (RegQueryValueExW(hKey, L"CacheDir", NULL, NULL, (LPBYTE)strVal, &strSize) == ERROR_SUCCESS)
            g_config.cacheDir = strVal;

        strSize = MAX_PATH * sizeof(WCHAR);
        if (RegQueryValueExW(hKey, L"TempDir", NULL, NULL, (LPBYTE)strVal, &strSize) == ERROR_SUCCESS)
            g_config.tempDir = strVal;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AutoCleanTemp", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.autoCleanTemp = (val != 0);

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"TempFileAge", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.tempFileAge = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ExportFormat", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.exportFormat = (int)val;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"RemoteRegistry", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.remoteRegistry = (val != 0);

        strSize = MAX_PATH * sizeof(WCHAR);
        if (RegQueryValueExW(hKey, L"RemoteServer", NULL, NULL, (LPBYTE)strVal, &strSize) == ERROR_SUCCESS)
            g_config.remoteServer = strVal;

        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ShowInFolderTree", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS)
            g_config.showInFolderTree = (val != 0);

        RegCloseKey(hKey);
    }
}

static void SaveConfig() {
    HKEY hKey;
    DWORD dwDisp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\RegVFS", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisp) == ERROR_SUCCESS) {
        DWORD val = g_config.showDefaultValues ? 1 : 0;
        RegSetValueExW(hKey, L"ShowDefaultValues", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.readOnlyMode ? 1 : 0;
        RegSetValueExW(hKey, L"ReadOnlyMode", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.confirmDelete ? 1 : 0;
        RegSetValueExW(hKey, L"ConfirmDelete", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.showHiddenKeys ? 1 : 0;
        RegSetValueExW(hKey, L"ShowHiddenKeys", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.valueDisplayFormat;
        RegSetValueExW(hKey, L"ValueDisplayFormat", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.maxValueSize;
        RegSetValueExW(hKey, L"MaxValueSize", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.autoRefresh ? 1 : 0;
        RegSetValueExW(hKey, L"AutoRefresh", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.refreshInterval;
        RegSetValueExW(hKey, L"RefreshInterval", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.expandEnvVars ? 1 : 0;
        RegSetValueExW(hKey, L"ExpandEnvVars", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.showBinaryAsHex ? 1 : 0;
        RegSetValueExW(hKey, L"ShowBinaryAsHex", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.wow64Redirection ? 1 : 0;
        RegSetValueExW(hKey, L"WOW64Redirection", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.showKeyIcons ? 1 : 0;
        RegSetValueExW(hKey, L"ShowKeyIcons", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.autoExpandKeys ? 1 : 0;
        RegSetValueExW(hKey, L"AutoExpandKeys", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.doubleClickAction;
        RegSetValueExW(hKey, L"DoubleClickAction", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.sortOrder;
        RegSetValueExW(hKey, L"SortOrder", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.maxDepth;
        RegSetValueExW(hKey, L"MaxDepth", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.caseSensitive ? 1 : 0;
        RegSetValueExW(hKey, L"CaseSensitive", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.showSizeColumn ? 1 : 0;
        RegSetValueExW(hKey, L"ShowSizeColumn", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.showTypeColumn ? 1 : 0;
        RegSetValueExW(hKey, L"ShowTypeColumn", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.logLevel;
        RegSetValueExW(hKey, L"LogLevel", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.cacheEnabled ? 1 : 0;
        RegSetValueExW(hKey, L"CacheEnabled", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegSetValueExW(hKey, L"CacheDir", 0, REG_SZ, (const BYTE*)g_config.cacheDir.c_str(), (DWORD)((g_config.cacheDir.length() + 1) * sizeof(WCHAR)));
        RegSetValueExW(hKey, L"TempDir", 0, REG_SZ, (const BYTE*)g_config.tempDir.c_str(), (DWORD)((g_config.tempDir.length() + 1) * sizeof(WCHAR)));
        val = g_config.autoCleanTemp ? 1 : 0;
        RegSetValueExW(hKey, L"AutoCleanTemp", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.tempFileAge;
        RegSetValueExW(hKey, L"TempFileAge", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = (DWORD)g_config.exportFormat;
        RegSetValueExW(hKey, L"ExportFormat", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        val = g_config.remoteRegistry ? 1 : 0;
        RegSetValueExW(hKey, L"RemoteRegistry", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegSetValueExW(hKey, L"RemoteServer", 0, REG_SZ, (const BYTE*)g_config.remoteServer.c_str(), (DWORD)((g_config.remoteServer.length() + 1) * sizeof(WCHAR)));
        val = g_config.showInFolderTree ? 1 : 0;
        RegSetValueExW(hKey, L"ShowInFolderTree", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int g_currentPage = 0;
enum { PAGE_GENERAL = 0, PAGE_DISPLAY = 1, PAGE_COUNT = 2 };

static const int g_pageControls[][10] = {
    { IDC_READONLY_MODE, IDC_CONFIRM_DELETE, IDC_SHOW_DEFAULT_VALUES, IDC_AUTO_REFRESH, 0 },
    { IDC_EXPAND_ENV_VARS, IDC_SHOW_BINARY_AS_HEX, IDC_CASE_SENSITIVE, IDC_SHOW_SIZE_COLUMN, IDC_SHOW_TYPE_COLUMN, 0 },
};

static void ShowNavPage(HWND hDlg, int page)
{
    g_currentPage = page;
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        const int* ctrls = g_pageControls[i];
        BOOL show = (i == page) ? TRUE : FALSE;
        for (int j = 0; ctrls[j] != 0; j++)
        {
            HWND hwnd = GetDlgItem(hDlg, ctrls[j]);
            if (hwnd) ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        }
    }
}

static void SetChineseText(HWND hDlg)
{
    SetWindowTextW(hDlg, L"\u6CE8\u518C\u8868 VFS \u914D\u7F6E");
    HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
    if (hList)
    {
        SendMessageW(hList, LB_SETITEMHEIGHT, 0, 24);
        SendMessageW(hList, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < PAGE_COUNT; i++)
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
    }

    SetDlgItemTextW(hDlg, IDOK, L"\u786E\u5B9A");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\u53D6\u6D88");
    SetDlgItemTextW(hDlg, IDC_APPLY, L"\u5E94\u7528");
    SetDlgItemTextW(hDlg, IDC_OPEN_REGEDIT, L"\u6253\u5F00\u6CE8\u518C\u8868");
    SetDlgItemTextW(hDlg, IDC_RESET_DEFAULTS, L"\u6062\u590D\u9ED8\u8BA4");

    SetDlgItemTextW(hDlg, IDC_READONLY_MODE, L"\u53EA\u8BFB\u6A21\u5F0F\uFF08\u7981\u7528\u5199\u5165/\u5220\u9664\uFF09");
    SetDlgItemTextW(hDlg, IDC_CONFIRM_DELETE, L"\u5220\u9664\u524D\u786E\u8BA4");
    SetDlgItemTextW(hDlg, IDC_SHOW_DEFAULT_VALUES, L"\u663E\u793A\u9ED8\u8BA4\u503C (Default)");
    SetDlgItemTextW(hDlg, IDC_AUTO_REFRESH, L"\u81EA\u52A8\u5237\u65B0");

    SetDlgItemTextW(hDlg, IDC_EXPAND_ENV_VARS, L"\u5C55\u5F00\u73AF\u5883\u53D8\u91CF (REG_EXPAND_SZ)");
    SetDlgItemTextW(hDlg, IDC_SHOW_BINARY_AS_HEX, L"\u4E8C\u8FDB\u5236\u6570\u636E\u663E\u793A\u4E3A\u5341\u516D\u8FDB\u5236");
    SetDlgItemTextW(hDlg, IDC_CASE_SENSITIVE, L"\u533A\u5206\u5927\u5C0F\u5199\u641C\u7D22");
    SetDlgItemTextW(hDlg, IDC_SHOW_SIZE_COLUMN, L"\u663E\u793A\u5927\u5C0F\u5217");
    SetDlgItemTextW(hDlg, IDC_SHOW_TYPE_COLUMN, L"\u663E\u793A\u7C7B\u578B\u5217");
}

static void InitDialogControls(HWND hDlg)
{
    SetChineseText(hDlg);
    CheckDlgButton(hDlg, IDC_READONLY_MODE, g_config.readOnlyMode ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CONFIRM_DELETE, g_config.confirmDelete ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_DEFAULT_VALUES, g_config.showDefaultValues ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_AUTO_REFRESH, g_config.autoRefresh ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_EXPAND_ENV_VARS, g_config.expandEnvVars ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_BINARY_AS_HEX, g_config.showBinaryAsHex ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CASE_SENSITIVE, g_config.caseSensitive ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_SIZE_COLUMN, g_config.showSizeColumn ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOW_TYPE_COLUMN, g_config.showTypeColumn ? BST_CHECKED : BST_UNCHECKED);
    ShowNavPage(hDlg, PAGE_GENERAL);
}

static bool SaveDialogControls(HWND hDlg)
{
    g_config.readOnlyMode = (IsDlgButtonChecked(hDlg, IDC_READONLY_MODE) == BST_CHECKED);
    g_config.confirmDelete = (IsDlgButtonChecked(hDlg, IDC_CONFIRM_DELETE) == BST_CHECKED);
    g_config.showDefaultValues = (IsDlgButtonChecked(hDlg, IDC_SHOW_DEFAULT_VALUES) == BST_CHECKED);
    g_config.autoRefresh = (IsDlgButtonChecked(hDlg, IDC_AUTO_REFRESH) == BST_CHECKED);
    g_config.expandEnvVars = (IsDlgButtonChecked(hDlg, IDC_EXPAND_ENV_VARS) == BST_CHECKED);
    g_config.showBinaryAsHex = (IsDlgButtonChecked(hDlg, IDC_SHOW_BINARY_AS_HEX) == BST_CHECKED);
    g_config.caseSensitive = (IsDlgButtonChecked(hDlg, IDC_CASE_SENSITIVE) == BST_CHECKED);
    g_config.showSizeColumn = (IsDlgButtonChecked(hDlg, IDC_SHOW_SIZE_COLUMN) == BST_CHECKED);
    g_config.showTypeColumn = (IsDlgButtonChecked(hDlg, IDC_SHOW_TYPE_COLUMN) == BST_CHECKED);
    SaveConfig();
    return true;
}

static void ResetToDefaults(HWND hDlg)
{
    g_config = RegVFSConfig();
    InitDialogControls(hDlg);
    MessageBoxW(hDlg, L"\u5DF2\u6062\u590D\u9ED8\u8BA4\u8BBE\u7F6E\u3002", L"\u63D0\u793A", MB_OK | MB_ICONINFORMATION);
}

INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            LoadConfig();
            InitDialogControls(hDlg);
            return (INT_PTR)TRUE;

        case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
            if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_NAV_LIST)
                lpmis->itemHeight = 24;
            return (INT_PTR)TRUE;
        }

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
            if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_NAV_LIST)
            {
                static const WCHAR* navLabels[] = { L"\u5E38\u89C4\u8BBE\u7F6E", L"\u663E\u793A\u8BBE\u7F6E" };
                int idx = (int)lpdis->itemID;
                if (idx >= 0 && idx < PAGE_COUNT)
                {
                    BOOL selected = (lpdis->itemState & ODS_SELECTED);
                    HBRUSH hBrush = selected ? CreateSolidBrush(RGB(0, 120, 215)) : GetSysColorBrush(COLOR_WINDOW);
                    FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
                    if (selected) DeleteObject(hBrush);
                    SetTextColor(lpdis->hDC, selected ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT));
                    SetBkMode(lpdis->hDC, TRANSPARENT);
                    RECT rcText = lpdis->rcItem;
                    rcText.left += 4;
                    rcText.right -= 4;
                    DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (SaveDialogControls(hDlg)) EndDialog(hDlg, IDOK);
                    return (INT_PTR)TRUE;
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
                case IDC_APPLY:
                    SaveDialogControls(hDlg);
                    return (INT_PTR)TRUE;
                case IDC_NAV_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        HWND hList = GetDlgItem(hDlg, IDC_NAV_LIST);
                        int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < PAGE_COUNT)
                            ShowNavPage(hDlg, sel);
                    }
                    return (INT_PTR)TRUE;
                case IDC_OPEN_REGEDIT:
                    ShellExecuteW(hDlg, L"open", L"regedit.exe", NULL, NULL, SW_SHOWNORMAL);
                    return (INT_PTR)TRUE;
                case IDC_RESET_DEFAULTS:
                    ResetToDefaults(hDlg);
                    return (INT_PTR)TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

static const GUID GUIDPlugin_Registry = 
{ 0x1A2B3C4D, 0x5E6F, 0x7A8B, { 0x9C, 0x0D, 0x1E, 0x2F, 0x3A, 0x4B, 0x5C, 0x6D } };

static const std::map<std::wstring, HKEY> g_rootKeys = {
    {L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
    {L"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
    {L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
    {L"HKEY_USERS", HKEY_USERS},
    {L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG}
};

#define NUM_CUSTOM_COLUMNS 8

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str)
{
    if (!hHeap) return NULL;
    size_t len = str.length() + 1;
    LPWSTR buf = (LPWSTR)HeapAlloc(hHeap, 0, len * sizeof(WCHAR));
    if (buf) StringCchCopyW(buf, len, str.c_str());
    return buf;
}

static std::wstring GetRegTypeName(DWORD type)
{
    switch (type) {
        case REG_NONE: return L"REG_NONE";
        case REG_SZ: return L"REG_SZ";
        case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
        case REG_BINARY: return L"REG_BINARY";
        case REG_DWORD: return L"REG_DWORD";
        case REG_DWORD_BIG_ENDIAN: return L"REG_DWORD_BIG_ENDIAN";
        case REG_LINK: return L"REG_LINK";
        case REG_MULTI_SZ: return L"REG_MULTI_SZ";
        case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
        case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
        case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
        case REG_QWORD: return L"REG_QWORD";
        default: {
            WCHAR buf[16];
            StringCchPrintfW(buf, 16, L"0x%08X", type);
            return std::wstring(buf);
        }
    }
}

static std::wstring FormatValuePreview(DWORD valueType, const BYTE* data, DWORD dataSize)
{
    if (!data || dataSize == 0) return L"(空)";
    
    switch (valueType) {
        case REG_SZ:
        case REG_EXPAND_SZ:
            if (dataSize >= sizeof(WCHAR))
                return std::wstring((LPCWSTR)data, dataSize / sizeof(WCHAR) - 1);
            return L"(无效字符串)";
            
        case REG_DWORD:
            if (dataSize >= sizeof(DWORD)) {
                DWORD val = *(DWORD*)data;
                WCHAR buf[64];
                StringCchPrintfW(buf, 64, L"0x%08X (%u)", val, val);
                return buf;
            }
            return L"(无效DWORD)";
            
        case REG_QWORD:
            if (dataSize >= sizeof(unsigned __int64)) {
                unsigned __int64 val = *(unsigned __int64*)data;
                WCHAR buf[64];
                StringCchPrintfW(buf, 64, L"0x%016llX (%llu)", val, val);
                return buf;
            }
            return L"(无效QWORD)";
            
        case REG_MULTI_SZ: {
            std::wstring result;
            LPCWSTR p = (LPCWSTR)data;
            DWORD charsLeft = dataSize / sizeof(WCHAR);
            while (charsLeft > 0 && *p) {
                if (!result.empty()) result += L"; ";
                std::wstring s(p);
                result += s;
                charsLeft -= s.length() + 1;
                p += s.length() + 1;
            }
            return result.empty() ? L"(空)" : result;
        }
        
        case REG_BINARY:
        default: {
            std::wstring result;
            DWORD showLen = min(dataSize, 32);
            for (DWORD i = 0; i < showLen; i++) {
                if (i > 0) result += L" ";
                WCHAR buf[8];
                StringCchPrintfW(buf, 8, L"%02X", data[i]);
                result += buf;
            }
            if (dataSize > 32) result += L" ...";
            return result;
        }
    }
}

static std::wstring ConvertToRegPath(LPCWSTR lpszVfsPath)
{
    if (!lpszVfsPath || !IsRegVfsPath(lpszVfsPath)) return L"";

    std::wstring path(lpszVfsPath);
    NormalizePath(path);

    if (path.length() <= 6) return L"";
    path = path.substr(6);
    while (!path.empty() && path.front() == L'/') path.erase(0, 1);
    while (!path.empty() && path.back() == L'/') path.pop_back();

    if (path.empty()) return L"";

    size_t firstSlash = path.find(L'/');
    std::wstring rootKeyStr = (firstSlash == std::wstring::npos) ? path : path.substr(0, firstSlash);
    std::wstring subPath = (firstSlash == std::wstring::npos) ? L"" : path.substr(firstSlash + 1);

    for (auto& c : subPath) if (c == L'/') c = L'\\';

    return subPath.empty() ? rootKeyStr : rootKeyStr + L"\\" + subPath;
}

static bool CopyRegistryKey(HKEY hSrcRootKey, LPCWSTR srcSubPath, HKEY hDstRootKey, LPCWSTR dstSubPath)
{
    HKEY hSrcKey = NULL, hDstKey = NULL;
    LONG result = ERROR_SUCCESS;

    std::wstring srcPath = srcSubPath ? srcSubPath : L"";
    result = srcPath.empty() ?
        RegOpenKeyExW(hSrcRootKey, NULL, 0, KEY_READ, &hSrcKey) :
        RegOpenKeyExW(hSrcRootKey, srcPath.c_str(), 0, KEY_READ, &hSrcKey);
    if (result != ERROR_SUCCESS) return false;

    result = RegCreateKeyExW(hDstRootKey, dstSubPath ? dstSubPath : L"", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hDstKey, NULL);
    if (result != ERROR_SUCCESS) { RegCloseKey(hSrcKey); return false; }

    DWORD valueIndex = 0;
    WCHAR valueName[MAX_PATH];
    DWORD valueNameSize, valueType, dataSize;

    while (true) {
        valueNameSize = MAX_PATH;
        dataSize = 0;
        result = RegEnumValueW(hSrcKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, NULL, &dataSize);
        if (result != ERROR_SUCCESS) break;

        if (dataSize > 0) {
            std::vector<BYTE> data(dataSize);
            RegEnumValueW(hSrcKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, data.data(), &dataSize);
            RegSetValueExW(hDstKey, valueName, 0, valueType, data.data(), dataSize);
        } else {
            RegSetValueExW(hDstKey, valueName, 0, valueType, NULL, 0);
        }
        valueIndex++;
    }

    DWORD subKeyIndex = 0;
    WCHAR subKeyName[MAX_PATH];
    DWORD subKeyNameSize;

    while (true) {
        subKeyNameSize = MAX_PATH;
        result = RegEnumKeyExW(hSrcKey, subKeyIndex, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
        if (result != ERROR_SUCCESS) break;

        std::wstring newSrcPath = srcPath.empty() ? subKeyName : srcPath + L"\\" + subKeyName;
        std::wstring newDstPath = (dstSubPath && *dstSubPath) ?
            std::wstring(dstSubPath) + L"\\" + subKeyName : subKeyName;

        CopyRegistryKey(hSrcRootKey, newSrcPath.c_str(), hDstRootKey, newDstPath.c_str());
        subKeyIndex++;
    }

    RegCloseKey(hSrcKey);
    RegCloseKey(hDstKey);
    return true;
}

static bool DeleteRegistryKeyRecursive(HKEY hRootKey, LPCWSTR subPath)
{
    if (!subPath || !*subPath) return false;

    HKEY hKey;
    LONG result = RegOpenKeyExW(hRootKey, subPath, 0, KEY_READ | KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    WCHAR subKeyName[MAX_PATH];
    DWORD subKeyNameSize;
    DWORD subKeyIndex = 0;

    while (true) {
        subKeyNameSize = MAX_PATH;
        result = RegEnumKeyExW(hKey, 0, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
        if (result != ERROR_SUCCESS) break;

        RegCloseKey(hKey);
        std::wstring fullPath = std::wstring(subPath) + L"\\" + subKeyName;
        if (!DeleteRegistryKeyRecursive(hRootKey, fullPath.c_str())) {
            RegOpenKeyExW(hRootKey, subPath, 0, KEY_READ | KEY_WRITE, &hKey);
            break;
        }
        result = RegOpenKeyExW(hRootKey, subPath, 0, KEY_READ | KEY_WRITE, &hKey);
        if (result != ERROR_SUCCESS) return false;
    }

    RegCloseKey(hKey);

    size_t lastSlash = std::wstring(subPath).rfind(L'\\');
    std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : std::wstring(subPath).substr(0, lastSlash);
    std::wstring keyName = (lastSlash == std::wstring::npos) ? subPath : std::wstring(subPath).substr(lastSlash + 1);

    HKEY hParentKey;
    result = parentPath.empty() ?
        RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hParentKey) :
        RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_WRITE, &hParentKey);
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteKeyW(hParentKey, keyName.c_str());
    RegCloseKey(hParentKey);
    return result == ERROR_SUCCESS;
}

static bool ExportRegistryKey(HKEY hRootKey, LPCWSTR subPath, LPCWSTR filePath)
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(hRootKey, subPath ? subPath : L"", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) return false;

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { RegCloseKey(hKey); return false; }

    WCHAR bom = 0xFEFF;
    DWORD written;
    WriteFile(hFile, &bom, sizeof(WCHAR), &written, NULL);

    std::wstring header = L"Windows Registry Editor Version 5.00\r\n\r\n";
    WriteFile(hFile, header.c_str(), (DWORD)(header.length() * sizeof(WCHAR)), &written, NULL);

    std::wstring keyPath;
    for (const auto& pair : g_rootKeys) {
        if (pair.second == hRootKey) {
            keyPath = L"[" + pair.first;
            break;
        }
    }
    if (subPath && *subPath) { keyPath += L"\\"; keyPath += subPath; }
    keyPath += L"]\r\n";
    WriteFile(hFile, keyPath.c_str(), (DWORD)(keyPath.length() * sizeof(WCHAR)), &written, NULL);

    DWORD valueIndex = 0;
    WCHAR valueName[MAX_PATH];
    DWORD valueNameSize, valueType, dataSize;

    while (true) {
        valueNameSize = MAX_PATH;
        dataSize = 0;
        result = RegEnumValueW(hKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, NULL, &dataSize);
        if (result != ERROR_SUCCESS) break;

        std::wstring line;
        if (valueName[0] == L'\0') {
            line = L"@=";
        } else {
            line = L"\"" + std::wstring(valueName) + L"\"=";
        }

        if (dataSize > 0) {
            std::vector<BYTE> data(dataSize);
            RegEnumValueW(hKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, data.data(), &dataSize);

            switch (valueType) {
            case REG_SZ:
            case REG_EXPAND_SZ:
                line += L"\"" + std::wstring((LPCWSTR)data.data()) + L"\"";
                break;
            case REG_DWORD:
                {
                    WCHAR buf[32];
                    StringCchPrintfW(buf, 32, L"dword:%08X", *(DWORD*)data.data());
                    line += buf;
                }
                break;
            case REG_QWORD:
                {
                    WCHAR buf[32];
                    StringCchPrintfW(buf, 32, L"qword:%016llX", *(unsigned __int64*)data.data());
                    line += buf;
                }
                break;
            default:
                line += L"hex:" + std::to_wstring(valueType) + L":";
                for (DWORD i = 0; i < dataSize; i++) {
                    WCHAR buf[8];
                    StringCchPrintfW(buf, 8, L"%02X", data[i]);
                    line += buf;
                    if (i < dataSize - 1) line += L",";
                }
                break;
            }
        } else {
            line += L"\"\"";
        }
        line += L"\r\n";
        WriteFile(hFile, line.c_str(), (DWORD)(line.length() * sizeof(WCHAR)), &written, NULL);
        valueIndex++;
    }

    std::wstring footer = L"\r\n";
    WriteFile(hFile, footer.c_str(), (DWORD)(footer.length() * sizeof(WCHAR)), &written, NULL);

    CloseHandle(hFile);
    RegCloseKey(hKey);
    return true;
}

static void SearchRegistryRecursive(HKEY hRootKey, const std::wstring& rootKeyName, const std::wstring& currentPath, SearchParams& params)
{
    if (g_searchCancelled) return;

    HKEY hKey;
    LONG result = currentPath.empty() ?
        RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
        RegOpenKeyExW(hRootKey, currentPath.c_str(), 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) return;

    std::wstring compareTerm = params.searchTerm;
    if (!params.caseSensitive) {
        for (auto& c : compareTerm) c = towlower(c);
    }

    if (params.searchKeyNames) {
        std::wstring keyName = currentPath.empty() ? L"" : currentPath.substr(currentPath.rfind(L'\\') + 1);
        std::wstring compareKeyName = keyName;
        if (!params.caseSensitive) {
            for (auto& c : compareKeyName) c = towlower(c);
        }
        if (compareKeyName.find(compareTerm) != std::wstring::npos) {
            g_searchResults += L"[\u9879] " + rootKeyName + (currentPath.empty() ? L"" : L"\\" + currentPath) + L"\r\n";
        }
    }

    DWORD valueIndex = 0;
    WCHAR valueName[MAX_PATH];
    DWORD valueNameSize, valueType, dataSize;

    while (!g_searchCancelled) {
        valueNameSize = MAX_PATH;
        dataSize = 0;
        result = RegEnumValueW(hKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, NULL, &dataSize);
        if (result != ERROR_SUCCESS) break;

        std::wstring valName = valueName[0] ? valueName : L"(\u9ED8\u8BA4)";
        std::wstring compareValName = valName;
        if (!params.caseSensitive) {
            for (auto& c : compareValName) c = towlower(c);
        }

        if (params.searchValueNames && compareValName.find(compareTerm) != std::wstring::npos) {
            g_searchResults += L"[\u503C\u540D] " + rootKeyName + (currentPath.empty() ? L"" : L"\\" + currentPath) + L" \\" + valName + L"\r\n";
        }

        if (params.searchValueData && dataSize > 0) {
            std::vector<BYTE> data(dataSize);
            RegEnumValueW(hKey, valueIndex, valueName, &valueNameSize, NULL, &valueType, data.data(), &dataSize);

            if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
                std::wstring strData((LPCWSTR)data.data(), dataSize / sizeof(WCHAR));
                std::wstring compareStrData = strData;
                if (!params.caseSensitive) {
                    for (auto& c : compareStrData) c = towlower(c);
                }
                if (compareStrData.find(compareTerm) != std::wstring::npos) {
                    g_searchResults += L"[\u503C\u6570\u636E] " + rootKeyName + (currentPath.empty() ? L"" : L"\\" + currentPath) + L" \\" + valName + L" = " + strData + L"\r\n";
                }
            }
        }
        valueIndex++;
    }

    DWORD subKeyIndex = 0;
    WCHAR subKeyName[MAX_PATH];
    DWORD subKeyNameSize;

    while (!g_searchCancelled) {
        subKeyNameSize = MAX_PATH;
        result = RegEnumKeyExW(hKey, subKeyIndex, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
        if (result != ERROR_SUCCESS) break;

        std::wstring newPath = currentPath.empty() ? subKeyName : currentPath + L"\\" + subKeyName;
        RegCloseKey(hKey);
        SearchRegistryRecursive(hRootKey, rootKeyName, newPath, params);
        result = RegOpenKeyExW(hRootKey, currentPath.empty() ? NULL : currentPath.c_str(), 0, KEY_READ, &hKey);
        if (result != ERROR_SUCCESS) return;
        subKeyIndex++;
    }

    RegCloseKey(hKey);
}

static std::wstring GetPermissionString(HKEY hKey)
{
    PSECURITY_DESCRIPTOR pSD = NULL;
    DWORD dwRet = GetSecurityInfo(hKey, SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION |
        GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &pSD);

    if (dwRet != ERROR_SUCCESS || !pSD) return L"\u65E0\u6CD5\u83B7\u53D6\u6743\u9650\u4FE1\u606F";

    BOOL bDaclPresent, bDaclDefaulted;
    PACL pDacl = NULL;
    GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted);

    std::wstring result;
    if (!bDaclPresent || !pDacl) {
        result = L"\u65E0\u9650\u5236\u8BBF\u95EE";
    } else {
        ACL_SIZE_INFORMATION aclSize;
        GetAclInformation(pDacl, &aclSize, sizeof(aclSize), AclSizeInformation);

        int fullControl = 0, readOnly = 0, other = 0;
        for (DWORD i = 0; i < aclSize.AceCount; i++) {
            ACCESS_ALLOWED_ACE* pAce;
            if (GetAce(pDacl, i, (LPVOID*)&pAce)) {
                DWORD accessMask = pAce->Mask;
                if ((accessMask & KEY_ALL_ACCESS) == KEY_ALL_ACCESS) fullControl++;
                else if ((accessMask & KEY_READ) == KEY_READ) readOnly++;
                else other++;
            }
        }

        if (fullControl > 0) result += L"\u5B8C\u5168\u63A7\u5236(" + std::to_wstring(fullControl) + L") ";
        if (readOnly > 0) result += L"\u53EA\u8BFB(" + std::to_wstring(readOnly) + L") ";
        if (other > 0) result += L"\u81EA\u5B9A\u4E49(" + std::to_wstring(other) + L")";
    }

    LocalFree(pSD);
    return result.empty() ? L"\u672A\u77E5" : result;
}

static void ShowPropertiesDialog(HWND hWndParent, HKEY hRootKey, LPCWSTR subPath, bool isKey)
{
    std::wstring info;

    if (isKey) {
        HKEY hKey;
        LONG result = subPath && *subPath ?
            RegOpenKeyExW(hRootKey, subPath, 0, KEY_READ | KEY_QUERY_VALUE | READ_CONTROL, &hKey) :
            RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_QUERY_VALUE | READ_CONTROL, &hKey);

        if (result == ERROR_SUCCESS) {
            DWORD subKeyCount = 0, valueCount = 0;
            FILETIME ftLastWrite;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL,
                &valueCount, NULL, NULL, NULL, &ftLastWrite);

            std::wstring rootKeyName;
            for (const auto& pair : g_rootKeys) {
                if (pair.second == hRootKey) {
                    rootKeyName = pair.first;
                    break;
                }
            }

            info = L"\u6CE8\u518C\u8868\u9879\u5C5E\u6027\n\n";
            info += L"\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\n";
            info += L"\u4F4D\u7F6E:\n    " + rootKeyName;
            if (subPath && *subPath) {
                std::wstring path = subPath;
                info += L"\\" + path;
            }
            info += L"\n\n";
            info += L"\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\n";
            info += L"\u7EDF\u8BA1\u4FE1\u606F:\n";
            info += L"    \u5B50\u9879\u6570\u91CF: " + std::to_wstring(subKeyCount) + L"\n";
            info += L"    \u503C\u6570\u91CF: " + std::to_wstring(valueCount) + L"\n\n";
            info += L"\u6743\u9650: " + GetPermissionString(hKey) + L"\n";

            RegCloseKey(hKey);
        }
    } else {
        info = L"\u6CE8\u518C\u8868\u503C\u5C5E\u6027\n\n";
        info += L"\u8DEF\u5F84: " + std::wstring(subPath ? subPath : L"") + L"\n";
    }

    MessageBoxW(hWndParent, info.c_str(), L"\u5C5E\u6027", MB_OK | MB_ICONINFORMATION);
}

static std::wstring GetRootKeyName(HKEY hRootKey)
{
    for (const auto& pair : g_rootKeys) {
        if (pair.second == hRootKey) return pair.first;
    }
    return L"";
}

struct RegFileInfo
{
    HKEY hOpenKey;
    std::vector<BYTE> data;
    size_t readPos;
    bool forWrite;
    bool modified;
    std::wstring valueName;
    DWORD valueType;
};

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) { LoadConfig(); return TRUE; }
extern "C" __declspec(dllexport) void VFS_Uninit() {}
extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) { return (HANDLE)1; }
extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    lpVFSInfo->idPlugin = GUIDPlugin_Registry;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    if (lpVFSInfo->lpszHandlePrefix) StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"reg://");
    if (lpVFSInfo->lpszName) StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"Registry");
    if (lpVFSInfo->lpszDescription) StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, L"Windows Registry Virtual File System");
    ExtractIconExW(L"regedit.exe", 0, &lpVFSInfo->hIconLarge, &lpVFSInfo->hIconSmall, 1);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    LPCWSTR prefixes = L"reg://\0";
    memcpy(lpszPrefix, prefixes, 7 * sizeof(WCHAR));
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData)
{
    static VFSCUSTOMCOLUMNW columns[NUM_CUSTOM_COLUMNS];
    
    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\x503C\x7C7B\x578B";
    columns[0].lpszKey = L"regtype";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;
    
    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\x6570\x636E\x5927\x5C0F";
    columns[1].lpszKey = L"regsize";
    columns[1].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_SIZE;
    columns[1].iID = 2;
    
    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\x503C\x9884\x89C8";
    columns[2].lpszKey = L"regpreview";
    columns[2].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[2].iID = 3;
    
    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\x6240\x5C5E\x6839\x952E";
    columns[3].lpszKey = L"regroot";
    columns[3].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[3].iID = 4;
    
    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\x5B8C\x6574\x8DEF\x5F84";
    columns[4].lpszKey = L"regfullpath";
    columns[4].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[4].iID = 5;
    
    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"\x5B50\x9879\x6570\x91CF";
    columns[5].lpszKey = L"regsubkeys";
    columns[5].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER;
    columns[5].iID = 6;
    
    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\x503C\x6570\x91CF";
    columns[6].lpszKey = L"regvaluecount";
    columns[6].dwFlags = VFSCCF_RIGHTJUSTIFY | VFSCCF_NUMBER;
    columns[6].iID = 7;
    
    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[7].lpNext = NULL;
    columns[7].lpszLabel = L"\x4FEE\x6539\x65F6\x95F4";
    columns[7].lpszKey = L"regmodified";
    columns[7].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[7].iID = 8;
    
    return columns;
}

static bool ParseRegPath(LPCWSTR lpszPath, HKEY& hRootKey, std::wstring& subPath)
{
    if (!lpszPath || !IsRegVfsPath(lpszPath)) return false;
    
    std::wstring path(lpszPath);
    NormalizePath(path);
    
    if (path.length() <= 6) return false;
    path = path.substr(6);
    if (path.empty()) return false;
    
    size_t firstSlash = path.find(L'/');
    std::wstring rootKeyStr = (firstSlash == std::wstring::npos) ? path : path.substr(0, firstSlash);
    subPath = (firstSlash == std::wstring::npos) ? L"" : path.substr(firstSlash + 1);
    
    // Convert to registry path format (backslashes)
    ToRegPath(subPath);
    
    for (const auto& pair : g_rootKeys) {
        if (_wcsicmp(pair.first.c_str(), rootKeyStr.c_str()) == 0) {
            hRootKey = pair.second;
            return true;
        }
    }
    return false;
}

static bool IsRegKeyPath(LPCWSTR lpszPath)
{
    if (!lpszPath || !IsRegVfsPath(lpszPath)) return false;

    HKEY hRootKey; std::wstring subPath;
    if (!ParseRegPath(lpszPath, hRootKey, subPath)) return false;
    if (subPath.empty()) return true;

    HKEY hKey;
    LONG result = RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) { RegCloseKey(hKey); return true; }

    result = RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (result == ERROR_SUCCESS) { RegCloseKey(hKey); return true; }

    result = RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ | KEY_WOW64_32KEY, &hKey);
    if (result == ERROR_SUCCESS) { RegCloseKey(hKey); return true; }

    return false;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || lpRDD->vfsReadOp == VFSREAD_FREEDIR || lpRDD->vfsReadOp == VFSREAD_CHANGEDIR) return TRUE;

    if (lpRDD->vfsReadOp == VFSREAD_NORMAL || lpRDD->vfsReadOp == VFSREAD_REFRESH || 
        lpRDD->vfsReadOp == VFSREAD_PARENT || lpRDD->vfsReadOp == VFSREAD_ROOT ||
        lpRDD->vfsReadOp == VFSREAD_BACK || lpRDD->vfsReadOp == VFSREAD_FORWARD ||
        lpRDD->vfsReadOp == VFSREAD_PRINTDIR) {
        std::wstring normalizedPath(lpRDD->lpszPath);
        NormalizePath(normalizedPath);
        
        if (normalizedPath == L"reg://" || normalizedPath.length() <= 6) {
            int numItems = (int)g_rootKeys.size();
            size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * numItems);
            LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
            if (!lpFDH) return FALSE;
            
            lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
            lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
            lpFDH->iNumItems = numItems;
            
            LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
            int idx = 0;
            FILETIME ft; GetSystemTimeAsFileTime(&ft);
            for (const auto& pair : g_rootKeys) {
                StringCchCopyW(lpFileData[idx].wfdData.cFileName, MAX_PATH, pair.first.c_str());
                lpFileData[idx].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                lpFileData[idx].wfdData.ftCreationTime = ft;
                lpFileData[idx].wfdData.ftLastAccessTime = ft;
                lpFileData[idx].wfdData.ftLastWriteTime = ft;
                lpFileData[idx].iNumColumns = 0;
                lpFileData[idx].lpvfsColumnData = NULL;
                idx++;
            }
            lpRDD->lpFileData = lpFDH;
            return TRUE;
        }

        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpRDD->lpszPath, hRootKey, subPath)) { SetLastError(ERROR_PATH_NOT_FOUND); return FALSE; }

        HKEY hKey;
        LONG result = subPath.empty() ? 
            RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKey) :
            RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKey);
        if (result != ERROR_SUCCESS) { SetLastError(ERROR_PATH_NOT_FOUND); return FALSE; }

        DWORD subKeyCount = 0, valueCount = 0;
        RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);

        int numItems = subKeyCount + valueCount;
        size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, allocSize);
        if (!lpFDH) { RegCloseKey(hKey); return FALSE; }
        
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);

        int addedItems = 0; 
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        FILETIME ftDefault; GetSystemTimeAsFileTime(&ftDefault);
        
        for (DWORD i = 0; i < subKeyCount; i++) {
            if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
            WCHAR subKeyName[MAX_PATH]; DWORD subKeyNameSize = MAX_PATH; FILETIME ftLastWrite;
            if (RegEnumKeyExW(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, &ftLastWrite) == ERROR_SUCCESS) {
                StringCchCopyW(lpFileData[addedItems].wfdData.cFileName, MAX_PATH, subKeyName);
                lpFileData[addedItems].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                lpFileData[addedItems].wfdData.ftLastWriteTime = ftLastWrite;
                lpFileData[addedItems].wfdData.ftCreationTime = ftDefault;
                lpFileData[addedItems].wfdData.ftLastAccessTime = ftDefault;
                
                lpFileData[addedItems].iNumColumns = NUM_CUSTOM_COLUMNS;
                lpFileData[addedItems].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, NUM_CUSTOM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));
                if (lpFileData[addedItems].lpvfsColumnData) {
                    DWORD childSubKeys = 0, childValues = 0;
                    HKEY hSubKey;
                    std::wstring subKeyPath = subPath.empty() ? subKeyName : subPath + L"\\" + subKeyName;
                    if (RegOpenKeyExW(hRootKey, subKeyPath.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                        RegQueryInfoKeyW(hSubKey, NULL, NULL, NULL, &childSubKeys, NULL, NULL, &childValues, NULL, NULL, NULL, NULL);
                        RegCloseKey(hSubKey);
                    }
                    
                    SYSTEMTIME st;
                    FileTimeToSystemTime(&ftLastWrite, &st);
                    WCHAR timeBuf[32];
                    StringCchPrintfW(timeBuf, 32, L"%04d/%02d/%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                    
                    std::wstring fullPath = GetRootKeyName(hRootKey);
                    if (!subPath.empty()) fullPath += std::wstring(L"\\") + subPath;
                    fullPath += std::wstring(L"\\") + subKeyName;
                    
                    lpFileData[addedItems].lpvfsColumnData[0].iColumnId = 1;
                    lpFileData[addedItems].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, L"\x6CE8\x518C\x8868\x9879");
                    lpFileData[addedItems].lpvfsColumnData[1].iColumnId = 2;
                    lpFileData[addedItems].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap, L"");
                    lpFileData[addedItems].lpvfsColumnData[2].iColumnId = 3;
                    lpFileData[addedItems].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap, L"");
                    lpFileData[addedItems].lpvfsColumnData[3].iColumnId = 4;
                    lpFileData[addedItems].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap, GetRootKeyName(hRootKey));
                    lpFileData[addedItems].lpvfsColumnData[4].iColumnId = 5;
                    lpFileData[addedItems].lpvfsColumnData[4].lpszValue = AllocString(lpRDD->hMemHeap, fullPath);
                    lpFileData[addedItems].lpvfsColumnData[5].iColumnId = 6;
                    lpFileData[addedItems].lpvfsColumnData[5].lpszValue = AllocString(lpRDD->hMemHeap, std::to_wstring(childSubKeys));
                    lpFileData[addedItems].lpvfsColumnData[6].iColumnId = 7;
                    lpFileData[addedItems].lpvfsColumnData[6].lpszValue = AllocString(lpRDD->hMemHeap, std::to_wstring(childValues));
                    lpFileData[addedItems].lpvfsColumnData[7].iColumnId = 8;
                    lpFileData[addedItems].lpvfsColumnData[7].lpszValue = AllocString(lpRDD->hMemHeap, timeBuf);
                }
                addedItems++;
            }
        }
        
        for (DWORD i = 0; i < valueCount; i++) {
            if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;
            WCHAR valName[MAX_PATH]; DWORD valNameSize = MAX_PATH; DWORD valueType = 0; DWORD dataSize = 0;
            if (RegEnumValueW(hKey, i, valName, &valNameSize, NULL, &valueType, NULL, &dataSize) == ERROR_SUCCESS) {
                std::wstring displayName = valName[0] ? valName : L"(Default)";
                StringCchCopyW(lpFileData[addedItems].wfdData.cFileName, MAX_PATH, displayName.c_str());
                lpFileData[addedItems].wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                lpFileData[addedItems].wfdData.nFileSizeLow = dataSize;
                lpFileData[addedItems].wfdData.ftCreationTime = ftDefault;
                lpFileData[addedItems].wfdData.ftLastAccessTime = ftDefault;
                lpFileData[addedItems].wfdData.ftLastWriteTime = ftDefault;
                
                lpFileData[addedItems].iNumColumns = NUM_CUSTOM_COLUMNS;
                lpFileData[addedItems].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY, NUM_CUSTOM_COLUMNS * sizeof(VFSFILEDATACOLUMNW));
                if (lpFileData[addedItems].lpvfsColumnData) {
                    std::wstring preview;
                    if (dataSize > 0) {
                        std::vector<BYTE> valData(dataSize);
                        DWORD readSize = dataSize;
                        if (RegEnumValueW(hKey, i, valName, &valNameSize, NULL, &valueType, valData.data(), &readSize) == ERROR_SUCCESS) {
                            preview = FormatValuePreview(valueType, valData.data(), readSize);
                            if (preview.length() > 100) preview = preview.substr(0, 100) + L"...";
                        }
                    }
                    
                    std::wstring fullPath = GetRootKeyName(hRootKey);
                    if (!subPath.empty()) fullPath += std::wstring(L"\\") + subPath;
                    fullPath += std::wstring(L"\\") + displayName;
                    
                    WCHAR sizeBuf[32];
                    StringCchPrintfW(sizeBuf, 32, L"%u", dataSize);
                    
                    lpFileData[addedItems].lpvfsColumnData[0].iColumnId = 1;
                    lpFileData[addedItems].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, GetRegTypeName(valueType));
                    lpFileData[addedItems].lpvfsColumnData[1].iColumnId = 2;
                    lpFileData[addedItems].lpvfsColumnData[1].lpszValue = AllocString(lpRDD->hMemHeap, sizeBuf);
                    lpFileData[addedItems].lpvfsColumnData[2].iColumnId = 3;
                    lpFileData[addedItems].lpvfsColumnData[2].lpszValue = AllocString(lpRDD->hMemHeap, preview);
                    lpFileData[addedItems].lpvfsColumnData[3].iColumnId = 4;
                    lpFileData[addedItems].lpvfsColumnData[3].lpszValue = AllocString(lpRDD->hMemHeap, GetRootKeyName(hRootKey));
                    lpFileData[addedItems].lpvfsColumnData[4].iColumnId = 5;
                    lpFileData[addedItems].lpvfsColumnData[4].lpszValue = AllocString(lpRDD->hMemHeap, fullPath);
                    lpFileData[addedItems].lpvfsColumnData[5].iColumnId = 6;
                    lpFileData[addedItems].lpvfsColumnData[5].lpszValue = AllocString(lpRDD->hMemHeap, L"0");
                    lpFileData[addedItems].lpvfsColumnData[6].iColumnId = 7;
                    lpFileData[addedItems].lpvfsColumnData[6].lpszValue = AllocString(lpRDD->hMemHeap, L"0");
                    lpFileData[addedItems].lpvfsColumnData[7].iColumnId = 8;
                    lpFileData[addedItems].lpvfsColumnData[7].lpszValue = AllocString(lpRDD->hMemHeap, L"");
                }
                addedItems++;
            }
        }
        
        RegCloseKey(hKey);
        lpFDH->iNumItems = addedItems; lpRDD->lpFileData = lpFDH; return TRUE;
    }
    SetLastError(ERROR_NO_MORE_FILES); return FALSE;
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    std::wstring normalizedPath(lpszPath);
    NormalizePath(normalizedPath);

    if (normalizedPath.length() <= 6 || normalizedPath == L"reg://") {
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
        if (!lpFDH) return NULL;
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = 1; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Registry");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        lpFileData->wfdData.ftCreationTime = ft; lpFileData->wfdData.ftLastAccessTime = ft; lpFileData->wfdData.ftLastWriteTime = ft;
        return lpFDH;
    }

    // Parse the path
    std::wstring path = normalizedPath.substr(6);
    size_t firstSlash = path.find(L'/');
    if (firstSlash == std::wstring::npos) {
        // Root key directory
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
        if (!lpFDH) return NULL;
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = 1; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, path.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        lpFileData->wfdData.ftCreationTime = ft; lpFileData->wfdData.ftLastAccessTime = ft; lpFileData->wfdData.ftLastWriteTime = ft;
        return lpFDH;
    }

    std::wstring rootKeyStr = path.substr(0, firstSlash);
    std::wstring remaining = path.substr(firstSlash + 1);
    
    // Find root key
    HKEY hRootKey = NULL;
    for (const auto& pair : g_rootKeys) {
        if (_wcsicmp(pair.first.c_str(), rootKeyStr.c_str()) == 0) { hRootKey = pair.second; break; }
    }
    if (!hRootKey) return NULL;

    // Convert path separators to backslashes for registry API
    std::wstring regPath = remaining;
    ToRegPath(regPath);
    
    // First check if this is a registry key
    HKEY hKey;
    LONG result = RegOpenKeyExW(hRootKey, regPath.c_str(), 0, KEY_READ, &hKey);
    if (result == ERROR_SUCCESS) {
        // It's a registry key (directory)
        LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
        if (!lpFDH) { RegCloseKey(hKey); return NULL; }
        
        lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = 1; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        
        size_t lastSlash = remaining.rfind(L'/');
        std::wstring nameStr = (lastSlash != std::wstring::npos) ? remaining.substr(lastSlash + 1) : remaining;
        if (nameStr.empty()) nameStr = rootKeyStr;
        
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, nameStr.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        lpFileData->wfdData.ftCreationTime = ft; lpFileData->wfdData.ftLastAccessTime = ft; lpFileData->wfdData.ftLastWriteTime = ft;
        RegCloseKey(hKey);
        return lpFDH;
    }

    // It's not a key, check if it's a value
    size_t lastSlash = remaining.rfind(L'/');
    std::wstring subPath = (lastSlash == std::wstring::npos) ? L"" : remaining.substr(0, lastSlash);
    std::wstring valueName = (lastSlash == std::wstring::npos) ? remaining : remaining.substr(lastSlash + 1);
    
    // Convert to reg path
    std::wstring parentRegPath = subPath;
    ToRegPath(parentRegPath);
    
    // Try to open the parent key
    result = parentRegPath.empty() ? 
        RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
        RegOpenKeyExW(hRootKey, parentRegPath.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) return NULL;

    // Try to query the value
    DWORD valueType = 0, dataSize = 0;
    LPCWSTR lpValueName = (valueName == L"(Default)") ? L"" : valueName.c_str();
    result = RegQueryValueExW(hKey, lpValueName, NULL, &valueType, NULL, &dataSize);
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) return NULL;

    // It's a registry value (file)
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(VFSFILEDATAHEADER) + sizeof(VFSFILEDATAW));
    if (!lpFDH) return NULL;
    
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER); lpFDH->iNumItems = 1; lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, valueName.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    lpFileData->wfdData.nFileSizeLow = dataSize;
    lpFileData->wfdData.nFileSizeHigh = 0;
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    lpFileData->wfdData.ftCreationTime = ft; lpFileData->wfdData.ftLastAccessTime = ft; lpFileData->wfdData.ftLastWriteTime = ft;
    
    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT) {
    if (!lpszFile || _wcsnicmp(lpszFile, L"reg://", 6) != 0) return NULL;
    
    std::wstring path = lpszFile + 6;
    while (!path.empty() && path.back() == L'/') path.pop_back();
    if (path.empty()) return NULL;
    
    size_t firstSlash = path.find(L'/');
    if (firstSlash == std::wstring::npos) return NULL;
    
    std::wstring rootKeyStr = path.substr(0, firstSlash);
    std::wstring remaining = path.substr(firstSlash + 1);
    
    HKEY hRootKey = NULL;
    for (const auto& pair : g_rootKeys) {
        if (_wcsicmp(pair.first.c_str(), rootKeyStr.c_str()) == 0) { hRootKey = pair.second; break; }
    }
    if (!hRootKey) return NULL;
    
    size_t lastSlash = remaining.rfind(L'/');
    std::wstring subPath = (lastSlash == std::wstring::npos) ? L"" : remaining.substr(0, lastSlash);
    std::wstring valueName = (lastSlash == std::wstring::npos) ? remaining : remaining.substr(lastSlash + 1);
    if (valueName.empty()) return NULL;
    
    // Convert to registry path format
    std::wstring regSubPath = subPath;
    ToRegPath(regSubPath);
    
    bool forWrite = (dwMode & GENERIC_WRITE) != 0;
    REGSAM samDesired = KEY_READ | (forWrite ? KEY_WRITE : 0);
    
    HKEY hKey;
    LONG result = regSubPath.empty() ? 
        RegOpenKeyExW(hRootKey, NULL, 0, samDesired, &hKey) :
        RegOpenKeyExW(hRootKey, regSubPath.c_str(), 0, samDesired, &hKey);
    if (result != ERROR_SUCCESS) return NULL;
    
    DWORD valueType = 0, dataSize = 0;
    LPCWSTR lpValueName = (valueName == L"(Default)") ? L"" : valueName.c_str();
    RegQueryValueExW(hKey, lpValueName, NULL, &valueType, NULL, &dataSize);
    
    RegFileInfo* ctx = new RegFileInfo();
    if (!ctx) { RegCloseKey(hKey); return NULL; }
    ctx->hOpenKey = hKey;
    ctx->readPos = 0;
    ctx->forWrite = forWrite;
    ctx->modified = false;
    ctx->valueName = valueName;
    ctx->valueType = valueType;
    
    if (dataSize > 0) {
        ctx->data.resize(dataSize);
        if (RegQueryValueExW(hKey, lpValueName, NULL, &valueType, ctx->data.data(), &dataSize) == ERROR_SUCCESS)
            ctx->valueType = valueType;
        else
            ctx->data.clear();
    }
    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    RegFileInfo* ctx = (RegFileInfo*)hFile; 
    if (!ctx) return FALSE;
    if (lpdwReadSize) *lpdwReadSize = 0;
    if (ctx->readPos >= ctx->data.size()) return TRUE;
    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->data.size() - ctx->readPos));
    if (bytesToRead > 0 && lpData) {
        CopyMemory(lpData, ctx->data.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;
        if (lpdwReadSize) *lpdwReadSize = bytesToRead;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize) {
    RegFileInfo* ctx = (RegFileInfo*)hFile; 
    if (!ctx || !ctx->forWrite) return FALSE;
    if (lpdwWriteSize) *lpdwWriteSize = 0;
    if (dwSize == 0 || !lpData) return TRUE;
    size_t newSize = ctx->readPos + dwSize;
    if (newSize > ctx->data.size()) ctx->data.resize(newSize);
    CopyMemory(ctx->data.data() + ctx->readPos, lpData, dwSize);
    ctx->readPos += dwSize;
    ctx->modified = true;
    if (lpdwWriteSize) *lpdwWriteSize = dwSize;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos) {
    RegFileInfo* ctx = (RegFileInfo*)hFile; 
    if (!ctx) return FALSE;
    __int64 newPos = 0;
    switch (dwMethod) {
        case FILE_BEGIN: newPos = iPos; break;
        case FILE_CURRENT: newPos = ctx->readPos + iPos; break;
        case FILE_END: newPos = ctx->data.size() + iPos; break;
        default: return FALSE;
    }
    if (newPos < 0) return FALSE;
    if (ctx->forWrite && (size_t)newPos > ctx->data.size()) ctx->data.resize((size_t)newPos);
    ctx->readPos = (size_t)min(newPos, (__int64)ctx->data.size());
    if (piNewPos) *piNewPos = ctx->readPos;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    RegFileInfo* ctx = (RegFileInfo*)hFile; 
    if (!ctx) return;
    if (ctx->modified && ctx->hOpenKey) {
        LPCWSTR lpValueName = (ctx->valueName == L"(Default)") ? L"" : ctx->valueName.c_str();
        DWORD vt = ctx->valueType ? ctx->valueType : REG_SZ;
        RegSetValueExW(ctx->hOpenKey, lpValueName, 0, vt, ctx->data.empty() ? NULL : ctx->data.data(), (DWORD)ctx->data.size());
    }
    if (ctx->hOpenKey) RegCloseKey(ctx->hOpenKey);
    delete ctx; 
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwFlags, int iSecurePasses) {
    if (!lpszFile || _wcsnicmp(lpszFile, L"reg://", 6) != 0) return FALSE;
    std::wstring path = lpszFile + 6;
    while (!path.empty() && path.back() == L'/') path.pop_back();
    if (path.empty()) return FALSE;
    
    size_t firstSlash = path.find(L'/');
    if (firstSlash == std::wstring::npos) return FALSE;
    std::wstring rootKeyStr = path.substr(0, firstSlash);
    std::wstring remaining = path.substr(firstSlash + 1);
    
    HKEY hRootKey = NULL;
    for (const auto& pair : g_rootKeys) {
        if (_wcsicmp(pair.first.c_str(), rootKeyStr.c_str()) == 0) { hRootKey = pair.second; break; }
    }
    if (!hRootKey) return FALSE;
    
    size_t lastSlash = remaining.rfind(L'/');
    std::wstring subPath = (lastSlash == std::wstring::npos) ? L"" : remaining.substr(0, lastSlash);
    std::wstring valueName = (lastSlash == std::wstring::npos) ? remaining : remaining.substr(lastSlash + 1);
    if (valueName.empty()) return FALSE;
    
    HKEY hKey;
    LONG result = subPath.empty() ? 
        RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hKey) :
        RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) return FALSE;
    
    LPCWSTR lpValueName = (valueName == L"(Default)") ? L"" : valueName.c_str();
    result = RegDeleteValueW(hKey, lpValueName);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    HKEY hRootKey; std::wstring subPath;
    if (!ParseRegPath(lpszPath, hRootKey, subPath)) return FALSE;
    HKEY hNewKey; DWORD dwDisp;
    LONG result = RegCreateKeyExW(hRootKey, subPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hNewKey, &dwDisp);
    if (result == ERROR_SUCCESS) { RegCloseKey(hNewKey); return TRUE; }
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags) {
    HKEY hRootKey; std::wstring subPath;
    if (!ParseRegPath(lpszPath, hRootKey, subPath)) return FALSE;
    if (subPath.empty()) return FALSE;
    
    size_t lastSlash = subPath.rfind(L'/');
    std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : subPath.substr(0, lastSlash);
    std::wstring keyToDelete = (lastSlash == std::wstring::npos) ? subPath : subPath.substr(lastSlash + 1);
    
    HKEY hParentKey;
    LONG result = parentPath.empty() ? 
        RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hParentKey) :
        RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_WRITE, &hParentKey);
    if (result != ERROR_SUCCESS) return FALSE;
    
    result = RegDeleteKeyW(hParentKey, keyToDelete.c_str());
    RegCloseKey(hParentKey);
    return (result == ERROR_SUCCESS) ? TRUE : FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) {
    if (!lpszOldName || !lpszNewName) return FALSE;
    if (!IsRegVfsPath(lpszOldName)) return FALSE;

    HKEY hRootKey; std::wstring subPath;
    if (!ParseRegPath(lpszOldName, hRootKey, subPath)) return FALSE;
    for (auto& c : subPath) if (c == L'/') c = L'\\';

    size_t lastSlash = subPath.rfind(L'\\');
    std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : subPath.substr(0, lastSlash);
    std::wstring oldName = (lastSlash == std::wstring::npos) ? subPath : subPath.substr(lastSlash + 1);

    std::wstring newNameStr(lpszNewName);
    for (auto& c : newNameStr) if (c == L'/') c = L'\\';
    size_t newLastSlash = newNameStr.rfind(L'\\');
    std::wstring newName = (newLastSlash == std::wstring::npos) ? newNameStr : newNameStr.substr(newLastSlash + 1);

    if (oldName.empty() || newName.empty()) return FALSE;

    bool isKey = IsRegKeyPath(lpszOldName);

    if (isKey) {
        HKEY hParentKey;
        LONG result = parentPath.empty() ?
            RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hParentKey) :
            RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_WRITE, &hParentKey);
        if (result != ERROR_SUCCESS) return FALSE;

        result = RegRenameKey(hParentKey, oldName.c_str(), newName.c_str());
        RegCloseKey(hParentKey);
        return (result == ERROR_SUCCESS) ? TRUE : FALSE;
    } else {
        HKEY hKey;
        LONG result = parentPath.empty() ?
            RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_WRITE, &hKey) :
            RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
        if (result != ERROR_SUCCESS) return FALSE;

        LPCWSTR lpOldName = (oldName == L"(\u9ED8\u8BA4)") ? L"" : oldName.c_str();
        DWORD valueType = 0, dataSize = 0;
        result = RegQueryValueExW(hKey, lpOldName, NULL, &valueType, NULL, &dataSize);
        if (result != ERROR_SUCCESS) { RegCloseKey(hKey); return FALSE; }

        std::vector<BYTE> data(dataSize);
        RegQueryValueExW(hKey, lpOldName, NULL, &valueType, data.data(), &dataSize);

        LPCWSTR lpNewName = (newName == L"(\u9ED8\u8BA4)") ? L"" : newName.c_str();
        result = RegSetValueExW(hKey, lpNewName, 0, valueType, data.data(), dataSize);
        if (result == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, lpOldName);
        }
        RegCloseKey(hKey);
        return (result == ERROR_SUCCESS) ? TRUE : FALSE;
    }
}
extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName) { return FALSE; }

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData) { return GetLastError(); }

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData)
{
    if (!lpMenuData) return FALSE;

    if (!lpszFiles || !lpszFiles[0]) {
        lpMenuData->fAllowContextMenu = TRUE;
        lpMenuData->fDefaultContextMenu = FALSE;
        lpMenuData->fCustomItemsBelow = FALSE;
        lpMenuData->lpCustomItems = NULL;
        lpMenuData->iNumCustomItems = 0;
        lpMenuData->fFreeCustomItems = FALSE;
        return TRUE;
    }

    lpMenuData->fAllowContextMenu = TRUE;
    lpMenuData->fDefaultContextMenu = FALSE;
    lpMenuData->fCustomItemsBelow = TRUE;

    static VFSCONTEXTMENUITEMW customItems[40];
    int idx = 0;

    bool isKey = IsRegKeyPath(lpszFiles);

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u6253\u5F00";
    customItems[idx].lpszCommand = L"$open";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = VFSCMF_SEPARATOR;
    customItems[idx].lpszLabel = L"";
    customItems[idx].lpszCommand = L"";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u590D\u5236";
    customItems[idx].lpszCommand = L"$clip_copy";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u526A\u5207";
    customItems[idx].lpszCommand = L"$clip_cut";
    idx++;

    if (isKey) {
        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u7C98\u8D34";
        customItems[idx].lpszCommand = L"$clip_paste";
        idx++;
    }

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = VFSCMF_SEPARATOR;
    customItems[idx].lpszLabel = L"";
    customItems[idx].lpszCommand = L"";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u91CD\u547D\u540D";
    customItems[idx].lpszCommand = L"$rename";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u5220\u9664";
    customItems[idx].lpszCommand = L"$delete";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = VFSCMF_SEPARATOR;
    customItems[idx].lpszLabel = L"";
    customItems[idx].lpszCommand = L"";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u5728\u6CE8\u518C\u8868\u7F16\u8F91\u5668\u4E2D\u6253\u5F00";
    customItems[idx].lpszCommand = L"$open_in_regedit";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u590D\u5236\u6CE8\u518C\u8868\u8DEF\u5F84";
    customItems[idx].lpszCommand = L"$copy_key_path";
    idx++;

    if (isKey) {
        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u641C\u7D22...";
        customItems[idx].lpszCommand = L"$search";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_SEPARATOR;
        customItems[idx].lpszLabel = L"";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_BEGINSUBMENU;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA\u6CE8\u518C\u8868\u9879";
        customItems[idx].lpszCommand = L"$new_key";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA\u5B57\u7B26\u4E32\u503C (REG_SZ)";
        customItems[idx].lpszCommand = L"$new_sz";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA DWORD \u503C (32\u4F4D)";
        customItems[idx].lpszCommand = L"$new_dword";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA QWORD \u503C (64\u4F4D)";
        customItems[idx].lpszCommand = L"$new_qword";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA\u4E8C\u8FDB\u5236\u503C (REG_BINARY)";
        customItems[idx].lpszCommand = L"$new_binary";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_ENDSUBMENU;
        customItems[idx].lpszLabel = L"\u65B0\u5EFA\u53EF\u6269\u5C55\u5B57\u7B26\u4E32\u503C (REG_EXPAND_SZ)";
        customItems[idx].lpszCommand = L"$new_expand_sz";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_SEPARATOR;
        customItems[idx].lpszLabel = L"";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u590D\u5236\u6CE8\u518C\u8868\u9879...";
        customItems[idx].lpszCommand = L"$copy_key";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u5BFC\u51FA\u6CE8\u518C\u8868\u9879...";
        customItems[idx].lpszCommand = L"$export_reg";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_SEPARATOR;
        customItems[idx].lpszLabel = L"";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u6DFB\u52A0\u5230\u6536\u85CF\u5939";
        customItems[idx].lpszCommand = L"$add_to_favorites";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u67E5\u770B\u6536\u85CF\u5939";
        customItems[idx].lpszCommand = L"$view_favorites";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u6E05\u7A7A\u6536\u85CF\u5939";
        customItems[idx].lpszCommand = L"$clear_favorites";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_SEPARATOR;
        customItems[idx].lpszLabel = L"";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_BEGINSUBMENU;
        customItems[idx].lpszLabel = L"\u6279\u91CF\u64CD\u4F5C";
        customItems[idx].lpszCommand = L"";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u6279\u91CF\u5BFC\u51FA\u5B50\u9879...";
        customItems[idx].lpszCommand = L"$batch_export";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u6279\u91CF\u590D\u5236\u5B50\u9879...";
        customItems[idx].lpszCommand = L"$batch_copy";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = 0;
        customItems[idx].lpszLabel = L"\u6279\u91CF\u5220\u9664\u5B50\u9879...";
        customItems[idx].lpszCommand = L"$batch_delete";
        idx++;

        customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
        customItems[idx].dwFlags = VFSCMF_ENDSUBMENU;
        customItems[idx].lpszLabel = L"\u7EDF\u8BA1\u5B50\u9879\u4FE1\u606F";
        customItems[idx].lpszCommand = L"$batch_stats";
        idx++;
    }

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = VFSCMF_SEPARATOR;
    customItems[idx].lpszLabel = L"";
    customItems[idx].lpszCommand = L"";
    idx++;

    customItems[idx].cbSize = sizeof(VFSCONTEXTMENUITEMW);
    customItems[idx].dwFlags = 0;
    customItems[idx].lpszLabel = L"\u5C5E\u6027";
    customItems[idx].lpszCommand = L"$properties";
    idx++;

    lpMenuData->lpCustomItems = customItems;
    lpMenuData->iNumCustomItems = idx;
    lpMenuData->fFreeCustomItems = FALSE;

    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData) {
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_DEFAULT;
    if (!IsRegVfsPath(lpVerbData->lpszPath)) return VFSCVRES_DEFAULT;

    if (lpVerbData->lpszVerb == NULL || wcslen(lpVerbData->lpszVerb) == 0 || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0)
    {
        bool isDir = (lpVerbData->dwFlags & DOPUSCVF_ISDIR) || IsRegKeyPath(lpVerbData->lpszPath);
        if (isDir) {
            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGEDIR;
        }

        HKEY hRootKey; std::wstring subPath;
        if (ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) {
            for (auto& c : subPath) if (c == L'/') c = L'\\';

            size_t lastSlash = subPath.rfind(L'\\');
            std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : subPath.substr(0, lastSlash);
            std::wstring valueName = (lastSlash == std::wstring::npos) ? subPath : subPath.substr(lastSlash + 1);

            HKEY hKey;
            LONG result = parentPath.empty() ?
                RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
                RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_READ, &hKey);

            if (result == ERROR_SUCCESS) {
                DWORD valueType = 0, dataSize = 0;
                LPCWSTR lpValueName = (valueName == L"(\u9ED8\u8BA4)") ? L"" : valueName.c_str();
                result = RegQueryValueExW(hKey, lpValueName, NULL, &valueType, NULL, &dataSize);

                if (result == ERROR_SUCCESS && dataSize > 0) {
                    std::vector<BYTE> data(dataSize);
                    RegQueryValueExW(hKey, lpValueName, NULL, &valueType, data.data(), &dataSize);

                    std::wstring preview = FormatValuePreview(valueType, data.data(), dataSize);
                    std::wstring typeName = GetRegTypeName(valueType);

                    std::wstring msg = L"\u6CE8\u518C\u8868\u503C\u4FE1\u606F\r\n\r\n";
                    msg += L"\u540D\u79F0: " + valueName + L"\r\n";
                    msg += L"\u7C7B\u578B: " + typeName + L"\r\n";
                    msg += L"\u5927\u5C0F: " + std::to_wstring(dataSize) + L" \u5B57\u8282\r\n\r\n";
                    msg += L"\u503C:\r\n" + preview;

                    MessageBoxW(lpVerbData->hwndParent, msg.c_str(), L"\u6CE8\u518C\u8868\u503C", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(lpVerbData->hwndParent,
                        (L"\u6CE8\u518C\u8868\u503C: " + valueName + L"\n\u7C7B\u578B: REG_SZ\n\u503C: (\u7A7A)").c_str(),
                        L"\u6CE8\u518C\u8868\u503C", MB_OK | MB_ICONINFORMATION);
                }
                RegCloseKey(hKey);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"clip_copy") == 0) {
        g_clipEntries.clear();
        g_clipOp = CLIP_COPY;
        ClipboardEntry entry;
        entry.vfsPath = lpVerbData->lpszPath;
        entry.isKey = IsRegKeyPath(lpVerbData->lpszPath);
        g_clipEntries.push_back(entry);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"clip_cut") == 0) {
        g_clipEntries.clear();
        g_clipOp = CLIP_CUT;
        ClipboardEntry entry;
        entry.vfsPath = lpVerbData->lpszPath;
        entry.isKey = IsRegKeyPath(lpVerbData->lpszPath);
        g_clipEntries.push_back(entry);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"clip_paste") == 0) {
        if (g_clipEntries.empty() || g_clipOp == CLIP_NONE) {
            MessageBoxW(lpVerbData->hwndParent, L"\u526A\u8D34\u677F\u4E3A\u7A7A", L"\u7C98\u8D34", MB_OK | MB_ICONWARNING);
            return VFSCVRES_HANDLED;
        }

        HKEY hDstRootKey; std::wstring dstSubPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hDstRootKey, dstSubPath)) return VFSCVRES_FAIL;
        for (auto& c : dstSubPath) if (c == L'/') c = L'\\';

        bool anySuccess = false;
        for (const auto& entry : g_clipEntries) {
            HKEY hSrcRootKey; std::wstring srcSubPath;
            if (!ParseRegPath(entry.vfsPath.c_str(), hSrcRootKey, srcSubPath)) continue;
            for (auto& c : srcSubPath) if (c == L'/') c = L'\\';

            if (entry.isKey) {
                size_t lastSlash = srcSubPath.rfind(L'\\');
                std::wstring keyName = (lastSlash == std::wstring::npos) ? srcSubPath : srcSubPath.substr(lastSlash + 1);
                std::wstring newDstPath = dstSubPath.empty() ? keyName : dstSubPath + L"\\" + keyName;

                if (CopyRegistryKey(hSrcRootKey, srcSubPath.c_str(), hDstRootKey, newDstPath.c_str())) {
                    anySuccess = true;
                    if (g_clipOp == CLIP_CUT) {
                        DeleteRegistryKeyRecursive(hSrcRootKey, srcSubPath.c_str());
                    }
                }
            } else {
                size_t lastSlash = srcSubPath.rfind(L'\\');
                std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : srcSubPath.substr(0, lastSlash);
                std::wstring valueName = (lastSlash == std::wstring::npos) ? srcSubPath : srcSubPath.substr(lastSlash + 1);

                HKEY hSrcKey;
                LONG result = parentPath.empty() ?
                    RegOpenKeyExW(hSrcRootKey, NULL, 0, KEY_READ, &hSrcKey) :
                    RegOpenKeyExW(hSrcRootKey, parentPath.c_str(), 0, KEY_READ, &hSrcKey);
                if (result != ERROR_SUCCESS) continue;

                DWORD valueType = 0, dataSize = 0;
                LPCWSTR lpValueName = (valueName == L"(\u9ED8\u8BA4)") ? L"" : valueName.c_str();
                result = RegQueryValueExW(hSrcKey, lpValueName, NULL, &valueType, NULL, &dataSize);
                if (result == ERROR_SUCCESS) {
                    std::vector<BYTE> data(dataSize);
                    RegQueryValueExW(hSrcKey, lpValueName, NULL, &valueType, data.data(), &dataSize);

                    HKEY hDstKey;
                    result = RegOpenKeyExW(hDstRootKey, dstSubPath.c_str(), 0, KEY_WRITE, &hDstKey);
                    if (result == ERROR_SUCCESS) {
                        result = RegSetValueExW(hDstKey, lpValueName, 0, valueType, data.data(), dataSize);
                        RegCloseKey(hDstKey);
                        if (result == ERROR_SUCCESS) {
                            anySuccess = true;
                            if (g_clipOp == CLIP_CUT) {
                                RegDeleteValueW(hSrcKey, lpValueName);
                            }
                        }
                    }
                }
                RegCloseKey(hSrcKey);
            }
        }

        if (g_clipOp == CLIP_CUT) {
            g_clipEntries.clear();
            g_clipOp = CLIP_NONE;
        }

        if (anySuccess) {
            if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
            return VFSCVRES_CHANGE;
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"rename") == 0) {
        bool isKey = IsRegKeyPath(lpVerbData->lpszPath);

        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        size_t lastSlash = subPath.rfind(L'\\');
        std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : subPath.substr(0, lastSlash);
        std::wstring oldName = (lastSlash == std::wstring::npos) ? subPath : subPath.substr(lastSlash + 1);

        if (oldName.empty()) return VFSCVRES_FAIL;

        WCHAR newName[MAX_PATH] = {0};
        StringCchCopyW(newName, MAX_PATH, oldName.c_str());

        if (MessageBoxW(lpVerbData->hwndParent,
            (L"\u8BF7\u8F93\u5165\u65B0\u540D\u79F0:\n\n\u5F53\u524D\u540D\u79F0: " + oldName).c_str(),
            L"\u91CD\u547D\u540D", MB_OKCANCEL | MB_ICONINFORMATION) == IDOK) {

            std::wstring input(newName);
            if (input.empty() || input == oldName) return VFSCVRES_HANDLED;

            if (isKey) {
                HKEY hParentKey;
                LONG result = parentPath.empty() ?
                    RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hParentKey) :
                    RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_WRITE, &hParentKey);
                if (result != ERROR_SUCCESS) return VFSCVRES_FAIL;

                result = RegRenameKey(hParentKey, oldName.c_str(), input.c_str());
                RegCloseKey(hParentKey);

                if (result == ERROR_SUCCESS) {
                    if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                    return VFSCVRES_CHANGE;
                }
                MessageBoxW(lpVerbData->hwndParent,
                    (L"\u91CD\u547D\u540D\u5931\u8D25: \u9519\u8BEF\u4EE3\u7801 " + std::to_wstring(result)).c_str(),
                    L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
            } else {
                HKEY hKey;
                LONG result = parentPath.empty() ?
                    RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_WRITE, &hKey) :
                    RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
                if (result != ERROR_SUCCESS) return VFSCVRES_FAIL;

                LPCWSTR lpOldName = (oldName == L"(\u9ED8\u8BA4)") ? L"" : oldName.c_str();
                DWORD valueType = 0, dataSize = 0;
                result = RegQueryValueExW(hKey, lpOldName, NULL, &valueType, NULL, &dataSize);
                if (result == ERROR_SUCCESS) {
                    std::vector<BYTE> data(dataSize);
                    RegQueryValueExW(hKey, lpOldName, NULL, &valueType, data.data(), &dataSize);

                    LPCWSTR lpNewName = (input == L"(\u9ED8\u8BA4)") ? L"" : input.c_str();
                    result = RegSetValueExW(hKey, lpNewName, 0, valueType, data.data(), dataSize);
                    if (result == ERROR_SUCCESS) {
                        RegDeleteValueW(hKey, lpOldName);
                        RegCloseKey(hKey);
                        if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                            StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                        return VFSCVRES_CHANGE;
                    }
                }
                RegCloseKey(hKey);
                MessageBoxW(lpVerbData->hwndParent, L"\u91CD\u547D\u540D\u5931\u8D25", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"delete") == 0) {
        bool isKey = IsRegKeyPath(lpVerbData->lpszPath);

        std::wstring regPath = ConvertToRegPath(lpVerbData->lpszPath);
        if (regPath.empty()) return VFSCVRES_FAIL;

        if (MessageBoxW(lpVerbData->hwndParent,
            (L"\u786E\u5B9A\u8981\u5220\u9664\u5417\uFF1F\n\n" + regPath + L"\n\n\u6B64\u64CD\u4F5C\u4E0D\u53EF\u64A4\u9500\uFF01").c_str(),
            L"\u786E\u8BA4\u5220\u9664", MB_YESNO | MB_ICONWARNING) != IDYES) return VFSCVRES_HANDLED;

        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        if (isKey) {
            if (DeleteRegistryKeyRecursive(hRootKey, subPath.c_str())) {
                if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                    StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                return VFSCVRES_CHANGE;
            }
        } else {
            size_t lastSlash = subPath.rfind(L'\\');
            std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : subPath.substr(0, lastSlash);
            std::wstring valueName = (lastSlash == std::wstring::npos) ? subPath : subPath.substr(lastSlash + 1);

            HKEY hKey;
            LONG result = parentPath.empty() ?
                RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hKey) :
                RegOpenKeyExW(hRootKey, parentPath.c_str(), 0, KEY_WRITE, &hKey);
            if (result == ERROR_SUCCESS) {
                LPCWSTR lpValueName = (valueName == L"(\u9ED8\u8BA4)") ? L"" : valueName.c_str();
                result = RegDeleteValueW(hKey, lpValueName);
                RegCloseKey(hKey);
                if (result == ERROR_SUCCESS) {
                    if (lpVerbData->lpszNewPath && lpVerbData->cchNewPathMax > 0)
                        StringCchCopyW(lpVerbData->lpszNewPath, lpVerbData->cchNewPathMax, lpVerbData->lpszPath);
                    return VFSCVRES_CHANGE;
                }
            }
        }
        MessageBoxW(lpVerbData->hwndParent, L"\u5220\u9664\u5931\u8D25", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"open_in_regedit") == 0) {
        std::wstring regPath = ConvertToRegPath(lpVerbData->lpszPath);
        if (regPath.empty()) return VFSCVRES_FAIL;

        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        std::wstring cmd = L"regedit.exe /m";

        if (CreateProcessW(NULL, (LPWSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            Sleep(500);
            HWND hwndRegedit = FindWindowW(L"RegEdit_RegEdit", NULL);
            if (hwndRegedit) {
                COPYDATASTRUCT cds;
                cds.dwData = 0;
                cds.cbData = (DWORD)((regPath.length() + 1) * sizeof(WCHAR));
                cds.lpData = (PVOID)regPath.c_str();
                SendMessageW(hwndRegedit, WM_COPYDATA, 0, (LPARAM)&cds);
                SetForegroundWindow(hwndRegedit);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"search") == 0) {
        HKEY hRootKey; std::wstring subPath;
        ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath);
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        if (MessageBoxW(lpVerbData->hwndParent,
            L"\u786E\u5B9A\u8981\u641C\u7D22\u6CE8\u518C\u8868\u5417\uFF1F\n\n\u641C\u7D22\u53EF\u80FD\u9700\u8981\u8F83\u957F\u65F6\u95F4\u3002\n\u70B9\u51FB\"\u662F\"\u5F00\u59CB\u641C\u7D22\uFF0C\u70B9\u51FB\"\u5426\"\u53D6\u6D88\u3002",
            L"\u641C\u7D22\u6CE8\u518C\u8868", MB_YESNO | MB_ICONQUESTION) == IDYES) {

            g_searchResults.clear();
            g_searchCancelled = false;

            g_searchResults = L"\u641C\u7D22\u7ED3\u679C (\u793A\u4F8B\u641C\u7D22\"Windows\"):\r\n";
            g_searchResults += L"\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n\r\n";

            SearchParams params;
            params.searchTerm = L"Windows";
            params.searchKeyNames = true;
            params.searchValueNames = true;
            params.searchValueData = true;
            params.caseSensitive = false;

            if (hRootKey) {
                std::wstring rootKeyName;
                for (const auto& pair : g_rootKeys) {
                    if (pair.second == hRootKey) {
                        rootKeyName = pair.first;
                        break;
                    }
                }
                SearchRegistryRecursive(hRootKey, rootKeyName, subPath, params);
            } else {
                for (const auto& pair : g_rootKeys) {
                    SearchRegistryRecursive(pair.second, pair.first, L"", params);
                }
            }

            g_searchResults += L"\r\n\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n";
            g_searchResults += L"\u641C\u7D22\u5B8C\u6210\r\n";

            MessageBoxW(lpVerbData->hwndParent, g_searchResults.c_str(), L"\u641C\u7D22\u7ED3\u679C", MB_OK | MB_ICONINFORMATION);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_key_path") == 0) {
        std::wstring regPath = ConvertToRegPath(lpVerbData->lpszPath);
        if (regPath.empty()) return VFSCVRES_FAIL;

        if (OpenClipboard(lpVerbData->hwndParent)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (regPath.length() + 1) * sizeof(WCHAR));
            if (hMem) {
                wcscpy_s((LPWSTR)GlobalLock(hMem), regPath.length() + 1, regPath.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"new_key") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;

        HKEY hKey;
        LONG result = subPath.empty() ? RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hKey) : RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_WRITE, &hKey);
        if (result != ERROR_SUCCESS) return VFSCVRES_FAIL;

        std::wstring newKeyName = L"\u65B0\u9879 #1";
        int counter = 1;
        while (RegOpenKeyExW(hKey, newKeyName.c_str(), 0, KEY_READ, NULL) == ERROR_SUCCESS) {
            counter++;
            newKeyName = L"\u65B0\u9879 #" + std::to_wstring(counter);
        }

        HKEY hNewKey;
        result = RegCreateKeyExW(hKey, newKeyName.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hNewKey, NULL);
        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS) {
            RegCloseKey(hNewKey);
            MessageBoxW(lpVerbData->hwndParent, (L"\u5DF2\u521B\u5EFA\u65B0\u6CE8\u518C\u8868\u9879: " + newKeyName).c_str(), L"\u65B0\u5EFA\u6CE8\u518C\u8868\u9879", MB_OK | MB_ICONINFORMATION);
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"new_sz") == 0 || _wcsicmp(lpVerbData->lpszVerb, L"new_dword") == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"new_qword") == 0 || _wcsicmp(lpVerbData->lpszVerb, L"new_binary") == 0 ||
        _wcsicmp(lpVerbData->lpszVerb, L"new_expand_sz") == 0) {

        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;

        HKEY hKey;
        LONG result = subPath.empty() ? RegOpenKeyExW(hRootKey, NULL, 0, KEY_WRITE, &hKey) : RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_WRITE, &hKey);
        if (result != ERROR_SUCCESS) return VFSCVRES_FAIL;

        std::wstring newValueName = L"\u65B0\u503C #1";
        int counter = 1;
        while (RegQueryValueExW(hKey, newValueName.c_str(), NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            counter++;
            newValueName = L"\u65B0\u503C #" + std::to_wstring(counter);
        }

        DWORD valueType = REG_NONE;
        BYTE* data = NULL;
        DWORD dataSize = 0;
        BYTE zeroData[8] = {0};

        if (_wcsicmp(lpVerbData->lpszVerb, L"new_dword") == 0) {
            valueType = REG_DWORD; data = zeroData; dataSize = 4;
        } else if (_wcsicmp(lpVerbData->lpszVerb, L"new_qword") == 0) {
            valueType = REG_QWORD; data = zeroData; dataSize = 8;
        } else if (_wcsicmp(lpVerbData->lpszVerb, L"new_binary") == 0) {
            valueType = REG_BINARY; data = NULL; dataSize = 0;
        } else if (_wcsicmp(lpVerbData->lpszVerb, L"new_expand_sz") == 0) {
            valueType = REG_EXPAND_SZ; zeroData[0] = 0; zeroData[1] = 0; data = zeroData; dataSize = 2;
        } else {
            valueType = REG_SZ; zeroData[0] = 0; zeroData[1] = 0; data = zeroData; dataSize = 2;
        }

        result = RegSetValueExW(hKey, newValueName.c_str(), 0, valueType, data, dataSize);
        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS) {
            MessageBoxW(lpVerbData->hwndParent, (L"\u5DF2\u521B\u5EFA\u65B0\u503C: " + newValueName).c_str(), L"\u65B0\u5EFA\u503C", MB_OK | MB_ICONINFORMATION);
            return VFSCVRES_CHANGEDIR;
        }
        return VFSCVRES_FAIL;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"export_reg") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        WCHAR filePath[MAX_PATH] = {0};
        OPENFILENAMEW ofn = {sizeof(ofn)};
        ofn.hwndOwner = lpVerbData->hwndParent;
        ofn.lpstrFilter = L"\u6CE8\u518C\u8868\u6587\u4EF6 (*.reg)\0*.reg\0\u6240\u6709\u6587\u4EF6 (*.*)\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"reg";
        ofn.lpstrTitle = L"\u5BFC\u51FA\u6CE8\u518C\u8868\u9879";

        if (GetSaveFileNameW(&ofn)) {
            if (ExportRegistryKey(hRootKey, subPath.empty() ? NULL : subPath.c_str(), filePath)) {
                MessageBoxW(lpVerbData->hwndParent, (L"\u5DF2\u5BFC\u51FA\u5230: " + std::wstring(filePath)).c_str(), L"\u5BFC\u51FA\u6210\u529F", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(lpVerbData->hwndParent, L"\u5BFC\u51FA\u5931\u8D25", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"copy_key") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        WCHAR dstPath[MAX_PATH] = {0};
        wcscpy_s(dstPath, MAX_PATH, lpVerbData->lpszPath);

        std::wstring pathStr(dstPath);
        size_t lastSlash = pathStr.rfind(L'/');
        if (lastSlash != std::wstring::npos) {
            std::wstring basePath = pathStr.substr(0, lastSlash);
            std::wstring keyName = pathStr.substr(lastSlash + 1);
            StringCchPrintfW(dstPath, MAX_PATH, L"%s/%s_\u526F\u672C", basePath.c_str(), keyName.c_str());
        }

        HKEY hDstRootKey; std::wstring dstSubPath;
        if (!ParseRegPath(dstPath, hDstRootKey, dstSubPath)) return VFSCVRES_FAIL;
        for (auto& c : dstSubPath) if (c == L'/') c = L'\\';

        if (CopyRegistryKey(hRootKey, subPath.c_str(), hDstRootKey, dstSubPath.c_str())) {
            MessageBoxW(lpVerbData->hwndParent, L"\u6CE8\u518C\u8868\u9879\u5DF2\u590D\u5236", L"\u590D\u5236\u6210\u529F", MB_OK | MB_ICONINFORMATION);
            return VFSCVRES_CHANGEDIR;
        } else {
            MessageBoxW(lpVerbData->hwndParent, L"\u590D\u5236\u6CE8\u518C\u8868\u9879\u5931\u8D25", L"\u9519\u8BEF", MB_OK | MB_ICONERROR);
            return VFSCVRES_FAIL;
        }
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"add_to_favorites") == 0) {
        std::wstring regPath = ConvertToRegPath(lpVerbData->lpszPath);
        if (regPath.empty()) return VFSCVRES_FAIL;

        if (g_favorites.find(regPath) == g_favorites.end()) {
            g_favorites.insert(regPath);
            MessageBoxW(lpVerbData->hwndParent, (L"\u5DF2\u6DFB\u52A0\u5230\u6536\u85CF\u5939: " + regPath).c_str(), L"\u6536\u85CF\u5939", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(lpVerbData->hwndParent, L"\u8BE5\u9879\u5DF2\u5728\u6536\u85CF\u5939\u4E2D", L"\u6536\u85CF\u5939", MB_OK | MB_ICONWARNING);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"view_favorites") == 0) {
        if (g_favorites.empty()) {
            MessageBoxW(lpVerbData->hwndParent, L"\u6536\u85CF\u5939\u4E3A\u7A7A", L"\u6536\u85CF\u5939", MB_OK | MB_ICONINFORMATION);
        } else {
            std::wstring favList = L"\u6536\u85CF\u5939\u5217\u8868:\r\n\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n\r\n";
            int i = 1;
            for (const auto& fav : g_favorites) {
                favList += std::to_wstring(i) + L". " + fav + L"\r\n";
                i++;
            }
            favList += L"\r\n\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n";
            favList += L"\u5171 " + std::to_wstring(g_favorites.size()) + L" \u9879";
            MessageBoxW(lpVerbData->hwndParent, favList.c_str(), L"\u6536\u85CF\u5939", MB_OK | MB_ICONINFORMATION);
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"clear_favorites") == 0) {
        if (g_favorites.empty()) {
            MessageBoxW(lpVerbData->hwndParent, L"\u6536\u85CF\u5939\u5DF2\u7ECF\u4E3A\u7A7A", L"\u6536\u85CF\u5939", MB_OK | MB_ICONINFORMATION);
        } else {
            if (MessageBoxW(lpVerbData->hwndParent,
                (L"\u786E\u5B9A\u8981\u6E05\u7A7A\u6536\u85CF\u5939\u5417\uFF1F\n\n\u5F53\u524D\u6709 " + std::to_wstring(g_favorites.size()) + L" \u9879\u3002").c_str(),
                L"\u6E05\u7A7A\u6536\u85CF\u5939", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                g_favorites.clear();
                MessageBoxW(lpVerbData->hwndParent, L"\u6536\u85CF\u5939\u5DF2\u6E05\u7A7A", L"\u6536\u85CF\u5939", MB_OK | MB_ICONINFORMATION);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"properties") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        bool isKey = true;
        if (!subPath.empty()) {
            HKEY hTestKey;
            LONG result = RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ, &hTestKey);
            if (result == ERROR_SUCCESS) {
                RegCloseKey(hTestKey);
                isKey = true;
            } else {
                isKey = false;
            }
        }

        ShowPropertiesDialog(lpVerbData->hwndParent, hRootKey, subPath.c_str(), isKey);
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"batch_export") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        WCHAR folderPath[MAX_PATH] = {0};
        BROWSEINFOW bi = {0};
        bi.hwndOwner = lpVerbData->hwndParent;
        bi.lpszTitle = L"\u9009\u62E9\u5BFC\u51FA\u76EE\u5F55";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            SHGetPathFromIDListW(pidl, folderPath);
            CoTaskMemFree(pidl);

            HKEY hKey;
            LONG result = subPath.empty() ?
                RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
                RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey);
            if (result == ERROR_SUCCESS) {
                DWORD subKeyCount = 0;
                RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

                int exported = 0;
                for (DWORD i = 0; i < subKeyCount; i++) {
                    WCHAR subKeyName[MAX_PATH];
                    DWORD subKeyNameSize = MAX_PATH;
                    if (RegEnumKeyExW(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                        std::wstring fullPath = subPath.empty() ? subKeyName : subPath + L"\\" + subKeyName;
                        std::wstring filePath = std::wstring(folderPath) + L"\\" + subKeyName + L".reg";
                        if (ExportRegistryKey(hRootKey, fullPath.c_str(), filePath.c_str())) {
                            exported++;
                        }
                    }
                }
                RegCloseKey(hKey);
                MessageBoxW(lpVerbData->hwndParent,
                    (L"\u5DF2\u5BFC\u51FA " + std::to_wstring(exported) + L" \u4E2A\u6CE8\u518C\u8868\u9879\u5230:\n" + folderPath).c_str(),
                    L"\u6279\u91CF\u5BFC\u51FA\u5B8C\u6210", MB_OK | MB_ICONINFORMATION);
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"batch_copy") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        if (MessageBoxW(lpVerbData->hwndParent,
            L"\u786E\u5B9A\u8981\u590D\u5236\u5F53\u524D\u76EE\u5F55\u4E0B\u6240\u6709\u5B50\u9879\u5417\uFF1F\n\n\u5C06\u5728\u5F53\u524D\u76EE\u5F55\u521B\u5EFA\u6240\u6709\u5B50\u9879\u7684\u526F\u672C\u3002",
            L"\u6279\u91CF\u590D\u5236", MB_YESNO | MB_ICONQUESTION) == IDYES) {

            HKEY hKey;
            LONG result = subPath.empty() ?
                RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_WRITE, &hKey) :
                RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
            if (result == ERROR_SUCCESS) {
                DWORD subKeyCount = 0;
                RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

                int copied = 0;
                for (DWORD i = 0; i < subKeyCount; i++) {
                    WCHAR subKeyName[MAX_PATH];
                    DWORD subKeyNameSize = MAX_PATH;
                    if (RegEnumKeyExW(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                        std::wstring srcPath = subPath.empty() ? subKeyName : subPath + L"\\" + subKeyName;
                        std::wstring dstPath = srcPath + L"_\u526F\u672C";

                        if (CopyRegistryKey(hRootKey, srcPath.c_str(), hRootKey, dstPath.c_str())) {
                            copied++;
                        }
                    }
                }
                RegCloseKey(hKey);
                MessageBoxW(lpVerbData->hwndParent,
                    (L"\u5DF2\u590D\u5236 " + std::to_wstring(copied) + L" \u4E2A\u6CE8\u518C\u8868\u9879").c_str(),
                    L"\u6279\u91CF\u590D\u5236\u5B8C\u6210", MB_OK | MB_ICONINFORMATION);
                return VFSCVRES_CHANGEDIR;
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"batch_delete") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        if (MessageBoxW(lpVerbData->hwndParent,
            L"\u26A0\uFE0F \u8B66\u544A\uFF01\n\n\u6B64\u64CD\u4F5C\u5C06\u5220\u9664\u5F53\u524D\u76EE\u5F55\u4E0B\u6240\u6709\u5B50\u9879\uFF01\n\u6B64\u64CD\u4F5C\u4E0D\u53EF\u64A4\u9500\uFF01\n\n\u786E\u5B9A\u8981\u7EE7\u7EED\u5417\uFF1F",
            L"\u6279\u91CF\u5220\u9664", MB_YESNO | MB_ICONWARNING) == IDYES) {

            if (MessageBoxW(lpVerbData->hwndParent,
                L"\u518D\u6B21\u786E\u8BA4\uFF1A\n\n\u60A8\u771F\u7684\u8981\u5220\u9664\u6240\u6709\u5B50\u9879\u5417\uFF1F\n\u8FD9\u53EF\u80FD\u4F1A\u5BFC\u81F4\u7CFB\u7EDF\u4E0D\u7A33\u5B9A\uFF01",
                L"\u6279\u91CF\u5220\u9664\u786E\u8BA4", MB_YESNO | MB_ICONSTOP) == IDYES) {

                HKEY hKey;
                LONG result = subPath.empty() ?
                    RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ | KEY_WRITE, &hKey) :
                    RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
                if (result == ERROR_SUCCESS) {
                    DWORD subKeyCount = 0;
                    RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

                    int deleted = 0;
                    std::vector<std::wstring> keysToDelete;
                    for (DWORD i = 0; i < subKeyCount; i++) {
                        WCHAR subKeyName[MAX_PATH];
                        DWORD subKeyNameSize = MAX_PATH;
                        if (RegEnumKeyExW(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                            keysToDelete.push_back(subKeyName);
                        }
                    }

                    for (const auto& keyName : keysToDelete) {
                        std::wstring fullPath = subPath.empty() ? keyName : subPath + L"\\" + keyName;
                        if (DeleteRegistryKeyRecursive(hRootKey, fullPath.c_str())) {
                            deleted++;
                        }
                    }

                    RegCloseKey(hKey);
                    MessageBoxW(lpVerbData->hwndParent,
                        (L"\u5DF2\u5220\u9664 " + std::to_wstring(deleted) + L" \u4E2A\u6CE8\u518C\u8868\u9879").c_str(),
                        L"\u6279\u91CF\u5220\u9664\u5B8C\u6210", MB_OK | MB_ICONINFORMATION);
                    return VFSCVRES_CHANGEDIR;
                }
            }
        }
        return VFSCVRES_HANDLED;
    }

    if (_wcsicmp(lpVerbData->lpszVerb, L"batch_stats") == 0) {
        HKEY hRootKey; std::wstring subPath;
        if (!ParseRegPath(lpVerbData->lpszPath, hRootKey, subPath)) return VFSCVRES_FAIL;
        for (auto& c : subPath) if (c == L'/') c = L'\\';

        HKEY hKey;
        LONG result = subPath.empty() ?
            RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
            RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            DWORD subKeyCount = 0, valueCount = 0;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);

            DWORD totalSubKeys = subKeyCount, totalValues = valueCount;
            DWORD totalSubSubKeys = 0, totalSubValues = 0;

            for (DWORD i = 0; i < subKeyCount; i++) {
                WCHAR subKeyName[MAX_PATH];
                DWORD subKeyNameSize = MAX_PATH;
                if (RegEnumKeyExW(hKey, i, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                    HKEY hSubKey;
                    std::wstring fullPath = subPath.empty() ? subKeyName : subPath + L"\\" + subKeyName;
                    if (RegOpenKeyExW(hRootKey, fullPath.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                        DWORD subSubKeys = 0, subValues = 0;
                        RegQueryInfoKeyW(hSubKey, NULL, NULL, NULL, &subSubKeys, NULL, NULL, &subValues, NULL, NULL, NULL, NULL);
                        totalSubSubKeys += subSubKeys;
                        totalSubValues += subValues;
                        RegCloseKey(hSubKey);
                    }
                }
            }

            RegCloseKey(hKey);

            std::wstring stats = L"\u7EDF\u8BA1\u4FE1\u606F\r\n\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n\r\n";
            stats += L"\u5F53\u524D\u76EE\u5F55:\r\n";
            stats += L"    \u5B50\u9879: " + std::to_wstring(subKeyCount) + L"\r\n";
            stats += L"    \u503C: " + std::to_wstring(valueCount) + L"\r\n\r\n";
            stats += L"\u5B50\u9879\u603B\u8BA1:\r\n";
            stats += L"    \u5B50\u9879\u7684\u5B50\u9879: " + std::to_wstring(totalSubSubKeys) + L"\r\n";
            stats += L"    \u5B50\u9879\u7684\u503C: " + std::to_wstring(totalSubValues) + L"\r\n\r\n";
            stats += L"\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\r\n";
            stats += L"\u603B\u8BA1: " + std::to_wstring(totalSubKeys + totalSubSubKeys) + L" \u9879, ";
            stats += std::to_wstring(totalValues + totalSubValues) + L" \u4E2A\u503C";

            MessageBoxW(lpVerbData->hwndParent, stats.c_str(), L"\u7EDF\u8BA1\u4FE1\u606F", MB_OK | MB_ICONINFORMATION);
        }
        return VFSCVRES_HANDLED;
    }

    return VFSCVRES_FAIL;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HWND hwndParent, LPWSTR lpszFiles) {
    if (!lpszFiles || !lpszFiles[0]) return NULL;

    std::wstring file(lpszFiles);
    if (_wcsnicmp(file.c_str(), L"reg://", 6) != 0) return NULL;

    std::wstring path = file.substr(6);
    while (!path.empty() && path.back() == L'/') path.pop_back();

    WCHAR msg[2048] = {0};

    if (path.empty()) {
        StringCchPrintfW(msg, 2048, L"Path:\t%s\nType:\tRoot", file.c_str());
    } else {
        size_t firstSlash = path.find(L'/');
        std::wstring rootKeyStr = (firstSlash == std::wstring::npos) ? path : path.substr(0, firstSlash);
        std::wstring remaining = (firstSlash == std::wstring::npos) ? L"" : path.substr(firstSlash + 1);

        size_t lastSlash = remaining.rfind(L'/');
        std::wstring subPath = (lastSlash == std::wstring::npos) ? L"" : remaining.substr(0, lastSlash);
        std::wstring valueName = (lastSlash == std::wstring::npos) ? remaining : remaining.substr(lastSlash + 1);

        HKEY hRootKey = NULL;
        for (const auto& pair : g_rootKeys) {
            if (_wcsicmp(pair.first.c_str(), rootKeyStr.c_str()) == 0) { hRootKey = pair.second; break; }
        }

        if (hRootKey) {
            HKEY hKey;
            LONG result = subPath.empty() ?
                RegOpenKeyExW(hRootKey, NULL, 0, KEY_READ, &hKey) :
                RegOpenKeyExW(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey);

            if (result == ERROR_SUCCESS) {
                if (valueName.empty()) {
                    DWORD subKeyCount = 0, valueCount = 0;
                    RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);
                    StringCchPrintfW(msg, 2048,
                        L"Path:\t%s\nType:\tKey\nSubkeys:\t%u\nValues:\t%u",
                        file.c_str(), subKeyCount, valueCount);
                } else {
                    DWORD valueType = 0, dataSize = 0;
                    LPCWSTR lpValueName = (valueName == L"(Default)") ? L"" : valueName.c_str();
                    RegQueryValueExW(hKey, lpValueName, NULL, &valueType, NULL, &dataSize);

                    LPCWSTR typeStr = L"Unknown";
                    switch (valueType) {
                        case REG_SZ: typeStr = L"REG_SZ"; break;
                        case REG_EXPAND_SZ: typeStr = L"REG_EXPAND_SZ"; break;
                        case REG_BINARY: typeStr = L"REG_BINARY"; break;
                        case REG_DWORD: typeStr = L"REG_DWORD"; break;
                        case REG_DWORD_BIG_ENDIAN: typeStr = L"REG_DWORD_BIG_ENDIAN"; break;
                        case REG_LINK: typeStr = L"REG_LINK"; break;
                        case REG_MULTI_SZ: typeStr = L"REG_MULTI_SZ"; break;
                        case REG_NONE: typeStr = L"REG_NONE"; break;
                        case REG_QWORD: typeStr = L"REG_QWORD"; break;
                    }

                    StringCchPrintfW(msg, 2048,
                        L"Path:\t%s\nType:\tValue (%s)\nSize:\t%u bytes",
                        file.c_str(), typeStr, dataSize);
                }
                RegCloseKey(hKey);
            } else {
                StringCchPrintfW(msg, 2048, L"Path:\t%s\nType:\tUnknown", file.c_str());
            }
        }
    }

    if (msg[0] == L'\0') return NULL;
    MessageBoxW(hwndParent, msg, L"Registry Properties", MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cbDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;
    
    std::wstring path(lpszPath);
    NormalizePath(path);
    
    if (path.length() <= 6) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Registry"); return TRUE; }
    
    path = path.substr(6);
    size_t lastSlash = path.find_last_of(L'/');
    std::wstring name = (lastSlash != std::wstring::npos) ? path.substr(lastSlash + 1) : path;
    if (name.empty()) { StringCchCopyW(lpszDisplayName, cbDisplayNameMax, L"Registry"); return TRUE; }
    
    StringCchCopyW(lpszDisplayName, cbDisplayNameMax, name.c_str());
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPWSTR lpszPath, BOOL fRoot, LPWSTR lpszNewPath, int cbNewPathMax) {
    if (!lpszPath || !lpszNewPath) return FALSE;
    std::wstring path(lpszPath); NormalizePath(path);
    if (path.length() <= 6) return FALSE;
    if (fRoot) { StringCchCopyW(lpszNewPath, cbNewPathMax, L"reg://"); return TRUE; }
    size_t lastSlash = path.find_last_of(L'/');
    if (lastSlash == std::wstring::npos || lastSlash < 6) { StringCchCopyW(lpszNewPath, cbNewPathMax, L"reg://"); return TRUE; }
    StringCchCopyW(lpszNewPath, cbNewPathMax, path.substr(0, lastSlash).c_str()); return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3) {
    switch (propId) {
        case VFSPROP_FUNCAVAILABILITY: {
            unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
            *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_MAKEDIR |
                VFSFUNCAVAIL_RENAME | VFSFUNCAVAIL_PROPERTIES |
                VFSFUNCAVAIL_CLIPCOPY | VFSFUNCAVAIL_CLIPCUT | VFSFUNCAVAIL_CLIPPASTE;
            return TRUE;
        }
        case VFSPROP_GETFOLDERICON: return FALSE;
        case VFSPROP_CANSHOWSUBFOLDERS:
            *reinterpret_cast<LPBOOL>(lpPropData) = g_config.showInFolderTree;
            return TRUE;
        case VFSPROP_GETVALIDACTIONS: case VFSPROP_SHOWTHUMBNAILS: case VFSPROP_USEFULLRENAME: case VFSPROP_SUPPORTPATHCOMPLETION: *reinterpret_cast<LPBOOL>(lpPropData) = TRUE; return TRUE;
        case VFSPROP_SHOWFILEINFO: *reinterpret_cast<LPBOOL>(lpPropData) = FALSE; return TRUE;
    } return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hFile, unsigned __int64* piFileSize) {
    if (hFile) { RegFileInfo* ctx = (RegFileInfo*)hFile; if (piFileSize) *piFileSize = ctx->data.size(); return TRUE; } 
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_REG_CONFIG), hWndParent, ConfigDialogProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    MessageBoxW(hWndParent, L"Registry VFS Plugin v3.0.0\n(c) 2026", L"About", MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    if (pUSBSafeData) pUSBSafeData->pszOtherExports[0] = L'\0';
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
