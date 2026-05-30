#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "StockClient.h"

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")

static void WriteResult(const char* filename, const std::wstring& content) {
    FILE* f = fopen(filename, "w, ccs=UTF-8");
    if (f) {
        fputws(content.c_str(), f);
        fclose(f);
    }
}

int main() {
    StockClient::Init();

    std::wostringstream oss;

    oss << L"=== Test: Eastmoney Industry Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_INDUSTRY);
        oss << L"Total: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 10; i++) {
            const auto& s = sectors[i];
            oss << L"  [" << i << L"] " << s.name << L" (" << s.code
                << L") count=" << s.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << s.changePercent << L"%"
                << L" lead=" << s.leadStockName
                << L" leadPrice=" << s.leadPrice
                << L"\r\n";
        }
    }

    oss << L"\r\n=== Test: Eastmoney Concept Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_CONCEPT);
        oss << L"Total: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 5; i++) {
            const auto& s = sectors[i];
            oss << L"  [" << i << L"] " << s.name << L" (" << s.code
                << L") count=" << s.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << s.changePercent << L"%"
                << L"\r\n";
        }
    }

    oss << L"\r\n=== Test: Eastmoney Area Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_AREA);
        oss << L"Total: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 5; i++) {
            const auto& s = sectors[i];
            oss << L"  [" << i << L"] " << s.name << L" (" << s.code
                << L") count=" << s.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << s.changePercent << L"%"
                << L"\r\n";
        }
    }

    oss << L"\r\n=== Test: SH Market Stocks (first 10) ===\r\n";
    {
        auto stocks = StockClient::ListMarketStocks(STOCK_MARKET_SH);
        oss << L"Total: " << stocks.size() << L"\r\n";
        for (size_t i = 0; i < stocks.size() && i < 10; i++) {
            const auto& s = stocks[i];
            oss << L"  [" << i << L"] " << s.name << L" (" << s.symbol
                << L") price=" << std::fixed << std::setprecision(2) << s.currentPrice
                << L" chg%=" << s.changePercent << L"%"
                << L" pe=" << s.pe << L" pb=" << s.pb
                << L" mktcap=" << s.marketCap
                << L"\r\n";
        }
    }

    StockClient::Cleanup();
    oss << L"\r\n=== Done ===\r\n";

    WriteResult("test_em_api_result.txt", oss.str());
    std::wcout << oss.str();

    return 0;
}
