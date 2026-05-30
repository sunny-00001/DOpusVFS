#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <strsafe.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Shell32.lib")

enum StockMarket {
    STOCK_MARKET_SH = 0,
    STOCK_MARKET_SZ = 1,
    STOCK_MARKET_HK = 2,
    STOCK_MARKET_US = 3,
    STOCK_MARKET_BJ = 4
};

struct StockQuote {
    std::wstring code;
    std::wstring symbol;
    std::wstring name;
    StockMarket market;
    double currentPrice;
    double previousClose;
    double openPrice;
    double highPrice;
    double lowPrice;
    int64_t volume;
    double amount;
    double changeAmount;
    double changePercent;
    double turnoverRate;

    StockQuote() : market(STOCK_MARKET_SH), currentPrice(0), previousClose(0),
        openPrice(0), highPrice(0), lowPrice(0), volume(0), amount(0),
        changeAmount(0), changePercent(0), turnoverRate(0) {}
};

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

static std::wstring GetCacheDirPath() {
    WCHAR appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\StockVFS\\cache";
        CreateDirectoryW((std::wstring(appData) + L"\\StockVFS").c_str(), NULL);
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L"";
}

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path,
                                int port = 443, bool https = true) {
    HINTERNET hSession = WinHttpOpen(L"StockVFS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    DWORD timeout = 10000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            response.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static std::wstring DecodeJsonUnicode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (i + 5 < str.length() && str[i] == '\\' && str[i + 1] == 'u') {
            unsigned int codePoint = 0;
            char hex[5] = {};
            memcpy(hex, str.c_str() + i + 2, 4);
            if (sscanf(hex, "%x", &codePoint) == 1 && codePoint > 0) {
                if (codePoint < 0x80) {
                    result += (char)codePoint;
                } else if (codePoint < 0x800) {
                    result += (char)(0xC0 | (codePoint >> 6));
                    result += (char)(0x80 | (codePoint & 0x3F));
                } else {
                    result += (char)(0xE0 | (codePoint >> 12));
                    result += (char)(0x80 | ((codePoint >> 6) & 0x3F));
                    result += (char)(0x80 | (codePoint & 0x3F));
                }
            }
            i += 5;
        } else {
            result += str[i];
        }
    }
    return Utf8ToWide(result);
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t startPos = json.find(searchKey);
    if (startPos == std::string::npos) return "";
    startPos += searchKey.length();
    size_t endPos = json.find('"', startPos);
    if (endPos == std::string::npos) return "";
    return json.substr(startPos, endPos - startPos);
}

static double ExtractJsonDouble(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t startPos = json.find(searchKey);
    if (startPos == std::string::npos) return 0.0;
    startPos += searchKey.length();
    while (startPos < json.length() && (json[startPos] == ' ' || json[startPos] == '\t')) startPos++;
    if (startPos >= json.length()) return 0.0;
    char* end = nullptr;
    double val = strtod(json.c_str() + startPos, &end);
    if (end == json.c_str() + startPos) return 0.0;
    return val;
}

static std::vector<StockQuote> FetchSinaMarketStocks(StockMarket market) {
    std::vector<StockQuote> allQuotes;
    std::wstring node;
    switch (market) {
    case STOCK_MARKET_SH: node = L"hs_a"; break;
    case STOCK_MARKET_SZ: node = L"sz_a"; break;
    case STOCK_MARKET_BJ: node = L"bj_a"; break;
    default: return allQuotes;
    }

    int page = 1;
    const int pageSize = 80;
    while (true) {
        std::wostringstream pathBuilder;
        pathBuilder << L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?"
                    << L"page=" << page << L"&num=" << pageSize
                    << L"&sort=symbol&asc=1&node=" << node
                    << L"&symbol=&_srand=" << GetTickCount64();
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", pathBuilder.str(), 80, false);
        if (response.empty()) break;

        if (response.find("code") == std::string::npos) break;

        size_t pos = 0;
        int count = 0;
        while ((pos = response.find("{", pos)) != std::string::npos) {
            size_t endPos = response.find("}", pos);
            if (endPos == std::string::npos) break;
            std::string obj = response.substr(pos, endPos - pos + 1);

            StockQuote q;
            std::string codeStr = ExtractJsonString(obj, "code");
            std::string nameStr = ExtractJsonString(obj, "name");

            q.code = Utf8ToWide(codeStr);
            q.name = DecodeJsonUnicode(nameStr);
            q.market = market;

            if (codeStr.length() >= 6) {
                if (codeStr.substr(0, 2) == "60" || codeStr.substr(0, 2) == "68") {
                    q.symbol = q.code + L".SH";
                } else {
                    q.symbol = q.code + L".SZ";
                }
            }

            q.currentPrice = ExtractJsonDouble(obj, "trade");
            q.previousClose = ExtractJsonDouble(obj, "settlement");
            q.openPrice = ExtractJsonDouble(obj, "open");
            q.highPrice = ExtractJsonDouble(obj, "high");
            q.lowPrice = ExtractJsonDouble(obj, "low");
            q.volume = (int64_t)ExtractJsonDouble(obj, "volume");
            q.amount = ExtractJsonDouble(obj, "amount");
            q.changeAmount = ExtractJsonDouble(obj, "pricechange");
            q.changePercent = ExtractJsonDouble(obj, "changepercent");
            q.turnoverRate = ExtractJsonDouble(obj, "turnoverratio");

            if (!q.code.empty()) {
                allQuotes.push_back(q);
                count++;
            }
            pos = endPos + 1;
        }

        if (count < pageSize) break;
        page++;
    }

    return allQuotes;
}

static std::wstring MarketToCacheKey(StockMarket market) {
    switch (market) {
    case STOCK_MARKET_SH: return L"market_sh";
    case STOCK_MARKET_SZ: return L"market_sz";
    case STOCK_MARKET_HK: return L"market_hk";
    case STOCK_MARKET_US: return L"market_us";
    case STOCK_MARKET_BJ: return L"market_bj";
    default: return L"market_unknown";
    }
}

static std::wstring QuoteToJsonFields(const StockQuote& q) {
    std::wostringstream oss;
    oss << L"\"code\":\"" << q.code << L"\",";
    oss << L"\"symbol\":\"" << q.symbol << L"\",";
    oss << L"\"name\":\"" << q.name << L"\",";
    oss << L"\"market\":" << (int)q.market << L",";
    oss << L"\"currentPrice\":" << std::fixed << std::setprecision(3) << q.currentPrice << L",";
    oss << L"\"previousClose\":" << q.previousClose << L",";
    oss << L"\"openPrice\":" << q.openPrice << L",";
    oss << L"\"highPrice\":" << q.highPrice << L",";
    oss << L"\"lowPrice\":" << q.lowPrice << L",";
    oss << L"\"volume\":" << q.volume << L",";
    oss << L"\"amount\":" << std::setprecision(2) << q.amount << L",";
    oss << L"\"changeAmount\":" << std::setprecision(3) << q.changeAmount << L",";
    oss << L"\"changePercent\":" << std::setprecision(3) << q.changePercent << L",";
    oss << L"\"turnoverRate\":" << std::setprecision(2) << q.turnoverRate;
    return oss.str();
}

static bool SaveMarketListToDisk(StockMarket market, const std::vector<StockQuote>& quotes) {
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n  \"timestamp\":" << GetTickCount64() << L",\n";
    oss << L"  \"market\":" << (int)market << L",\n";
    oss << L"  \"stocks\":[\n";
    for (size_t i = 0; i < quotes.size(); i++) {
        oss << L"    {" << QuoteToJsonFields(quotes[i]) << L"}";
        if (i < quotes.size() - 1) oss << L",";
        oss << L"\n";
    }
    oss << L"  ]\n}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    std::wstring filePath = cacheDir + L"\\" + MarketToCacheKey(market) + L".json";
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();
    return true;
}

static StockQuote ParseQuoteFromJsonObj(const std::wstring& obj) {
    StockQuote q;
    auto extractStr = [](const std::wstring& obj, const std::wstring& key) -> std::wstring {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = obj.find(searchKey);
        if (kPos == std::wstring::npos) return L"";
        size_t colonPos = obj.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return L"";
        size_t q1 = obj.find(L'"', colonPos + 1);
        if (q1 == std::wstring::npos) return L"";
        size_t q2 = obj.find(L'"', q1 + 1);
        if (q2 == std::wstring::npos) return L"";
        return obj.substr(q1 + 1, q2 - q1 - 1);
    };
    auto extractDbl = [&extractStr](const std::wstring& obj, const std::wstring& key) -> double {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = obj.find(searchKey);
        if (kPos == std::wstring::npos) return 0.0;
        size_t colonPos = obj.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return 0.0;
        size_t valStart = colonPos + 1;
        while (valStart < obj.length() && (obj[valStart] == L' ' || obj[valStart] == L'\t')) valStart++;
        size_t valEnd = valStart;
        while (valEnd < obj.length() && obj[valEnd] != L',' && obj[valEnd] != L'}' && obj[valEnd] != L']') valEnd++;
        std::wstring numStr = obj.substr(valStart, valEnd - valStart);
        return _wtof(numStr.c_str());
    };
    auto extractInt = [](const std::wstring& obj, const std::wstring& key) -> int {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = obj.find(searchKey);
        if (kPos == std::wstring::npos) return 0;
        size_t colonPos = obj.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return 0;
        size_t valStart = colonPos + 1;
        while (valStart < obj.length() && (obj[valStart] == L' ' || obj[valStart] == L'\t')) valStart++;
        size_t valEnd = valStart;
        while (valEnd < obj.length() && obj[valEnd] >= L'0' && obj[valEnd] <= L'9') valEnd++;
        return _wtoi(obj.substr(valStart, valEnd - valStart).c_str());
    };

    q.code = extractStr(obj, L"code");
    q.symbol = extractStr(obj, L"symbol");
    q.name = extractStr(obj, L"name");
    q.market = (StockMarket)extractInt(obj, L"market");
    q.currentPrice = extractDbl(obj, L"currentPrice");
    q.previousClose = extractDbl(obj, L"previousClose");
    q.openPrice = extractDbl(obj, L"openPrice");
    q.highPrice = extractDbl(obj, L"highPrice");
    q.lowPrice = extractDbl(obj, L"lowPrice");
    q.volume = (int64_t)extractDbl(obj, L"volume");
    q.amount = extractDbl(obj, L"amount");
    q.changeAmount = extractDbl(obj, L"changeAmount");
    q.changePercent = extractDbl(obj, L"changePercent");
    q.turnoverRate = extractDbl(obj, L"turnoverRate");
    return q;
}

static std::vector<StockQuote> LoadMarketListFromDisk(StockMarket market) {
    std::vector<StockQuote> result;
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return result;

    std::wstring filePath = cacheDir + L"\\" + MarketToCacheKey(market) + L".json";
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return result;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (content.empty()) return result;

    std::wstring json = Utf8ToWide(content);
    size_t arrStart = json.find(L'[');
    if (arrStart == std::wstring::npos) return result;

    size_t pos = arrStart + 1;
    while (pos < json.length()) {
        size_t objStart = json.find(L'{', pos);
        if (objStart == std::wstring::npos) break;
        size_t objEnd = json.find(L'}', objStart);
        if (objEnd == std::wstring::npos) break;

        std::wstring obj = json.substr(objStart, objEnd - objStart + 1);
        StockQuote q = ParseQuoteFromJsonObj(obj);
        if (!q.code.empty()) {
            result.push_back(q);
        }
        pos = objEnd + 1;
    }

    return result;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    printf("=== End-to-End Cache Flow Test ===\n\n");

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    printf("--- Step 1: Fetch live data from Sina API ---\n");
    auto t1 = GetTickCount64();
    std::vector<StockQuote> liveQuotes = FetchSinaMarketStocks(STOCK_MARKET_SH);
    auto t2 = GetTickCount64();
    printf("Fetched %zu stocks in %llums\n", liveQuotes.size(), t2 - t1);

    if (liveQuotes.empty()) {
        printf("FAILED: Could not fetch live data. Aborting.\n");
        WSACleanup();
        return 1;
    }

    printf("  First 3 stocks:\n");
    for (size_t i = 0; i < 3 && i < liveQuotes.size(); i++) {
        const auto& q = liveQuotes[i];
        printf("    [%zu] %s (%s) price=%.3f chg=%.3f vol=%lld\n",
            i, WideToUtf8(q.name).c_str(), WideToUtf8(q.code).c_str(),
            q.currentPrice, q.changePercent, q.volume);
    }

    printf("\n--- Step 2: Save to disk cache ---\n");
    bool saved = SaveMarketListToDisk(STOCK_MARKET_SH, liveQuotes);
    printf("Save result: %s\n", saved ? "SUCCESS" : "FAILED");

    printf("\n--- Step 3: Load from disk cache ---\n");
    auto t3 = GetTickCount64();
    std::vector<StockQuote> cachedQuotes = LoadMarketListFromDisk(STOCK_MARKET_SH);
    auto t4 = GetTickCount64();
    printf("Loaded %zu stocks from disk in %llums\n", cachedQuotes.size(), t4 - t3);

    printf("\n--- Step 4: Verify data integrity ---\n");
    bool integrityOk = true;
    if (cachedQuotes.size() != liveQuotes.size()) {
        printf("FAILED: Size mismatch (live=%zu, cached=%zu)\n", liveQuotes.size(), cachedQuotes.size());
        integrityOk = false;
    } else {
        int mismatchCount = 0;
        for (size_t i = 0; i < liveQuotes.size(); i++) {
            const auto& live = liveQuotes[i];
            const auto& cached = cachedQuotes[i];
            if (live.code != cached.code || live.currentPrice != cached.currentPrice ||
                live.volume != cached.volume) {
                if (mismatchCount < 3) {
                    printf("  Mismatch at [%zu]: live(code=%s price=%.3f vol=%lld) vs cached(code=%s price=%.3f vol=%lld)\n",
                        i, WideToUtf8(live.code).c_str(), live.currentPrice, live.volume,
                        WideToUtf8(cached.code).c_str(), cached.currentPrice, cached.volume);
                }
                mismatchCount++;
            }
        }
        if (mismatchCount > 0) {
            printf("FAILED: %d mismatches out of %zu stocks\n", mismatchCount, liveQuotes.size());
            integrityOk = false;
        }
    }
    if (integrityOk) printf("Data integrity: PASS\n");

    printf("\n--- Step 5: Simulate cache-first + async refresh flow ---\n");
    printf("Simulating: user opens stock://market/sh\n");
    auto t5 = GetTickCount64();
    std::vector<StockQuote> diskQuotes = LoadMarketListFromDisk(STOCK_MARKET_SH);
    auto t6 = GetTickCount64();
    printf("  [Cache hit] Returned %zu stocks in %llums (from disk cache)\n", diskQuotes.size(), t6 - t5);
    printf("  [Background] Starting async refresh...\n");

    std::atomic<bool> refreshDone(false);
    std::thread asyncThread([&refreshDone]() {
        auto t7 = GetTickCount64();
        std::vector<StockQuote> freshQuotes = FetchSinaMarketStocks(STOCK_MARKET_SH);
        auto t8 = GetTickCount64();
        printf("  [Background] Fetched %zu fresh stocks in %llums\n", freshQuotes.size(), t8 - t7);
        if (!freshQuotes.empty()) {
            SaveMarketListToDisk(STOCK_MARKET_SH, freshQuotes);
            printf("  [Background] Updated disk cache\n");
        }
        refreshDone.store(true);
    });

    printf("  [Foreground] User sees cached data immediately\n");
    printf("  [Foreground] First 3 stocks from cache:\n");
    for (size_t i = 0; i < 3 && i < diskQuotes.size(); i++) {
        const auto& q = diskQuotes[i];
        printf("    [%zu] %s (%s) price=%.3f\n",
            i, WideToUtf8(q.name).c_str(), WideToUtf8(q.code).c_str(), q.currentPrice);
    }

    asyncThread.join();
    printf("  [Background] Async refresh complete: %s\n", refreshDone.load() ? "YES" : "NO");

    printf("\n--- Step 6: Verify refreshed cache ---\n");
    std::vector<StockQuote> refreshedQuotes = LoadMarketListFromDisk(STOCK_MARKET_SH);
    printf("Loaded %zu stocks from refreshed cache\n", refreshedQuotes.size());

    printf("\n=== Performance Summary ===\n");
    printf("  Live API fetch:      %llums\n", t2 - t1);
    printf("  Disk cache load:     %llums\n", t6 - t5);
    printf("  Speed improvement:   %.1fx faster\n", (t2 - t1 > 0) ? (double)(t2 - t1) / (t6 - t5) : 0.0);

    printf("\n=== Test Complete ===\n");
    WSACleanup();
    return 0;
}
