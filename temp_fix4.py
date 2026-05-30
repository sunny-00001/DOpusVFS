import re

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: Restore DT_CENTER for proper centering with margins
old_draw = '                RECT rcText = lpdis->rcItem; rcText.left += 4;\n                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);'
new_draw = '                RECT rcText = lpdis->rcItem; rcText.left += 4; rcText.right -= 4;\n                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'

if old_draw in content:
    content = content.replace(old_draw, new_draw)
    print('Fixed: Restored DT_CENTER with proper margins')
else:
    print('Warning: Old draw code not found')

# Fix 2: Change SWP_NOMOVE to SWP_NOSIZE in WM_SIZE handler
# SWP_NOMOVE means "don't move" - buttons won't reposition!
# SWP_NOSIZE means "don't resize" - allows repositioning
content = content.replace('SWP_NOMOVE | SWP_NOZORDER);', 'SWP_NOSIZE | SWP_NOZORDER);')

print('Fixed: Changed SWP_NOMOVE to SWP_NOSIZE for button repositioning')

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Done')
