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

    std::wcout << L"=== Test 1: Direct connection (no DNS override) ===" << std::endl;
    {
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), 80, 0);
        if (!hConnect) {
            std::wcout << L"WinHttpConnect failed: " << GetLastError() << std::endl;
        } else {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                     NULL, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (!hRequest) {
                std::wcout << L"WinHttpOpenRequest failed: " << GetLastError() << std::endl;
            } else {
                std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\nReferer: https://quote.eastmoney.com\r\n";
                BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
                if (!bResult) {
                    std::wcout << L"WinHttpSendRequest failed: " << GetLastError() << std::endl;
                } else {
                    if (!WinHttpReceiveResponse(hRequest, NULL)) {
                        std::wcout << L"WinHttpReceiveResponse failed: " << GetLastError() << std::endl;
                    } else {
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
                        std::wcout << L"Response length: " << response.length() << std::endl;
                        if (response.length() < 3000) {
                            std::cout << "Response: " << response << std::endl;
                        } else {
                            std::cout << "Response (first 3000): " << response.substr(0, 3000) << std::endl;
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
    }

    std::wcout << L"\r\n=== Test 2: HTTPS connection ===" << std::endl;
    {
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), 443, 0);
        if (!hConnect) {
            std::wcout << L"WinHttpConnect (HTTPS) failed: " << GetLastError() << std::endl;
        } else {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                     NULL, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) {
                std::wcout << L"WinHttpOpenRequest (HTTPS) failed: " << GetLastError() << std::endl;
            } else {
                DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                 SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));

                std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\nReferer: https://quote.eastmoney.com\r\n";
                BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
                if (!bResult) {
                    std::wcout << L"WinHttpSendRequest (HTTPS) failed: " << GetLastError() << std::endl;
                } else {
                    if (!WinHttpReceiveResponse(hRequest, NULL)) {
                        std::wcout << L"WinHttpReceiveResponse (HTTPS) failed: " << GetLastError() << std::endl;
                    } else {
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
                        std::wcout << L"Response length: " << response.length() << std::endl;
                        if (response.length() < 3000) {
                            std::cout << "Response: " << response << std::endl;
                        } else {
                            std::cout << "Response (first 3000): " << response.substr(0, 3000) << std::endl;
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
    }

    WinHttpCloseHandle(hSession);
    return 0;
}
