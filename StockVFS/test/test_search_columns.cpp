#include <windows.h>
#include <iostream>
#include <fstream>
#include "../src/StockClient.h"

int main() {
    StockClient::Init();

    std::ofstream out("d:\\vfs\\test_search_columns.txt");

    out << "=== Test: Search + GetQuotes ===" << std::endl;

    auto searchResult = StockClient::SearchStock(L"\u817e\u8baf");
    out << "Search results: " << searchResult.totalCount << std::endl;

    std::vector<std::wstring> symbols;
    for (int i = 0; i < (std::min)(searchResult.totalCount, 10); i++) {
        auto& s = searchResult.stocks[i];
        std::string sym = StockClient::WideToUtf8(s.symbol);
        std::string name = StockClient::WideToUtf8(s.name);
        std::string code = StockClient::WideToUtf8(s.code);
        out << "  Search[" << i << "] symbol=" << sym << " name=" << name
            << " code=" << code << " market=" << s.market << std::endl;
        symbols.push_back(s.symbol);
    }

    out << "\n--- GetQuotes batch ---" << std::endl;
    auto quotes = StockClient::GetQuotes(symbols);
    out << "Quotes returned: " << quotes.size() << std::endl;

    for (size_t i = 0; i < quotes.size(); i++) {
        auto& q = quotes[i];
        std::string sym = StockClient::WideToUtf8(q.symbol);
        std::string name = StockClient::WideToUtf8(q.name);
        std::string code = StockClient::WideToUtf8(q.code);
        out << "  Quote[" << i << "] symbol=" << sym << " name=" << name
            << " code=" << code << " market=" << q.market
            << " price=" << q.currentPrice << " chgAmt=" << q.changeAmount
            << " chgPct=" << q.changePercent << " open=" << q.openPrice
            << " high=" << q.highPrice << " low=" << q.lowPrice
            << " prevClose=" << q.previousClose << " vol=" << q.volume
            << std::endl;
    }

    out << "\n--- GetQuote individual ---" << std::endl;
    for (size_t i = 0; i < (std::min)(symbols.size(), (size_t)5); i++) {
        auto q = StockClient::GetQuote(symbols[i]);
        std::string sym = StockClient::WideToUtf8(q.symbol);
        std::string name = StockClient::WideToUtf8(q.name);
        out << "  Single[" << i << "] symbol=" << sym << " name=" << name
            << " price=" << q.currentPrice << " chgAmt=" << q.changeAmount
            << " chgPct=" << q.changePercent << " open=" << q.openPrice
            << " high=" << q.highPrice << " low=" << q.lowPrice
            << " prevClose=" << q.previousClose << " vol=" << q.volume
            << std::endl;
    }

    StockClient::Cleanup();
    out << "\n=== Test Complete ===" << std::endl;
    return 0;
}
