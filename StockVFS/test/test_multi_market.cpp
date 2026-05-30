#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

std::string GbkToUtf8(const std::string& gbk) {
    if (gbk.empty()) return "";
    int wlen = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (wlen <= 0) return "";
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], wlen);
    return WideToUtf8(wide);
}

std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false) {
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

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

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

void TestEastMoneyAPI(const char* name, const std::wstring& fs) {
    printf("\n--- Testing %s (fs=%ls) ---\n", name, fs.c_str());

    std::wstring path = L"/api/qt/clist/get?pn=1&pz=5&po=1&np=1&fltt=2&invt=2&fid=f3&fs="
                       + fs
                       + L"&fields=f2,f3,f4,f5,f6,f12,f13,f14,f15,f16,f17,f18";

    std::string response = SendHttpGet(L"82.push2.eastmoney.com", path, 80, false);
    if (response.empty()) {
        printf("  FAILED: Empty response\n");
        return;
    }

    std::string utf8;
    if (response.find('\0') != std::string::npos || (unsigned char)response[0] > 0x7f) {
        utf8 = GbkToUtf8(response);
    } else {
        utf8 = response;
    }

    printf("  Response (first 500 chars): %.500s\n", utf8.c_str());
}

void TestSinaHKList() {
    printf("\n--- Testing Sina HK Stock List API ---\n");

    std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=hk_main&symbol=&_s_r_a=auto";
    std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
    if (response.empty()) {
        printf("  FAILED: Empty response\n");
        return;
    }
    printf("  Response (first 500 chars): %.500s\n", response.c_str());
}

void TestSinaHKStockData() {
    printf("\n--- Testing Sina HK Stock Data API ---\n");

    std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHKStockData?page=1&num=5&sort=changepercent&asc=0&node=hk_main";
    std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
    if (response.empty()) {
        printf("  FAILED: Empty response\n");
        return;
    }
    printf("  Response (first 500 chars): %.500s\n", response.c_str());
}

void TestSinaHKQuote() {
    printf("\n--- Testing Sina HK Quote (hk00001) ---\n");

    std::wstring path = L"/list=hk00001";
    std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
    if (response.empty()) {
        printf("  FAILED: Empty response\n");
        return;
    }
    std::string utf8 = GbkToUtf8(response);
    printf("  Response: %s\n", utf8.c_str());
}

void TestSinaUSQuote() {
    printf("\n--- Testing Sina US Quote (gb_aapl) ---\n");

    std::wstring path = L"/list=gb_aapl";
    std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
    if (response.empty()) {
        printf("  FAILED: Empty response\n");
        return;
    }
    std::string utf8 = GbkToUtf8(response);
    printf("  Response: %s\n", utf8.c_str());
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== Multi-Market API Test ===\n");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    TestEastMoneyAPI("HK Stocks", L"m:128+t:3,m:128+t:4,m:128+t:1,m:128+t:2");
    TestEastMoneyAPI("US Stocks", L"m:105+t:1,m:105+t:2");
    TestEastMoneyAPI("BJ Stocks", L"m:0+t:81+s:2048");
    TestEastMoneyAPI("SH A Stocks", L"m:1+t:2,m:1+t:23");

    TestSinaHKList();
    TestSinaHKStockData();
    TestSinaHKQuote();
    TestSinaUSQuote();

    printf("\n=== Test Complete ===\n");
    WSACleanup();
    return 0;
}
