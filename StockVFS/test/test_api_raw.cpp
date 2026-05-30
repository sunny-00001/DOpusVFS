#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>

std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 443, bool https = true) {
    HINTERNET hSession = WinHttpOpen(L"StockVFS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    DWORD dwSize = 0;
    do {
        DWORD dwDownloaded = 0;
        DWORD dwAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable) || dwAvailable == 0) break;
        std::vector<char> buffer(dwAvailable + 1);
        WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded);
        response.append(buffer.data(), dwDownloaded);
        dwSize = dwAvailable;
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    std::wstring path = L"/api/qt/clist/get?pn=1&pz=5000&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:1+t:2,m:1+t:23&fields=f2,f3,f4,f5,f6,f12,f14,f15,f16,f17,f18";

    std::cout << "Requesting Eastmoney API..." << std::endl;
    std::string response = SendHttpGet(L"80.push2.eastmoney.com", path, 80, false);

    std::cout << "Response length: " << response.length() << std::endl;

    size_t totalPos = response.find("\"total\":");
    if (totalPos != std::string::npos) {
        size_t valStart = totalPos + 8;
        size_t valEnd = response.find_first_of(",}", valStart);
        if (valEnd != std::string::npos) {
            std::string totalStr = response.substr(valStart, valEnd - valStart);
            std::cout << "Total stocks: " << totalStr << std::endl;
        }
    }

    size_t diffPos = response.find("\"diff\":");
    if (diffPos != std::string::npos) {
        size_t arrStart = response.find('[', diffPos);
        if (arrStart != std::string::npos) {
            int count = 0;
            size_t pos = arrStart + 1;
            while (pos < response.length()) {
                size_t objStart = response.find('{', pos);
                if (objStart == std::string::npos) break;
                size_t objEnd = response.find('}', objStart);
                if (objEnd == std::string::npos) break;
                count++;
                pos = objEnd + 1;
            }
            std::cout << "Parsed objects: " << count << std::endl;
        }
    }

    std::cout << "First 500 chars: " << response.substr(0, 500) << std::endl;

    return 0;
}
