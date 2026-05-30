#!/usr/bin/env python3
# fix_processvfs_layout.py - 修复ProcessVFS配置对话框布局（新参数）

import re

file_path = r"D:\VFS\ProcessVFS\src\ProcessVFS.cpp"

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 修复WM_SIZE处理函数
old_wm_size = """case WM_SIZE:
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

new_wm_size = """case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width == 0 || height == 0) return (INT_PTR)TRUE;
            int btnHeight = 12;
            int margin = 4;
            int btnTop = height - margin - btnHeight;
            int btnWidth = 50;
            int openTaskMgrWidth = 54;
            SetWindowPos(GetDlgItem(hDlg, IDOK), NULL, width - margin - btnWidth * 3 - margin * 2, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hDlg, IDCANCEL), NULL, width - margin - btnWidth * 2 - margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hDlg, IDC_APPLY), NULL, width - margin - btnWidth, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, openTaskMgrWidth, 0, SWP_NOSIZE | SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hDlg, IDC_RESET_DEFAULTS), NULL, margin + openTaskMgrWidth + margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            int listHeight = height - margin * 3 - btnHeight;
            SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 64, listHeight, SWP_NOZORDER);
            return (INT_PTR)TRUE;
        }"""

if old_wm_size in content:
    content = content.replace(old_wm_size, new_wm_size)
    print("✅ 已修复 WM_SIZE 处理函数")
    print("  - 按钮高度: 14px → 12px")
    print("  - 导航列表宽度: 120px → 64px")
    print("  - 打开任务管理按钮宽度: 50px → 54px")
    print("  - 导航列表高度: height - 26px → height - 24px")
else:
    print("❌ 未找到 WM_SIZE 代码（可能已修改）")
    # 尝试查找类似代码
    if "case WM_SIZE:" in content:
        print("   - 找到 WM_SIZE，但代码不匹配")

# 保存文件
with open(file_path, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"\n✅ 文件已保存: {file_path}")
print("\n请重新编译 ProcessVFS.dll")
