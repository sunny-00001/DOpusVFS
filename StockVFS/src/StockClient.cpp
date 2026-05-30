#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <fstream>

#include "StockClient.h"

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Shell32.lib")

HINTERNET StockClient::s_hSession = NULL;
StockConfig StockClient::s_config = {
    STOCK_SOURCE_SINA, L"", L"",
    60, 0, true, false, 50, L".st",
    STOCK_PROXY_NONE, L"", 0
};
std::mutex StockClient::s_cacheMutex;
std::mutex StockClient::s_configMutex;
std::mutex StockClient::s_watchlistMutex;
std::unordered_map<std::wstring, StockClient::QuoteCacheEntry> StockClient::s_quoteCache;
std::unordered_map<std::wstring, StockClient::SearchCacheEntry> StockClient::s_searchCache;
std::unordered_map<int, StockClient::MarketListCacheEntry> StockClient::s_marketListCache;
StockClient::IndexListCacheEntry StockClient::s_indexListCache;
std::unordered_map<int, StockClient::SectorListCacheEntry> StockClient::s_sectorListCache;
std::unordered_map<std::wstring, StockClient::SectorStocksCacheEntry> StockClient::s_sectorStocksCache;

std::function<void()> StockClient::s_refreshCallback;
std::mutex StockClient::s_diskCacheMutex;
std::atomic<bool> StockClient::s_marketLoading[5] = {};
std::atomic<bool> StockClient::s_quoteLoading(false);
std::unordered_map<std::wstring, std::atomic<bool>> StockClient::s_searchLoading;
std::mutex StockClient::s_searchLoadingMutex;

void StockClient::Init() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    LoadConfig();
    EnsureSession();
    CleanupExpiredCache();
    LoadDiskCache();
}

void StockClient::Cleanup() {
    InvalidateCache();
    if (s_hSession) { WinHttpCloseHandle(s_hSession); s_hSession = NULL; }
    WSACleanup();
}

bool StockClient::EnsureSession() {
    if (s_hSession) return true;

    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    std::wstring proxyName;
    if (s_config.proxyType == STOCK_PROXY_HTTP || s_config.proxyType == STOCK_PROXY_SOCKS5) {
        accessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        std::wstring scheme = (s_config.proxyType == STOCK_PROXY_SOCKS5) ? L"socks=" : L"http://";
        proxyName = scheme + s_config.proxyHost + L":" + std::to_wstring(s_config.proxyPort);
    }

    s_hSession = WinHttpOpen(L"StockVFS/1.0", accessType,
                             proxyName.empty() ? WINHTTP_NO_PROXY_NAME : proxyName.c_str(),
                             WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s_hSession) return false;

    DWORD timeout = 10000;
    WinHttpSetOption(s_hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(s_hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(s_hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    return true;
}

std::wstring StockClient::GetConfigFilePath() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        StringCchCatW(path, MAX_PATH, L"\\StockVFS");
        CreateDirectoryW(path, NULL);
        StringCchCatW(path, MAX_PATH, L"\\config.json");
        return path;
    }
    return L"";
}

std::wstring StockClient::GetWatchlistFilePath() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        StringCchCatW(path, MAX_PATH, L"\\StockVFS");
        CreateDirectoryW(path, NULL);
        StringCchCatW(path, MAX_PATH, L"\\watchlist.json");
        return path;
    }
    return L"";
}

std::string StockClient::WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

std::wstring StockClient::Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

std::wstring StockClient::GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

std::string StockClient::WideToGbk(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(936, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string gbk(len, '\0');
    WideCharToMultiByte(936, 0, wide.c_str(), (int)wide.length(), &gbk[0], len, NULL, NULL);
    return gbk;
}

std::wstring StockClient::SymbolToSinaCode(const std::wstring& symbol) {
    if (symbol.length() < 4) return symbol;
    size_t dotPos = symbol.find(L'.');
    if (dotPos == std::wstring::npos) return symbol;
    std::wstring code = symbol.substr(0, dotPos);
    std::wstring suffix = symbol.substr(dotPos + 1);
    if (suffix == L"SH") return L"sh" + code;
    if (suffix == L"SZ") return L"sz" + code;
    if (suffix == L"HK") return L"hk" + code;
    if (suffix == L"US") return L"gb_" + code;
    if (suffix == L"BJ") return L"bj" + code;
    return code;
}

std::wstring StockClient::SinaCodeToSymbol(const std::wstring& sinaCode) {
    if (sinaCode.length() < 2) return sinaCode;
    if (sinaCode.substr(0, 2) == L"sh") return sinaCode.substr(2) + L".SH";
    if (sinaCode.substr(0, 2) == L"sz") return sinaCode.substr(2) + L".SZ";
    if (sinaCode.substr(0, 2) == L"hk") return sinaCode.substr(2) + L".HK";
    if (sinaCode.substr(0, 3) == L"gb_") return sinaCode.substr(3) + L".US";
    if (sinaCode.substr(0, 2) == L"bj") return sinaCode.substr(2) + L".BJ";
    return sinaCode;
}

StockMarket StockClient::GetMarketFromSymbol(const std::wstring& symbol) {
    size_t dotPos = symbol.find(L'.');
    if (dotPos == std::wstring::npos) return STOCK_MARKET_SH;
    std::wstring suffix = symbol.substr(dotPos + 1);
    if (suffix == L"SH") return STOCK_MARKET_SH;
    if (suffix == L"SZ") return STOCK_MARKET_SZ;
    if (suffix == L"HK") return STOCK_MARKET_HK;
    if (suffix == L"US") return STOCK_MARKET_US;
    if (suffix == L"BJ") return STOCK_MARKET_BJ;
    return STOCK_MARKET_SH;
}

std::wstring StockClient::GetMarketName(StockMarket market) {
    switch (market) {
    case STOCK_MARKET_SH: return L"\u6caa\u5e02";
    case STOCK_MARKET_SZ: return L"\u6df1\u5e02";
    case STOCK_MARKET_HK: return L"\u6e2f\u80a1";
    case STOCK_MARKET_US: return L"\u7f8e\u80a1";
    case STOCK_MARKET_BJ: return L"\u5317\u4ea4";
    default: return L"\u672a\u77e5";
    }
}

std::wstring StockClient::FormatPrice(double price, StockMarket market) {
    WCHAR buf[32];
    if (price == 0.0) {
        StringCchPrintfW(buf, 32, L"0.00");
    } else if (market == STOCK_MARKET_HK) {
        StringCchPrintfW(buf, 32, L"%.3f", price);
    } else if (market == STOCK_MARKET_US) {
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

std::wstring StockClient::FormatVolume(int64_t vol) {
    WCHAR buf[32];
    if (vol >= 100000000) {
        StringCchPrintfW(buf, 32, L"%.2f\u4ebf", vol / 100000000.0);
    } else if (vol >= 10000) {
        StringCchPrintfW(buf, 32, L"%.2f\u4e07", vol / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%lld", vol);
    }
    return buf;
}

std::wstring StockClient::FormatAmount(double amt) {
    WCHAR buf[32];
    if (amt >= 100000000.0) {
        StringCchPrintfW(buf, 32, L"%.2f\u4ebf", amt / 100000000.0);
    } else if (amt >= 10000.0) {
        StringCchPrintfW(buf, 32, L"%.2f\u4e07", amt / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%.2f", amt);
    }
    return buf;
}

std::wstring StockClient::FormatMarketCap(double cap) {
    WCHAR buf[32];
    if (cap >= 10000.0) {
        StringCchPrintfW(buf, 32, L"%.2f\u4e07\u4ebf", cap / 10000.0);
    } else {
        StringCchPrintfW(buf, 32, L"%.2f\u4ebf", cap);
    }
    return buf;
}

std::string StockClient::SendHttpGet(const std::wstring& host, const std::wstring& path,
                                      int port, bool https) {
    if (!EnsureSession()) return "";

    std::wstring connectHost = host;
    std::wstring hostHeader;

    {
        std::string hostA;
        hostA.reserve(host.size());
        for (auto c : host) hostA.push_back((char)c);

        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* result = nullptr;
        if (getaddrinfo(hostA.c_str(), nullptr, &hints, &result) == 0 && result) {
            char ipStr[INET_ADDRSTRLEN] = {};
            struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
            std::wstring ipW(ipStr, ipStr + strlen(ipStr));
            connectHost = ipW;
            hostHeader = host;
            freeaddrinfo(result);
        }
    }

    HINTERNET hConnect = WinHttpConnect(s_hSession, connectHost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) return "";

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return "";
    }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    if (!hostHeader.empty()) {
        headers += L"Host: " + hostHeader + L"\r\n";
    }
    if (host == L"hq.sinajs.cn" || host == L"suggest3.sinajs.cn"
        || host == L"vip.stock.finance.sina.com.cn") {
        headers += L"Referer: https://finance.sina.com.cn\r\n";
    }
    if (host.find(L"eastmoney.com") != std::wstring::npos) {
        headers += L"Referer: https://quote.eastmoney.com\r\n";
    }

    BOOL bResult = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &sz, NULL);

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    std::string response;
    DWORD dwAvailable = 0;
    do {
        WinHttpQueryDataAvailable(hRequest, &dwAvailable);
        if (dwAvailable > 0) {
            std::vector<char> buf(dwAvailable + 1);
            DWORD dwRead = 0;
            WinHttpReadData(hRequest, buf.data(), dwAvailable, &dwRead);
            if (dwRead > 0) response.append(buf.data(), dwRead);
        }
    } while (dwAvailable > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

StockQuote StockClient::ParseSinaQuote(const std::string& raw, const std::wstring& symbol) {
    StockQuote quote;
    quote.symbol = symbol;
    quote.market = GetMarketFromSymbol(symbol);

    size_t dotPos = symbol.find(L'.');
    if (dotPos != std::wstring::npos) {
        quote.code = symbol.substr(0, dotPos);
    } else {
        quote.code = symbol;
    }

    size_t eqPos = raw.find('=');
    if (eqPos == std::string::npos) return quote;

    std::string valueStr = raw.substr(eqPos + 1);
    if (valueStr.size() < 2 || valueStr[0] != '"') return quote;

    valueStr = valueStr.substr(1);
    if (!valueStr.empty() && valueStr.back() == '"') valueStr.pop_back();
    if (!valueStr.empty() && valueStr.back() == ';') valueStr.pop_back();
    if (valueStr.empty()) return quote;

    std::vector<std::string> fields;
    std::istringstream iss(valueStr);
    std::string field;
    while (std::getline(iss, field, ',')) {
        fields.push_back(field);
    }

    StockMarket market = GetMarketFromSymbol(symbol);

    if (market == STOCK_MARKET_SH || market == STOCK_MARKET_SZ || market == STOCK_MARKET_BJ) {
        if (fields.size() < 32) return quote;
        quote.name = GbkToWide(fields[0]);
        quote.openPrice = atof(fields[1].c_str());
        quote.previousClose = atof(fields[2].c_str());
        quote.currentPrice = atof(fields[3].c_str());
        quote.highPrice = atof(fields[4].c_str());
        quote.lowPrice = atof(fields[5].c_str());
        quote.volume = _atoi64(fields[8].c_str());
        quote.amount = atof(fields[9].c_str());
        if (quote.previousClose > 0) {
            quote.changeAmount = quote.currentPrice - quote.previousClose;
            quote.changePercent = (quote.changeAmount / quote.previousClose) * 100.0;
        }
    } else if (market == STOCK_MARKET_HK) {
        if (fields.size() < 13) return quote;
        quote.name = GbkToWide(fields[1]);
        quote.openPrice = atof(fields[2].c_str());
        quote.previousClose = atof(fields[3].c_str());
        quote.currentPrice = atof(fields[6].c_str());
        quote.highPrice = atof(fields[4].c_str());
        quote.lowPrice = atof(fields[5].c_str());
        quote.volume = _atoi64(fields[12].c_str());
        quote.amount = atof(fields[11].c_str());
        if (quote.previousClose > 0) {
            quote.changeAmount = quote.currentPrice - quote.previousClose;
            quote.changePercent = (quote.changeAmount / quote.previousClose) * 100.0;
        }
    } else if (market == STOCK_MARKET_US) {
        if (fields.size() < 11) return quote;
        quote.name = GbkToWide(fields[0]);
        quote.currentPrice = atof(fields[1].c_str());
        quote.changePercent = atof(fields[2].c_str());
        quote.changeAmount = atof(fields[4].c_str());
        quote.previousClose = quote.currentPrice - quote.changeAmount;
        quote.openPrice = atof(fields[5].c_str());
        quote.highPrice = atof(fields[6].c_str());
        quote.lowPrice = atof(fields[7].c_str());
        quote.volume = _atoi64(fields[10].c_str());
    }

    GetSystemTimeAsFileTime(&quote.updateTime);
    return quote;
}

StockQuote StockClient::GetQuote(const std::wstring& symbol) {
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_quoteCache.find(symbol);
        if (it != s_quoteCache.end()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
            if ((now - it->second.timestamp) < cacheMs) {
                return it->second.quote;
            }
        }
    }

    StockQuote cachedQuote;
    bool hasCachedQuote = false;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_quoteCache.find(symbol);
        if (it != s_quoteCache.end()) {
            cachedQuote = it->second.quote;
            hasCachedQuote = true;
        }
    }

    if (!hasCachedQuote) {
        cachedQuote = LoadQuoteFromDisk(symbol);
        if (cachedQuote.currentPrice > 0 || !cachedQuote.name.empty()) {
            hasCachedQuote = true;
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            QuoteCacheEntry entry;
            entry.quote = cachedQuote;
            entry.timestamp = 0;
            s_quoteCache[symbol] = entry;
        }
    }

    if (hasCachedQuote) {
        AsyncRefreshQuote(symbol);
        return cachedQuote;
    }

    StockQuote quote;

    if (s_config.dataSource == STOCK_SOURCE_SINA) {
        std::wstring sinaCode = SymbolToSinaCode(symbol);
        std::wstring path = L"/list=" + sinaCode;
        std::string response = SendHttpGet(L"hq.sinajs.cn", path);
        if (!response.empty()) {
            quote = ParseSinaQuote(response, symbol);
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        QuoteCacheEntry entry;
        entry.quote = quote;
        entry.timestamp = GetTickCount64();
        s_quoteCache[symbol] = entry;
    }

    SaveQuoteToDisk(symbol, quote);

    return quote;
}

std::vector<StockQuote> StockClient::GetQuotes(const std::vector<std::wstring>& symbols) {
    if (symbols.empty()) return {};

    std::vector<StockQuote> results;
    std::vector<std::wstring> uncached;

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        ULONGLONG now = GetTickCount64();
        ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
        for (const auto& sym : symbols) {
            auto it = s_quoteCache.find(sym);
            if (it != s_quoteCache.end() && (now - it->second.timestamp) < cacheMs) {
                results.push_back(it->second.quote);
            } else {
                uncached.push_back(sym);
            }
        }
    }

    if (uncached.empty()) return results;

    if (s_config.dataSource == STOCK_SOURCE_SINA) {
        std::wstring codes;
        for (const auto& sym : uncached) {
            if (!codes.empty()) codes += L",";
            codes += SymbolToSinaCode(sym);
        }
        std::wstring path = L"/list=" + codes;
        std::string response = SendHttpGet(L"hq.sinajs.cn", path);
        if (!response.empty()) {
            std::istringstream iss(response);
            std::string line;
            size_t idx = 0;
            while (std::getline(iss, line) && idx < uncached.size()) {
                if (line.find('=') != std::string::npos) {
                    StockQuote q = ParseSinaQuote(line, uncached[idx]);
                    results.push_back(q);
                    {
                        std::lock_guard<std::mutex> lock(s_cacheMutex);
                        QuoteCacheEntry entry;
                        entry.quote = q;
                        entry.timestamp = GetTickCount64();
                        s_quoteCache[uncached[idx]] = entry;
                    }
                    idx++;
                }
            }
        }
    }

    return results;
}

static std::wstring UrlEncode(const std::wstring& input) {
    std::string utf8 = StockClient::WideToUtf8(input);
    std::wstring encoded;
    for (unsigned char c : utf8) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += (wchar_t)c;
        } else {
            WCHAR buf[4];
            StringCchPrintfW(buf, 4, L"%%%02X", c);
            encoded += buf;
        }
    }
    return encoded;
}

StockSearchResult StockClient::SearchSinaStock(const std::wstring& keyword) {
    StockSearchResult result;
    std::wstring encoded = UrlEncode(keyword);
    std::wstring path = L"/suggest/type=&key=" + encoded + L"&name=suggestdata";
    std::string response = SendHttpGet(L"suggest3.sinajs.cn", path);
    if (response.empty()) return result;

    size_t eqPos = response.find('=');
    if (eqPos == std::string::npos) return result;

    std::string valueStr = response.substr(eqPos + 1);
    if (valueStr.size() >= 2 && valueStr[0] == '"') valueStr = valueStr.substr(1);
    if (!valueStr.empty() && valueStr.back() == '"') valueStr.pop_back();
    if (!valueStr.empty() && valueStr.back() == ';') valueStr.pop_back();
    if (valueStr.empty()) return result;

    std::istringstream iss(valueStr);
    std::string item;
    while (std::getline(iss, item, ';')) {
        std::vector<std::string> parts;
        std::istringstream itemIss(item);
        std::string part;
        while (std::getline(itemIss, part, ',')) {
            parts.push_back(part);
        }

        if (parts.size() < 6) continue;

        StockInfo info;
        std::string typeCode = parts[1];

        if (typeCode == "11" || typeCode == "12" || typeCode == "13" || typeCode == "14"
            || typeCode == "21" || typeCode == "22" || typeCode == "31" || typeCode == "32"
            || typeCode == "33" || typeCode == "41" || typeCode == "42" || typeCode == "103") {
            info.code = Utf8ToWide(parts[2]);
            info.name = GbkToWide(parts[4]);

            if (typeCode == "11" || typeCode == "21") {
                info.symbol = info.code + L".SH";
                info.market = STOCK_MARKET_SH;
            } else if (typeCode == "12" || typeCode == "22") {
                info.symbol = info.code + L".SZ";
                info.market = STOCK_MARKET_SZ;
            } else if (typeCode == "13" || typeCode == "31" || typeCode == "33") {
                info.symbol = info.code + L".HK";
                info.market = STOCK_MARKET_HK;
            } else if (typeCode == "14" || typeCode == "41" || typeCode == "42") {
                info.symbol = info.code + L".US";
                info.market = STOCK_MARKET_US;
            } else if (typeCode == "32") {
                info.symbol = info.code + L".BJ";
                info.market = STOCK_MARKET_BJ;
            } else if (typeCode == "103") {
                info.symbol = info.code + L".HK";
                info.market = STOCK_MARKET_HK;
            }

            result.stocks.push_back(info);
        }
    }

    result.totalCount = (int)result.stocks.size();
    return result;
}

StockSearchResult StockClient::SearchStock(const std::wstring& keyword) {
    if (keyword.empty()) return {};

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_searchCache.find(keyword);
        if (it != s_searchCache.end()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = 60000;
            if ((now - it->second.timestamp) < cacheMs) {
                return it->second.result;
            }
        }
    }

    StockSearchResult cachedResult;
    bool hasCachedResult = false;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_searchCache.find(keyword);
        if (it != s_searchCache.end()) {
            cachedResult = it->second.result;
            hasCachedResult = true;
        }
    }

    if (!hasCachedResult) {
        cachedResult = LoadSearchFromDisk(keyword);
        if (!cachedResult.stocks.empty()) {
            hasCachedResult = true;
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            SearchCacheEntry entry;
            entry.result = cachedResult;
            entry.timestamp = 0;
            s_searchCache[keyword] = entry;
        }
    }

    if (hasCachedResult) {
        AsyncRefreshSearch(keyword);
        return cachedResult;
    }

    StockSearchResult result;

    switch (s_config.dataSource) {
    case STOCK_SOURCE_SINA:
        result = SearchSinaStock(keyword);
        break;
    default:
        result = SearchSinaStock(keyword);
        break;
    }

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        SearchCacheEntry entry;
        entry.result = result;
        entry.timestamp = GetTickCount64();
        s_searchCache[keyword] = entry;
    }

    SaveSearchToDisk(keyword, result);

    return result;
}

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

std::vector<StockQuote> StockClient::ListSinaMarketStocks(StockMarket market) {
    std::vector<StockQuote> result;

    std::wstring fsParam;
    switch (market) {
    case STOCK_MARKET_SH:
        fsParam = L"m:1+t:2,m:1+t:23";
        break;
    case STOCK_MARKET_SZ:
        fsParam = L"m:0+t:6,m:0+t:80";
        break;
    case STOCK_MARKET_BJ:
        fsParam = L"m:0+t:81+s:2048";
        break;
    default:
        return result;
    }

    const int PAGE_SIZE = 500;
    int page = 1;

    while (true) {
        std::wstring path = L"/api/qt/clist/get?pn=" + std::to_wstring(page)
                            + L"&pz=" + std::to_wstring(PAGE_SIZE)
                            + L"&po=1&np=1&fltt=2&invt=2&fid=f3&fs="
                            + fsParam
                            + L"&fields=f2,f3,f4,f5,f6,f7,f8,f9,f10,f12,f13,f14,f15,f16,f17,f18,f20,f21,f23,f24,f25,f22,f11,f62,f128,f136,f115,f152";

        std::string response = SendHttpGet(L"push2delay.eastmoney.com", path, 80, false);
        if (response.empty()) break;

        size_t diffPos = response.find("\"diff\"");
        if (diffPos == std::string::npos) break;

        size_t arrStart = response.find('[', diffPos);
        if (arrStart == std::string::npos) break;

        int itemCount = 0;
        size_t pos = arrStart + 1;
        while (pos < response.length()) {
            size_t objStart = response.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = response.find('}', objStart);
            if (objEnd == std::string::npos) break;

            itemCount++;
            std::string obj = response.substr(objStart, objEnd - objStart + 1);

            std::string code = ExtractJsonString(obj, "f12");
            int marketId = (int)ExtractJsonDouble(obj, "f13");
            std::string name = ExtractJsonString(obj, "f14");

            if (code.empty()) {
                pos = objEnd + 1;
                continue;
            }

            StockQuote quote;
            quote.code = Utf8ToWide(code);
            quote.name = DecodeJsonUnicode(name);

            if (marketId == 1) {
                quote.symbol = quote.code + L".SH";
                quote.market = STOCK_MARKET_SH;
            } else if (marketId == 0) {
                quote.symbol = quote.code + L".SZ";
                quote.market = STOCK_MARKET_SZ;
            } else if (marketId == 2) {
                quote.symbol = quote.code + L".BJ";
                quote.market = STOCK_MARKET_BJ;
            } else {
                quote.symbol = quote.code;
                quote.market = market;
            }

            quote.currentPrice = ExtractJsonDouble(obj, "f2");
            quote.changePercent = ExtractJsonDouble(obj, "f3");
            quote.changeAmount = ExtractJsonDouble(obj, "f4");
            quote.volume = (int64_t)ExtractJsonDouble(obj, "f5");
            quote.amount = ExtractJsonDouble(obj, "f6");
            quote.turnoverRate = ExtractJsonDouble(obj, "f8");
            quote.pe = ExtractJsonDouble(obj, "f9");
            quote.pb = ExtractJsonDouble(obj, "f23");
            quote.marketCap = ExtractJsonDouble(obj, "f20");
            quote.circulatingCap = ExtractJsonDouble(obj, "f21");
            quote.openPrice = ExtractJsonDouble(obj, "f17");
            quote.highPrice = ExtractJsonDouble(obj, "f15");
            quote.lowPrice = ExtractJsonDouble(obj, "f16");
            quote.previousClose = ExtractJsonDouble(obj, "f18");

            GetSystemTimeAsFileTime(&quote.updateTime);

            result.push_back(quote);
            pos = objEnd + 1;
        }

        if (itemCount == 0) break;
        if (itemCount < PAGE_SIZE) break;
        page++;
    }

    return result;
}

std::vector<StockQuote> StockClient::ListMarketStocks(StockMarket market) {
    int key = (int)market;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_marketListCache.find(key);
        if (it != s_marketListCache.end()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
            if ((now - it->second.timestamp) < cacheMs) {
                return it->second.quotes;
            }
        }
    }

    bool hasCachedData = false;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_marketListCache.find(key);
        if (it != s_marketListCache.end() && !it->second.quotes.empty()) {
            hasCachedData = true;
        }
    }

    if (!hasCachedData) {
        std::vector<StockQuote> diskQuotes = LoadMarketListFromDisk(market);
        if (!diskQuotes.empty()) {
            hasCachedData = true;
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            MarketListCacheEntry entry;
            entry.quotes = diskQuotes;
            entry.timestamp = 0;
            s_marketListCache[key] = entry;
        }
    }

    if (hasCachedData) {
        AsyncRefreshMarketList(market);
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        return s_marketListCache[key].quotes;
    }

    std::vector<StockQuote> quotes = ListSinaMarketStocks(market);

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        MarketListCacheEntry entry;
        entry.quotes = quotes;
        entry.timestamp = GetTickCount64();
        s_marketListCache[key] = entry;
    }

    SaveMarketListToDisk(market, quotes);

    return quotes;
}

std::vector<WatchlistEntry> StockClient::GetWatchlistInternal() {
    std::vector<WatchlistEntry> entries;
    std::wstring filePath = GetWatchlistFilePath();
    if (filePath.empty()) return entries;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return entries;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) return entries;

    std::wstring json = Utf8ToWide(content);
    size_t arrStart = json.find(L'[');
    if (arrStart == std::wstring::npos) return entries;
    arrStart++;

    size_t pos = arrStart;
    while (pos < json.length()) {
        size_t objStart = json.find(L'{', pos);
        if (objStart == std::wstring::npos) break;

        size_t objEnd = json.find(L'}', objStart);
        if (objEnd == std::wstring::npos) break;

        std::wstring obj = json.substr(objStart, objEnd - objStart + 1);

        WatchlistEntry entry;
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

        entry.symbol = extractStr(obj, L"symbol");
        entry.name = extractStr(obj, L"name");

        std::wstring timeStr = extractStr(obj, L"addedTime");
        if (!timeStr.empty()) {
            entry.addedTime = _wcstoi64(timeStr.c_str(), nullptr, 10);
        }

        if (!entry.symbol.empty()) {
            entries.push_back(entry);
        }

        pos = objEnd + 1;
    }

    return entries;
}

std::vector<WatchlistEntry> StockClient::GetWatchlist() {
    std::lock_guard<std::mutex> lock(s_watchlistMutex);
    return GetWatchlistInternal();
}

bool StockClient::AddToWatchlist(const std::wstring& symbol, const std::wstring& name) {
    std::lock_guard<std::mutex> lock(s_watchlistMutex);

    auto entries = GetWatchlistInternal();
    for (const auto& e : entries) {
        if (_wcsicmp(e.symbol.c_str(), symbol.c_str()) == 0) return true;
    }

    WatchlistEntry entry;
    entry.symbol = symbol;
    entry.name = name;
    entry.addedTime = GetTickCount64();
    entries.push_back(entry);

    std::wstring filePath = GetWatchlistFilePath();
    if (filePath.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n  \"stocks\": [\n";
    for (size_t i = 0; i < entries.size(); i++) {
        oss << L"    {\"symbol\": \"" << entries[i].symbol << L"\", ";
        oss << L"\"name\": \"" << entries[i].name << L"\", ";
        oss << L"\"addedTime\": " << entries[i].addedTime << L"}";
        if (i < entries.size() - 1) oss << L",";
        oss << L"\n";
    }
    oss << L"  ]\n}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();

    return true;
}

bool StockClient::RemoveFromWatchlist(const std::wstring& symbol) {
    std::lock_guard<std::mutex> lock(s_watchlistMutex);

    auto entries = GetWatchlistInternal();
    bool found = false;
    std::vector<WatchlistEntry> newEntries;
    for (const auto& e : entries) {
        if (_wcsicmp(e.symbol.c_str(), symbol.c_str()) == 0) {
            found = true;
        } else {
            newEntries.push_back(e);
        }
    }

    if (!found) return true;

    std::wstring filePath = GetWatchlistFilePath();
    if (filePath.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n  \"stocks\": [\n";
    for (size_t i = 0; i < newEntries.size(); i++) {
        oss << L"    {\"symbol\": \"" << newEntries[i].symbol << L"\", ";
        oss << L"\"name\": \"" << newEntries[i].name << L"\", ";
        oss << L"\"addedTime\": " << newEntries[i].addedTime << L"}";
        if (i < newEntries.size() - 1) oss << L",";
        oss << L"\n";
    }
    oss << L"  ]\n}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();

    return true;
}

bool StockClient::IsInWatchlist(const std::wstring& symbol) {
    auto entries = GetWatchlist();
    for (const auto& e : entries) {
        if (_wcsicmp(e.symbol.c_str(), symbol.c_str()) == 0) return true;
    }
    return false;
}

bool StockClient::LoadConfig() {
    std::lock_guard<std::mutex> lock(s_configMutex);

    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty()) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) return false;

    std::wstring json = Utf8ToWide(content);

    auto extractInt = [](const std::wstring& json, const std::wstring& key, int defVal) -> int {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = json.find(searchKey);
        if (kPos == std::wstring::npos) return defVal;
        size_t colonPos = json.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return defVal;
        size_t valStart = colonPos + 1;
        while (valStart < json.length() && (json[valStart] == L' ' || json[valStart] == L'\t')) valStart++;
        size_t valEnd = valStart;
        while (valEnd < json.length() && json[valEnd] >= L'0' && json[valEnd] <= L'9') valEnd++;
        if (valEnd == valStart) return defVal;
        return _wtoi(json.substr(valStart, valEnd - valStart).c_str());
    };

    auto extractStr = [](const std::wstring& json, const std::wstring& key) -> std::wstring {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = json.find(searchKey);
        if (kPos == std::wstring::npos) return L"";
        size_t colonPos = json.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return L"";
        size_t q1 = json.find(L'"', colonPos + 1);
        if (q1 == std::wstring::npos) return L"";
        size_t q2 = json.find(L'"', q1 + 1);
        if (q2 == std::wstring::npos) return L"";
        return json.substr(q1 + 1, q2 - q1 - 1);
    };

    auto extractBool = [](const std::wstring& json, const std::wstring& key, bool defVal) -> bool {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t kPos = json.find(searchKey);
        if (kPos == std::wstring::npos) return defVal;
        size_t colonPos = json.find(L':', kPos + searchKey.length());
        if (colonPos == std::wstring::npos) return defVal;
        size_t valStart = colonPos + 1;
        while (valStart < json.length() && (json[valStart] == L' ' || json[valStart] == L'\t')) valStart++;
        if (json.substr(valStart, 4) == L"true") return true;
        if (json.substr(valStart, 5) == L"false") return false;
        return defVal;
    };

    s_config.dataSource = (StockDataSource)extractInt(json, L"dataSource", 0);
    s_config.tushareToken = extractStr(json, L"tushareToken");
    s_config.alphaVantageKey = extractStr(json, L"alphaVantageKey");
    s_config.cacheTimeout = extractInt(json, L"cacheTimeout", 60);
    s_config.refreshInterval = extractInt(json, L"refreshInterval", 0);
    s_config.showChangeColor = extractBool(json, L"showChangeColor", true);
    s_config.showVolumeInName = extractBool(json, L"showVolumeInName", false);
    s_config.itemsPerPage = extractInt(json, L"itemsPerPage", 50);
    s_config.metaPrefix = extractStr(json, L"metaPrefix");
    if (s_config.metaPrefix.empty()) s_config.metaPrefix = L".st";
    s_config.proxyType = (StockProxyType)extractInt(json, L"proxyType", 0);
    s_config.proxyHost = extractStr(json, L"proxyHost");
    s_config.proxyPort = extractInt(json, L"proxyPort", 0);

    return true;
}

bool StockClient::SaveConfig() {
    std::lock_guard<std::mutex> lock(s_configMutex);

    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n";
    oss << L"  \"dataSource\": " << (int)s_config.dataSource << L",\n";
    oss << L"  \"tushareToken\": \"" << s_config.tushareToken << L"\",\n";
    oss << L"  \"alphaVantageKey\": \"" << s_config.alphaVantageKey << L"\",\n";
    oss << L"  \"cacheTimeout\": " << s_config.cacheTimeout << L",\n";
    oss << L"  \"refreshInterval\": " << s_config.refreshInterval << L",\n";
    oss << L"  \"showChangeColor\": " << (s_config.showChangeColor ? L"true" : L"false") << L",\n";
    oss << L"  \"showVolumeInName\": " << (s_config.showVolumeInName ? L"true" : L"false") << L",\n";
    oss << L"  \"itemsPerPage\": " << s_config.itemsPerPage << L",\n";
    oss << L"  \"metaPrefix\": \"" << s_config.metaPrefix << L"\",\n";
    oss << L"  \"proxyType\": " << (int)s_config.proxyType << L",\n";
    oss << L"  \"proxyHost\": \"" << s_config.proxyHost << L"\",\n";
    oss << L"  \"proxyPort\": " << s_config.proxyPort << L"\n";
    oss << L"}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();

    return true;
}

StockConfig& StockClient::GetConfig() {
    return s_config;
}

void StockClient::CleanupExpiredCache() {
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return;

    ULONGLONG expireMs = 24ULL * 60 * 60 * 1000;

    WIN32_FIND_DATAW findData;
    std::wstring searchPath = cacheDir + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        ULONGLONG fileAge = 0;
        ULONGLONG fileTimeNow = 0;
        GetSystemTimeAsFileTime((LPFILETIME)&fileTimeNow);
        ULONGLONG fileTime = ((ULONGLONG)findData.ftLastWriteTime.dwHighDateTime << 32) | findData.ftLastWriteTime.dwLowDateTime;
        if (fileTimeNow > fileTime) {
            fileAge = (fileTimeNow - fileTime) / 10000;
        }

        if (fileAge > expireMs) {
            std::wstring filePath = cacheDir + L"\\" + findData.cFileName;
            DeleteFileW(filePath.c_str());
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

void StockClient::InvalidateCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_quoteCache.clear();
    s_searchCache.clear();
    s_marketListCache.clear();
    s_indexListCache.indices.clear();
    s_indexListCache.timestamp = 0;
    s_sectorListCache.clear();
    s_sectorStocksCache.clear();
}

void StockClient::SetRefreshCallback(std::function<void()> callback) {
    s_refreshCallback = callback;
}

std::wstring StockClient::GetCacheDirPath() {
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
    oss << L"\"turnoverRate\":" << std::setprecision(2) << q.turnoverRate << L",";
    oss << L"\"pe\":" << std::setprecision(2) << q.pe << L",";
    oss << L"\"pb\":" << std::setprecision(2) << q.pb << L",";
    oss << L"\"marketCap\":" << std::setprecision(2) << q.marketCap << L",";
    oss << L"\"circulatingCap\":" << std::setprecision(2) << q.circulatingCap;
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
        std::wstring val = extractStr(obj, key);
        if (val.empty()) {
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
        }
        return _wtof(val.c_str());
    };
    auto extractInt = [&extractStr](const std::wstring& obj, const std::wstring& key) -> int {
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
    q.pe = extractDbl(obj, L"pe");
    q.pb = extractDbl(obj, L"pb");
    q.marketCap = extractDbl(obj, L"marketCap");
    q.circulatingCap = extractDbl(obj, L"circulatingCap");
    GetSystemTimeAsFileTime(&q.updateTime);
    return q;
}

bool StockClient::SaveMarketListToDisk(StockMarket market, const std::vector<StockQuote>& quotes) {
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
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();
    return true;
}

std::vector<StockQuote> StockClient::LoadMarketListFromDisk(StockMarket market) {
    std::vector<StockQuote> result;
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return result;

    std::wstring filePath = cacheDir + L"\\" + MarketToCacheKey(market) + L".json";
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
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

bool StockClient::SaveQuoteToDisk(const std::wstring& symbol, const StockQuote& quote) {
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n  \"timestamp\":" << GetTickCount64() << L",\n";
    oss << L"  \"quote\":{" << QuoteToJsonFields(quote) << L"}\n}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    std::wstring safeSymbol = symbol;
    for (auto& c : safeSymbol) {
        if (c == L'.' || c == L':' || c == L'/') c = L'_';
    }
    std::wstring filePath = cacheDir + L"\\quote_" + safeSymbol + L".json";
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();
    return true;
}

StockQuote StockClient::LoadQuoteFromDisk(const std::wstring& symbol) {
    StockQuote result;
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return result;

    std::wstring safeSymbol = symbol;
    for (auto& c : safeSymbol) {
        if (c == L'.' || c == L':' || c == L'/') c = L'_';
    }
    std::wstring filePath = cacheDir + L"\\quote_" + safeSymbol + L".json";
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return result;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (content.empty()) return result;

    std::wstring json = Utf8ToWide(content);
    size_t objStart = json.find(L'{', json.find(L"\"quote\""));
    if (objStart == std::wstring::npos) return result;
    size_t objEnd = json.find(L'}', objStart);
    if (objEnd == std::wstring::npos) return result;

    std::wstring obj = json.substr(objStart, objEnd - objStart + 1);
    result = ParseQuoteFromJsonObj(obj);
    return result;
}

bool StockClient::SaveSearchToDisk(const std::wstring& keyword, const StockSearchResult& searchResult) {
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return false;

    std::wostringstream oss;
    oss << L"{\n  \"timestamp\":" << GetTickCount64() << L",\n";
    oss << L"  \"keyword\":\"" << keyword << L"\",\n";
    oss << L"  \"totalCount\":" << searchResult.totalCount << L",\n";
    oss << L"  \"stocks\":[\n";
    for (size_t i = 0; i < searchResult.stocks.size(); i++) {
        const auto& s = searchResult.stocks[i];
        oss << L"    {\"code\":\"" << s.code << L"\",";
        oss << L"\"symbol\":\"" << s.symbol << L"\",";
        oss << L"\"name\":\"" << s.name << L"\",";
        oss << L"\"market\":" << (int)s.market << L"}";
        if (i < searchResult.stocks.size() - 1) oss << L",";
        oss << L"\n";
    }
    oss << L"  ]\n}\n";

    std::wstring json = oss.str();
    std::string utf8 = WideToUtf8(json);

    DWORD hash = 0;
    for (wchar_t c : keyword) hash = hash * 31 + (DWORD)c;
    std::wstring filePath = cacheDir + L"\\search_" + std::to_wstring(hash) + L".json";
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.length());
    file.close();
    return true;
}

StockSearchResult StockClient::LoadSearchFromDisk(const std::wstring& keyword) {
    StockSearchResult result;
    std::wstring cacheDir = GetCacheDirPath();
    if (cacheDir.empty()) return result;

    DWORD hash = 0;
    for (wchar_t c : keyword) hash = hash * 31 + (DWORD)c;
    std::wstring filePath = cacheDir + L"\\search_" + std::to_wstring(hash) + L".json";
    std::lock_guard<std::mutex> lock(s_diskCacheMutex);
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

        StockInfo info;
        info.code = extractStr(obj, L"code");
        info.symbol = extractStr(obj, L"symbol");
        info.name = extractStr(obj, L"name");
        info.market = (StockMarket)extractInt(obj, L"market");

        if (!info.code.empty()) {
            result.stocks.push_back(info);
        }
        pos = objEnd + 1;
    }

    result.totalCount = (int)result.stocks.size();
    return result;
}

void StockClient::LoadDiskCache() {
    for (int m = 0; m < 5; m++) {
        std::vector<StockQuote> quotes = LoadMarketListFromDisk((StockMarket)m);
        if (!quotes.empty()) {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            MarketListCacheEntry entry;
            entry.quotes = quotes;
            entry.timestamp = 0;
            s_marketListCache[m] = entry;
        }
    }
}

void StockClient::AsyncRefreshMarketList(StockMarket market) {
    int key = (int)market;
    if (key < 0 || key >= 5) return;
    bool expected = false;
    if (!s_marketLoading[key].compare_exchange_strong(expected, true)) return;

    std::thread([market]() {
        std::vector<StockQuote> quotes = ListSinaMarketStocks(market);
        if (!quotes.empty()) {
            int key = (int)market;
            {
                std::lock_guard<std::mutex> lock(s_cacheMutex);
                MarketListCacheEntry entry;
                entry.quotes = quotes;
                entry.timestamp = GetTickCount64();
                s_marketListCache[key] = entry;
            }
            SaveMarketListToDisk(market, quotes);
        }
        s_marketLoading[(int)market].store(false);
        if (s_refreshCallback) s_refreshCallback();
    }).detach();
}

std::vector<IndexQuote> StockClient::FetchSinaIndices() {
    std::vector<IndexQuote> result;

    static const struct { const wchar_t* code; const wchar_t* name; } majorIndices[] = {
        { L"sh000001", L"\u4e0a\u8bc1\u6307\u6570" },
        { L"sz399001", L"\u6df1\u8bc1\u6210\u6307" },
        { L"sz399006", L"\u521b\u4e1a\u677f\u6307" },
        { L"sh000300", L"\u6caa\u6df1300" },
        { L"sh000016", L"\u4e0a\u8bc150" },
        { L"sh000905", L"\u4e2d\u8bc1500" },
        { L"sz399005", L"\u4e2d\u5c0f\u677f\u6307" },
        { L"sh000688", L"\u79d1\u521b50" },
        { L"sh000852", L"\u4e2d\u8bc11000" },
        { L"sz399673", L"\u521b\u4e1a\u677f50" },
        { L"sh000903", L"\u4e2d\u8bc1100" },
        { L"sz399303", L"\u56fd\u8bc12000" },
        { L"sh000002", L"\u4e0a\u8bc1A\u6307" },
        { L"sh000003", L"B\u80a1\u6307\u6570" },
        { L"sh000010", L"\u4e0a\u8bc1180" },
        { L"sh000011", L"\u57fa\u91d1\u6307\u6570" },
        { L"sh000012", L"\u56fd\u503a\u6307\u6570" },
        { L"sh000015", L"\u7ea2\u7b79\u6307\u6570" },
        { L"sh000017", L"\u65b0\u7efc\u6307" },
        { L"sz399002", L"\u6df1\u8bc1A\u6307" },
        { L"sz399003", L"\u6df1\u8bc1B\u6307" },
        { L"sz399004", L"\u6df1\u8bc1100" },
        { L"sz399100", L"\u65b0\u6307\u6570" },
        { L"sz399106", L"\u6df1\u8bc1\u7efc\u6307" },
        { L"sz399333", L"\u4e2d\u5c0f\u677f\u6210\u6307" },
        { L"sz399602", L"\u521b\u4e1a\u4e2d\u5c0f\u6307" },
        { L"sz399610", L"\u4e07\u5f97\u5168\u6307" },
        { L"sh000019", L"\u6cbf\u6d77\u53d1\u5c55" },
        { L"sh000020", L"\u4e0a\u8bc1\u6d88\u8d39" },
        { L"sh000021", L"\u4e0a\u8bc1\u7535\u4fe1" },
        { L"sh000022", L"\u4e0a\u8bc1\u533b\u836f" },
        { L"sh000025", L"\u4e0a\u8bc1\u57fa\u5efa" },
        { L"sh000026", L"\u4e0a\u8bc1\u516c\u7528" },
        { L"sh000027", L"\u4e0a\u8bc1\u80fd\u6e90" },
        { L"sh000028", L"\u4e0a\u8bc1\u91d1\u878d" },
        { L"sh000029", L"\u4e0a\u8bc1\u79d1\u6280" },
        { L"sh000030", L"\u4e0a\u8bc1\u6750\u6599" },
        { L"sh000031", L"\u4e0a\u8bc1\u5de5\u4e1a" },
        { L"sh000032", L"\u4e0a\u8bc1\u53ef\u8f6c\u503a" },
        { L"sh000033", L"\u4e0a\u8bc1\u623f\u5730\u4ea7" },
        { L"sh000034", L"\u4e0a\u8bc1\u4fe1\u606f" },
        { L"sh000035", L"\u4e0a\u8bc1\u5546\u8d38" },
        { L"sh000036", L"\u4e0a\u8bc1\u519b\u5de5" },
        { L"sh000037", L"\u4e0a\u8bc1\u822a\u7a7a" },
        { L"sh000038", L"\u4e0a\u8bc1\u7535\u529b" },
        { L"sh000039", L"\u4e0a\u8bc1\u8fd0\u8f93" },
        { L"sh000040", L"\u4e0a\u8bc1\u6587\u5a31" },
        { L"sh000041", L"\u4e0a\u8bc1\u7efc\u5408" },
        { L"sh000042", L"\u4e0a\u8bc1\u7535\u5b50" },
        { L"sh000043", L"\u4e0a\u8bc1\u94f6\u884c" },
        { L"sh000044", L"\u4e0a\u8bc1\u975e\u94f6" },
        { L"sh000045", L"\u4e0a\u8bc1\u7164\u70ad" },
        { L"sh000046", L"\u4e0a\u8bc1\u519c\u4e1a" },
        { L"sh000047", L"\u4e0a\u8bc1\u5efa\u7b51" },
        { L"sh000048", L"\u4e0a\u8bc1\u8f6f\u4ef6" },
        { L"sh000049", L"\u4e0a\u8bc1\u670d\u52a1" },
        { L"sh000050", L"\u4e0a\u8bc1\u7535\u5b50" },
        { L"sh000051", L"\u4e0a\u8bc1\u7535\u5668" },
        { L"sh000052", L"\u4e0a\u8bc1\u7eba\u7ec7" },
        { L"sh000053", L"\u4e0a\u8bc1\u8f7b\u5de5" },
        { L"sh000054", L"\u4e0a\u8bc1\u4f20\u5a92" },
        { L"sh000055", L"\u4e0a\u8bc1\u6c42\u6234" },
        { L"sh000056", L"\u4e0a\u8bc1\u7f8e\u5bb9" },
        { L"sh000057", L"\u4e0a\u8bc1\u751f\u7269" },
        { L"sh000058", L"\u4e0a\u8bc1\u6c11\u751f" },
        { L"sh000059", L"\u4e0a\u8bc1\u7f51\u7edc" },
        { L"sh000060", L"\u4e0a\u8bc1\u79d1\u521b" },
        { L"sh000061", L"\u4e0a\u8bc1\u519c\u6797" },
        { L"sh000062", L"\u4e0a\u8bc1\u623f\u5efa" },
        { L"sh000063", L"\u4e0a\u8bc1\u4ea4\u901a" },
        { L"sh000064", L"\u4e0a\u8bc1\u6c7d\u8f66" },
        { L"sh000065", L"\u4e0a\u8bc1\u673a\u68b0" },
        { L"sh000066", L"\u4e0a\u8bc1\u94a2\u94c1" },
        { L"sh000067", L"\u4e0a\u8bc1\u6709\u8272" },
        { L"sh000068", L"\u4e0a\u8bc1\u77f3\u6cb9" },
        { L"sh000069", L"\u4e0a\u8bc1\u5316\u5de5" },
        { L"sh000070", L"\u4e0a\u8bc1\u533b\u7597" },
        { L"sh000071", L"\u4e0a\u8bc1\u6559\u80b2" },
        { L"sh000072", L"\u4e0a\u8bc1\u73af\u4fdd" },
        { L"sh000073", L"\u4e0a\u8bc1\u534a\u5bfc\u4f53" },
        { L"sh000074", L"\u4e0a\u8bc1\u65b0\u80fd\u6e90" },
        { NULL, NULL }
    };

    std::wstring codes;
    for (int i = 0; majorIndices[i].code != NULL; i++) {
        if (!codes.empty()) codes += L",";
        codes += majorIndices[i].code;
    }

    std::wstring path = L"/list=" + codes;
    std::string response = SendHttpGet(L"hq.sinajs.cn", path);
    if (response.empty()) return result;

    std::istringstream iss(response);
    std::string line;
    int idx = 0;
    while (std::getline(iss, line) && majorIndices[idx].code != NULL) {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) { idx++; continue; }

        std::string valueStr = line.substr(eqPos + 1);
        if (valueStr.size() < 2 || valueStr[0] != '"') { idx++; continue; }
        valueStr = valueStr.substr(1);
        if (!valueStr.empty() && valueStr.back() == '"') valueStr.pop_back();
        if (!valueStr.empty() && valueStr.back() == ';') valueStr.pop_back();
        if (valueStr.empty()) { idx++; continue; }

        std::vector<std::string> fields;
        std::istringstream fiss(valueStr);
        std::string field;
        while (std::getline(fiss, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() < 9) { idx++; continue; }

        IndexQuote iq;
        iq.name = majorIndices[idx].name;
        std::wstring wcode = majorIndices[idx].code;
        if (wcode.length() > 2) {
            iq.code = wcode.substr(2);
        }
        iq.symbol = wcode;
        iq.currentPoint = atof(fields[1].c_str());
        iq.previousClose = atof(fields[2].c_str());
        iq.openPoint = atof(fields[3].c_str());
        iq.highPoint = atof(fields[4].c_str());
        iq.lowPoint = atof(fields[5].c_str());
        if (fields.size() > 8) iq.volume = _atoi64(fields[8].c_str());
        if (fields.size() > 9) iq.amount = atof(fields[9].c_str());
        if (iq.previousClose > 0) {
            iq.changeAmount = iq.currentPoint - iq.previousClose;
            iq.changePercent = (iq.changeAmount / iq.previousClose) * 100.0;
        }

        GetSystemTimeAsFileTime(&iq.updateTime);
        result.push_back(iq);
        idx++;
    }

    return result;
}

std::vector<IndexQuote> StockClient::ListIndices() {
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (!s_indexListCache.indices.empty()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
            if ((now - s_indexListCache.timestamp) < cacheMs) {
                return s_indexListCache.indices;
            }
        }
    }

    std::vector<IndexQuote> indices = FetchSinaIndices();

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_indexListCache.indices = indices;
        s_indexListCache.timestamp = GetTickCount64();
    }

    return indices;
}

std::vector<SectorInfo> StockClient::FetchSinaSectors(SectorType type) {
    std::vector<SectorInfo> result;

    std::wstring fsParam;
    switch (type) {
    case SECTOR_TYPE_INDUSTRY:
        fsParam = L"m:90+t:2+f:!50";
        break;
    case SECTOR_TYPE_CONCEPT:
        fsParam = L"m:90+t:3+f:!50";
        break;
    case SECTOR_TYPE_AREA:
        fsParam = L"m:90+t:1+f:!50";
        break;
    default:
        return result;
    }

    const int PAGE_SIZE = 500;
    int page = 1;

    while (true) {
        std::wstring path = L"/api/qt/clist/get?pn=" + std::to_wstring(page)
                            + L"&pz=" + std::to_wstring(PAGE_SIZE)
                            + L"&po=1&np=1&fltt=2&invt=2&fid=f3&fs="
                            + fsParam
                            + L"&fields=f2,f3,f4,f12,f14,f104,f105,f128,f136,f140,f141";

        std::string response = SendHttpGet(L"push2delay.eastmoney.com", path, 80, false);
        if (response.empty()) break;

        size_t diffPos = response.find("\"diff\"");
        if (diffPos == std::string::npos) break;

        size_t arrStart = response.find('[', diffPos);
        if (arrStart == std::string::npos) break;

        int itemCount = 0;
        size_t pos = arrStart + 1;
        while (pos < response.length()) {
            size_t objStart = response.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = response.find('}', objStart);
            if (objEnd == std::string::npos) break;

            itemCount++;
            std::string obj = response.substr(objStart, objEnd - objStart + 1);

            SectorInfo si;
            si.code = Utf8ToWide(ExtractJsonString(obj, "f12"));
            si.name = Utf8ToWide(ExtractJsonString(obj, "f14"));
            si.changePercent = ExtractJsonDouble(obj, "f3");
            si.changeAmount = ExtractJsonDouble(obj, "f4");
            si.avgPrice = ExtractJsonDouble(obj, "f2");
            si.leadStockCode = Utf8ToWide(ExtractJsonString(obj, "f128"));
            si.leadStockName = Utf8ToWide(ExtractJsonString(obj, "f136"));
            si.leadChgPct = ExtractJsonDouble(obj, "f140");
            si.leadPrice = ExtractJsonDouble(obj, "f141");
            si.stockCount = (int)ExtractJsonDouble(obj, "f104") + (int)ExtractJsonDouble(obj, "f105");

            if (!si.name.empty()) {
                result.push_back(si);
            }

            pos = objEnd + 1;
        }

        if (itemCount == 0) break;
        if (itemCount < PAGE_SIZE) break;
        page++;
    }

    return result;
}

std::vector<SectorInfo> StockClient::ListSectors(SectorType type) {
    int key = (int)type;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_sectorListCache.find(key);
        if (it != s_sectorListCache.end()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
            if ((now - it->second.timestamp) < cacheMs) {
                return it->second.sectors;
            }
        }
    }

    std::vector<SectorInfo> sectors = FetchSinaSectors(type);

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        SectorListCacheEntry entry;
        entry.sectors = sectors;
        entry.timestamp = GetTickCount64();
        s_sectorListCache[key] = entry;
    }

    return sectors;
}

std::vector<StockQuote> StockClient::FetchSectorStocks(const std::wstring& sectorCode) {
    std::vector<StockQuote> result;

    const int PAGE_SIZE = 500;
    int page = 1;

    while (true) {
        std::wstring path = L"/api/qt/clist/get?pn=" + std::to_wstring(page)
                            + L"&pz=" + std::to_wstring(PAGE_SIZE)
                            + L"&po=1&np=1&fltt=2&invt=2&fid=f3&fs=b:"
                            + sectorCode
                            + L"+f:!50&fields=f2,f3,f4,f5,f6,f7,f8,f9,f10,f12,f13,f14,f15,f16,f17,f18,f20,f21,f23,f24,f25,f22,f11,f62,f128,f136,f115,f152";

        std::string response = SendHttpGet(L"push2delay.eastmoney.com", path, 80, false);
        if (response.empty()) break;

        size_t diffPos = response.find("\"diff\"");
        if (diffPos == std::string::npos) break;

        size_t arrStart = response.find('[', diffPos);
        if (arrStart == std::string::npos) break;

        int itemCount = 0;
        size_t pos = arrStart + 1;
        while (pos < response.length()) {
            size_t objStart = response.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = response.find('}', objStart);
            if (objEnd == std::string::npos) break;

            itemCount++;
            std::string obj = response.substr(objStart, objEnd - objStart + 1);

            std::string code = ExtractJsonString(obj, "f12");
            int marketId = (int)ExtractJsonDouble(obj, "f13");
            std::string name = ExtractJsonString(obj, "f14");

            if (code.empty()) {
                pos = objEnd + 1;
                continue;
            }

            StockQuote quote;
            quote.code = Utf8ToWide(code);
            quote.name = DecodeJsonUnicode(name);

            if (marketId == 1) {
                quote.symbol = quote.code + L".SH";
                quote.market = STOCK_MARKET_SH;
            } else if (marketId == 0) {
                quote.symbol = quote.code + L".SZ";
                quote.market = STOCK_MARKET_SZ;
            } else if (marketId == 2) {
                quote.symbol = quote.code + L".BJ";
                quote.market = STOCK_MARKET_BJ;
            } else {
                quote.symbol = quote.code;
                quote.market = STOCK_MARKET_SH;
            }

            quote.currentPrice = ExtractJsonDouble(obj, "f2");
            quote.changePercent = ExtractJsonDouble(obj, "f3");
            quote.changeAmount = ExtractJsonDouble(obj, "f4");
            quote.volume = (int64_t)ExtractJsonDouble(obj, "f5");
            quote.amount = ExtractJsonDouble(obj, "f6");
            quote.turnoverRate = ExtractJsonDouble(obj, "f8");
            quote.pe = ExtractJsonDouble(obj, "f9");
            quote.pb = ExtractJsonDouble(obj, "f23");
            quote.marketCap = ExtractJsonDouble(obj, "f20");
            quote.circulatingCap = ExtractJsonDouble(obj, "f21");
            quote.openPrice = ExtractJsonDouble(obj, "f17");
            quote.highPrice = ExtractJsonDouble(obj, "f15");
            quote.lowPrice = ExtractJsonDouble(obj, "f16");
            quote.previousClose = ExtractJsonDouble(obj, "f18");

            GetSystemTimeAsFileTime(&quote.updateTime);

            result.push_back(quote);
            pos = objEnd + 1;
        }

        if (itemCount == 0) break;
        if (itemCount < PAGE_SIZE) break;
        page++;
    }

    return result;
}

std::vector<StockQuote> StockClient::ListSectorStocks(const std::wstring& sectorCode) {
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_sectorStocksCache.find(sectorCode);
        if (it != s_sectorStocksCache.end()) {
            ULONGLONG now = GetTickCount64();
            ULONGLONG cacheMs = (ULONGLONG)s_config.cacheTimeout * 1000;
            if ((now - it->second.timestamp) < cacheMs) {
                return it->second.stocks;
            }
        }
    }

    std::vector<StockQuote> stocks = FetchSectorStocks(sectorCode);

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        SectorStocksCacheEntry entry;
        entry.stocks = stocks;
        entry.timestamp = GetTickCount64();
        s_sectorStocksCache[sectorCode] = entry;
    }

    return stocks;
}

void StockClient::AsyncRefreshQuote(const std::wstring& symbol) {
    bool expected = false;
    if (!s_quoteLoading.compare_exchange_strong(expected, true)) return;

    std::thread([symbol]() {
        StockQuote quote;
        if (s_config.dataSource == STOCK_SOURCE_SINA) {
            std::wstring sinaCode = SymbolToSinaCode(symbol);
            std::wstring path = L"/list=" + sinaCode;
            std::string response = SendHttpGet(L"hq.sinajs.cn", path);
            if (!response.empty()) {
                quote = ParseSinaQuote(response, symbol);
            }
        }
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            QuoteCacheEntry entry;
            entry.quote = quote;
            entry.timestamp = GetTickCount64();
            s_quoteCache[symbol] = entry;
        }
        SaveQuoteToDisk(symbol, quote);
        s_quoteLoading.store(false);
        if (s_refreshCallback) s_refreshCallback();
    }).detach();
}

void StockClient::AsyncRefreshSearch(const std::wstring& keyword) {
    {
        std::lock_guard<std::mutex> lock(s_searchLoadingMutex);
        if (s_searchLoading.find(keyword) == s_searchLoading.end()) {
            s_searchLoading[keyword].store(false);
        }
        if (s_searchLoading[keyword].load()) return;
        s_searchLoading[keyword].store(true);
    }

    std::thread([keyword]() {
        StockSearchResult result = SearchSinaStock(keyword);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            SearchCacheEntry entry;
            entry.result = result;
            entry.timestamp = GetTickCount64();
            s_searchCache[keyword] = entry;
        }
        SaveSearchToDisk(keyword, result);
        {
            std::lock_guard<std::mutex> lock(s_searchLoadingMutex);
            s_searchLoading[keyword].store(false);
        }
        if (s_refreshCallback) s_refreshCallback();
    }).detach();
}
