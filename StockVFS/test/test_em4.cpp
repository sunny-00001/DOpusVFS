#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

int main() {
    FILE* out = nullptr;
    fopen_s(&out, "d:\\vfs\\test_em4.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { fprintf(out, "WinHttpOpen failed\n"); fclose(out); return 1; }

    // Test 1: push2.eastmoney.com HTTPS
    fprintf(out, "=== Test 1: push2.eastmoney.com HTTPS ===\n");
    {
        HINTERNET hConnect = WinHttpConnect(hSession, L"push2.eastmoney.com", 443, 0);
        if (!hConnect) { fprintf(out, "Connect failed: %lu\n", GetLastError()); }
        else {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                L"/api/qt/clist/get?pn=1&pz=3&fs=m:90+t:2+f:!50&fields=f2,f3,f12,f14",
                NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (!hRequest) { fprintf(out, "OpenRequest failed: %lu\n", GetLastError()); }
            else {
                DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));

                if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0, szStatus = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &szStatus, NULL);
                        fprintf(out, "Status: %lu\n", statusCode);

                        std::string response;
                        DWORD avail = 0;
                        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                            std::vector<char> buf(avail + 1);
                            DWORD read = 0;
                            WinHttpReadData(hRequest, buf.data(), avail, &read);
                            if (read > 0) response.append(buf.data(), read);
                        }
                        fprintf(out, "Response (%zu): %.2000s\n", response.length(), response.c_str());
                    } else {
                        fprintf(out, "ReceiveResponse failed: %lu\n", GetLastError());
                    }
                } else {
                    fprintf(out, "SendRequest failed: %lu\n", GetLastError());
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
    }

    // Test 2: datacenter-web.eastmoney.com
    fprintf(out, "\n=== Test 2: datacenter-web.eastmoney.com ===\n");
    {
        HINTERNET hConnect = WinHttpConnect(hSession, L"datacenter-web.eastmoney.com", 443, 0);
        if (!hConnect) { fprintf(out, "Connect failed: %lu\n", GetLastError()); }
        else {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                L"/api/data/v1/get?sortColumns=CHANGE_RATE&sortTypes=-1&pageSize=5&pageNumber=1&reportName=RPT_BOARD_INDUSTRY&columns=ALL&source=WEB&client=WEB",
                NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (!hRequest) { fprintf(out, "OpenRequest failed: %lu\n", GetLastError()); }
            else {
                DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));

                if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD statusCode = 0, szStatus = sizeof(statusCode);
                        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &szStatus, NULL);
                        fprintf(out, "Status: %lu\n", statusCode);

                        std::string response;
                        DWORD avail = 0;
                        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                            std::vector<char> buf(avail + 1);
                            DWORD read = 0;
                            WinHttpReadData(hRequest, buf.data(), avail, &read);
                            if (read > 0) response.append(buf.data(), read);
                        }
                        fprintf(out, "Response (%zu): %.2000s\n", response.length(), response.c_str());
                    } else {
                        fprintf(out, "ReceiveResponse failed: %lu\n", GetLastError());
                    }
                } else {
                    fprintf(out, "SendRequest failed: %lu\n", GetLastError());
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
    }

    WinHttpCloseHandle(hSession);
    fclose(out);
    printf("Done.\n");
    return 0;
}
