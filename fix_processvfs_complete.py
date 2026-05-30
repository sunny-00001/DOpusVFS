#!/usr/bin/env python3
# fix_processvfs_complete.py - 完整修复ProcessVFS配置对话框布局

import re

# 修复资源文件
rc_file = r"D:\VFS\ProcessVFS\resource.rc"
with open(rc_file, 'r', encoding='utf-8') as f:
    rc_content = f.read()

print("=== 修复资源文件 resource.rc ===\n")

# 1. 导航列表宽度：120 → 64
old_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 120, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
new_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 64, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
if old_nav in rc_content:
    rc_content = rc_content.replace(old_nav, new_nav)
    print("✅ 导航列表宽度: 120 → 64")
else:
    print("❌ 未找到导航列表定义")

# 2. 按钮高度：14 → 12 (IDOK, IDCANCEL, IDC_APPLY, IDC_OPEN_TASKMGR, IDC_RESET_DEFAULTS)
button_replacements = [
    ('    DEFPUSHBUTTON   "OK", IDOK, 338, 432, 50, 14', '    DEFPUSHBUTTON   "OK", IDOK, 338, 432, 50, 12'),
    ('    PUSHBUTTON      "Cancel", IDCANCEL, 392, 432, 50, 14', '    PUSHBUTTON      "Cancel", IDCANCEL, 392, 432, 50, 12'),
    ('    PUSHBUTTON      "Apply", IDC_APPLY, 446, 432, 50, 14', '    PUSHBUTTON      "Apply", IDC_APPLY, 446, 432, 50, 12'),
    ('    PUSHBUTTON      "TaskMgr", IDC_OPEN_TASKMGR, 4, 432, 50, 14', '    PUSHBUTTON      "TaskMgr", IDC_OPEN_TASKMGR, 4, 432, 54, 12'),
    ('    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 58, 432, 50, 14', '    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 58, 432, 50, 12'),
]

for old, new in button_replacements:
    if old in rc_content:
        rc_content = rc_content.replace(old, new)
        print(f"✅ {old.split(',')[0].strip()} 高度: 14 → 12")
    else:
        print(f"❌ 未找到: {old.split(',')[0].strip()}")

# 3. 右侧内容区X坐标：128 → 68 (第一列控件)
# 第二列：255 → 196 (68 + 128 = 196，保持128px间距)
rc_lines = rc_content.split('\n')
new_rc_lines = []
for line in rc_lines:
    # 第一列：X=128 → X=68
    if ', 128, ' in line and 'LTEXT' in line or 'CONTROL' in line or 'EDITTEXT' in line or 'COMBOBOX' in line:
        line = line.replace(', 128, ', ', 68, ')
    # 第二列：X=255 → X=196
    if ', 255, ' in line:
        line = line.replace(', 255, ', ', 196, ')
    new_rc_lines.append(line)

rc_content = '\n'.join(new_rc_lines)
print("✅ 右侧内容区X坐标: 128 → 68")
print("✅ 第二列X坐标: 255 → 196")

# 4. 下拉框高度：10 → 8 (减少2px)
rc_content = rc_content.replace(', 100, 10,', ', 100, 8,')

# 5. 浏览按钮(IDC_BROWSE_LOG)高度：14 → 12
rc_content = rc_content.replace('IDC_BROWSE_LOG, 213, 60, 30, 14', 'IDC_BROWSE_LOG, 148, 60, 30, 12')
print("✅ 下拉框高度: 10 → 8")
print("✅ 浏览按钮高度: 14 → 12")
print("✅ 浏览按钮X坐标: 213 → 148")

# 保存资源文件
with open(rc_file, 'w', encoding='utf-8') as f:
    f.write(rc_content)
print(f"\n✅ 资源文件已保存: {rc_file}\n")

# 修复C++文件中的WM_SIZE代码
cpp_file = r"D:\VFS\ProcessVFS\src\ProcessVFS.cpp"
with open(cpp_file, 'r', encoding='utf-8') as f:
    cpp_content = f.read()

print("=== 修复C++文件 ProcessVFS.cpp ===\n")

# 修复WM_SIZE中的按钮位置计算
# 打开任务管理按钮应该是54px宽
old_wm_size_part = '''        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);'''
new_wm_size_part = '''        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, openTaskMgrWidth, btnHeight, SWP_NOZORDER);'''
if old_wm_size_part in cpp_content:
    cpp_content = cpp_content.replace(old_wm_size_part, new_wm_size_part)
    print("✅ IDC_OPEN_TASKMGR: 使用openTaskMgrWidth=54和btnHeight=12")
else:
    print("❌ 未找到IDC_OPEN_TASKMGR的SetWindowPos调用")

# 对于其他按钮，需要获取当前宽度并保持，只改高度
# 但为了简化，我们在WM_SIZE中统一处理所有按钮的高度
old_wm_size_full = '''    case WM_SIZE:
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
        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, openTaskMgrWidth, btnHeight, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_RESET_DEFAULTS), NULL, margin + openTaskMgrWidth + margin, btnTop, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        int listHeight = height - margin * 3 - btnHeight;
        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 64, listHeight, SWP_NOZORDER);
        return (INT_PTR)TRUE;
    }'''

# 新版本：所有按钮都显式设置高度=12，保持宽度不变
new_wm_size_full = '''    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width == 0 || height == 0) return (INT_PTR)TRUE;
        int btnHeight = 12;
        int margin = 4;
        int btnTop = height - margin - btnHeight;
        int btnWidth = 50;
        int openTaskMgrWidth = 54;
        
        // 所有按钮高度=12px，打开任务管理宽度=54px
        HWND hBtn;
        RECT rcBtn;
        
        // IDOK: 确定按钮
        hBtn = GetDlgItem(hDlg, IDOK);
        GetWindowRect(hBtn, &rcBtn);
        MapWindowPoints(NULL, hDlg, (POINT*)&rcBtn, 2);
        SetWindowPos(hBtn, NULL, width - margin - btnWidth * 3 - margin * 2, btnTop, rcBtn.right - rcBtn.left, btnHeight, SWP_NOZORDER);
        
        // IDCANCEL: 取消按钮
        hBtn = GetDlgItem(hDlg, IDCANCEL);
        GetWindowRect(hBtn, &rcBtn);
        MapWindowPoints(NULL, hDlg, (POINT*)&rcBtn, 2);
        SetWindowPos(hBtn, NULL, width - margin - btnWidth * 2 - margin, btnTop, rcBtn.right - rcBtn.left, btnHeight, SWP_NOZORDER);
        
        // IDC_APPLY: 应用按钮
        hBtn = GetDlgItem(hDlg, IDC_APPLY);
        GetWindowRect(hBtn, &rcBtn);
        MapWindowPoints(NULL, hDlg, (POINT*)&rcBtn, 2);
        SetWindowPos(hBtn, NULL, width - margin - btnWidth, btnTop, rcBtn.right - rcBtn.left, btnHeight, SWP_NOZORDER);
        
        // IDC_OPEN_TASKMGR: 打开任务管理（宽度=54px）
        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, openTaskMgrWidth, btnHeight, SWP_NOZORDER);
        
        // IDC_RESET_DEFAULTS: 重置默认
        hBtn = GetDlgItem(hDlg, IDC_RESET_DEFAULTS);
        GetWindowRect(hBtn, &rcBtn);
        MapWindowPoints(NULL, hDlg, (POINT*)&rcBtn, 2);
        SetWindowPos(hBtn, NULL, margin + openTaskMgrWidth + margin, btnTop, rcBtn.right - rcBtn.left, btnHeight, SWP_NOZORDER);
        
        // 导航列表：宽度=64px，高度=height-24px
        int listHeight = height - margin * 3 - btnHeight;
        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 64, listHeight, SWP_NOZORDER);
        
        // 右侧内容区：X=68px
        int rightContentX = 68;
        int rightContentWidth = width - rightContentX - margin;
        // TODO: 如果需要，可以在这里调整右侧控件的position
        
        return (INT_PTR)TRUE;
    }'''

if old_wm_size_full in cpp_content:
    cpp_content = cpp_content.replace(old_wm_size_full, new_wm_size_full)
    print("✅ WM_SIZE: 所有按钮高度=12px")
    print("✅ WM_SIZE: 打开任务管理宽度=54px")
    print("✅ WM_SIZE: 导航列表宽度=64px")
else:
    print("❌ 未找到完整的WM_SIZE代码")

# 保存C++文件
with open(cpp_file, 'w', encoding='utf-8') as f:
    f.write(cpp_content)
print(f"\n✅ C++文件已保存: {cpp_file}")
print("\n请重新编译 ProcessVFS.dll")
