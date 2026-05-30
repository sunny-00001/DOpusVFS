#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>

#pragma comment(lib, "winhttp.lib")

std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80) {
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return result;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return result; }
    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_ADDREQ_FLAG_ADD);
    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD size = 0;
            do {
                DWORD downloaded = 0;
                WinHttpQueryDataAvailable(hRequest, &size);
                if (!size) break;
                std::string buffer(size, '\0');
                WinHttpReadData(hRequest, &buffer[0], size, &downloaded);
                result.append(buffer.data(), downloaded);
            } while (size > 0);
        }
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

int main() {
    std::cout << "=== Sina Stock List API Test ===" << std::endl;
    std::string response = SendHttpGet(L"hq.sinajs.cn", L"/list=sh600519", 80);
    if (response.empty()) { std::cout << "FAILED: Empty response" << std::endl; return 1; }
    
    size_t eqPos = response.find('=');
    if (eqPos == std::string::npos) { std::cout << "FAILED: No = found" << std::endl; return 1; }
    std::string valueStr = response.substr(eqPos + 1);
    if (valueStr.size() >= 2 && valueStr[0] == '"') valueStr = valueStr.substr(1);
    if (!valueStr.empty() && valueStr.back() == '"') valueStr.pop_back();
    if (!valueStr.empty() && valueStr.back() == ';') valueStr.pop_back();
    
    size_t commaPos = valueStr.find(',');
    std::string nameGbk = (commaPos != std::string::npos) ? valueStr.substr(0, commaPos) : valueStr;
    
    std::wstring nameWide = GbkToWide(nameGbk);
    
    std::wcout.imbue(std::locale(""));
    std::wcout << L"Raw GBK bytes count: " << nameGbk.length() << std::endl;
    std::wcout << L"Wide string length: " << nameWide.length() << std::endl;
    std::wcout << L"Stock name (GBK->Wide): " << nameWide << std::endl;
    
    std::cout << "\n=== Sina Market List API Test ===" << std::endl;
    std::string listResp = SendHttpGet(L"vip.stock.finance.sina.com.cn",
        L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=sh_a&symbol=&_s_r_a=auto", 80);
    if (listResp.empty()) { std::cout << "FAILED: Empty response" << std::endl; return 1; }
    
    std::cout << "Response length: " << listResp.length() << std::endl;
    std::cout << "First 200 chars: " << listResp.substr(0, 200) << std::endl;
    
    size_t pos = listResp.find("\"name\":\"");
    if (pos != std::string::npos) {
        size_t nameStart = pos + 8;
        size_t nameEnd = listResp.find('"', nameStart);
        std::string name = listResp.substr(nameStart, nameEnd - nameStart);
        std::wstring nameW;
        for (size_t i = 0; i < name.length(); ) {
            if (name[i] == '\\' && i + 1 < name.length() && name[i+1] == 'u') {
                unsigned int codepoint = 0;
                for (int j = 0; j < 4 && i + 2 + j < name.length(); j++) {
                    char c = name[i + 2 + j];
                    codepoint <<= 4;
                    if (c >= '0' && c <= '9') codepoint |= c - '0';
                    else if (c >= 'a' && c <= 'f') codepoint |= c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') codepoint |= c - 'A' + 10;
                }
                nameW += (wchar_t)codepoint;
                i += 6;
            } else {
                nameW += (wchar_t)(unsigned char)name[i];
                i++;
            }
        }
        std::wcout << L"First stock name (JSON Unicode): " << nameW << std::endl;
    }
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
