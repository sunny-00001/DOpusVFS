#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <functional>

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

std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int wlen = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (wlen <= 0) return L"";
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], wlen);
    return wide;
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
    return response;
}

struct TestResult {
    std::string name;
    bool passed;
    std::string detail;
};

int main() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== Phase 2 Integration Test ===\n\n");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::vector<TestResult> results;

    auto test = [&](const std::string& name, bool condition, const std::string& detail) {
        results.push_back({name, condition, detail});
        printf("[%s] %s: %s\n", condition ? "PASS" : "FAIL", name.c_str(), detail.c_str());
    };

    printf("--- Test 1: HK Quote (hk00001) ---\n");
    std::string r1 = SendHttpGet(L"hq.sinajs.cn", L"/list=hk00001");
    bool hkOk = !r1.empty() && r1.find("hq_str_hk00001") != std::string::npos && r1.find("\"\"") == std::string::npos;
    test("HK Quote API", hkOk, hkOk ? "Got HK stock data" : "Failed to get HK stock data");
    if (hkOk) {
        printf("  Raw: %.200s\n", r1.c_str());
    }

    printf("\n--- Test 2: US Quote (gb_aapl) ---\n");
    std::string r2 = SendHttpGet(L"hq.sinajs.cn", L"/list=gb_aapl");
    bool usOk = !r2.empty() && r2.find("hq_str_gb_aapl") != std::string::npos && r2.find("\"\"") == std::string::npos;
    test("US Quote API", usOk, usOk ? "Got US stock data" : "Failed to get US stock data");
    if (usOk) {
        printf("  Raw: %.200s\n", r2.c_str());
    }

    printf("\n--- Test 3: BJ Stock in hs_a list ---\n");
    std::string r3 = SendHttpGet(L"vip.stock.finance.sina.com.cn",
                                  L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=80&sort=changepercent&asc=0&node=hs_a&symbol=&_s_r_a=auto");
    bool bjInList = !r3.empty() && r3.find("\"bj") != std::string::npos;
    test("BJ Stock in hs_a list", bjInList, bjInList ? "Found bj stocks in hs_a list" : "No bj stocks found");
    if (bjInList) {
        size_t bjPos = r3.find("\"bj");
        printf("  Found bj stock at position %zu: %.50s\n", bjPos, r3.substr(bjPos, 50).c_str());
    }

    printf("\n--- Test 4: SH Quote (sh600519) ---\n");
    std::string r4 = SendHttpGet(L"hq.sinajs.cn", L"/list=sh600519");
    bool shOk = !r4.empty() && r4.find("hq_str_sh600519") != std::string::npos && r4.find("\"\"") == std::string::npos;
    test("SH Quote API", shOk, shOk ? "Got SH stock data" : "Failed to get SH stock data");

    printf("\n--- Test 5: Search HK (腾讯) ---\n");
    std::string r5 = SendHttpGet(L"suggest3.sinajs.cn", L"/suggest/type=&key=腾讯&name=suggestdata");
    bool searchHkOk = !r5.empty() && r5.find("00700") != std::string::npos;
    test("Search HK Stock", searchHkOk, searchHkOk ? "Found HK stock in search" : "Failed to find HK stock in search");

    printf("\n--- Test 6: Search US (apple) ---\n");
    std::string r6 = SendHttpGet(L"suggest3.sinajs.cn", L"/suggest/type=&key=apple&name=suggestdata");
    bool searchUsOk = !r6.empty() && (r6.find("AAPL") != std::string::npos || r6.find("aapl") != std::string::npos);
    test("Search US Stock", searchUsOk, searchUsOk ? "Found US stock in search" : "Failed to find US stock in search");

    printf("\n--- Test 7: Search BJ (北交所) ---\n");
    std::string r7 = SendHttpGet(L"suggest3.sinajs.cn", L"/suggest/type=&key=北交所&name=suggestdata");
    bool searchBjOk = !r7.empty() && r7.find("32") != std::string::npos;
    test("Search BJ Stock", searchBjOk, searchBjOk ? "Found BJ stock type in search" : "No BJ stock type found in search");
    if (!r7.empty()) {
        std::string utf8 = r7;
        if ((unsigned char)r7[0] > 0x7f) {
            int wlen = MultiByteToWideChar(936, 0, r7.c_str(), (int)r7.length(), NULL, 0);
            if (wlen > 0) {
                std::wstring wide(wlen, L'\0');
                MultiByteToWideChar(936, 0, r7.c_str(), (int)r7.length(), &wide[0], wlen);
                utf8 = WideToUtf8(wide);
            }
        }
        printf("  Response (first 300): %.300s\n", utf8.c_str());
    }

    printf("\n=== Test Summary ===\n");
    int passed = 0, failed = 0;
    for (const auto& r : results) {
        if (r.passed) passed++; else failed++;
    }
    printf("Passed: %d, Failed: %d, Total: %d\n", passed, failed, (int)results.size());

    WSACleanup();
    return failed > 0 ? 1 : 0;
}
