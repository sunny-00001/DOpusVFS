#pragma once
#include <windows.h>
#include <string>

bool ShowTrashDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remoteType);
