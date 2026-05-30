#include <windows.h>
#include <winhttp.h>
#include <strsafe.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")

static std::wstring FormatPrice(double price, int market) {
    WCHAR buf[32];
    if (price == 0.0) {
        StringCchPrintfW(buf, 32, L"0.00");
    } else if (market == 2) {
        StringCchPrintfW(buf, 32, L"%.3f", price);
    } else if (market == 3) {
        StringCchPrintfW(buf, 32, L"%.2f", price);
    } else {
        StringCchPrintfW(buf, 32, L"%.3f", price);
        std::wstring s = buf;
        size_t dotPos = s.find(L'.');
        if (dotPos != std::wstring::npos) {
            while (s.length() > dotPos + 3 && s.back() == L'0') s.pop_back();
        }
        return s;
    }
    return buf;
}

static std::wstring FormatVolume(int64_t vol) {
    WCHAR buf[32];
    if (vol >= 100000000) {
        StringCchPrintfW(buf, 32, L"%.2f\u4ebf", vol / 100000000.0);
    } else if (vol >= 10000) {
        StringCchPrintfW(buf, 32, L"%.2f\u4e07", vol / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%lld", vol);
    }
    return buf;
}

static std::wstring FormatAmount(double amt) {
    WCHAR buf[32];
    if (amt >= 100000000.0) {
        StringCchPrintfW(buf, 32, L"%.2f\u4ebf", amt / 100000000.0);
    } else if (amt >= 10000.0) {
        StringCchPrintfW(buf, 32, L"%.2f\u4e07", amt / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%.2f", amt);
    }
    return buf;
}

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
    result.reserve(s.length());
    for (size_t i = 0; i < s.length(); ) {
        if (i + 5 < s.length() && s[i] == '\\' && s[i+1] == 'u') {
            unsigned int cp = 0;
            bool valid = true;
            for (int j = 0; j < 4; j++) {
                char c = s[i + 2 + j];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= c - '0';
                else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
                else { valid = false; break; }
            }
            if (valid) {
                result += (wchar_t)cp;
                i += 6;
            } else {
                result += (wchar_t)(unsigned char)s[i];
                i++;
            }
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
    std::cout << "=== End-to-End Data Display Test ===" << std::endl;

    std::cout << "\n--- 1. FormatPrice Tests ---" << std::endl;
    struct { double price; int market; const char* desc; } priceTests[] = {
        {45.720, 0, "A-share 3-decimal"}, {85.640, 0, "A-share 3-decimal"},
        {7.930, 0, "A-share 3-decimal"}, {0.0, 0, "Zero price"},
        {38.100, 0, "A-share with trailing zero"}, {100.5, 0, "A-share 1-decimal"},
    };
    for (auto& t : priceTests) {
        std::wstring s = FormatPrice(t.price, t.market);
        std::wcout << L"  " << t.desc << L": price=" << t.price << L" -> \"" << s << L"\"" << std::endl;
    }

    std::cout << "\n--- 2. FormatVolume Tests ---" << std::endl;
    struct { int64_t vol; const char* desc; } volTests[] = {
        {15290556, "15M volume"}, {448294000, "448M volume"}, {0, "Zero volume"},
        {9999, "Small volume"}, {15000, "1.5wan volume"},
    };
    for (auto& t : volTests) {
        std::wstring s = FormatVolume(t.vol);
        std::wcout << L"  " << t.desc << L": vol=" << t.vol << L" -> \"" << s << L"\"" << std::endl;
    }

    std::cout << "\n--- 3. FormatAmount Tests ---" << std::endl;
    struct { double amt; const char* desc; } amtTests[] = {
        {676818397, "6.7yi amount"}, {3633910000.0, "36yi amount"}, {0.0, "Zero amount"},
        {9999.0, "Small amount"},
    };
    for (auto& t : amtTests) {
        std::wstring s = FormatAmount(t.amt);
        std::wcout << L"  " << t.desc << L": amt=" << t.amt << L" -> \"" << s << L"\"" << std::endl;
    }

    std::cout << "\n--- 4. Change Amount Display Tests ---" << std::endl;
    struct { double chgAmt; int market; const char* desc; } chgTests[] = {
        {7.62, 0, "Positive change"}, {-1.50, 0, "Negative change"},
        {0.0, 0, "Zero change"}, {14.27, 0, "Large positive"},
    };
    for (auto& t : chgTests) {
        std::wstring changeSign = t.chgAmt >= 0 ? L"+" : L"";
        std::wstring priceStr = FormatPrice(t.chgAmt, t.market);
        WCHAR chgBuf[32];
        StringCchPrintfW(chgBuf, 32, L"%s%s", changeSign.c_str(), priceStr.c_str());
        std::wcout << L"  " << t.desc << L": chgAmt=" << t.chgAmt << L" -> \"" << chgBuf << L"\"" << std::endl;
    }

    std::cout << "\n--- 5. Live API Data Test ---" << std::endl;
    std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn",
        L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=sh_a&symbol=&_s_r_a=auto", 80);

    if (response.empty()) {
        std::cout << "FAILED: Empty response from API" << std::endl;
        return 1;
    }

    size_t arrStart = response.find('[');
    if (arrStart == std::string::npos) {
        std::cout << "FAILED: No array in response" << std::endl;
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

        std::string code = ExtractJsonString(obj, "code");
        std::string name = ExtractJsonString(obj, "name");
        std::string tradeStr = ExtractJsonString(obj, "trade");
        double currentPrice = tradeStr.empty() ? 0.0 : atof(tradeStr.c_str());
        double changePercent = ExtractJsonDouble(obj, "changepercent");
        double changeAmount = ExtractJsonDouble(obj, "pricechange");
        double openPrice = atof(ExtractJsonString(obj, "open").c_str());
        double highPrice = atof(ExtractJsonString(obj, "high").c_str());
        double lowPrice = atof(ExtractJsonString(obj, "low").c_str());
        double prevClose = atof(ExtractJsonString(obj, "settlement").c_str());
        double volume = ExtractJsonDouble(obj, "volume");
        double amount = ExtractJsonDouble(obj, "amount");
        double turnover = ExtractJsonDouble(obj, "turnoverratio");

        std::wstring nameW = DecodeJsonUnicode(name);

        std::cout << "\n  Stock " << (count+1) << " (code=" << code << "):" << std::endl;
        std::wcout << L"    Name: " << nameW << std::endl;
        std::wcout << L"    CurrentPrice: " << currentPrice << L" -> FormatPrice: \"" << FormatPrice(currentPrice, 0) << L"\"" << std::endl;
        std::wcout << L"    ChangeAmount: " << changeAmount << L" -> Display: \"" << (changeAmount >= 0 ? L"+" : L"") << FormatPrice(changeAmount, 0) << L"\"" << std::endl;
        std::wcout << L"    ChangePercent: " << changePercent << L" -> Display: \"" << (changeAmount >= 0 ? L"+" : L"") << std::fixed << std::setprecision(2) << changePercent << L"%\"" << std::endl;
        std::wcout << L"    OpenPrice: " << openPrice << L" -> FormatPrice: \"" << FormatPrice(openPrice, 0) << L"\"" << std::endl;
        std::wcout << L"    HighPrice: " << highPrice << L" -> FormatPrice: \"" << FormatPrice(highPrice, 0) << L"\"" << std::endl;
        std::wcout << L"    LowPrice: " << lowPrice << L" -> FormatPrice: \"" << FormatPrice(lowPrice, 0) << L"\"" << std::endl;
        std::wcout << L"    PrevClose: " << prevClose << L" -> FormatPrice: \"" << FormatPrice(prevClose, 0) << L"\"" << std::endl;
        std::wcout << L"    Volume: " << (int64_t)volume << L" -> FormatVolume: \"" << FormatVolume((int64_t)volume) << L"\"" << std::endl;
        std::wcout << L"    Amount: " << amount << L" -> FormatAmount: \"" << FormatAmount(amount) << L"\"" << std::endl;
        std::wcout << L"    TurnoverRate: " << turnover << L" -> Display: \"" << std::fixed << std::setprecision(2) << turnover << L"%\"" << std::endl;

        pos = objEnd + 1;
        count++;
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
