#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &utf8[0], len, nullptr, nullptr);
    return utf8;
}

static std::wstring UrlEncode(const std::wstring& input) {
    std::string utf8 = WideToUtf8(input);
    std::wstring encoded;
    for (unsigned char c : utf8) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += (wchar_t)c;
        } else {
            WCHAR buf[4];
            swprintf_s(buf, L"%%%02X", c);
            encoded += buf;
        }
    }
    return encoded;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    std::wstring keyword = L"腾讯";
    std::wstring encoded = UrlEncode(keyword);
    std::wcout << L"Keyword: " << keyword << std::endl;
    std::wcout << L"URL encoded: " << encoded << std::endl;

    std::wstring path = L"/suggest/type=&key=" + encoded + L"&name=suggestdata";
    std::wcout << L"Full path: " << path << std::endl;

    HINTERNET hSession = WinHttpOpen(L"StockVFS-Debug/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::wcout << L"WinHttpOpen failed: " << GetLastError() << std::endl;
        return 1;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, L"suggest3.sinajs.cn", 80, 0);
    if (!hConnect) {
        std::wcout << L"WinHttpConnect failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return 1;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        std::wcout << L"WinHttpOpenRequest failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n"
                          L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &sz, NULL);
    std::wcout << L"HTTP Status: " << statusCode << std::endl;

    DWORD avail = 0;
    std::string response;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        std::vector<char> buf(avail + 1);
        DWORD read = 0;
        WinHttpReadData(hRequest, buf.data(), avail, &read);
        response.append(buf.data(), read);
    }

    int wlen = MultiByteToWideChar(CP_ACP, 0, response.c_str(), (int)response.length(), nullptr, 0);
    std::wstring wresponse(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, response.c_str(), (int)response.length(), &wresponse[0], wlen);

    std::wcout << L"Response length: " << response.length() << std::endl;
    std::wcout << L"Response (first 500): " << wresponse.substr(0, (std::min)(wresponse.length(), (size_t)500)) << std::endl;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
}
