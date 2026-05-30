import re

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: Change DT_CENTER to DT_LEFT for proper left margin
old_draw = '                RECT rcText = lpdis->rcItem; rcText.left += 4; rcText.right -= 4;\n                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'
new_draw = '                RECT rcText = lpdis->rcItem; rcText.left += 4;\n                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);'

if old_draw in content:
    content = content.replace(old_draw, new_draw)
    print('Fixed: Changed DT_CENTER to DT_LEFT')
else:
    print('Warning: Old draw code not found, trying alternative...')
    # Try without rcText.right -= 4
    old_draw2 = '                RECT rcText = lpdis->rcItem; rcText.left += 4; rcText.right -= 4;\n                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'
    if old_draw2 in content:
        content = content.replace(old_draw2, new_draw)
        print('Fixed with alternative method')
    else:
        print('Error: Could not find the draw code')

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Done')
