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
#include <codecvt>
#include <locale>

#include "StockClient.h"

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")

static void WriteToFile(const char* filename, const std::wstring& content) {
    FILE* f = fopen(filename, "w, ccs=UTF-8");
    if (f) {
        fputws(content.c_str(), f);
        fclose(f);
    }
}

static void ClearCache() {
    WCHAR appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\StockVFS\\cache";
        std::wstring pattern = dir + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring filePath = dir + L"\\" + fd.cFileName;
                    DeleteFileW(filePath.c_str());
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
}

int main() {
    ClearCache();
    StockClient::Init();

    std::wostringstream oss;

    oss << L"=== Test: Index List ===\r\n";
    {
        auto indices = StockClient::ListIndices();
        oss << L"Total indices: " << indices.size() << L"\r\n";
        for (size_t i = 0; i < indices.size() && i < 10; i++) {
            const auto& idx = indices[i];
            oss << L"  [" << i << L"] " << idx.name << L" (" << idx.symbol
                << L") point=" << std::fixed << std::setprecision(2) << idx.currentPoint
                << L" chg=" << idx.changeAmount
                << L" chg%=" << idx.changePercent << L"%\r\n";
        }
        if (indices.size() > 10) {
            oss << L"  ... and " << (indices.size() - 10) << L" more\r\n";
        }
    }

    oss << L"\r\n=== Test: Industry Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_INDUSTRY);
        oss << L"Total industry sectors: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 5; i++) {
            const auto& sec = sectors[i];
            oss << L"  [" << i << L"] " << sec.name << L" (" << sec.code
                << L") count=" << sec.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << sec.changePercent << L"%"
                << L" lead=" << sec.leadStockName << L"\r\n";
        }
        if (sectors.size() > 5) {
            oss << L"  ... and " << (sectors.size() - 5) << L" more\r\n";
        }
    }

    oss << L"\r\n=== Test: Concept Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_CONCEPT);
        oss << L"Total concept sectors: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 5; i++) {
            const auto& sec = sectors[i];
            oss << L"  [" << i << L"] " << sec.name << L" (" << sec.code
                << L") count=" << sec.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << sec.changePercent << L"%"
                << L" lead=" << sec.leadStockName << L"\r\n";
        }
        if (sectors.size() > 5) {
            oss << L"  ... and " << (sectors.size() - 5) << L" more\r\n";
        }
    }

    oss << L"\r\n=== Test: Area Sectors ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_AREA);
        oss << L"Total area sectors: " << sectors.size() << L"\r\n";
        for (size_t i = 0; i < sectors.size() && i < 5; i++) {
            const auto& sec = sectors[i];
            oss << L"  [" << i << L"] " << sec.name << L" (" << sec.code
                << L") count=" << sec.stockCount
                << L" chg%=" << std::fixed << std::setprecision(2) << sec.changePercent << L"%"
                << L" lead=" << sec.leadStockName << L"\r\n";
        }
        if (sectors.size() > 5) {
            oss << L"  ... and " << (sectors.size() - 5) << L" more\r\n";
        }
    }

    oss << L"\r\n=== Test: Sector Stocks (first industry sector) ===\r\n";
    {
        auto sectors = StockClient::ListSectors(SECTOR_TYPE_INDUSTRY);
        if (!sectors.empty()) {
            const auto& firstSector = sectors[0];
            oss << L"  Getting stocks for sector: " << firstSector.name << L" (" << firstSector.code << L")\r\n";
            auto stocks = StockClient::ListSectorStocks(firstSector.code);
            oss << L"  Total stocks: " << stocks.size() << L"\r\n";
            for (size_t i = 0; i < stocks.size() && i < 5; i++) {
                const auto& s = stocks[i];
                oss << L"    [" << i << L"] " << s.name << L" (" << s.symbol
                    << L") price=" << std::fixed << std::setprecision(2) << s.currentPrice
                    << L" chg%=" << s.changePercent << L"%"
                    << L" pe=" << s.pe << L" pb=" << s.pb
                    << L" mktcap=" << s.marketCap << L"\r\n";
            }
        } else {
            oss << L"  No industry sectors available\r\n";
        }
    }

    oss << L"\r\n=== Test: Stock Extra Fields (PE/PB/MarketCap) ===\r\n";
    {
        auto stocks = StockClient::ListMarketStocks(STOCK_MARKET_SH);
        oss << L"Total SH stocks: " << stocks.size() << L"\r\n";
        int withPE = 0, withPB = 0, withMktCap = 0;
        for (size_t i = 0; i < stocks.size() && i < 10; i++) {
            const auto& s = stocks[i];
            if (s.pe > 0) withPE++;
            if (s.pb > 0) withPB++;
            if (s.marketCap > 0) withMktCap++;
            oss << L"  [" << i << L"] " << s.name << L" (" << s.symbol
                << L") price=" << std::fixed << std::setprecision(2) << s.currentPrice
                << L" pe=" << s.pe << L" pb=" << s.pb
                << L" mktcap=" << s.marketCap << L" circap=" << s.circulatingCap << L"\r\n";
        }
        oss << L"  First 10 stocks: " << withPE << L" with PE, " << withPB << L" with PB, " << withMktCap << L" with MarketCap\r\n";
    }

    StockClient::Cleanup();
    oss << L"\r\n=== All tests completed ===\r\n";

    WriteToFile("test_index_sector_vfs_result.txt", oss.str());
    std::wcout << oss.str();

    return 0;
}
