#pragma once
#include <string>
#include "DataStructs.h"

namespace PathParser {
    void NormalizePath(std::wstring& path);
    bool Parse(const std::wstring& fullPath, RclonePathInfo& info);
    bool IsRootPath(LPCWSTR pszPath);
    std::wstring ExtractFsName(const std::wstring& fs);
    std::wstring FormatSize(uint64_t size);
    std::wstring FormatTime(const FILETIME& ft);
    std::wstring GetRemoteTypeCN(const std::wstring& type);
}
