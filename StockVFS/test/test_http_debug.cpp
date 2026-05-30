#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

void TestWinInet() {
    std::cout << "=== WinInet test ===" << std::endl;

    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) { std::cout << "InternetOpen failed: " << GetLastError() << std::endl; return; }

    HINTERNET hConnect = InternetConnectA(hInternet, "80.push2.eastmoney.com", 80, NULL, NULL,
        INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { std::cout << "InternetConnect failed: " << GetLastError() << std::endl; InternetCloseHandle(hInternet); return; }

    const char* acceptTypes[] = {"*/*", NULL};
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET",
        "/api/qt/clist/get?pn=1&pz=5&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:1+t:2,m:1+t:23&fields=f12,f14",
        NULL, "https://quote.eastmoney.com", acceptTypes, 0, 0);
    if (!hRequest) { std::cout << "HttpOpenRequest failed: " << GetLastError() << std::endl; InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return; }

    std::string headers = "Referer: https://quote.eastmoney.com\r\n";
    if (!HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), NULL, 0)) {
        std::cout << "HttpSendRequest failed: " << GetLastError() << std::endl;
        InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
        return;
    }

    DWORD statusCode = 0, sz = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &sz, NULL);
    std::cout << "HTTP status: " << statusCode << std::endl;

    std::string response;
    DWORD dwAvailable = 0;
    while (InternetQueryDataAvailable(hRequest, &dwAvailable, 0, 0) && dwAvailable > 0) {
        std::vector<char> buf(dwAvailable + 1);
        DWORD dwRead = 0;
        InternetReadFile(hRequest, buf.data(), dwAvailable, &dwRead);
        if (dwRead > 0) response.append(buf.data(), dwRead);
        else break;
    }

    std::cout << "Response length: " << response.size() << std::endl;
    if (!response.empty()) {
        size_t showLen = response.size() > 500 ? 500 : response.size();
        std::cout << "Response: " << response.substr(0, showLen) << std::endl;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
}

void TestRawSocket() {
    std::cout << "\n=== Raw socket test ===" << std::endl;
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("80.push2.eastmoney.com", "80", &hints, &result) != 0 || !result) {
        std::cout << "DNS resolve failed" << std::endl;
        WSACleanup();
        return;
    }

    char ipStr[INET_ADDRSTRLEN] = {};
    struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
    std::cout << "Resolved to: " << ipStr << std::endl;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) != 0) {
        std::cout << "connect failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return;
    }
    freeaddrinfo(result);

    std::string req = "GET /api/qt/clist/get?pn=1&pz=5&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:1+t:2,m:1+t:23&fields=f12,f14 HTTP/1.0\r\n"
        "Host: 80.push2.eastmoney.com\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n"
        "Referer: https://quote.eastmoney.com\r\n"
        "Connection: close\r\n\r\n";
    send(sock, req.c_str(), (int)req.length(), 0);

    std::string response;
    char buf[4096];
    int n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, n);
    }
    closesocket(sock);

    std::cout << "Response length: " << response.size() << std::endl;
    if (!response.empty()) {
        size_t showLen = response.size() > 800 ? 800 : response.size();
        std::cout << "Response: " << response.substr(0, showLen) << std::endl;
    }

    WSACleanup();
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    TestWinInet();
    TestRawSocket();
    return 0;
}
