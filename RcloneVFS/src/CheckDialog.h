#pragma once
#include <windows.h>
#include <string>

bool ShowCheckDialog(HWND hwndParent, const std::wstring& srcFs, const std::wstring& srcRemote, const std::wstring& dstFs, const std::wstring& dstRemote);
