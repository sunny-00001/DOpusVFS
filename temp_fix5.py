import re

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: Navigation text - use DT_LEFT with vertical centering
old_draw = '''                RECT rcText = lpdis->rcItem; rcText.left += 4; rcText.right -= 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'''

new_draw = '''                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);'''

if old_draw in content:
    content = content.replace(old_draw, new_draw)
    print('Fixed: Navigation text now left-aligned with 4px left margin')
else:
    print('Warning: Navigation draw code not found')

# Fix 2: WM_SIZE handler - remove SWP_NOSIZE flag (we ARE moving the buttons)
old_wmsize = '''    case WM_SIZE:
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
        int listHeight = height - margin * 2 - btnHeight - margin;
        SetWindowPos(GetDlgItem(hDlg, IDC_NAV_LIST), NULL, 0, 0, 120, listHeight, SWP_NOSIZE | SWP_NOZORDER);
        return (INT_PTR)TRUE;
    }'''

new_wmsize = '''    case WM_SIZE:
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
    }'''

if old_wmsize in content:
    content = content.replace(old_wmsize, new_wmsize)
    print('Fixed: WM_SIZE handler updated - buttons will now stay at edges')
else:
    print('Error: WM_SIZE code not found')
    # Try to find it
    idx = content.find('case WM_SIZE:')
    if idx != -1:
        print('Found WM_SIZE at index', idx)
        print(repr(content[idx:idx+200]))

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Done')
