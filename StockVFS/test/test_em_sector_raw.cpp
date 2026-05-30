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

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

int main() {
    const wchar_t* host = L"push2.eastmoney.com";
    std::wstring path = L"/api/qt/clist/get?pn=1&pz=10&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14,f104,f105,f128,f136,f140,f141";

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

    std::wstring headers = L"Referer: https://quote.eastmoney.com\r\n";
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

    std::wstring wresp = Utf8ToWide(response);

    FILE* fout = fopen("test_em_sector_raw.txt", "w, ccs=UTF-8");
    if (fout) {
        fwprintf(fout, L"HTTP Status: %d\r\nResponse length: %zu bytes\r\n\r\n", statusCode, response.length());
        fputws(wresp.c_str(), fout);
        fclose(fout);
    }

    std::wcout << L"HTTP Status: " << statusCode << L", Response length: " << response.length() << L" bytes" << std::endl;
    std::wcout << L"Check test_em_sector_raw.txt" << std::endl;
    return 0;
}
