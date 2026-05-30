import re

with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find WM_DRAWITEM and modify rcText margins
old_drawitem = '''                RECT rcText = lpdis->rcItem; rcText.left += 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'''

new_drawitem = '''                RECT rcText = lpdis->rcItem; rcText.left += 4; rcText.right -= 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'''

if old_drawitem in content:
    content = content.replace(old_drawitem, new_drawitem)
    with open('D:/VFS/ProcessVFS/src/ProcessVFS.cpp', 'w', encoding='utf-8') as f:
        f.write(content)
    print('Successfully updated WM_DRAWITEM handler')
else:
    print('Error: Old DRAWITEM code not found')
    # Find similar code
    idx = content.find('rcText.left += 4')
    if idx != -1:
        print('Found rcText at index', idx)
        print(repr(content[idx:idx+100]))
