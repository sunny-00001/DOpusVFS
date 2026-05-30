#include <windows.h>
#include <iostream>
#include <string>
#include <cassert>
#include <vector>

#include "../src/StockClient.h"

static int g_passCount = 0;
static int g_failCount = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            std::wcout << L"  [PASS] " << msg << std::endl; \
            g_passCount++; \
        } else { \
            std::wcout << L"  [FAIL] " << msg << std::endl; \
            g_failCount++; \
        } \
    } while(0)

void TestUtilityFunctions() {
    std::wcout << L"\n=== \u5de5\u5177\u51fd\u6570\u6d4b\u8bd5 ===" << std::endl;

    {
        auto result = StockClient::SymbolToSinaCode(L"600519.SH");
        TEST_ASSERT(result == L"sh600519", L"SymbolToSinaCode SH");
    }
    {
        auto result = StockClient::SymbolToSinaCode(L"000001.SZ");
        TEST_ASSERT(result == L"sz000001", L"SymbolToSinaCode SZ");
    }
    {
        auto result = StockClient::SymbolToSinaCode(L"00700.HK");
        TEST_ASSERT(result == L"hk00700", L"SymbolToSinaCode HK");
    }
    {
        auto result = StockClient::SymbolToSinaCode(L"AAPL.US");
        TEST_ASSERT(result == L"gb_AAPL", L"SymbolToSinaCode US");
    }

    {
        auto result = StockClient::SinaCodeToSymbol(L"sh600519");
        TEST_ASSERT(result == L"600519.SH", L"SinaCodeToSymbol SH");
    }
    {
        auto result = StockClient::SinaCodeToSymbol(L"sz000001");
        TEST_ASSERT(result == L"000001.SZ", L"SinaCodeToSymbol SZ");
    }

    {
        auto result = StockClient::GetMarketFromSymbol(L"600519.SH");
        TEST_ASSERT(result == STOCK_MARKET_SH, L"GetMarketFromSymbol SH");
    }
    {
        auto result = StockClient::GetMarketFromSymbol(L"000001.SZ");
        TEST_ASSERT(result == STOCK_MARKET_SZ, L"GetMarketFromSymbol SZ");
    }
    {
        auto result = StockClient::GetMarketFromSymbol(L"00700.HK");
        TEST_ASSERT(result == STOCK_MARKET_HK, L"GetMarketFromSymbol HK");
    }
    {
        auto result = StockClient::GetMarketFromSymbol(L"AAPL.US");
        TEST_ASSERT(result == STOCK_MARKET_US, L"GetMarketFromSymbol US");
    }

    {
        auto result = StockClient::GetMarketName(STOCK_MARKET_SH);
        TEST_ASSERT(result == L"\u6caa\u5e02", L"GetMarketName SH");
    }
    {
        auto result = StockClient::GetMarketName(STOCK_MARKET_SZ);
        TEST_ASSERT(result == L"\u6df1\u5e02", L"GetMarketName SZ");
    }
    {
        auto result = StockClient::GetMarketName(STOCK_MARKET_HK);
        TEST_ASSERT(result == L"\u6e2f\u80a1", L"GetMarketName HK");
    }
    {
        auto result = StockClient::GetMarketName(STOCK_MARKET_US);
        TEST_ASSERT(result == L"\u7f8e\u80a1", L"GetMarketName US");
    }

    {
        auto result = StockClient::FormatPrice(123.45, STOCK_MARKET_SH);
        TEST_ASSERT(result.find(L"123.45") != std::wstring::npos, L"FormatPrice A\u80a1");
    }
    {
        auto result = StockClient::FormatPrice(123.456, STOCK_MARKET_HK);
        TEST_ASSERT(result.find(L"123.456") != std::wstring::npos, L"FormatPrice \u6e2f\u80a1");
    }

    {
        auto result = StockClient::FormatVolume(150000000);
        TEST_ASSERT(result.find(L"\u4ebf") != std::wstring::npos, L"FormatVolume \u4ebf");
    }
    {
        auto result = StockClient::FormatVolume(50000);
        TEST_ASSERT(result.find(L"\u4e07") != std::wstring::npos, L"FormatVolume \u4e07");
    }

    {
        auto result = StockClient::FormatAmount(250000000.0);
        TEST_ASSERT(result.find(L"\u4ebf") != std::wstring::npos, L"FormatAmount \u4ebf");
    }

    {
        std::wstring testStr = L"Hello\u4e16\u754c123";
        std::string utf8 = StockClient::WideToUtf8(testStr);
        std::wstring back = StockClient::Utf8ToWide(utf8);
        TEST_ASSERT(back == testStr, L"WideToUtf8/Utf8ToWide \u5f80\u8fd4\u8f6c\u6362");
    }
}

void TestConfigManagement() {
    std::wcout << L"\n=== \u914d\u7f6e\u7ba1\u7406\u6d4b\u8bd5 ===" << std::endl;

    {
        StockConfig& cfg = StockClient::GetConfig();
        TEST_ASSERT(cfg.dataSource == STOCK_SOURCE_SINA, L"\u9ed8\u8ba4\u6570\u636e\u6e90\u4e3a\u65b0\u6d6a");
        TEST_ASSERT(cfg.cacheTimeout == 5, L"\u9ed8\u8ba4\u7f13\u5b58\u65f6\u95f4\u4e3a5\u79d2");
        TEST_ASSERT(cfg.showChangeColor == true, L"\u9ed8\u8ba4\u663e\u793a\u6da8\u8dcc\u989c\u8272");
        TEST_ASSERT(cfg.metaPrefix == L".st", L"\u9ed8\u8ba4\u5143\u6570\u636e\u524d\u7f00\u4e3a.st");
    }

    {
        bool result = StockClient::SaveConfig();
        TEST_ASSERT(result, L"\u4fdd\u5b58\u914d\u7f6e\u6587\u4ef6");
    }

    {
        StockConfig& cfg = StockClient::GetConfig();
        int oldTimeout = cfg.cacheTimeout;
        cfg.cacheTimeout = 10;
        StockClient::SaveConfig();
        StockClient::LoadConfig();
        TEST_ASSERT(StockClient::GetConfig().cacheTimeout == 10, L"\u914d\u7f6e\u52a0\u8f7d\u9a8c\u8bc1");
        cfg.cacheTimeout = oldTimeout;
        StockClient::SaveConfig();
    }
}

void TestWatchlistManagement() {
    std::wcout << L"\n=== \u81ea\u9009\u80a1\u7ba1\u7406\u6d4b\u8bd5 ===" << std::endl;

    const std::wstring testSymbol = L"999999.SH";
    const std::wstring testName = L"\u6d4b\u8bd5\u80a1\u7968";

    StockClient::RemoveFromWatchlist(testSymbol);

    {
        bool inList = StockClient::IsInWatchlist(testSymbol);
        TEST_ASSERT(!inList, L"\u521d\u59cb\u72b6\u6001\uff1a\u6d4b\u8bd5\u80a1\u7968\u4e0d\u5728\u81ea\u9009");
    }

    {
        bool result = StockClient::AddToWatchlist(testSymbol, testName);
        TEST_ASSERT(result, L"\u6dfb\u52a0\u80a1\u7968\u5230\u81ea\u9009");
    }

    {
        bool inList = StockClient::IsInWatchlist(testSymbol);
        TEST_ASSERT(inList, L"\u6dfb\u52a0\u540e\u80a1\u7968\u5728\u81ea\u9009\u4e2d");
    }

    {
        bool result = StockClient::AddToWatchlist(testSymbol, testName);
        TEST_ASSERT(result, L"\u91cd\u590d\u6dfb\u52a0\u4e0d\u62a5\u9519");
    }

    {
        auto watchlist = StockClient::GetWatchlist();
        bool found = false;
        for (const auto& entry : watchlist) {
            if (entry.symbol == testSymbol) {
                found = true;
                break;
            }
        }
        TEST_ASSERT(found, L"\u81ea\u9009\u5217\u8868\u4e2d\u80fd\u627e\u5230\u6dfb\u52a0\u7684\u80a1\u7968");
    }

    {
        bool result = StockClient::RemoveFromWatchlist(testSymbol);
        TEST_ASSERT(result, L"\u4ece\u81ea\u9009\u4e2d\u5220\u9664\u80a1\u7968");
    }

    {
        bool inList = StockClient::IsInWatchlist(testSymbol);
        TEST_ASSERT(!inList, L"\u5220\u9664\u540e\u80a1\u7968\u4e0d\u5728\u81ea\u9009\u4e2d");
    }
}

void TestQuoteRetrieval() {
    std::wcout << L"\n=== \u5b9e\u65f6\u884c\u60c5\u83b7\u53d6\u6d4b\u8bd5 ===" << std::endl;

    {
        StockQuote quote = StockClient::GetQuote(L"600519.SH");
        TEST_ASSERT(quote.symbol == L"600519.SH", L"\u83b7\u53d6\u8305\u53f0\u80a1\u7968\u4ee3\u7801\u6b63\u786e");
        if (quote.currentPrice > 0) {
            std::wcout << L"  [INFO] \u8d35\u5dde\u8305\u53f0: " << quote.name
                       << L" \u4ef7\u683c=" << StockClient::FormatPrice(quote.currentPrice, quote.market)
                       << L" \u6da8\u8dcc=" << quote.changePercent << L"%" << std::endl;
            TEST_ASSERT(!quote.name.empty(), L"\u80a1\u7968\u540d\u79f0\u975e\u7a7a");
            TEST_ASSERT(quote.currentPrice > 0, L"\u5f53\u524d\u4ef7\u683c\u5927\u4e8e0");
        } else {
            std::wcout << L"  [WARN] \u65e0\u6cd5\u83b7\u53d6\u5b9e\u65f6\u884c\u60c5\uff08\u53ef\u80fd\u662f\u975e\u4ea4\u6613\u65f6\u95f4\u6216\u7f51\u7edc\u95ee\u9898\uff09" << std::endl;
        }
    }

    {
        std::vector<std::wstring> symbols = {L"600519.SH", L"000001.SZ"};
        auto quotes = StockClient::GetQuotes(symbols);
        TEST_ASSERT(quotes.size() >= 1, L"\u6279\u91cf\u83b7\u53d6\u884c\u60c5\u8fd4\u56de\u7ed3\u679c");
    }
}

void TestSearchFunction() {
    std::wcout << L"\n=== \u641c\u7d22\u529f\u80fd\u6d4b\u8bd5 ===" << std::endl;

    {
        auto result = StockClient::SearchStock(L"\u8305\u53f0");
        if (!result.stocks.empty()) {
            bool found = false;
            for (const auto& s : result.stocks) {
                if (s.name.find(L"\u8305\u53f0") != std::wstring::npos) {
                    found = true;
                    break;
                }
            }
            TEST_ASSERT(found, L"\u641c\u7d22\u8305\u53f0\u627e\u5230\u5339\u914d\u7ed3\u679c");
            std::wcout << L"  [INFO] \u641c\u7d22\u7ed3\u679c\u6570: " << result.stocks.size() << std::endl;
        } else {
            std::wcout << L"  [WARN] \u641c\u7d22\u65e0\u7ed3\u679c\uff08\u53ef\u80fd\u662f\u7f51\u7edc\u95ee\u9898\uff09" << std::endl;
        }
    }

    {
        auto result = StockClient::SearchStock(L"600519");
        if (!result.stocks.empty()) {
            TEST_ASSERT(true, L"\u6309\u4ee3\u7801\u641c\u7d22\u6709\u7ed3\u679c");
        } else {
            std::wcout << L"  [WARN] \u4ee3\u7801\u641c\u7d22\u65e0\u7ed3\u679c" << std::endl;
        }
    }

    {
        auto result = StockClient::SearchStock(L"");
        TEST_ASSERT(result.stocks.empty(), L"\u7a7a\u5173\u952e\u8bcd\u641c\u7d22\u8fd4\u56de\u7a7a");
    }
}

void TestCacheFunction() {
    std::wcout << L"\n=== \u7f13\u5b58\u529f\u80fd\u6d4b\u8bd5 ===" << std::endl;

    StockClient::InvalidateCache();

    {
        DWORD start = GetTickCount();
        StockClient::GetQuote(L"600519.SH");
        DWORD firstTime = GetTickCount() - start;

        start = GetTickCount();
        StockClient::GetQuote(L"600519.SH");
        DWORD secondTime = GetTickCount() - start;

        TEST_ASSERT(secondTime <= firstTime + 50, L"\u7f13\u5b58\u547d\u4e2d\u65f6\u95f4\u66f4\u77ed");
        std::wcout << L"  [INFO] \u9996\u6b21: " << firstTime << L"ms, \u7f13\u5b58: " << secondTime << L"ms" << std::endl;
    }

    {
        StockClient::InvalidateCache();
        StockQuote q1 = StockClient::GetQuote(L"000001.SZ");
        StockQuote q2 = StockClient::GetQuote(L"000001.SZ");
        if (q1.currentPrice > 0) {
            TEST_ASSERT(q1.currentPrice == q2.currentPrice, L"\u7f13\u5b58\u6570\u636e\u4e00\u81f4");
        }
    }
}

void TestMarketListFunction() {
    std::wcout << L"\n=== \u5e02\u573a\u80a1\u7968\u5217\u8868\u6d4b\u8bd5 ===" << std::endl;

    {
        auto stocks = StockClient::ListMarketStocks(STOCK_MARKET_SH);
        if (!stocks.empty()) {
            TEST_ASSERT(stocks.size() > 100, L"\u6caa\u5e02\u80a1\u7968\u6570\u91cf\u8d85\u8fc7100");
            std::wcout << L"  [INFO] \u6caa\u5e02\u80a1\u7968\u6570: " << stocks.size() << std::endl;
            std::wcout << L"  [INFO] \u7b2c1\u53ea: " << stocks[0].code << L" " << stocks[0].name
                       << L" \u4ef7\u683c=" << stocks[0].currentPrice << std::endl;
            TEST_ASSERT(!stocks[0].code.empty(), L"\u80a1\u7968\u4ee3\u7801\u975e\u7a7a");
            TEST_ASSERT(!stocks[0].name.empty(), L"\u80a1\u7968\u540d\u79f0\u975e\u7a7a");
        } else {
            std::wcout << L"  [WARN] \u6caa\u5e02\u5217\u8868\u4e3a\u7a7a\uff08\u53ef\u80fd\u662f\u7f51\u7edc\u95ee\u9898\uff09" << std::endl;
        }
    }

    {
        auto stocks = StockClient::ListMarketStocks(STOCK_MARKET_SZ);
        if (!stocks.empty()) {
            TEST_ASSERT(stocks.size() > 100, L"\u6df1\u5e02\u80a1\u7968\u6570\u91cf\u8d85\u8fc7100");
            std::wcout << L"  [INFO] \u6df1\u5e02\u80a1\u7968\u6570: " << stocks.size() << std::endl;
        } else {
            std::wcout << L"  [WARN] \u6df1\u5e02\u5217\u8868\u4e3a\u7a7a\uff08\u53ef\u80fd\u662f\u7f51\u7edc\u95ee\u9898\uff09" << std::endl;
        }
    }

    {
        auto stocks1 = StockClient::ListMarketStocks(STOCK_MARKET_SH);
        auto stocks2 = StockClient::ListMarketStocks(STOCK_MARKET_SH);
        if (!stocks1.empty() && !stocks2.empty()) {
            TEST_ASSERT(stocks1.size() == stocks2.size(), L"\u7f13\u5b58\u547d\u4e2d\u65f6\u6570\u636e\u4e00\u81f4");
        }
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout.imbue(std::locale(""));

    std::wcout << L"======================================" << std::endl;
    std::wcout << L"  StockVFS \u5355\u5143\u6d4b\u8bd5" << std::endl;
    std::wcout << L"======================================" << std::endl;

    StockClient::Init();

    TestUtilityFunctions();
    TestConfigManagement();
    TestWatchlistManagement();
    TestQuoteRetrieval();
    TestSearchFunction();
    TestCacheFunction();
    TestMarketListFunction();

    StockClient::Cleanup();

    std::wcout << L"\n======================================" << std::endl;
    std::wcout << L"  \u6d4b\u8bd5\u7ed3\u679c: " << g_passCount << L" \u901a\u8fc7, " << g_failCount << L" \u5931\u8d25" << std::endl;
    std::wcout << L"======================================" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
