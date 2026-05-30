#include "PathParser.h"
#include <strsafe.h>

namespace PathParser {

static bool ContainsDangerousChars(const std::wstring& path) {
    for (wchar_t c : path) {
        if (c == L'\0' || c == L'\n' || c == L'\r' || c == L'\t') {
            return true;
        }
    }
    return false;
}

void NormalizePath(std::wstring& path) {
    for (auto& c : path) { if (c == L'\\') c = L'/'; }
    while (path.length() > 9 && path[9] == L'/') {
        path.erase(9, 1);
    }
    size_t pos;
    while ((pos = path.find(L"//", 9)) != std::wstring::npos) {
        path.erase(pos, 1);
    }
    while (path.length() > 9 && path.back() == L'/') path.pop_back();
}

bool Parse(const std::wstring& fullPath, RclonePathInfo& info) {
    if (fullPath.empty()) return false;
    
    if (fullPath.length() > 4096) return false;
    
    if (ContainsDangerousChars(fullPath)) return false;
    
    std::wstring normalized = fullPath;
    NormalizePath(normalized);

    if (normalized.length() < 9) return false;

    if (normalized.length() == 9) {
        info.isRoot = true;
        info.isRemoteRoot = false;
        info.remote = L"";
        info.remotePath = L"";
        info.fs = L"";
        return true;
    }

    std::wstring pathWithoutProto = normalized.substr(9);
    size_t firstSlash = pathWithoutProto.find(L'/');

    if (firstSlash == std::wstring::npos) {
        info.isRoot = false;
        info.isRemoteRoot = true;
        info.remote = pathWithoutProto;
        info.remotePath = L"";
        info.fs = pathWithoutProto + L":";
    } else {
        info.isRoot = false;
        info.isRemoteRoot = false;
        info.remote = pathWithoutProto.substr(0, firstSlash);
        info.remotePath = pathWithoutProto.substr(firstSlash + 1);
        info.fs = pathWithoutProto.substr(0, firstSlash) + L":";
    }

    return true;
}

bool IsRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, L"rclone://") == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    if (_wcsicmp(s.c_str(), L"rclone:") == 0) return true;
    NormalizePath(s);
    if (s.length() == 9) return true;
    size_t firstSlash = s.find(L'/', 9);
    if (firstSlash == std::wstring::npos) return true;
    return false;
}

std::wstring ExtractFsName(const std::wstring& fs) {
    std::wstring name = fs;
    while (!name.empty() && name.back() == L':') name.pop_back();
    return name;
}

std::wstring FormatSize(uint64_t size) {
    WCHAR buf[64];
    if (size == 0) { StringCchCopyW(buf, 64, L"0 B"); return buf; }
    if (size >= 1099511627776ULL) StringCchPrintfW(buf, 64, L"%.2f TB", (double)size / 1099511627776.0);
    else if (size >= 1073741824ULL) StringCchPrintfW(buf, 64, L"%.2f GB", (double)size / 1073741824.0);
    else if (size >= 1048576ULL) StringCchPrintfW(buf, 64, L"%.2f MB", (double)size / 1048576.0);
    else if (size >= 1024ULL) StringCchPrintfW(buf, 64, L"%.2f KB", (double)size / 1024.0);
    else StringCchPrintfW(buf, 64, L"%llu B", size);
    return buf;
}

std::wstring FormatTime(const FILETIME& ft) {
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return L"-";
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring GetRemoteTypeCN(const std::wstring& type) {
    if (type.empty()) return L"-";
    if (type == L"drive") return L"Google Drive";
    if (type == L"onedrive") return L"OneDrive";
    if (type == L"dropbox") return L"Dropbox";
    if (type == L"s3") return L"Amazon S3";
    if (type == L"swift") return L"OpenStack Swift";
    if (type == L"azureblob") return L"Azure Blob";
    if (type == L"local") return L"本地磁盘";
    if (type == L"ftp") return L"FTP";
    if (type == L"sftp") return L"SFTP";
    if (type == L"webdav") return L"WebDAV";
    if (type == L"pcloud") return L"pCloud";
    if (type == L"mega") return L"Mega";
    if (type == L"box") return L"Box";
    if (type == L"crypt") return L"加密存储";
    if (type == L"union") return L"联合存储";
    if (type == L"cache") return L"缓存存储";
    if (type == L"chunker") return L"分块存储";
    if (type == L"compress") return L"压缩存储";
    if (type == L"alias") return L"别名存储";
    if (type == L"googlecloudstorage") return L"Google Cloud";
    if (type == L"hubic") return L"HubiC";
    if (type == L"b2") return L"Backblaze B2";
    if (type == L"qingstor") return L"青云 QingStor";
    if (type == L"koofr") return L"Koofr";
    if (type == L"mailru") return L"Mail.ru Cloud";
    if (type == L"yandex") return L"Yandex Disk";
    if (type == L"hdfs") return L"HDFS";
    if (type == L"fichier") return L"1Fichier";
    if (type == L"premiumizeme") return L"Premiumize.me";
    if (type == L"putio") return L"Put.io";
    if (type == L"sharefile") return L"ShareFile";
    if (type == L"sia") return L"Sia";
    if (type == L"storj") return L"Storj";
    if (type == L"sugar") return L"SugarSync";
    if (type == L"uptobox") return L"Uptobox";
    if (type == L"jottacloud") return L"Jottacloud";
    return type;
}

}
