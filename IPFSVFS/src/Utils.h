#pragma once
#include <windows.h>
#include <strsafe.h>
#include <string>
#include <iomanip>
#include <sstream>

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
    return result;
}

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
    return result;
}

inline FILETIME ParseIPFSTime(const std::string& timeStr) {
    FILETIME ft = { 0 };
    SYSTEMTIME st = { 0 };
    int y, M, d, h, m, s;
    if (sscanf_s(timeStr.c_str(), "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s) == 6) {
        st.wYear = y; st.wMonth = M; st.wDay = d;
        st.wHour = h; st.wMinute = m; st.wSecond = s;
        SystemTimeToFileTime(&st, &ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    return ft;
}

inline std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            escaped << c;
        } else {
            escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

inline std::wstring FormatFileSize(uint64_t size) {
    WCHAR buf[64];
    if (size < 1024) {
        StringCchPrintfW(buf, 64, L"%llu B", size);
    } else if (size < 1024 * 1024) {
        StringCchPrintfW(buf, 64, L"%.1f KB", size / 1024.0);
    } else if (size < 1024ULL * 1024 * 1024) {
        StringCchPrintfW(buf, 64, L"%.1f MB", size / (1024.0 * 1024));
    } else if (size < 1024ULL * 1024 * 1024 * 1024) {
        StringCchPrintfW(buf, 64, L"%.2f GB", size / (1024.0 * 1024 * 1024));
    } else {
        StringCchPrintfW(buf, 64, L"%.2f TB", size / (1024.0 * 1024 * 1024 * 1024));
    }
    return std::wstring(buf);
}
