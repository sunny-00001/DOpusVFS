#pragma once
#include <windows.h>
#include <string>

bool ShowPropertyDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote);
