#pragma once
#include <windows.h>
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

inline FILETIME ParseRcloneTime(const std::string& timeStr) {
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
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

inline std::wstring FormatFileSize(uint64_t size) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(1);
    
    if (size >= 1024ULL * 1024 * 1024 * 1024) {
        oss << (double)size / (1024.0 * 1024 * 1024 * 1024) << L" TB";
    } else if (size >= 1024ULL * 1024 * 1024) {
        oss << (double)size / (1024.0 * 1024 * 1024) << L" GB";
    } else if (size >= 1024ULL * 1024) {
        oss << (double)size / (1024.0 * 1024) << L" MB";
    } else if (size >= 1024) {
        oss << (double)size / 1024.0 << L" KB";
    } else {
        oss << size << L" B";
    }
    
    return oss.str();
}
