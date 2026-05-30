#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ws2_32.lib")

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &sz, NULL);

    std::string response;
    if (statusCode == 200) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::vector<char> buf(avail + 1);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), avail, &read);
            response.append(buf.data(), read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

int main() {
    std::wstring codes = L"sh000001,sz399001,sz399006,sh000300";
    std::wstring path = L"/list=" + codes;

    std::string response = SendHttpGet(L"hq.sinajs.cn", path);

    std::ofstream out("test_sina_index_raw.txt", std::ios::binary);
    out.write(response.c_str(), response.length());
    out.close();

    std::wstring wideResp = GbkToWide(response);

    FILE* fout = fopen("test_sina_index_parsed.txt", "w, ccs=UTF-8");
    if (fout) {
        fputws(L"=== Raw response (decoded) ===\r\n", fout);
        fputws(wideResp.c_str(), fout);
        fputws(L"\r\n\r\n=== Field-by-field parsing ===\r\n", fout);

        std::istringstream iss(response);
        std::string line;
        int lineNum = 0;
        while (std::getline(iss, line)) {
            lineNum++;
            std::wstring wline = GbkToWide(line);
            fwprintf(fout, L"\r\nLine %d: %s\r\n", lineNum, wline.c_str());

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string valueStr = line.substr(eqPos + 1);
            if (valueStr.size() < 2 || valueStr[0] != '"') continue;
            valueStr = valueStr.substr(1);
            if (!valueStr.empty() && valueStr.back() == '"') valueStr.pop_back();
            if (!valueStr.empty() && valueStr.back() == ';') valueStr.pop_back();

            std::vector<std::string> fields;
            std::istringstream fiss(valueStr);
            std::string field;
            while (std::getline(fiss, field, ',')) {
                fields.push_back(field);
            }

            fwprintf(fout, L"  Total fields: %zu\r\n", fields.size());
            for (size_t i = 0; i < fields.size() && i < 20; i++) {
                std::wstring wfield = GbkToWide(fields[i]);
                fwprintf(fout, L"  fields[%zu] = %s\r\n", i, wfield.c_str());
            }
        }
        fclose(fout);
    }

    std::wcout << L"Done. Check test_sina_index_parsed.txt" << std::endl;
    return 0;
}
