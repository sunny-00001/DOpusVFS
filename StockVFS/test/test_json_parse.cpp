#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

static double ExtractJsonDouble(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t kPos = json.find(searchKey);
    if (kPos == std::string::npos) return 0.0;
    size_t valStart = kPos + searchKey.length();
    while (valStart < json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
    if (valStart >= json.length()) return 0.0;
    if (json[valStart] == '-' && valStart + 1 < json.length() && json[valStart + 1] == '1') {
        return 0.0;
    }
    char* end = nullptr;
    double val = strtod(json.c_str() + valStart, &end);
    if (end == json.c_str() + valStart) return 0.0;
    return val;
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t kPos = json.find(searchKey);
    if (kPos == std::string::npos) return "";
    size_t valStart = kPos + searchKey.length();
    size_t valEnd = json.find('"', valStart);
    if (valEnd == std::string::npos) return "";
    return json.substr(valStart, valEnd - valStart);
}

static std::wstring DecodeJsonUnicode(const std::string& s) {
    std::wstring result;
    for (size_t i = 0; i < s.length(); ) {
        if (s[i] == '\\' && i + 1 < s.length() && s[i+1] == 'u') {
            unsigned int cp = 0;
            for (int j = 0; j < 4 && i + 2 + j < s.length(); j++) {
                char c = s[i + 2 + j];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= c - '0';
                else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
            }
            result += (wchar_t)cp;
            i += 6;
        } else {
            result += (wchar_t)(unsigned char)s[i];
            i++;
        }
    }
    return result;
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
    std::cout << "=== Test Sina Market List API ===" << std::endl;

    std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn",
        L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=sh_a&symbol=&_s_r_a=auto", 80);

    if (response.empty()) {
        std::cout << "FAILED: Empty response" << std::endl;
        return 1;
    }

    std::cout << "Response length: " << response.length() << std::endl;

    size_t arrStart = response.find('[');
    if (arrStart == std::string::npos) {
        std::cout << "FAILED: No array found" << std::endl;
        return 1;
    }

    size_t pos = arrStart + 1;
    int count = 0;
    while (pos < response.length() && count < 3) {
        size_t objStart = response.find('{', pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = response.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string obj = response.substr(objStart, objEnd - objStart + 1);
        std::cout << "\n--- Stock " << (count+1) << " ---" << std::endl;
        std::cout << "Raw JSON object: " << obj.substr(0, 200) << "..." << std::endl;

        std::string code = ExtractJsonString(obj, "code");
        std::string name = ExtractJsonString(obj, "name");
        std::string tradeStr = ExtractJsonString(obj, "trade");
        double changePercent = ExtractJsonDouble(obj, "changepercent");
        double pricechange = ExtractJsonDouble(obj, "pricechange");
        double volume = ExtractJsonDouble(obj, "volume");
        double amount = ExtractJsonDouble(obj, "amount");
        std::string openStr = ExtractJsonString(obj, "open");
        std::string highStr = ExtractJsonString(obj, "high");
        std::string lowStr = ExtractJsonString(obj, "low");
        std::string settlementStr = ExtractJsonString(obj, "settlement");

        std::cout << "Code: " << code << std::endl;
        std::cout << "Name (raw): " << name << std::endl;
        std::wstring nameW = DecodeJsonUnicode(name);
        std::cout << "Name (decoded codepoints):";
        for (wchar_t wc : nameW) {
            std::cout << " U+" << std::hex << (unsigned int)wc << std::dec;
        }
        std::cout << std::endl;
        std::cout << "Trade: " << tradeStr << " -> " << (tradeStr.empty() ? 0.0 : atof(tradeStr.c_str())) << std::endl;
        std::cout << "ChangePercent: " << changePercent << std::endl;
        std::cout << "PriceChange: " << pricechange << std::endl;
        std::cout << "Volume: " << volume << std::endl;
        std::cout << "Amount: " << amount << std::endl;
        std::cout << "Open: " << openStr << " -> " << (openStr.empty() ? 0.0 : atof(openStr.c_str())) << std::endl;
        std::cout << "High: " << highStr << " -> " << (highStr.empty() ? 0.0 : atof(highStr.c_str())) << std::endl;
        std::cout << "Low: " << lowStr << " -> " << (lowStr.empty() ? 0.0 : atof(lowStr.c_str())) << std::endl;
        std::cout << "Settlement: " << settlementStr << " -> " << (settlementStr.empty() ? 0.0 : atof(settlementStr.c_str())) << std::endl;

        pos = objEnd + 1;
        count++;
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
