#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ws2_32.lib")

int main() {
    std::wstring host = L"push2.eastmoney.com";
    std::wstring path = L"/api/qt/clist/get?pn=1&pz=5&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14,f104,f105,f128,f136,f140,f141";

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::wcout << L"WinHttpOpen failed: " << GetLastError() << std::endl;
        return 1;
    }

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
        int dnsResult = getaddrinfo(hostA.c_str(), nullptr, &hints, &result);
        if (dnsResult == 0 && result) {
            char ipStr[INET_ADDRSTRLEN] = {};
            struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
            std::wstring ipW(ipStr, ipStr + strlen(ipStr));
            connectHost = ipW;
            hostHeader = host;
            freeaddrinfo(result);
            std::wcout << L"DNS resolved: " << host << L" -> " << connectHost << std::endl;
        } else {
            std::wcout << L"DNS resolution failed for " << host << L": " << dnsResult << std::endl;
        }
    }

    HINTERNET hConnect = WinHttpConnect(hSession, connectHost.c_str(), 80, 0);
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

    std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    if (!hostHeader.empty()) {
        headers += L"Host: " + hostHeader + L"\r\n";
    }
    headers += L"Referer: https://quote.eastmoney.com\r\n";

    std::wcout << L"Request headers: " << headers << std::endl;

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) {
        std::wcout << L"WinHttpSendRequest failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::wcout << L"WinHttpReceiveResponse failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                         NULL, &statusCode, &sz, NULL);
    std::wcout << L"HTTP Status: " << statusCode << std::endl;

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

    std::wcout << L"Response length: " << response.length() << L" bytes" << std::endl;

    FILE* f = fopen("test_em_debug_response.txt", "wb");
    if (f) {
        fwrite(response.c_str(), 1, response.length(), f);
        fclose(f);
    }

    if (response.length() < 2000) {
        std::cout << "Response: " << response << std::endl;
    } else {
        std::cout << "Response (first 2000 bytes): " << response.substr(0, 2000) << std::endl;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return 0;
}
