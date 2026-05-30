#pragma once
#include <windows.h>
#include <string>

bool ShowShareDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote);
