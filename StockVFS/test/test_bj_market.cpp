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

std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
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
                                            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    if (!hostHeader.empty()) headers += L"Host: " + hostHeader + L"\r\n";
    if (host.find(L"sinajs.cn") != std::wstring::npos) headers += L"Referer: https://finance.sina.com.cn\r\n";
    if (host.find(L"eastmoney.com") != std::wstring::npos) headers += L"Referer: https://quote.eastmoney.com\r\n";

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &sz, NULL);

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
    printf("  HTTP Status: %lu, Response length: %zu\n", statusCode, response.length());
    return response;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== BJ Market & Sina Node Test ===\n");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    printf("\n--- Test 1: Sina BJ node (bj_a) ---\n");
    std::string r1 = SendHttpGet(L"vip.stock.finance.sina.com.cn",
                                  L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=bj_a&symbol=&_s_r_a=auto");
    if (!r1.empty()) {
        std::string utf8 = GbkToUtf8(r1);
        printf("  Response (first 500): %.500s\n", utf8.c_str());
    }

    printf("\n--- Test 2: Sina BJ node (bj_bx) ---\n");
    std::string r2 = SendHttpGet(L"vip.stock.finance.sina.com.cn",
                                  L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=bj_bx&symbol=&_s_r_a=auto");
    if (!r2.empty()) {
        std::string utf8 = GbkToUtf8(r2);
        printf("  Response (first 500): %.500s\n", utf8.c_str());
    }

    printf("\n--- Test 3: Sina BJ node (neeq_bj) ---\n");
    std::string r3 = SendHttpGet(L"vip.stock.finance.sina.com.cn",
                                  L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=neeq_bj&symbol=&_s_r_a=auto");
    if (!r3.empty()) {
        std::string utf8 = GbkToUtf8(r3);
        printf("  Response (first 500): %.500s\n", utf8.c_str());
    }

    printf("\n--- Test 4: Sina BJ quote (bj430047) ---\n");
    std::string r4 = SendHttpGet(L"hq.sinajs.cn", L"/list=bj430047");
    if (!r4.empty()) {
        std::string utf8 = GbkToUtf8(r4);
        printf("  Response: %s\n", utf8.c_str());
    }

    printf("\n--- Test 5: Sina BJ quote (bj830799) ---\n");
    std::string r5 = SendHttpGet(L"hq.sinajs.cn", L"/list=bj830799");
    if (!r5.empty()) {
        std::string utf8 = GbkToUtf8(r5);
        printf("  Response: %s\n", utf8.c_str());
    }

    printf("\n--- Test 6: Sina SH+A list (hs_a) ---\n");
    std::string r6 = SendHttpGet(L"vip.stock.finance.sina.com.cn",
                                  L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=hs_a&symbol=&_s_r_a=auto");
    if (!r6.empty()) {
        std::string utf8 = GbkToUtf8(r6);
        printf("  Response (first 500): %.500s\n", utf8.c_str());
    }

    printf("\n=== Test Complete ===\n");
    WSACleanup();
    return 0;
}
