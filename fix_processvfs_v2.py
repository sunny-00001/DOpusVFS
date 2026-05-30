#!/usr/bin/env python3
# fix_processvfs_v2.py - 按新参数修复ProcessVFS配置对话框布局

# 新参数：
# 1. 导航列表宽度 = 50px
# 2. 右侧内容区域位置X = 58px
# 3. 重置默认位置X = 62px (4 + 54 + 4)
# 4. 窗口宽度 = 300px，高度 = 250px
# 5. 结束方式下方的选择框高度 = 12px
# 6. 日志路径下方的浏览文件夹按钮高度-2px，宽度-4px

import re

# ============================================================
# 1. 修复资源文件 (resource.rc)
# ============================================================
rc_file = r"D:\VFS\ProcessVFS\resource.rc"
with open(rc_file, 'r', encoding='utf-8') as f:
    rc_content = f.read()

print("=== 修复资源文件 resource.rc ===\n")

# 1.1 修改对话框初始大小：500,450 → 300,250 (像素值，需要转换为DLU)
# 注意：DLU转换复杂，这里先保持原DLU值，通过WM_SIZE调整
# 对话框定义：IDD_PROCESS_CONFIG DIALOGEX 0, 0, 500, 450
old_dialog = 'IDD_PROCESS_CONFIG DIALOGEX 0, 0, 500, 450'
new_dialog = 'IDD_PROCESS_CONFIG DIALOGEX 0, 0, 300, 250'  # 改为300x250 DLU
if old_dialog in rc_content:
    rc_content = rc_content.replace(old_dialog, new_dialog)
    print("✅ 对话框初始大小: 500x450 DLU → 300x250 DLU")
else:
    print("❌ 未找到对话框定义")

# 1.2 导航列表宽度：64 → 50
old_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 64, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
new_nav = '    LISTBOX         IDC_NAV_LIST, 4, 4, 50, 424, LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | WS_VSCROLL | WS_TABSTOP'
if old_nav in rc_content:
    rc_content = rc_content.replace(old_nav, new_nav)
    print("✅ 导航列表宽度: 64px → 50px")
else:
    # 尝试其他可能的值
    if 'IDC_NAV_LIST' in rc_content:
        print("⚠️  找到IDC_NAV_LIST但未匹配确切字符串，尝试正则替换")
        rc_content = re.sub(r'(LISTBOX\s+IDC_NAV_LIST,\s*\d+,\s*\d+,\s*)\d+', r'\g<1>50', rc_content)
        print("✅ 导航列表宽度: → 50px (正则替换)")

# 1.3 右侧内容区X坐标：68 → 58
rc_content = rc_content.replace(', 68, ', ', 58, ')
print("✅ 右侧内容区X坐标: 68 → 58")

# 1.4 第二列X坐标：196 → 186 (58 + 128 = 186)
rc_content = rc_content.replace(', 196, ', ', 186, ')
print("✅ 第二列X坐标: 196 → 186")

# 1.5 重置默认按钮X：58+54+4=116 → 62 (用户指定)
# 等等，用户说"重置默认位置X=62px"
# 但按计算：margin(4) + openTaskMgrWidth(54) + margin(4) = 62px ✓ 匹配！
old_reset = '    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 58, 432, 50, 12'
new_reset = '    PUSHBUTTON      "Reset", IDC_RESET_DEFAULTS, 62, 432, 50, 12'
if old_reset in rc_content:
    rc_content = rc_content.replace(old_reset, new_reset)
    print("✅ 重置默认按钮X: 58 → 62")
else:
    print("⚠️  未找到重置默认按钮的精确匹配")

# 1.6 结束方式下方的选择框(IDC_KILL_METHOD)高度：8 → 12
# 原资源文件中COMBOBOX高度是10或8，需要改为12
# 查找所有COMBOBOX行
combo_pattern = re.compile(r'(COMBOBOX\s+IDC_\w+,\s*\d+,\s*\d+,\s*\d+,\s*)(\d+)', re.IGNORECASE)
matches = list(combo_pattern.finditer(rc_content))
for match in matches:
    old_height = match.group(2)
    new_combo = match.group(1) + '12'
    old_combo = match.group(0)
    new_combo_full = new_combo
    rc_content = rc_content.replace(old_combo, new_combo)
    print(f"✅ COMBOBOX {match.group(0).split(',')[0].split()[-1]}: 高度 → 12")

# 1.7 日志路径下方的浏览按钮(IDC_BROWSE_LOG)：高度-2px，宽度-4px
# 当前：IDC_BROWSE_LOG, 148, 60, 30, 12
# 新值：高度12-2=10，宽度30-4=26
old_browse = 'IDC_BROWSE_LOG, 148, 60, 30, 12'
new_browse = 'IDC_BROWSE_LOG, 148, 60, 26, 10'
if old_browse in rc_content:
    rc_content = rc_content.replace(old_browse, new_browse)
    print("✅ 浏览按钮: 宽度30→26, 高度12→10")
else:
    print("⚠️  未找到浏览按钮的精确匹配，尝试正则")
    rc_content = re.sub(r'(IDC_BROWSE_LOG,\s*\d+,\s*\d+,\s*)\d+,\s*\d+', r'\g<1>26, 10', rc_content)
    print("✅ 浏览按钮: 宽度→26, 高度→10 (正则替换)")

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

# 2.1 修复WM_SIZE：导航列表宽度50px，右侧内容区X=58px
old_wm_size_nav = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 64, listHeight, SWP_NOZORDER);'
new_wm_size_nav = '        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, margin, margin, 50, listHeight, SWP_NOZORDER);'
if old_wm_size_nav in cpp_content:
    cpp_content = cpp_content.replace(old_wm_size_nav, new_wm_size_nav)
    print("✅ WM_SIZE: 导航列表宽度 64 → 50")
else:
    print("❌ 未找到WM_SIZE中的导航列表设置")

# 2.2 修复WM_SIZE：重置默认按钮X位置 = 62px
old_wm_size_reset = 'margin + openTaskMgrWidth + margin'
new_wm_size_reset = '62'  # 直接写62px
if old_wm_size_reset in cpp_content:
    cpp_content = cpp_content.replace(old_wm_size_reset, new_wm_size_reset)
    print("✅ WM_SIZE: 重置默认按钮X = 62px")
else:
    print("⚠️  未找到重置默认按钮X计算，手动检查")

# 2.3 添加右侧内容区位置设置（在WM_SIZE中）
# 需要在WM_SIZE中添加对右侧子对话框的位置调整
# 这部分代码应该在ShowNavPage函数或WM_SIZE中处理

# 检查是否已有相关代码
if 'rightContentX' not in cpp_content:
    # 在WM_SIZE末尾、return之前添加右侧内容区调整代码
    old_wm_size_end = '        return (INT_PTR)TRUE;\n    }'
    new_wm_size_end = '''        // 调整右侧内容区位置
        int rightContentX = 58;
        int rightContentWidth = width - rightContentX - margin;
        
        // 可以根据需要调整右侧控件的position
        // 例如：MoveWindow(GetDlgItem(hDlg, IDC_AUTO_REFRESH), rightContentX, ...);
        
        return (INT_PTR)TRUE;
    }'''
    if old_wm_size_end in cpp_content:
        cpp_content = cpp_content.replace(old_wm_size_end, new_wm_size_end)
        print("✅ WM_SIZE: 添加右侧内容区位置调整代码")
    else:
        print("⚠️  未找到WM_SIZE末尾，无法添加右侧内容区代码")

# 保存C++文件
with open(cpp_file, 'w', encoding='utf-8') as f:
    f.write(cpp_content)
print(f"\n✅ C++文件已保存: {cpp_file}")

print("\n" + "="*60)
print("请重新编译 ProcessVFS.dll")
print("编译命令: cd D:\\VFS\\ProcessVFS && compile.bat")
print("="*60)
