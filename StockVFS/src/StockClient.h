#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>

enum StockMarket {
    STOCK_MARKET_SH = 0,
    STOCK_MARKET_SZ = 1,
    STOCK_MARKET_HK = 2,
    STOCK_MARKET_US = 3,
    STOCK_MARKET_BJ = 4
};

enum StockDataSource {
    STOCK_SOURCE_SINA = 0,
    STOCK_SOURCE_EASTMONEY = 1,
    STOCK_SOURCE_TUSHARE = 2,
    STOCK_SOURCE_ALPHAVANTAGE = 3
};

enum StockProxyType {
    STOCK_PROXY_NONE = 0,
    STOCK_PROXY_HTTP = 1,
    STOCK_PROXY_SOCKS5 = 2
};

enum SectorType {
    SECTOR_TYPE_INDUSTRY = 0,
    SECTOR_TYPE_CONCEPT = 1,
    SECTOR_TYPE_AREA = 2
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
    double pe;
    double pb;
    double marketCap;
    double circulatingCap;
    FILETIME updateTime;

    StockQuote() : market(STOCK_MARKET_SH), currentPrice(0), previousClose(0),
        openPrice(0), highPrice(0), lowPrice(0), volume(0), amount(0),
        changeAmount(0), changePercent(0), turnoverRate(0), pe(0), pb(0),
        marketCap(0), circulatingCap(0) {
        memset(&updateTime, 0, sizeof(FILETIME));
    }
};

struct IndexQuote {
    std::wstring code;
    std::wstring symbol;
    std::wstring name;
    double currentPoint;
    double previousClose;
    double openPoint;
    double highPoint;
    double lowPoint;
    int64_t volume;
    double amount;
    double changeAmount;
    double changePercent;
    FILETIME updateTime;

    IndexQuote() : currentPoint(0), previousClose(0), openPoint(0),
        highPoint(0), lowPoint(0), volume(0), amount(0),
        changeAmount(0), changePercent(0) {
        memset(&updateTime, 0, sizeof(FILETIME));
    }
};

struct SectorInfo {
    std::wstring code;
    std::wstring name;
    int stockCount;
    double changePercent;
    double changeAmount;
    double avgPrice;
    int64_t volume;
    double amount;
    std::wstring leadStockCode;
    std::wstring leadStockName;
    double leadChgPct;
    double leadPrice;
    double leadChgAmt;

    SectorInfo() : stockCount(0), changePercent(0), changeAmount(0),
        avgPrice(0), volume(0), amount(0), leadChgPct(0), leadPrice(0), leadChgAmt(0) {}
};

struct StockInfo {
    std::wstring code;
    std::wstring symbol;
    std::wstring name;
    StockMarket market;
    std::wstring industry;

    StockInfo() : market(STOCK_MARKET_SH) {}
};

struct StockSearchResult {
    std::vector<StockInfo> stocks;
    int totalCount;

    StockSearchResult() : totalCount(0) {}
};

struct WatchlistEntry {
    std::wstring symbol;
    std::wstring name;
    ULONGLONG addedTime;
};

struct StockConfig {
    StockDataSource dataSource;
    std::wstring tushareToken;
    std::wstring alphaVantageKey;
    int cacheTimeout;
    int refreshInterval;
    bool showChangeColor;
    bool showVolumeInName;
    int itemsPerPage;
    std::wstring metaPrefix;
    StockProxyType proxyType;
    std::wstring proxyHost;
    int proxyPort;
};

class StockClient {
public:
    static void Init();
    static void Cleanup();

    static bool LoadConfig();
    static bool SaveConfig();
    static StockConfig& GetConfig();

    static StockQuote GetQuote(const std::wstring& symbol);
    static std::vector<StockQuote> GetQuotes(const std::vector<std::wstring>& symbols);

    static StockSearchResult SearchStock(const std::wstring& keyword);

    static std::vector<WatchlistEntry> GetWatchlist();
    static bool AddToWatchlist(const std::wstring& symbol, const std::wstring& name);
    static bool RemoveFromWatchlist(const std::wstring& symbol);
    static bool IsInWatchlist(const std::wstring& symbol);

    static std::vector<StockQuote> ListMarketStocks(StockMarket market);

    static std::vector<IndexQuote> ListIndices();
    static std::vector<SectorInfo> ListSectors(SectorType type);
    static std::vector<StockQuote> ListSectorStocks(const std::wstring& sectorCode);

    static void InvalidateCache();
    static void CleanupExpiredCache();

    static void SetRefreshCallback(std::function<void()> callback);
    static void LoadDiskCache();
    static std::wstring GetCacheDirPath();

    static std::string WideToUtf8(const std::wstring& wide);
    static std::wstring Utf8ToWide(const std::string& utf8);
    static std::wstring GbkToWide(const std::string& gbk);
    static std::string WideToGbk(const std::wstring& wide);
    static std::wstring SymbolToSinaCode(const std::wstring& symbol);
    static std::wstring SinaCodeToSymbol(const std::wstring& sinaCode);
    static StockMarket GetMarketFromSymbol(const std::wstring& symbol);
    static std::wstring GetMarketName(StockMarket market);
    static std::wstring FormatPrice(double price, StockMarket market);
    static std::wstring FormatVolume(int64_t vol);
    static std::wstring FormatAmount(double amt);
    static std::wstring FormatMarketCap(double cap);

private:
    static HINTERNET s_hSession;
    static StockConfig s_config;
    static std::mutex s_cacheMutex;
    static std::mutex s_configMutex;
    static std::mutex s_watchlistMutex;

    struct QuoteCacheEntry {
        StockQuote quote;
        ULONGLONG timestamp;
    };
    struct SearchCacheEntry {
        StockSearchResult result;
        ULONGLONG timestamp;
    };
    static std::unordered_map<std::wstring, QuoteCacheEntry> s_quoteCache;
    static std::unordered_map<std::wstring, SearchCacheEntry> s_searchCache;

    struct MarketListCacheEntry {
        std::vector<StockQuote> quotes;
        ULONGLONG timestamp;
    };
    static std::unordered_map<int, MarketListCacheEntry> s_marketListCache;

    struct IndexListCacheEntry {
        std::vector<IndexQuote> indices;
        ULONGLONG timestamp;
    };
    static IndexListCacheEntry s_indexListCache;

    struct SectorListCacheEntry {
        std::vector<SectorInfo> sectors;
        ULONGLONG timestamp;
    };
    static std::unordered_map<int, SectorListCacheEntry> s_sectorListCache;

    struct SectorStocksCacheEntry {
        std::vector<StockQuote> stocks;
        ULONGLONG timestamp;
    };
    static std::unordered_map<std::wstring, SectorStocksCacheEntry> s_sectorStocksCache;

    static std::function<void()> s_refreshCallback;
    static std::mutex s_diskCacheMutex;
    static std::atomic<bool> s_marketLoading[5];
    static std::atomic<bool> s_quoteLoading;
    static std::unordered_map<std::wstring, std::atomic<bool>> s_searchLoading;
    static std::mutex s_searchLoadingMutex;

    static bool SaveMarketListToDisk(StockMarket market, const std::vector<StockQuote>& quotes);
    static std::vector<StockQuote> LoadMarketListFromDisk(StockMarket market);
    static bool SaveQuoteToDisk(const std::wstring& symbol, const StockQuote& quote);
    static StockQuote LoadQuoteFromDisk(const std::wstring& symbol);
    static bool SaveSearchToDisk(const std::wstring& keyword, const StockSearchResult& result);
    static StockSearchResult LoadSearchFromDisk(const std::wstring& keyword);

    static void AsyncRefreshMarketList(StockMarket market);
    static void AsyncRefreshQuote(const std::wstring& symbol);
    static void AsyncRefreshSearch(const std::wstring& keyword);

    static std::string SendHttpGet(const std::wstring& host, const std::wstring& path,
                                    int port = 443, bool https = true);
    static bool EnsureSession();
    static std::wstring GetConfigFilePath();
    static std::wstring GetWatchlistFilePath();

    static std::vector<WatchlistEntry> GetWatchlistInternal();

    static StockQuote ParseSinaQuote(const std::string& raw, const std::wstring& symbol);
    static StockSearchResult SearchSinaStock(const std::wstring& keyword);
    static std::vector<StockQuote> ListSinaMarketStocks(StockMarket market);
    static std::vector<IndexQuote> FetchSinaIndices();
    static std::vector<SectorInfo> FetchSinaSectors(SectorType type);
    static std::vector<StockQuote> FetchSectorStocks(const std::wstring& sectorCode);
};
