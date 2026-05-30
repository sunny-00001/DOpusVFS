#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "DataStructs.h"

bool ShowVersionsDialog(HWND hwndParent, const std::wstring& fs, const std::wstring& remote);
std::vector<RcloneVersionInfo> GetVersionsList(const std::wstring& fs, const std::wstring& remote);
