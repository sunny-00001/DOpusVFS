#include <windows.h>
#include <iostream>
#include "../src/StockClient.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout.imbue(std::locale(""));

    StockClient::Init();

    std::wcout << L"=== 市场列表测试 ===" << std::endl;

    std::wcout << L"正在获取沪市股票列表..." << std::endl;
    auto stocks = StockClient::ListMarketStocks(STOCK_MARKET_SH);
    std::wcout << L"沪市股票数量: " << stocks.size() << std::endl;

    if (!stocks.empty()) {
        int show = (stocks.size() > 10) ? 10 : (int)stocks.size();
        for (int i = 0; i < show; i++) {
            std::wcout << L"  " << stocks[i].code << L" " << stocks[i].name
                       << L" 价格=" << stocks[i].currentPrice
                       << L" 涨跌=" << stocks[i].changePercent << L"%"
                       << L" 成交量=" << stocks[i].volume
                       << std::endl;
        }
    } else {
        std::wcout << L"获取失败或列表为空" << std::endl;
    }

    std::wcout << L"\n正在获取深市股票列表..." << std::endl;
    auto szStocks = StockClient::ListMarketStocks(STOCK_MARKET_SZ);
    std::wcout << L"深市股票数量: " << szStocks.size() << std::endl;

    if (!szStocks.empty()) {
        int show = (szStocks.size() > 5) ? 5 : (int)szStocks.size();
        for (int i = 0; i < show; i++) {
            std::wcout << L"  " << szStocks[i].code << L" " << szStocks[i].name
                       << L" 价格=" << szStocks[i].currentPrice << std::endl;
        }
    }

    StockClient::Cleanup();
    std::wcout << L"\n测试完成" << std::endl;
    return 0;
}
