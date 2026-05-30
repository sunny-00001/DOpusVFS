#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

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
    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\nUser-Agent: Mozilla/5.0\r\n";
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
    fopen_s(&out, "d:\\vfs\\test_sector_api2.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    fprintf(out, "=== Test 1: \u884c\u4e1a\u677f\u5757 (newFLJK.php?param=industry) ===\n");
    {
        std::string response = SendHttpGet(L"money.finance.sina.com.cn", L"/q/view/newFLJK.php?param=industry", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            std::wstring wresp = GbkToWide(response);
            std::string utf8 = WideToUtf8(wresp);
            fprintf(out, "First 3000: %.3000s\n", utf8.c_str());
        }
    }

    fprintf(out, "\n=== Test 2: \u6982\u5ff5\u677f\u5757\u6210\u5206\u80a1 (node=gn_hwqc) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=gn_hwqc&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 3: \u6307\u6570\u5217\u8868 (node=gs_ss) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=20&sort=changepercent&asc=0&node=gs_ss&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 4: \u6307\u6570\u5217\u8868 (node=gs_z) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=20&sort=changepercent&asc=0&node=gs_z&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 5: \u6307\u6570\u5217\u8868 (node=gs_sz) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=20&sort=changepercent&asc=0&node=gs_sz&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 2000: %.2000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 6: \u677f\u5757\u884c\u60c5 (bk_\u524d\u7f00 \u5e26\u5f15\u53f7\u5185\u5bb9) ===\n");
    {
        std::wstring path = L"/list=bk0428";
        std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "Full response: %.1000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 7: \u677f\u5757\u884c\u60c5 (\u591a\u4e2a\u677f\u5757\u4ee3\u7801) ===\n");
    {
        std::wstring path = L"/list=bk0428,bk0477,bk0733,bk0447,bk0493";
        std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "Full response: %.3000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 8: \u4e1c\u65b9\u8d22\u5bcc\u884c\u4e1a\u677f\u5757\u5217\u8868 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=20&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:90+t:2&fields=f2,f3,f4,f12,f14";
        std::string response = SendHttpGet(L"push2.eastmoney.com", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 9: \u4e1c\u65b9\u8d22\u5bcc\u6982\u5ff5\u677f\u5757\u5217\u8868 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=20&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:90+t:3&fields=f2,f3,f4,f12,f14";
        std::string response = SendHttpGet(L"push2.eastmoney.com", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    fprintf(out, "\n=== Test 10: \u4e1c\u65b9\u8d22\u5bcc\u6307\u6570\u5217\u8868 ===\n");
    {
        std::wstring path = L"/api/qt/clist/get?pn=1&pz=20&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:1+s:000&fields=f2,f3,f4,f5,f6,f7,f8,f12,f14,f15,f16,f17,f18";
        std::string response = SendHttpGet(L"push2.eastmoney.com", path, 80, false);
        fprintf(out, "Response length: %zu\n", response.length());
        fprintf(out, "First 3000: %.3000s\n", response.c_str());
    }

    fclose(out);
    printf("Done. Output: d:\\vfs\\test_sector_api2.txt\n");
    return 0;
}
