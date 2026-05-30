import re

file_path = r"D:\VFS\ProcessVFS\src\ProcessVFS.cpp"

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 修复1: WM_DRAWITEM中设置右边距4px
old_drawitem = """                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);"""

new_drawitem = """                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                rcText.right -= 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);"""

if old_drawitem in content:
    content = content.replace(old_drawitem, new_drawitem)
    print("✓ 修复1: WM_DRAWITEM添加右边距4px")
else:
    print("✗ 修复1: 未找到WM_DRAWITEM代码")

# 修复2: WM_SIZE中所有SetWindowPos调用添加SWP_NOSIZE标志
# 按钮移动时需要保持大小不变
old_size = """    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width == 0 || height == 0) return (INT_PTR)TRUE;
        int btnHeight = 14;
        int margin = 4;
        int btnTop = height - margin - btnHeight;
        int btnWidth = 50;
        SetWindowPos(GetDlgItem(hDlg, IDOK), NULL, width - margin - btnWidth * 3 - margin * 2, btnTop, 0, 0, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDCANCEL), NULL, width - margin - btnWidth * 2 - margin, btnTop, 0, 0, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_APPLY), NULL, width - margin - btnWidth, btnTop, 0, 0, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, 0, 0, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_RESET_DEFAULTS), NULL, margin + btnWidth + margin, btnTop, 0, 0, SWP_NOZORDER);
        int listHeight = height - margin * 2 - btnHeight - margin;
        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, 0, 0, 120, listHeight, SWP_NOMOVE | SWP_NOZORDER);
        return (INT_PTR)TRUE;
    }"""

new_size = """    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width == 0 || height == 0) return (INT_PTR)TRUE;
        int btnHeight = 14;
        int margin = 4;
        int btnTop = height - margin - btnHeight;
        int btnWidth = 50;
        SetWindowPos(GetDlgItem(hDlg, IDOK), NULL, width - margin - btnWidth * 3 - margin * 2, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDCANCEL), NULL, width - margin - btnWidth * 2 - margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_APPLY), NULL, width - margin - btnWidth, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_RESET_DEFAULTS), NULL, margin + btnWidth + margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        int listHeight = height - margin * 3 - btnHeight;
        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, 4, 4, 120, listHeight, SWP_NOZORDER);
        return (INT_PTR)TRUE;
    }"""

if old_size in content:
    content = content.replace(old_size, new_size)
    print("✓ 修复2: WM_SIZE添加SWP_NOSIZE标志，修正导航栏高度计算")
else:
    print("✗ 修复2: 未找到WM_SIZE代码")
    # 尝试查找并手动修复
    print("尝试查找WM_SIZE...")
    if "case WM_SIZE:" in content:
        print("找到WM_SIZE，但需要手动检查")
    else:
        print("未找到WM_SIZE")

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("\n完成！现在需要编译项目。")
