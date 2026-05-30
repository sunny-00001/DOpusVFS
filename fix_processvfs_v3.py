#!/usr/bin/env python3
# fix_processvfs_v3.py - 按新参数修复布局

# 新参数：
# 1. 导航列表宽度 = 40px
# 2. 右侧内容区域位置X = 48px
# 3. 窗口宽度=300px，高度=250px (已设置)
# 4. 结束方式下方选择框(IDC_KILL_METHOD)高度=10px，宽度-10px
# 5. 默认操作下方选择框(IDC_DEFAULT_ACTION)高度=10px，宽度-10px

import re

# ============================================================
# 1. 修复资源文件 (resource.rc)
# ============================================================
rc_file = r"D:\VFS\ProcessVFS\resource.rc"
with open(rc_file, 'r', encoding='utf-8') as f:
    rc_content = f.read()

print("=== 修复资源文件 resource.rc ===\n")

# 1.1 导航列表宽度：50 → 40
old_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 50, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
new_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 40, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
if old_nav in rc_content:
    rc_content = rc_content.replace(old_nav, new_nav)
    print("✅ 导航列表宽度: 50 → 40")
else:
    print("❌ 未找到导航列表定义，尝试正则")
    rc_content = re.sub(r'(LISTBOX\s+IDC_NAV_LIST,\s*\d+,\s*\d+,\s*)\d+', r'\g<1>40', rc_content)
    print("✅ 导航列表宽度: → 40 (正则替换)")

# 1.2 右侧内容区X坐标：58 → 48
rc_content = rc_content.replace(', 58, ', ', 48, ')
print("✅ 右侧内容区X坐标: 58 → 48")

# 1.3 第二列X坐标：186 → 176 (48 + 128 = 176)
rc_content = rc_content.replace(', 186, ', ', 176, ')
print("✅ 第二列X坐标: 186 → 176")

# 1.4 默认操作下拉框(IDC_DEFAULT_ACTION)：高度12→10，宽度-10px
# 原：IDC_DEFAULT_ACTION, 48, 48, 100, 12
# 新：IDC_DEFAULT_ACTION, 48, 48, 90, 10
old_default_action = '    COMBOBOX        IDC_DEFAULT_ACTION, 48, 48, 100, 12, CBS_DROPDOWNLIST | WS_TABSTOP'
new_default_action = '    COMBOBOX        IDC_DEFAULT_ACTION, 48, 48, 90, 10, CBS_DROPDOWNLIST | WS_TABSTOP'
if old_default_action in rc_content:
    rc_content = rc_content.replace(old_default_action, new_default_action)
    print("✅ 默认操作下拉框: 宽度100→90, 高度12→10")
else:
    print("⚠️  未找到默认操作下拉框精确匹配")
    # 正则替换
    rc_content = re.sub(r'(IDC_DEFAULT_ACTION,\s*\d+,\s*\d+,\s*)\d+,\s*\d+', r'\g<1>90, 10', rc_content)
    print("✅ 默认操作下拉框: → 90x10 (正则替换)")

# 1.5 结束方式下拉框(IDC_KILL_METHOD)：高度12→10，宽度-10px
# 原：IDC_KILL_METHOD, 48, 12, 100, 12
# 新：IDC_KILL_METHOD, 48, 12, 90, 10
old_kill_method = '    COMBOBOX        IDC_KILL_METHOD, 48, 12, 100, 12, CBS_DROPDOWNLIST | WS_TABSTOP'
new_kill_method = '    COMBOBOX        IDC_KILL_METHOD, 48, 12, 90, 10, CBS_DROPDOWNLIST | WS_TABSTOP'
if old_kill_method in rc_content:
    rc_content = rc_content.replace(old_kill_method, new_kill_method)
    print("✅ 结束方式下拉框: 宽度100→90, 高度12→10")
else:
    print("⚠️  未找到结束方式下拉框精确匹配")
    # 正则替换
    rc_content = re.sub(r'(IDC_KILL_METHOD,\s*\d+,\s*\d+,\s*)\d+,\s*\d+', r'\g<1>90, 10', rc_content)
    print("✅ 结束方式下拉框: → 90x10 (正则替换)")

# 保存资源文件
with open(rc_file, 'w', encoding='utf-8') as f:
    f.write(rc_content)
print(f"\n✅ 资源文件已保存: {rc_file}\n")

# ============================================================
# 2. 修复C++文件 (ProcessVFS.cpp)
# ============================================================
cpp_file = r"D:\VFS\ProcessVFS\src\ProcessVFS.cpp"
with open(cpp_file, 'r', encoding='utf-8') as f:
    cpp_content = f.read()

print("=== 修复C++文件 ProcessVFS.cpp ===\n")

# 2.1 修复WM_SIZE：导航列表宽度 50 → 40
old_wm_nav = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 50, listHeight, SWP_NOZORDER);'
new_wm_nav = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 40, listHeight, SWP_NOZORDER);'
if old_wm_nav in cpp_content:
    cpp_content = cpp_content.replace(old_wm_nav, new_wm_nav)
    print("✅ WM_SIZE: 导航列表宽度 50 → 40")
else:
    print("❌ 未找到WM_SIZE中的导航列表设置")

# 2.2 修复WM_SIZE：右侧内容区X = 48px
# 查找rightContentX设置
old_right_x = '        int rightContentX = 58;'
new_right_x = '        int rightContentX = 48;'
if old_right_x in cpp_content:
    cpp_content = cpp_content.replace(old_right_x, new_right_x)
    print("✅ WM_SIZE: rightContentX 58 → 48")
else:
    print("⚠️  未找到rightContentX设置，添加...")
    # 在WM_SIZE中添加
    if 'rightContentX' not in cpp_content:
        old_pattern = '        int listHeight = height - margin * 3 - btnHeight;'
        new_pattern = '''        int listHeight = height - margin * 3 - btnHeight;
        
        // 右侧内容区位置
        int rightContentX = 48;
        int rightContentWidth = width - rightContentX - margin;'''
        if old_pattern in cpp_content:
            cpp_content = cpp_content.replace(old_pattern, new_pattern)
            print("✅ WM_SIZE: 添加rightContentX = 48")

# 保存C++文件
with open(cpp_file, 'w', encoding='utf-8') as f:
    f.write(cpp_content)
print(f"\n✅ C++文件已保存: {cpp_file}")

print("\n" + "="*60)
print("请重新编译 ProcessVFS.dll")
print("编译命令: cd D:\\VFS\\ProcessVFS && compile.bat")
print("="*60)
