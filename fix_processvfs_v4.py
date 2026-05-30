#!/usr/bin/env python3
# fix_processvfs_v4.py - 修复布局：按钮位置、导航宽度、下拉框宽度等

# 新参数：
# 1. 导航列表宽度：40 - 4 = 36px
# 2. 右侧内容区域位置X：48 - 4 = 44px
# 3. 窗口宽度=300px，高度=250px (已设置)
# 4. 所有下拉框宽度：90 - 8 = 82px
# 5. 浏览文件夹按钮位置X - 8px
# 6. 修复底部按钮消失问题（按钮Y坐标超出对话框高度）

import re

# ============================================================
# 1. 修复资源文件 (resource.rc)
# ============================================================
rc_file = r"D:\VFS\ProcessVFS\resource.rc"
with open(rc_file, 'r', encoding='utf-8') as f:
    rc_content = f.read()

print("=== 修复资源文件 resource.rc ===\n")

# 1.1 导航列表宽度：40 → 36
old_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 40, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
new_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 36, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
if old_nav in rc_content:
    rc_content = rc_content.replace(old_nav, new_nav)
    print("✅ 导航列表宽度: 40 → 36")
else:
    print("⚠️  未找到导航列表精确匹配，尝试正则")
    rc_content = re.sub(r'(LISTBOX\s+IDC_NAV_LIST,\s*\d+,\s*\d+,\s*)\d+', r'\g<1>36', rc_content)
    print("✅ 导航列表宽度: → 36 (正则替换)")

# 1.2 右侧内容区X坐标：48 → 44
rc_content = rc_content.replace(', 48, ', ', 44, ')
print("✅ 右侧内容区X坐标: 48 → 44")

# 1.3 第二列X坐标：176 → 172 (44 + 128 = 172)
rc_content = rc_content.replace(', 176, ', ', 172, ')
print("✅ 第二列X坐标: 176 → 172")

# 1.4 所有下拉框宽度：90 → 82
# IDC_DEFAULT_ACTION 和 IDC_KILL_METHOD
rc_content = rc_content.replace(', 90, 10,', ', 82, 10,')
print("✅ 所有下拉框宽度: 90 → 82")

# 1.5 浏览文件夹按钮位置X - 8px
# 当前：IDC_BROWSE_LOG, 148, 60, 26, 10
# 新X：148 - 8 = 140
old_browse = 'IDC_BROWSE_LOG, 148, 60, 26, 10'
new_browse = 'IDC_BROWSE_LOG, 140, 60, 26, 10'
if old_browse in rc_content:
    rc_content = rc_content.replace(old_browse, new_browse)
    print("✅ 浏览按钮X坐标: 148 → 140")
else:
    print("⚠️  未找到浏览按钮精确匹配")
    rc_content = re.sub(r'(IDC_BROWSE_LOG,\s*)\d+(,\s*60,)', r'\g<1>140\g<2>', rc_content)
    print("✅ 浏览按钮X坐标: → 140 (正则替换)")

# 1.6 【关键】修复底部按钮位置 - Y坐标需要改！
# 对话框高度=250 DLU，按钮高度=12 DLU
# 按钮Y坐标应该是：250 - 4 - 12 = 234 DLU
# 原Y=432已超出对话框底部！

# IDOK: 确定按钮
old_btn_ok = '    DEFPUSHBUTTON   "OK", IDOK, 338, 432, 50, 12'
new_btn_ok = '    DEFPUSHBUTTON   "OK", IDOK, 186, 234, 50, 12'  # X需要重新计算
if old_btn_ok in rc_content:
    rc_content = rc_content.replace(old_btn_ok, new_btn_ok)
    print("✅ 确定按钮位置: Y 432 → 234")
else:
    print("⚠️  未找到确定按钮精确匹配")

# IDCANCEL: 取消按钮
old_btn_cancel = '    PUSHBUTTON      "Cancel", IDCANCEL, 392, 432, 50, 12'
new_btn_cancel = '    PUSHBUTTON      "Cancel", IDCANCEL, 240, 234, 50, 12'
if old_btn_cancel in rc_content:
    rc_content = rc_content.replace(old_btn_cancel, new_btn_cancel)
    print("✅ 取消按钮位置: Y 432 → 234")
else:
    print("⚠️  未找到取消按钮精确匹配")

# IDC_APPLY: 应用按钮
old_btn_apply = '    PUSHBUTTON      "Apply", IDC_APPLY, 446, 432, 50, 12'
new_btn_apply = '    PUSHBUTTON      "Apply", IDC_APPLY, 294, 234, 50, 12'
if old_btn_apply in rc_content:
    rc_content = rc_content.replace(old_btn_apply, new_btn_apply)
    print("✅ 应用按钮位置: Y 432 → 234")
else:
    print("⚠️  未找到应用按钮精确匹配")

# IDC_OPEN_TASKMGR: 打开任务管理按钮
old_btn_taskmgr = '    PUSHBUTTON      "TaskMgr", IDC_OPEN_TASKMGR, 4, 432, 54, 12'
new_btn_taskmgr = '    PUSHBUTTON      "TaskMgr", IDC_OPEN_TASKMGR, 4, 234, 54, 12'
if old_btn_taskmgr in rc_content:
    rc_content = rc_content.replace(old_btn_taskmgr, new_btn_taskmgr)
    print("✅ 打开任务管理按钮位置: Y 432 → 234")
else:
    print("⚠️  未找到打开任务管理按钮精确匹配")

# IDC_RESET_DEFAULTS: 重置默认按钮
old_btn_reset = '    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 62, 432, 50, 12'
new_btn_reset = '    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 62, 234, 50, 12'
if old_btn_reset in rc_content:
    rc_content = rc_content.replace(old_btn_reset, new_btn_reset)
    print("✅ 重置默认按钮位置: Y 432 → 234")
else:
    print("⚠️  未找到重置默认按钮精确匹配")

# 如果上面精确匹配失败，使用正则批量修改所有Y=432的按钮
if ' 432, 50, 12' in rc_content or ' 432, 54, 12' in rc_content:
    print("\n⚠️  检测到Y=432的按钮，使用正则修复...")
    # 匹配按钮行并将Y坐标从432改为234
    rc_content = re.sub(r'(PUSHBUTTON\s+[^,]+,\s*[^,]+,\s*[^,]+,)\s*432(,\s*\d+,\s*12)', r'\g<1>234\g<2>', rc_content)
    rc_content = re.sub(r'(DEFPUSHBUTTON\s+[^,]+,\s*[^,]+,\s*[^,]+,)\s*432(,\s*\d+,\s*12)', r'\g<1>234\g<2>', rc_content)
    print("✅ 所有按钮Y坐标: 432 → 234 (正则替换)")

# 1.7 重新计算按钮X坐标（适应300px宽度）
# 对话框宽度=300 DLU
# 确定按钮X = 300 - 4 - 50 = 246
# 取消按钮X = 300 - 4 - 50*2 - 4 = 192
# 应用按钮X = 300 - 4 - 50 = 246 (应该是最右边)
# 等等，我需要重新理解布局...

# 实际上，对于300 DLU宽度的对话框：
# 右对齐按钮的X坐标应该是：
# IDOK (确定): width - 4 - 50 = 246
# IDCANCEL (取消): width - 4 - 100 - 4 = 192  
# IDC_APPLY (应用): width - 4 - 50 = 246
# 这不对，三个按钮应该并排

# 正确的计算（从右到左）：
# IDC_APPLY: X = 300 - 4 - 50 = 246
# IDCANCEL: X = 246 - 4 - 50 = 192
# IDOK: X = 192 - 4 - 50 = 138

# 使用正则重新设置按钮X坐标
print("\n✅ 重新计算按钮X坐标（适应300 DLU宽度）...")
rc_content = re.sub(r'(DEFPUSHBUTTON\s+"OK",\s*IDOK,\s*)\d+(,\s*234,)', r'\g<1>138\g<2>', rc_content)
rc_content = re.sub(r'(PUSHBUTTON\s+"Cancel",\s*IDCANCEL,\s*)\d+(,\s*234,)', r'\g<1>192\g<2>', rc_content)
rc_content = re.sub(r'(PUSHBUTTON\s+"Apply",\s*IDC_APPLY,\s*)\d+(,\s*234,)', r'\g<1>246\g<2>', rc_content)

# 保存资源文件
with open(rc_file, 'w', encoding='utf-8') as f:
    f.write(rc_content)
print(f"\n✅ 资源文件已保存: {rc_file}\n")

# ============================================================
# 2. 修复C++文件 (ProcessVFS.cpp) - WM_SIZE中的按钮位置
# ============================================================
cpp_file = r"D:\VFS\ProcessVFS\src\ProcessVFS.cpp"
with open(cpp_file, 'r', encoding='utf-8') as f:
    cpp_content = f.read()

print("=== 修复C++文件 ProcessVFS.cpp ===\n")

# 2.1 修复WM_SIZE：导航列表宽度 40 → 36
old_nav_wm = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 40, listHeight, SWP_NOZORDER);'
new_nav_wm = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 36, listHeight, SWP_NOZORDER);'
if old_nav_wm in cpp_content:
    cpp_content = cpp_content.replace(old_nav_wm, new_nav_wm)
    print("✅ WM_SIZE: 导航列表宽度 40 → 36")
else:
    print("❌ 未找到WM_SIZE中的导航列表设置")

# 2.2 修复WM_SIZE：右侧内容区X = 44px
old_right_x = '        int rightContentX = 48;'
new_right_x = '        int rightContentX = 44;'
if old_right_x in cpp_content:
    cpp_content = cpp_content.replace(old_right_x, new_right_x)
    print("✅ WM_SIZE: rightContentX 48 → 44")
else:
    print("⚠️  未找到rightContentX设置")

# 2.3 修复WM_SIZE：所有下拉框宽度 - 8px
# 需要在WM_INITDIALOG或WM_SIZE中设置下拉框宽度
# 先检查是否有相关代码
if 'ComboBox_SetMinVisible' not in cpp_content and 'CB_SETITEMHEIGHT' not in cpp_content:
    print("ℹ️  未找到下拉框高度设置代码（将在资源文件中控制）")

# 保存C++文件
with open(cpp_file, 'w', encoding='utf-8') as f:
    f.write(cpp_content)
print(f"\n✅ C++文件已保存: {cpp_file}")

print("\n" + "="*60)
print("请重新编译 ProcessVFS.dll")
print("编译命令: cd D:\\VFS\\ProcessVFS && compile.bat")
print("="*60)
