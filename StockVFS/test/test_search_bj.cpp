#include <windows.h>
#include <iostream>
#include <fstream>
#include "../src/StockClient.h"

int main() {
    StockClient::Init();

    std::ofstream out("d:\\vfs\\test_search_result.txt");
    out << "=== Search & BJ List Test ===" << std::endl;

    out << "\n--- Test 1: Search '腾讯' ---" << std::endl;
    auto searchResult = StockClient::SearchStock(L"腾讯");
    out << "Search results: " << searchResult.totalCount << std::endl;
    for (int i = 0; i < (std::min)(searchResult.totalCount, 10); i++) {
        std::string name = StockClient::WideToUtf8(searchResult.stocks[i].name);
        std::string symbol = StockClient::WideToUtf8(searchResult.stocks[i].symbol);
        std::string code = StockClient::WideToUtf8(searchResult.stocks[i].code);
        out << "  [" << i << "] " << name << " (" << symbol << ") code=" << code << " market=" << searchResult.stocks[i].market << std::endl;
    }

    out << "\n--- Test 2: Search 'apple' ---" << std::endl;
    auto searchResult2 = StockClient::SearchStock(L"apple");
    out << "Search results: " << searchResult2.totalCount << std::endl;
    for (int i = 0; i < (std::min)(searchResult2.totalCount, 5); i++) {
        std::string name = StockClient::WideToUtf8(searchResult2.stocks[i].name);
        std::string symbol = StockClient::WideToUtf8(searchResult2.stocks[i].symbol);
        std::string code = StockClient::WideToUtf8(searchResult2.stocks[i].code);
        out << "  [" << i << "] " << name << " (" << symbol << ") code=" << code << " market=" << searchResult2.stocks[i].market << std::endl;
    }

    out << "\n--- Test 3: BJ Market List ---" << std::endl;
    auto bjStocks = StockClient::ListMarketStocks(STOCK_MARKET_BJ);
    out << "BJ stock count: " << bjStocks.size() << std::endl;
    for (int i = 0; i < (std::min)((int)bjStocks.size(), 5); i++) {
        std::string name = StockClient::WideToUtf8(bjStocks[i].name);
        std::string code = StockClient::WideToUtf8(bjStocks[i].code);
        out << "  [" << i << "] " << code << " " << name << " price=" << bjStocks[i].currentPrice << std::endl;
    }

    StockClient::Cleanup();
    out << "\n=== Test Complete ===" << std::endl;
    return 0;
}
