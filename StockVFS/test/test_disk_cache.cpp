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

bool SaveMarketListToDisk(StockMarket market, const std::vector<StockQuote>& quotes) {
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

std::vector<StockQuote> LoadMarketListFromDisk(StockMarket market) {
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
    printf("=== Disk Cache Test ===\n\n");

    std::wstring cacheDir = GetCacheDirPath();
    printf("Cache dir: %s\n", WideToUtf8(cacheDir).c_str());

    printf("\n--- Test 1: Save market list to disk ---\n");
    std::vector<StockQuote> testQuotes;
    for (int i = 0; i < 5; i++) {
        StockQuote q;
        q.code = L"688" + std::to_wstring(100 + i);
        q.symbol = q.code + L".SH";
        q.name = L"TestStock_" + std::to_wstring(i);
        q.market = STOCK_MARKET_SH;
        q.currentPrice = 10.5 + i * 1.23;
        q.previousClose = 10.0 + i;
        q.openPrice = 10.2 + i;
        q.highPrice = 11.0 + i;
        q.lowPrice = 9.5 + i;
        q.volume = 1000000 + i * 100000;
        q.amount = 10000000.0 + i * 1000000;
        q.changeAmount = 0.5 + i * 0.1;
        q.changePercent = 5.0 + i * 0.5;
        q.turnoverRate = 2.5 + i * 0.3;
        testQuotes.push_back(q);
    }

    bool saved = SaveMarketListToDisk(STOCK_MARKET_SH, testQuotes);
    printf("Save result: %s\n", saved ? "SUCCESS" : "FAILED");

    printf("\n--- Test 2: Load market list from disk ---\n");
    std::vector<StockQuote> loaded = LoadMarketListFromDisk(STOCK_MARKET_SH);
    printf("Loaded %zu stocks\n", loaded.size());
    for (size_t i = 0; i < loaded.size() && i < 3; i++) {
        const auto& q = loaded[i];
        printf("  [%zu] code=%s name=%s price=%.3f chgAmt=%.3f chgPct=%.3f vol=%lld amt=%.2f\n",
            i, WideToUtf8(q.code).c_str(), WideToUtf8(q.name).c_str(),
            q.currentPrice, q.changeAmount, q.changePercent, q.volume, q.amount);
    }

    printf("\n--- Test 3: Verify data integrity ---\n");
    bool match = true;
    if (loaded.size() != testQuotes.size()) {
        printf("FAILED: size mismatch (expected %zu, got %zu)\n", testQuotes.size(), loaded.size());
        match = false;
    } else {
        for (size_t i = 0; i < testQuotes.size(); i++) {
            const auto& orig = testQuotes[i];
            const auto& load = loaded[i];
            if (orig.code != load.code || orig.symbol != load.symbol ||
                orig.currentPrice != load.currentPrice ||
                orig.changeAmount != load.changeAmount ||
                orig.volume != load.volume) {
                printf("FAILED: stock %zu mismatch\n", i);
                printf("  orig: code=%s price=%.3f chgAmt=%.3f vol=%lld\n",
                    WideToUtf8(orig.code).c_str(), orig.currentPrice, orig.changeAmount, orig.volume);
                printf("  load: code=%s price=%.3f chgAmt=%.3f vol=%lld\n",
                    WideToUtf8(load.code).c_str(), load.currentPrice, load.changeAmount, load.volume);
                match = false;
            }
        }
    }
    if (match) printf("Data integrity: PASS\n");

    printf("\n--- Test 4: Verify cache file content ---\n");
    std::wstring filePath = cacheDir + L"\\" + MarketToCacheKey(STOCK_MARKET_SH) + L".json";
    std::ifstream file(filePath, std::ios::binary);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        printf("File size: %zu bytes\n", content.length());
        printf("First 200 chars: %.200s\n", content.c_str());
    } else {
        printf("FAILED: Cannot open cache file\n");
    }

    printf("\n=== Test Complete ===\n");
    return 0;
}
