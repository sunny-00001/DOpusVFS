#include <windows.h>
#include <winhttp.h>
#include <strsafe.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

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

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

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
        StringCchPrintfW(buf, 32, L"%.2fyi", vol / 100000000.0);
    } else if (vol >= 10000) {
        StringCchPrintfW(buf, 32, L"%.2fwan", vol / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%lld", vol);
    }
    return buf;
}

static std::wstring FormatAmount(double amt) {
    WCHAR buf[32];
    if (amt >= 100000000.0) {
        StringCchPrintfW(buf, 32, L"%.2fyi", amt / 100000000.0);
    } else if (amt >= 10000.0) {
        StringCchPrintfW(buf, 32, L"%.2fwan", amt / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%.2f", amt);
    }
    return buf;
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
    SetConsoleOutputCP(CP_UTF8);

    printf("=== Full Data Display Verification ===\n\n");

    printf("--- FormatPrice Tests ---\n");
    double prices[] = {45.720, 85.640, 7.930, 0.0, 38.100, 100.5, 0.005, 1234.567};
    for (double p : prices) {
        std::wstring s = FormatPrice(p, 0);
        printf("  price=%.3f -> \"%s\"\n", p, WideToUtf8(s).c_str());
    }

    printf("\n--- FormatVolume Tests ---\n");
    int64_t vols[] = {15290556, 448294000, 0, 9999, 15000};
    for (int64_t v : vols) {
        std::wstring s = FormatVolume(v);
        printf("  vol=%lld -> \"%s\"\n", v, WideToUtf8(s).c_str());
    }

    printf("\n--- FormatAmount Tests ---\n");
    double amts[] = {676818397, 3633910000.0, 0.0, 9999.0};
    for (double a : amts) {
        std::wstring s = FormatAmount(a);
        printf("  amt=%.0f -> \"%s\"\n", a, WideToUtf8(s).c_str());
    }

    printf("\n--- Change Display Tests ---\n");
    double chgs[] = {7.62, -1.50, 0.0, 14.27, -0.05};
    for (double c : chgs) {
        WCHAR chgBuf[32];
        if (c > 0) {
            StringCchPrintfW(chgBuf, 32, L"+%s", FormatPrice(c, 0).c_str());
        } else if (c < 0) {
            StringCchPrintfW(chgBuf, 32, L"%s", FormatPrice(c, 0).c_str());
        } else {
            StringCchPrintfW(chgBuf, 32, L"0.00");
        }
        printf("  change=%.2f -> \"%s\"\n", c, WideToUtf8(chgBuf).c_str());
    }

    printf("\n--- Live API Data ---\n");
    std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn",
        L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=sh_a&symbol=&_s_r_a=auto", 80);

    if (response.empty()) {
        printf("FAILED: Empty response\n");
        return 1;
    }
    printf("Response length: %zu\n", response.length());

    size_t arrStart = response.find('[');
    if (arrStart == std::string::npos) {
        printf("FAILED: No array in response\n");
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

        printf("\n  Stock %d (code=%s):\n", count+1, code.c_str());
        printf("    Name: %s\n", WideToUtf8(nameW).c_str());
        printf("    CurrentPrice: %.3f -> \"%s\"\n", currentPrice, WideToUtf8(FormatPrice(currentPrice, 0)).c_str());

        WCHAR chgBuf[32];
        if (changeAmount > 0) StringCchPrintfW(chgBuf, 32, L"+%s", FormatPrice(changeAmount, 0).c_str());
        else if (changeAmount < 0) StringCchPrintfW(chgBuf, 32, L"%s", FormatPrice(changeAmount, 0).c_str());
        else StringCchPrintfW(chgBuf, 32, L"0.00");
        printf("    ChangeAmount: %.3f -> \"%s\"\n", changeAmount, WideToUtf8(chgBuf).c_str());

        WCHAR pctBuf[32];
        if (changePercent > 0) StringCchPrintfW(pctBuf, 32, L"+%.2f%%", changePercent);
        else if (changePercent < 0) StringCchPrintfW(pctBuf, 32, L"%.2f%%", changePercent);
        else StringCchPrintfW(pctBuf, 32, L"0.00%%");
        printf("    ChangePercent: %.3f -> \"%s\"\n", changePercent, WideToUtf8(pctBuf).c_str());

        printf("    OpenPrice: %.3f -> \"%s\"\n", openPrice, WideToUtf8(FormatPrice(openPrice, 0)).c_str());
        printf("    HighPrice: %.3f -> \"%s\"\n", highPrice, WideToUtf8(FormatPrice(highPrice, 0)).c_str());
        printf("    LowPrice: %.3f -> \"%s\"\n", lowPrice, WideToUtf8(FormatPrice(lowPrice, 0)).c_str());
        printf("    PrevClose: %.3f -> \"%s\"\n", prevClose, WideToUtf8(FormatPrice(prevClose, 0)).c_str());
        printf("    Volume: %.0f -> \"%s\"\n", volume, WideToUtf8(FormatVolume((int64_t)volume)).c_str());
        printf("    Amount: %.0f -> \"%s\"\n", amount, WideToUtf8(FormatAmount(amount)).c_str());

        WCHAR turnBuf[32];
        if (turnover > 0) StringCchPrintfW(turnBuf, 32, L"%.2f%%", turnover);
        else StringCchPrintfW(turnBuf, 32, L"--");
        printf("    TurnoverRate: %.2f -> \"%s\"\n", turnover, WideToUtf8(turnBuf).c_str());

        pos = objEnd + 1;
        count++;
    }

    printf("\n=== Test Complete ===\n");
    return 0;
}
