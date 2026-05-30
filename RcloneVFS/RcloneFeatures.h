#pragma once
#include <string>
#include "DataStructs.h"

class RcloneFeatures {
public:
    static RcloneBackendFeatures Get(const std::wstring& fs, const std::wstring& remoteType);
    static void Invalidate(const std::wstring& fs);
    static void InvalidateAll();

private:
    static RcloneBackendFeatures LookupKnownType(const std::wstring& remoteType);
    static RcloneBackendFeatures DetectViaApi(const std::wstring& fs);
};
