#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <ctime>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 443, bool https = true) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, L"https://quote.eastmoney.com", NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"Referer: https://quote.eastmoney.com/center/boardlist.html\r\nAccept: */*\r\nAccept-Language: zh-CN,zh;q=0.9\r\nConnection: keep-alive";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResult = WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0);
    if (!bResult) {
        DWORD err = GetLastError();
        printf("SendRequest failed: %lu\n", err);
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) {
        DWORD err = GetLastError();
        printf("ReceiveResponse failed: %lu\n", err);
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD statusCode = 0;
    DWORD szStatusCode = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &szStatusCode, NULL);
    printf("HTTP Status: %lu\n", statusCode);

    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead);
        if (bytesRead > 0) response.append(buffer.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

int main() {
    FILE* out = nullptr;
    fopen_s(&out, "d:\\vfs\\test_em3.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    // Test with JSONP callback
    fprintf(out, "=== Test: JSONP callback ===\n");
    {
        srand((unsigned)time(NULL));
        int cbNum = rand() % 1000000;
        char cbStr[64];
        snprintf(cbStr, sizeof(cbStr), "jQuery%d_%lld", cbNum, (long long)time(NULL) * 1000);
        std::wstring cbWStr(cbStr, cbStr + strlen(cbStr));

        std::wstring path = L"/api/qt/clist/get?cb=" + cbWStr +
            L"&pn=1&pz=5&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14&_="
            + std::to_wstring((long long)time(NULL) * 1000);

        printf("Trying JSONP HTTPS push2.eastmoney.com...\n");
        std::string response = SendHttpGet(L"push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    // Test with JSONP on 62.push2
    fprintf(out, "\n=== Test: JSONP 62.push2.eastmoney.com ===\n");
    {
        srand((unsigned)time(NULL));
        int cbNum = rand() % 1000000;
        char cbStr[64];
        snprintf(cbStr, sizeof(cbStr), "jQuery%d_%lld", cbNum, (long long)time(NULL) * 1000);
        std::wstring cbWStr(cbStr, cbStr + strlen(cbStr));

        std::wstring path = L"/api/qt/clist/get?cb=" + cbWStr +
            L"&pn=1&pz=5&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14&_="
            + std::to_wstring((long long)time(NULL) * 1000);

        printf("Trying JSONP HTTPS 62.push2...\n");
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    // Test without JSONP
    fprintf(out, "\n=== Test: No callback push2 HTTPS ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=5&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14&_="
            + std::to_wstring((long long)time(NULL) * 1000);

        printf("Trying no callback HTTPS push2...\n");
        std::string response = SendHttpGet(L"push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    fclose(out);
    printf("Test complete.\n");
    return 0;
}
