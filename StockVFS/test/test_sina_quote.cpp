#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

std::string GbkToUtf8(const std::string& gbk) {
    if (gbk.empty()) return "";
    int wlen = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (wlen <= 0) return "";
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], wlen);
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false,
                         const std::wstring& extraHeaders = L"") {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    std::wstring connectHost = host;
    std::wstring hostHeader;

    {
        std::string hostA;
        hostA.reserve(host.size());
        for (auto c : host) hostA.push_back((char)c);

        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* result = nullptr;
        if (getaddrinfo(hostA.c_str(), nullptr, &hints, &result) == 0 && result) {
            char ipStr[INET_ADDRSTRLEN] = {};
            struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
            std::wstring ipW(ipStr, ipStr + strlen(ipStr));
            connectHost = ipW;
            hostHeader = host;
            freeaddrinfo(result);
        }
    }

    HINTERNET hConnect = WinHttpConnect(hSession, connectHost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    if (!hostHeader.empty()) headers += L"Host: " + hostHeader + L"\r\n";
    if (host.find(L"eastmoney.com") != std::wstring::npos) {
        headers += L"Referer: https://quote.eastmoney.com\r\n";
    }
    if (host.find(L"sinajs.cn") != std::wstring::npos) {
        headers += L"Referer: https://finance.sina.com.cn\r\n";
    }
    if (!extraHeaders.empty()) headers += extraHeaders;

    printf("  Request headers: %ls\n", headers.c_str());

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) { 
        printf("  WinHttpSendRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); 
        return ""; 
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) { 
        printf("  WinHttpReceiveResponse failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); 
        return ""; 
    }

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &sz, NULL);

    std::string response;
    DWORD dwAvailable = 0;
    do {
        WinHttpQueryDataAvailable(hRequest, &dwAvailable);
        if (dwAvailable > 0) {
            std::vector<char> buf(dwAvailable + 1);
            DWORD dwRead = 0;
            WinHttpReadData(hRequest, buf.data(), dwAvailable, &dwRead);
            if (dwRead > 0) response.append(buf.data(), dwRead);
        }
    } while (dwAvailable > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    printf("  HTTP Status: %lu\n", statusCode);
    return response;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== Sina Quote API Test ===\n");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    printf("\n--- Test 1: Sina HK Quote (hk00001) with Referer ---\n");
    std::string r1 = SendHttpGet(L"hq.sinajs.cn", L"/list=hk00001", 80, false,
                                  L"Referer: https://finance.sina.com.cn\r\n");
    if (!r1.empty()) printf("  Response: %s\n", GbkToUtf8(r1).c_str());

    printf("\n--- Test 2: Sina US Quote (gb_aapl) with Referer ---\n");
    std::string r2 = SendHttpGet(L"hq.sinajs.cn", L"/list=gb_aapl", 80, false,
                                  L"Referer: https://finance.sina.com.cn\r\n");
    if (!r2.empty()) printf("  Response: %s\n", GbkToUtf8(r2).c_str());

    printf("\n--- Test 3: Sina SH Quote (sh600519) with Referer ---\n");
    std::string r3 = SendHttpGet(L"hq.sinajs.cn", L"/list=sh600519", 80, false,
                                  L"Referer: https://finance.sina.com.cn\r\n");
    if (!r3.empty()) printf("  Response: %s\n", GbkToUtf8(r3).c_str());

    printf("\n--- Test 4: Sina HK Quote via HTTPS ---\n");
    std::string r4 = SendHttpGet(L"hq.sinajs.cn", L"/list=hk00001", 443, true,
                                  L"Referer: https://finance.sina.com.cn\r\n");
    if (!r4.empty()) printf("  Response: %s\n", GbkToUtf8(r4).c_str());

    printf("\n--- Test 5: Eastmoney HK via different servers ---\n");
    const wchar_t* servers[] = { L"push2.eastmoney.com", L"82.push2.eastmoney.com", 
                                  L"34.push2.eastmoney.com", L"16.push2.eastmoney.com" };
    for (auto srv : servers) {
        printf("  Server: %ls\n", srv);
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=3&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:128+t:3,m:128+t:4,m:128+t:1,m:128+t:2&fields=f2,f3,f12,f14";
        std::string r = SendHttpGet(srv, path, 80, false);
        if (!r.empty()) {
            std::string utf8 = r;
            if ((unsigned char)r[0] > 0x7f) utf8 = GbkToUtf8(r);
            printf("  Response (first 300 chars): %.300s\n", utf8.c_str());
        }
    }

    printf("\n=== Test Complete ===\n");
    WSACleanup();
    return 0;
}
