#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    std::wstring wide(len, '\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 443, bool https = true) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
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
    fopen_s(&out, "d:\\vfs\\test_em_sector.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    // Test 1: 东方财富行业板块列表
    fprintf(out, "=== Test 1: 东方财富行业板块列表 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=100&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f8,f12,f14,f104,f105,f128,f136,f140,f141";
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "First 2000 chars: %.2000s\n", response.c_str());
        }
    }

    // Test 2: 东方财富概念板块列表
    fprintf(out, "\n=== Test 2: 东方财富概念板块列表 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=100&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:3+f:!50&fields=f2,f3,f4,f8,f12,f14,f104,f105,f128,f136,f140,f141";
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "First 2000 chars: %.2000s\n", response.c_str());
        }
    }

    // Test 3: 东方财富地域板块列表
    fprintf(out, "\n=== Test 3: 东方财富地域板块列表 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=100&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:1+f:!50&fields=f2,f3,f4,f8,f12,f14,f104,f105,f128,f136,f140,f141";
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "First 2000 chars: %.2000s\n", response.c_str());
        }
    }

    // Test 4: 东方财富板块成分股
    fprintf(out, "\n=== Test 4: 东方财富板块成分股(BK0477=白酒) ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=10&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=b:BK0477+f:!50&fields=f2,f3,f4,f5,f6,f7,f8,f9,f10,f12,f14,f15,f16,f17,f18,f20,f21,f23";
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "First 2000 chars: %.2000s\n", response.c_str());
        }
    }

    // Test 5: 新浪申万行业列表(通过东方财富获取)
    fprintf(out, "\n=== Test 5: 东方财富申万行业板块 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=200&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&fltt=2&invt=2&fid=f3&fs=m:90+t:2&fields=f2,f3,f4,f8,f12,f14,f104,f105,f128,f136";
        std::string response = SendHttpGet(L"62.push2.eastmoney.com", path, 443, true);
        fprintf(out, "Response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "First 2000 chars: %.2000s\n", response.c_str());
        }
    }

    fclose(out);
    printf("Test complete. Output: d:\\vfs\\test_em_sector.txt\n");
    return 0;
}
