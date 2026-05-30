#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ws2_32.lib")

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

int main() {
    const wchar_t* host = L"vip.stock.finance.sina.com.cn";
    std::wstring path = L"/newSinaHy/newFLJK.php?param=industry";

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { std::wcout << L"WinHttpOpen failed\n"; return 1; }

    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); std::wcout << L"WinHttpConnect failed\n"; return 1; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); std::wcout << L"WinHttpOpenRequest failed\n"; return 1; }

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &sz, NULL);

    std::string response;
    if (statusCode == 200) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::vector<char> buf(avail + 1);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), avail, &read);
            response.append(buf.data(), read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::wstring wresp = GbkToWide(response);

    FILE* fout = fopen("test_sector_raw.txt", "w, ccs=UTF-8");
    if (fout) {
        fwprintf(fout, L"HTTP Status: %d\r\nResponse length: %zu bytes\r\n\r\n", statusCode, response.length());
        fputws(wresp.c_str(), fout);
        fclose(fout);
    }

    std::wcout << L"HTTP Status: " << statusCode << L", Response length: " << response.length() << L" bytes" << std::endl;
    std::wcout << L"Check test_sector_raw.txt" << std::endl;
    return 0;
}
