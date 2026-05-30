import re

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find and replace WM_SIZE handler
in_wm_size = False
new_lines = []
i = 0
while i < len(lines):
    line = lines[i]
    if 'case WM_SIZE:' in line:
        in_wm_size = True
        # Write new WM_SIZE handler
        new_lines.append('    case WM_SIZE:\n')
        new_lines.append('    {\n')
        new_lines.append('        int width = LOWORD(lParam);\n')
        new_lines.append('        int height = HIWORD(lParam);\n')
        new_lines.append('        if (width == 0 || height == 0) return (INT_PTR)TRUE;\n')
        new_lines.append('        int btnHeight = 14;\n')
        new_lines.append('        int margin = 4;\n')
        new_lines.append('        int btnTop = height - margin - btnHeight;\n')
        new_lines.append('        int btnWidth = 50;\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDOK), NULL, width - margin - btnWidth * 3 - margin * 2, btnTop, 0, 0, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDCANCEL), NULL, width - margin - btnWidth * 2 - margin, btnTop, 0, 0, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDC_APPLY), NULL, width - margin - btnWidth, btnTop, 0, 0, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDC_OPEN_TASKMGR), NULL, margin, btnTop, 0, 0, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDC_RESET_DEFAULTS), NULL, margin + btnWidth + margin, btnTop, 0, 0, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        int listHeight = height - margin * 2 - btnHeight - margin;\n')
        new_lines.append('        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, 0, 0, 120, listHeight, SWP_NOMOVE | SWP_NOZORDER);\n')
        new_lines.append('        return (INT_PTR)TRUE;\n')
        new_lines.append('    }\n')
        # Skip old WM_SIZE handler
        while i < len(lines) and '}' not in lines[i]:
            i += 1
        i += 1  # Skip the closing brace
    else:
        new_lines.append(line)
        i += 1

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print('Successfully updated WM_SIZE handler')
