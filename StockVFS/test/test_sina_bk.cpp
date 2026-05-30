#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, NULL, NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResult = WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

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
    fopen_s(&out, "d:\\vfs\\test_sina_bk.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    // Test 1: 新浪行业分类列表
    fprintf(out, "=== Test 1: newSinaHy.php ===\n");
    {
        std::string response = SendHttpGet(L"finance.sina.com.cn", L"/q/view/newSinaHy.php", 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    // Test 2: 新浪概念分类列表
    fprintf(out, "\n=== Test 2: newFLJK.php ===\n");
    {
        std::string response = SendHttpGet(L"money.finance.sina.com.cn", L"/q/view/newFLJK.php?param=class", 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    // Test 3: 新浪申万行业列表
    fprintf(out, "\n=== Test 3: SwHy.php ===\n");
    {
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", L"/q/view/SwHy.php", 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    fclose(out);
    printf("Done.\n");
    return 0;
}
