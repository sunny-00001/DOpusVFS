#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <sstream>
#include <unordered_map>

#include "vfs_plugins.h"
#include "plugin_support.h"
#include "StockClient.h"
#include "resource.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Advapi32.lib")

static const GUID GUIDPlugin_Stock =
{ 0xA1B2C3D4, 0xE5F6, 0x4A7B, { 0x8C, 0x9D, 0x0E, 0x1F, 0x2A, 0x3B, 0x4C, 0x5D } };

#define STOCK_VFS_PREFIX L"stock://"
#define STOCK_VFS_PREFIX_LEN 8

static HMODULE g_hModule = NULL;
static std::atomic<bool> g_PluginUnloading(false);
static std::atomic<int> g_ActiveThreads(0);

static HWND g_hWndNotify = NULL;
static DWORD g_dwNotifyData = 0;

static bool IsStockVfsPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    return _wcsnicmp(pszPath, STOCK_VFS_PREFIX, STOCK_VFS_PREFIX_LEN) == 0;
}

static bool IsStockRootPath(LPCWSTR pszPath) {
    if (!pszPath) return false;
    if (_wcsicmp(pszPath, STOCK_VFS_PREFIX) == 0) return true;
    std::wstring s = pszPath;
    while (!s.empty() && s.back() == L'/') s.pop_back();
    return _wcsicmp(s.c_str(), L"stock:") == 0;
}

enum StockMetaType {
    STOCK_META_NONE = 0,
    STOCK_META_ROOT,
    STOCK_META_SIGNAL,
    STOCK_META_INFO
};

struct StockPathInfo {
    std::wstring symbol;
    std::wstring searchQuery;
    StockMarket market;
    std::wstring metaPrefix;
    StockMetaType metaType;
    std::wstring metaItem;
    SectorType sectorType;
    std::wstring sectorCode;
    bool isRoot;
    bool isWatchlist;
    bool isMarket;
    bool isSearch;
    bool isStockDir;
    bool isQuoteFile;
    bool isMetadata;
    bool isSignalFile;
    bool isVirtualFile;
    bool isReadOnly;
    bool isMarketList;
    bool isIndex;
    bool isIndexList;
    bool isSector;
    bool isSectorList;
    bool isSectorStocks;

    StockPathInfo() : market(STOCK_MARKET_SH), metaType(STOCK_META_NONE),
        sectorType(SECTOR_TYPE_INDUSTRY),
        isRoot(false), isWatchlist(false), isMarket(false), isSearch(false),
        isStockDir(false), isQuoteFile(false), isMetadata(false),
        isSignalFile(false), isVirtualFile(false), isReadOnly(false),
        isMarketList(false), isIndex(false), isIndexList(false),
        isSector(false), isSectorList(false), isSectorStocks(false) {}
};

struct StockContentInfo {
    std::vector<BYTE> data;
    size_t readPos;
    bool isWrite;
    std::wstring symbol;
    std::wstring fileType;
};

static std::vector<std::wstring> SplitPath(const std::wstring& path) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = path.find(L'/', start)) != std::wstring::npos) {
        if (pos > start) {
            parts.push_back(path.substr(start, pos - start));
        }
        start = pos + 1;
    }
    if (start < path.length()) {
        parts.push_back(path.substr(start));
    }
    return parts;
}

static StockPathInfo ParseStockPath(LPCWSTR pszPath) {
    StockPathInfo info;
    if (!pszPath || !IsStockVfsPath(pszPath)) return info;

    std::wstring rest = pszPath + STOCK_VFS_PREFIX_LEN;
    while (!rest.empty() && rest.front() == L'/') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == L'/') rest.pop_back();

    if (rest.empty()) {
        info.isRoot = true;
        return info;
    }

    StockConfig& cfg = StockClient::GetConfig();
    std::wstring metaPfx = cfg.metaPrefix.empty() ? L".st" : cfg.metaPrefix;

    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.isRoot = true;
        return info;
    }

    const std::wstring& seg0 = segs[0];

    if (seg0 == L"\u81ea\u9009\u80a1") {
        info.isWatchlist = true;
        if (segs.size() == 1) return info;

        info.symbol = segs[1];
        info.isStockDir = true;
        if (segs.size() == 2) return info;

        const std::wstring& seg2 = segs[2];

        if (seg2 == L"\u5b9e\u65f6\u884c\u60c5.txt") {
            info.isQuoteFile = true;
            info.isVirtualFile = true;
            info.isReadOnly = true;
            return info;
        }

        if (seg2 == metaPfx) {
            info.isMetadata = true;
            info.metaPrefix = metaPfx;
            if (segs.size() == 3) {
                info.metaType = STOCK_META_ROOT;
                return info;
            }

            const std::wstring& metaSeg = segs[3];
            if (metaSeg == L"star.signal") {
                info.metaType = STOCK_META_SIGNAL;
                info.metaItem = L"star";
                info.isSignalFile = true;
                info.isVirtualFile = true;
                return info;
            }
            if (metaSeg == L"info.txt") {
                info.metaType = STOCK_META_INFO;
                info.metaItem = L"info";
                info.isVirtualFile = true;
                info.isReadOnly = true;
                return info;
            }
        }

        return info;
    }

    if (seg0 == L"\u6caa\u5e02") {
        info.isMarket = true;
        info.market = STOCK_MARKET_SH;
        info.isMarketList = true;
    } else if (seg0 == L"\u6df1\u5e02") {
        info.isMarket = true;
        info.market = STOCK_MARKET_SZ;
        info.isMarketList = true;
    } else if (seg0 == L"\u5317\u4ea4") {
        info.isMarket = true;
        info.market = STOCK_MARKET_BJ;
        info.isMarketList = true;
    } else if (seg0 == L"\u6e2f\u80a1") {
        info.isMarket = true;
        info.market = STOCK_MARKET_HK;
        info.isMarketList = true;
    } else if (seg0 == L"\u7f8e\u80a1") {
        info.isMarket = true;
        info.market = STOCK_MARKET_US;
        info.isMarketList = true;
    }

    if (info.isMarket) {
        if (segs.size() >= 2) {
            const std::wstring& seg1 = segs[1];
            if (seg1.size() > 4 && seg1.substr(seg1.size() - 4) == L".txt") {
                info.isVirtualFile = true;
                info.isReadOnly = true;
                info.symbol = seg1;
                return info;
            }
            info.symbol = seg1;
            info.isStockDir = true;
            info.isMarketList = false;
        }
        return info;
    }

    if (seg0 == L"\u5e02\u573a") {
        info.isMarket = true;
        if (segs.size() == 1) return info;

        const std::wstring& seg1 = segs[1];
        if (seg1 == L"\u6caa\u5e02") {
            info.market = STOCK_MARKET_SH;
            info.isMarketList = true;
        } else if (seg1 == L"\u6df1\u5e02") {
            info.market = STOCK_MARKET_SZ;
            info.isMarketList = true;
        } else if (seg1 == L"\u5317\u4ea4") {
            info.market = STOCK_MARKET_BJ;
            info.isMarketList = true;
        } else if (seg1 == L"\u6e2f\u80a1") {
            info.market = STOCK_MARKET_HK;
            info.isMarketList = true;
        } else if (seg1 == L"\u7f8e\u80a1") {
            info.market = STOCK_MARKET_US;
            info.isMarketList = true;
        }

        if (segs.size() >= 3) {
            const std::wstring& seg2 = segs[2];
            if (seg2.size() > 4 && seg2.substr(seg2.size() - 4) == L".txt") {
                info.isVirtualFile = true;
                info.isReadOnly = true;
                info.symbol = seg2;
                return info;
            }
            info.symbol = seg2;
            info.isStockDir = true;
            info.isMarketList = false;
        }
        return info;
    }

    if (seg0 == L"\u6307\u6570") {
        info.isIndex = true;
        if (segs.size() == 1) {
            info.isIndexList = true;
            return info;
        }
        info.symbol = segs[1];
        info.isStockDir = true;
        if (segs.size() >= 3) {
            const std::wstring& seg2 = segs[2];
            if (seg2 == L"\u5b9e\u65f6\u884c\u60c5.txt") {
                info.isQuoteFile = true;
                info.isVirtualFile = true;
                info.isReadOnly = true;
                info.isStockDir = false;
            }
        }
        return info;
    }

    if (seg0 == L"\u677f\u5757") {
        info.isSector = true;
        if (segs.size() == 1) return info;

        const std::wstring& seg1 = segs[1];
        if (seg1 == L"\u884c\u4e1a") {
            info.sectorType = SECTOR_TYPE_INDUSTRY;
            info.isSectorList = true;
            if (segs.size() == 2) return info;
            info.sectorCode = segs[2];
            info.isSectorStocks = true;
            info.isSectorList = false;
        } else if (seg1 == L"\u6982\u5ff5") {
            info.sectorType = SECTOR_TYPE_CONCEPT;
            info.isSectorList = true;
            if (segs.size() == 2) return info;
            info.sectorCode = segs[2];
            info.isSectorStocks = true;
            info.isSectorList = false;
        } else if (seg1 == L"\u5730\u57df") {
            info.sectorType = SECTOR_TYPE_AREA;
            info.isSectorList = true;
            if (segs.size() == 2) return info;
            info.sectorCode = segs[2];
            info.isSectorStocks = true;
            info.isSectorList = false;
        }
        return info;
    }

    if (seg0 == L"\u641c\u7d22") {
        info.isSearch = true;
        if (segs.size() == 1) return info;

        info.searchQuery = segs[1];
        for (size_t i = 2; i < segs.size(); i++) {
            info.searchQuery += L" " + segs[i];
        }

        if (segs.size() >= 3) {
            size_t lastDot = segs[segs.size() - 1].find(L'.');
            if (lastDot != std::wstring::npos) {
                info.symbol = segs[segs.size() - 1];
                info.isStockDir = true;
            }
        }
        return info;
    }

    if (segs.size() >= 1) {
        size_t dotPos = seg0.find(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring suffix = seg0.substr(dotPos + 1);
            if (suffix == L"SH" || suffix == L"SZ" || suffix == L"HK" || suffix == L"US" || suffix == L"BJ") {
                info.symbol = seg0;
                info.isStockDir = true;
                info.market = StockClient::GetMarketFromSymbol(seg0);

                if (segs.size() >= 2) {
                    const std::wstring& seg1 = segs[1];
                    if (seg1 == L"\u5b9e\u65f6\u884c\u60c5.txt") {
                        info.isQuoteFile = true;
                        info.isVirtualFile = true;
                        info.isReadOnly = true;
                    } else if (seg1 == metaPfx) {
                        info.isMetadata = true;
                        info.metaPrefix = metaPfx;
                        if (segs.size() >= 3) {
                            const std::wstring& metaSeg = segs[2];
                            if (metaSeg == L"star.signal") {
                                info.metaType = STOCK_META_SIGNAL;
                                info.metaItem = L"star";
                                info.isSignalFile = true;
                                info.isVirtualFile = true;
                            } else if (metaSeg == L"info.txt") {
                                info.metaType = STOCK_META_INFO;
                                info.metaItem = L"info";
                                info.isVirtualFile = true;
                                info.isReadOnly = true;
                            }
                        }
                    }
                }
                return info;
            }
        }
    }

    return info;
}

static LPWSTR AllocString(HANDLE hHeap, const std::wstring& str) {
    size_t len = (str.length() + 1) * sizeof(WCHAR);
    LPWSTR p = (LPWSTR)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, len);
    if (p) StringCchCopyW(p, str.length() + 1, str.c_str());
    return p;
}

static LPVFSFILEDATAHEADER AllocateFileDataHeader(HANDLE hHeap, int numItems) {
    size_t allocSize = sizeof(VFSFILEDATAHEADER) + (sizeof(VFSFILEDATAW) * (numItems > 0 ? numItems : 1));
    LPVFSFILEDATAHEADER lpFDH = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, allocSize);
    if (!lpFDH) return NULL;
    lpFDH->cbSize = sizeof(VFSFILEDATAHEADER);
    lpFDH->cbFileDataSize = sizeof(VFSFILEDATAW);
    lpFDH->iNumItems = numItems;
    return lpFDH;
}

static void AddVirtualDir(LPVFSFILEDATAW lpFileData, const std::wstring& name) {
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, name.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
}

static void AddVirtualFile(LPVFSFILEDATAW lpFileData, const std::wstring& name, DWORD size = 0) {
    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, name.c_str());
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
    lpFileData->wfdData.nFileSizeLow = size;
}

static int AddErrorFile(LPVFSREADDIRDATAW lpRDD, const std::wstring& errorTitle, const std::wstring& errorDetail) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualFile(&lpFileData[0], errorTitle, (DWORD)(errorDetail.length() * sizeof(WCHAR)));

    lpFileData[0].iNumColumns = 1;
    lpFileData[0].lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(lpRDD->hMemHeap, HEAP_ZERO_MEMORY,
                                                                      sizeof(VFSFILEDATACOLUMNW));
    if (lpFileData[0].lpvfsColumnData) {
        lpFileData[0].lpvfsColumnData[0].iColumnId = 1;
        lpFileData[0].lpvfsColumnData[0].lpszValue = AllocString(lpRDD->hMemHeap, errorDetail);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static std::wstring FormatQuoteContent(const StockQuote& quote) {
    std::wostringstream oss;

    StockMarket market = quote.market;
    std::wstring marketName = StockClient::GetMarketName(market);

    oss << L"\u80a1\u7968\u540d\u79f0: " << quote.name << L" (" << quote.symbol << L")\r\n";
    oss << L"\u5e02\u573a: " << marketName << L"\r\n";
    oss << L"\r\n";

    oss << L"\u5f53\u524d\u4ef7\u683c: " << StockClient::FormatPrice(quote.currentPrice, market) << L"\r\n";

    std::wstring changeSign = quote.changeAmount >= 0 ? L"+" : L"";
    oss << L"\u6da8\u8dcc\u989d: " << changeSign << StockClient::FormatPrice(quote.changeAmount, market) << L"\r\n";
    oss << L"\u6da8\u8dcc\u5e45: " << changeSign;
    WCHAR pctBuf[16];
    StringCchPrintfW(pctBuf, 16, L"%.2f%%", quote.changePercent);
    oss << pctBuf << L"\r\n";

    oss << L"\r\n";
    oss << L"\u4eca\u5f00: " << StockClient::FormatPrice(quote.openPrice, market) << L"\r\n";
    oss << L"\u6700\u9ad8: " << StockClient::FormatPrice(quote.highPrice, market) << L"\r\n";
    oss << L"\u6700\u4f4e: " << StockClient::FormatPrice(quote.lowPrice, market) << L"\r\n";
    oss << L"\u6628\u6536: " << StockClient::FormatPrice(quote.previousClose, market) << L"\r\n";
    oss << L"\r\n";
    oss << L"\u6210\u4ea4\u91cf: " << StockClient::FormatVolume(quote.volume) << L"\r\n";
    oss << L"\u6210\u4ea4\u989d: " << StockClient::FormatAmount(quote.amount) << L"\r\n";

    if (quote.turnoverRate > 0) {
        StringCchPrintfW(pctBuf, 16, L"%.2f%%", quote.turnoverRate);
        oss << L"\u6362\u624b\u7387: " << pctBuf << L"\r\n";
    }

    if (quote.pe > 0) {
        WCHAR peBuf[16];
        StringCchPrintfW(peBuf, 16, L"%.2f", quote.pe);
        oss << L"\u5e02\u76c8\u7387: " << peBuf << L"\r\n";
    }
    if (quote.pb > 0) {
        WCHAR pbBuf[16];
        StringCchPrintfW(pbBuf, 16, L"%.2f", quote.pb);
        oss << L"\u5e02\u51c0\u7387: " << pbBuf << L"\r\n";
    }
    if (quote.marketCap > 0) {
        oss << L"\u603b\u5e02\u503c: " << StockClient::FormatMarketCap(quote.marketCap) << L"\r\n";
    }
    if (quote.circulatingCap > 0) {
        oss << L"\u6d41\u901a\u5e02\u503c: " << StockClient::FormatMarketCap(quote.circulatingCap) << L"\r\n";
    }

    SYSTEMTIME st;
    FileTimeToSystemTime(&quote.updateTime, &st);
    WCHAR timeBuf[64];
    StringCchPrintfW(timeBuf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    oss << L"\r\n\u66f4\u65b0\u65f6\u95f4: " << timeBuf << L"\r\n";

    return oss.str();
}

static std::wstring FormatIndexQuoteContent(const IndexQuote& idx) {
    std::wostringstream oss;

    oss << L"\u6307\u6570\u540d\u79f0: " << idx.name << L" (" << idx.symbol << L")\r\n";
    oss << L"\r\n";

    WCHAR buf[32];
    StringCchPrintfW(buf, 32, L"%.2f", idx.currentPoint);
    oss << L"\u5f53\u524d\u70b9\u4f4d: " << buf << L"\r\n";

    std::wstring changeSign = idx.changeAmount >= 0 ? L"+" : L"";
    StringCchPrintfW(buf, 32, L"%.2f", idx.changeAmount);
    oss << L"\u6da8\u8dcc\u70b9: " << changeSign << buf << L"\r\n";
    WCHAR pctBuf[16];
    StringCchPrintfW(pctBuf, 16, L"%.2f%%", idx.changePercent);
    oss << L"\u6da8\u8dcc\u5e45: " << changeSign << pctBuf << L"\r\n";

    oss << L"\r\n";
    StringCchPrintfW(buf, 32, L"%.2f", idx.openPoint);
    oss << L"\u4eca\u5f00: " << buf << L"\r\n";
    StringCchPrintfW(buf, 32, L"%.2f", idx.highPoint);
    oss << L"\u6700\u9ad8: " << buf << L"\r\n";
    StringCchPrintfW(buf, 32, L"%.2f", idx.lowPoint);
    oss << L"\u6700\u4f4e: " << buf << L"\r\n";
    StringCchPrintfW(buf, 32, L"%.2f", idx.previousClose);
    oss << L"\u6628\u6536: " << buf << L"\r\n";
    oss << L"\r\n";
    oss << L"\u6210\u4ea4\u91cf: " << StockClient::FormatVolume(idx.volume) << L"\r\n";
    oss << L"\u6210\u4ea4\u989d: " << StockClient::FormatAmount(idx.amount) << L"\r\n";

    SYSTEMTIME st;
    FileTimeToSystemTime(&idx.updateTime, &st);
    WCHAR timeBuf[64];
    StringCchPrintfW(timeBuf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    oss << L"\r\n\u66f4\u65b0\u65f6\u95f4: " << timeBuf << L"\r\n";

    return oss.str();
}

static std::wstring FormatInfoContent(const StockQuote& quote) {
    std::wostringstream oss;

    oss << L"\u80a1\u7968\u4ee3\u7801: " << quote.symbol << L"\r\n";
    oss << L"\u80a1\u7968\u540d\u79f0: " << quote.name << L"\r\n";
    oss << L"\u5e02\u573a: " << StockClient::GetMarketName(quote.market) << L"\r\n";
    oss << L"\u4ee3\u7801: " << quote.code << L"\r\n";

    bool isStarred = StockClient::IsInWatchlist(quote.symbol);
    oss << L"\u81ea\u9009: " << (isStarred ? L"\u2605 \u5df2\u6dfb\u52a0" : L"\u2606 \u672a\u6dfb\u52a0") << L"\r\n";

    return oss.str();
}

static std::wstring GetStockDisplayName(const StockQuote& quote) {
    std::wstring name = quote.name;
    if (name.empty()) name = quote.symbol;

    if (quote.currentPrice > 0) {
        WCHAR buf[16];
        StringCchPrintfW(buf, 16, L"%.2f", quote.changePercent);
        std::wstring sign = quote.changePercent >= 0 ? L"+" : L"";
        name += L" [" + sign + std::wstring(buf) + L"%]";
    }

    return name;
}

static void FillStockColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const StockQuote& quote) {
    const int NUM_COLS = 16;
    lpFileData->iNumColumns = NUM_COLS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                    NUM_COLS * sizeof(VFSFILEDATACOLUMNW));
    if (!lpFileData->lpvfsColumnData) return;

    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, quote.name);

    lpFileData->lpvfsColumnData[1].iColumnId = 2;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, StockClient::FormatPrice(quote.currentPrice, quote.market));

    WCHAR chgBuf[32];
    if (quote.changeAmount > 0) {
        StringCchPrintfW(chgBuf, 32, L"+%s", StockClient::FormatPrice(quote.changeAmount, quote.market).c_str());
    } else if (quote.changeAmount < 0) {
        StringCchPrintfW(chgBuf, 32, L"%s", StockClient::FormatPrice(quote.changeAmount, quote.market).c_str());
    } else {
        StringCchPrintfW(chgBuf, 32, L"0.00");
    }
    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, chgBuf);

    WCHAR pctBuf[32];
    if (quote.changePercent > 0) {
        StringCchPrintfW(pctBuf, 32, L"+%.2f%%", quote.changePercent);
    } else if (quote.changePercent < 0) {
        StringCchPrintfW(pctBuf, 32, L"%.2f%%", quote.changePercent);
    } else {
        StringCchPrintfW(pctBuf, 32, L"0.00%%");
    }
    lpFileData->lpvfsColumnData[3].iColumnId = 4;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, pctBuf);

    lpFileData->lpvfsColumnData[4].iColumnId = 5;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, StockClient::FormatPrice(quote.openPrice, quote.market));

    lpFileData->lpvfsColumnData[5].iColumnId = 6;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, StockClient::FormatPrice(quote.highPrice, quote.market));

    lpFileData->lpvfsColumnData[6].iColumnId = 7;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, StockClient::FormatPrice(quote.lowPrice, quote.market));

    lpFileData->lpvfsColumnData[7].iColumnId = 8;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, StockClient::FormatPrice(quote.previousClose, quote.market));

    lpFileData->lpvfsColumnData[8].iColumnId = 9;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, StockClient::FormatVolume(quote.volume));

    lpFileData->lpvfsColumnData[9].iColumnId = 10;
    lpFileData->lpvfsColumnData[9].lpszValue = AllocString(hHeap, StockClient::FormatAmount(quote.amount));

    WCHAR turnBuf[32];
    if (quote.turnoverRate > 0) {
        StringCchPrintfW(turnBuf, 32, L"%.2f%%", quote.turnoverRate);
    } else {
        StringCchPrintfW(turnBuf, 32, L"--");
    }
    lpFileData->lpvfsColumnData[10].iColumnId = 11;
    lpFileData->lpvfsColumnData[10].lpszValue = AllocString(hHeap, turnBuf);

    lpFileData->lpvfsColumnData[11].iColumnId = 12;
    lpFileData->lpvfsColumnData[11].lpszValue = AllocString(hHeap, StockClient::GetMarketName(quote.market));

    WCHAR peBuf[32];
    if (quote.pe > 0) {
        StringCchPrintfW(peBuf, 32, L"%.2f", quote.pe);
    } else {
        StringCchPrintfW(peBuf, 32, L"--");
    }
    lpFileData->lpvfsColumnData[12].iColumnId = 13;
    lpFileData->lpvfsColumnData[12].lpszValue = AllocString(hHeap, peBuf);

    WCHAR pbBuf[32];
    if (quote.pb > 0) {
        StringCchPrintfW(pbBuf, 32, L"%.2f", quote.pb);
    } else {
        StringCchPrintfW(pbBuf, 32, L"--");
    }
    lpFileData->lpvfsColumnData[13].iColumnId = 14;
    lpFileData->lpvfsColumnData[13].lpszValue = AllocString(hHeap, pbBuf);

    lpFileData->lpvfsColumnData[14].iColumnId = 15;
    lpFileData->lpvfsColumnData[14].lpszValue = AllocString(hHeap, StockClient::FormatMarketCap(quote.marketCap));

    lpFileData->lpvfsColumnData[15].iColumnId = 16;
    lpFileData->lpvfsColumnData[15].lpszValue = AllocString(hHeap, StockClient::FormatMarketCap(quote.circulatingCap));
}

static void FillIndexColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const IndexQuote& idx) {
    const int NUM_COLS = 11;
    lpFileData->iNumColumns = NUM_COLS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                    NUM_COLS * sizeof(VFSFILEDATACOLUMNW));
    if (!lpFileData->lpvfsColumnData) return;

    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, idx.name);

    WCHAR ptBuf[32];
    StringCchPrintfW(ptBuf, 32, L"%.2f", idx.currentPoint);
    lpFileData->lpvfsColumnData[1].iColumnId = 2;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, ptBuf);

    WCHAR chgBuf[32];
    if (idx.changeAmount > 0) {
        StringCchPrintfW(chgBuf, 32, L"+%.2f", idx.changeAmount);
    } else if (idx.changeAmount < 0) {
        StringCchPrintfW(chgBuf, 32, L"%.2f", idx.changeAmount);
    } else {
        StringCchPrintfW(chgBuf, 32, L"0.00");
    }
    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, chgBuf);

    WCHAR pctBuf[32];
    if (idx.changePercent > 0) {
        StringCchPrintfW(pctBuf, 32, L"+%.2f%%", idx.changePercent);
    } else if (idx.changePercent < 0) {
        StringCchPrintfW(pctBuf, 32, L"%.2f%%", idx.changePercent);
    } else {
        StringCchPrintfW(pctBuf, 32, L"0.00%%");
    }
    lpFileData->lpvfsColumnData[3].iColumnId = 4;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, pctBuf);

    WCHAR openBuf[32];
    StringCchPrintfW(openBuf, 32, L"%.2f", idx.openPoint);
    lpFileData->lpvfsColumnData[4].iColumnId = 5;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, openBuf);

    WCHAR highBuf[32];
    StringCchPrintfW(highBuf, 32, L"%.2f", idx.highPoint);
    lpFileData->lpvfsColumnData[5].iColumnId = 6;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, highBuf);

    WCHAR lowBuf[32];
    StringCchPrintfW(lowBuf, 32, L"%.2f", idx.lowPoint);
    lpFileData->lpvfsColumnData[6].iColumnId = 7;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, lowBuf);

    WCHAR prevBuf[32];
    StringCchPrintfW(prevBuf, 32, L"%.2f", idx.previousClose);
    lpFileData->lpvfsColumnData[7].iColumnId = 8;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, prevBuf);

    lpFileData->lpvfsColumnData[8].iColumnId = 9;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, StockClient::FormatVolume(idx.volume));

    lpFileData->lpvfsColumnData[9].iColumnId = 10;
    lpFileData->lpvfsColumnData[9].lpszValue = AllocString(hHeap, StockClient::FormatAmount(idx.amount));

    lpFileData->lpvfsColumnData[10].iColumnId = 12;
    lpFileData->lpvfsColumnData[10].lpszValue = AllocString(hHeap, idx.code);
}

static void FillSectorColumnData(LPVFSFILEDATAW lpFileData, HANDLE hHeap, const SectorInfo& sec) {
    const int NUM_COLS = 9;
    lpFileData->iNumColumns = NUM_COLS;
    lpFileData->lpvfsColumnData = (LPVFSFILEDATACOLUMNW)HeapAlloc(hHeap, HEAP_ZERO_MEMORY,
                                                                    NUM_COLS * sizeof(VFSFILEDATACOLUMNW));
    if (!lpFileData->lpvfsColumnData) return;

    lpFileData->lpvfsColumnData[0].iColumnId = 1;
    lpFileData->lpvfsColumnData[0].lpszValue = AllocString(hHeap, sec.name);

    WCHAR pctBuf[32];
    if (sec.changePercent > 0) {
        StringCchPrintfW(pctBuf, 32, L"+%.2f%%", sec.changePercent);
    } else if (sec.changePercent < 0) {
        StringCchPrintfW(pctBuf, 32, L"%.2f%%", sec.changePercent);
    } else {
        StringCchPrintfW(pctBuf, 32, L"0.00%%");
    }
    lpFileData->lpvfsColumnData[1].iColumnId = 4;
    lpFileData->lpvfsColumnData[1].lpszValue = AllocString(hHeap, pctBuf);

    WCHAR chgBuf[32];
    if (sec.changeAmount > 0) {
        StringCchPrintfW(chgBuf, 32, L"+%.2f", sec.changeAmount);
    } else if (sec.changeAmount < 0) {
        StringCchPrintfW(chgBuf, 32, L"%.2f", sec.changeAmount);
    } else {
        StringCchPrintfW(chgBuf, 32, L"0.00");
    }
    lpFileData->lpvfsColumnData[2].iColumnId = 3;
    lpFileData->lpvfsColumnData[2].lpszValue = AllocString(hHeap, chgBuf);

    WCHAR cntBuf[32];
    StringCchPrintfW(cntBuf, 32, L"%d", sec.stockCount);
    lpFileData->lpvfsColumnData[3].iColumnId = 17;
    lpFileData->lpvfsColumnData[3].lpszValue = AllocString(hHeap, cntBuf);

    lpFileData->lpvfsColumnData[4].iColumnId = 9;
    lpFileData->lpvfsColumnData[4].lpszValue = AllocString(hHeap, StockClient::FormatVolume(sec.volume));

    lpFileData->lpvfsColumnData[5].iColumnId = 10;
    lpFileData->lpvfsColumnData[5].lpszValue = AllocString(hHeap, StockClient::FormatAmount(sec.amount));

    lpFileData->lpvfsColumnData[6].iColumnId = 18;
    lpFileData->lpvfsColumnData[6].lpszValue = AllocString(hHeap, sec.leadStockName);

    WCHAR leadPctBuf[32];
    if (sec.leadChgPct > 0) {
        StringCchPrintfW(leadPctBuf, 32, L"+%.2f%%", sec.leadChgPct);
    } else if (sec.leadChgPct < 0) {
        StringCchPrintfW(leadPctBuf, 32, L"%.2f%%", sec.leadChgPct);
    } else {
        StringCchPrintfW(leadPctBuf, 32, L"0.00%%");
    }
    lpFileData->lpvfsColumnData[7].iColumnId = 19;
    lpFileData->lpvfsColumnData[7].lpszValue = AllocString(hHeap, leadPctBuf);

    lpFileData->lpvfsColumnData[8].iColumnId = 12;
    lpFileData->lpvfsColumnData[8].lpszValue = AllocString(hHeap, sec.code);
}

static int ReadRootDirectory(LPVFSREADDIRDATAW lpRDD) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 8);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualDir(&lpFileData[0], L"\u6caa\u5e02");
    AddVirtualDir(&lpFileData[1], L"\u6df1\u5e02");
    AddVirtualDir(&lpFileData[2], L"\u5317\u4ea4");
    AddVirtualDir(&lpFileData[3], L"\u6e2f\u80a1");
    AddVirtualDir(&lpFileData[4], L"\u7f8e\u80a1");
    AddVirtualDir(&lpFileData[5], L"\u81ea\u9009\u80a1");
    AddVirtualDir(&lpFileData[6], L"\u6307\u6570");
    AddVirtualDir(&lpFileData[7], L"\u677f\u5757");

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadWatchlistDirectory(LPVFSREADDIRDATAW lpRDD) {
    auto watchlist = StockClient::GetWatchlist();
    if (watchlist.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u81ea\u9009\u80a1\u4e3a\u7a7a.txt",
            L"\u5c1a\u672a\u6dfb\u52a0\u81ea\u9009\u80a1\uff0c\u8bf7\u901a\u8fc7\u641c\u7d22\u627e\u5230\u80a1\u7968\u540e\u521b\u5efa star.signal \u6dfb\u52a0");
    }

    std::vector<std::wstring> symbols;
    for (const auto& entry : watchlist) {
        symbols.push_back(entry.symbol);
    }

    std::vector<StockQuote> quotes;
    if (!symbols.empty()) {
        quotes = StockClient::GetQuotes(symbols);
    }

    int numItems = (int)watchlist.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        std::wstring displayName = watchlist[i].symbol;

        StockQuote matchingQuote;
        bool foundQuote = false;
        for (const auto& q : quotes) {
            if (_wcsicmp(q.symbol.c_str(), watchlist[i].symbol.c_str()) == 0) {
                matchingQuote = q;
                foundQuote = true;
                break;
            }
        }

        if (foundQuote && !matchingQuote.name.empty()) {
            displayName = GetStockDisplayName(matchingQuote);
        }

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, watchlist[i].symbol.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        if (foundQuote) {
            FillStockColumnData(&lpFileData[i], lpRDD->hMemHeap, matchingQuote);
        }
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadMarketDirectory(LPVFSREADDIRDATAW lpRDD) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 5);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualDir(&lpFileData[0], L"\u6caa\u5e02");
    AddVirtualDir(&lpFileData[1], L"\u6df1\u5e02");
    AddVirtualDir(&lpFileData[2], L"\u5317\u4ea4");
    AddVirtualDir(&lpFileData[3], L"\u6e2f\u80a1");
    AddVirtualDir(&lpFileData[4], L"\u7f8e\u80a1");

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadMarketStockList(LPVFSREADDIRDATAW lpRDD, const StockPathInfo& pathInfo) {
    if (pathInfo.market == STOCK_MARKET_HK || pathInfo.market == STOCK_MARKET_US) {
        std::wstring marketName = (pathInfo.market == STOCK_MARKET_HK) ? L"\u6e2f\u80a1" : L"\u7f8e\u80a1";
        std::wstring hint;
        if (pathInfo.market == STOCK_MARKET_HK) {
            hint = L"\u6e2f\u80a1\u884c\u60c5\u652f\u6301\u5df2\u5c31\u7eea\uff0c\u5217\u8868\u529f\u80fd\u6682\u4e0d\u53ef\u7528\r\n\r\n"
                   L"\u67e5\u770b\u6e2f\u80a1\u884c\u60c5\u7684\u65b9\u6cd5\uff1a\r\n"
                   L"1. \u5728\u5730\u5740\u680f\u8f93\u5165\u8def\u5f84 \u6e2f\u80a1/00001.HK \u67e5\u770b\u6307\u5b9a\u80a1\u7968\r\n"
                   L"2. \u5728\u5730\u5740\u680f\u8f93\u5165\u8def\u5f84 \u641c\u7d22/\u817e\u8baf \u641c\u7d22\u80a1\u7968\r\n"
                   L"3. \u5728\u80a1\u7968\u76ee\u5f55\u4e2d\u7f16\u8f91 watchlist.txt \u6dfb\u52a0\u5230\u81ea\u9009\u80a1";
        } else {
            hint = L"\u7f8e\u80a1\u884c\u60c5\u652f\u6301\u5df2\u5c31\u7eea\uff0c\u5217\u8868\u529f\u80fd\u6682\u4e0d\u53ef\u7528\r\n\r\n"
                   L"\u67e5\u770b\u7f8e\u80a1\u884c\u60c5\u7684\u65b9\u6cd5\uff1a\r\n"
                   L"1. \u5728\u5730\u5740\u680f\u8f93\u5165\u8def\u5f84 \u7f8e\u80a1/AAPL.US \u67e5\u770b\u6307\u5b9a\u80a1\u7968\r\n"
                   L"2. \u5728\u5730\u5740\u680f\u8f93\u5165\u8def\u5f84 \u641c\u7d22/apple \u641c\u7d22\u80a1\u7968\r\n"
                   L"3. \u5728\u80a1\u7968\u76ee\u5f55\u4e2d\u7f16\u8f91 watchlist.txt \u6dfb\u52a0\u5230\u81ea\u9009\u80a1";
        }
        return AddErrorFile(lpRDD,
            L"\u26a0 " + marketName + L"\u5217\u8868\u6682\u4e0d\u53ef\u7528.txt",
            hint);
    }

    std::vector<StockQuote> stocks = StockClient::ListMarketStocks(pathInfo.market);

    if (stocks.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u65e0\u6cd5\u83b7\u53d6\u80a1\u7968\u5217\u8868.txt",
            L"\u8bf7\u68c0\u67e5\u7f51\u7edc\u8fde\u63a5\u6216\u7a0d\u540e\u91cd\u8bd5");
    }

    int numItems = (int)stocks.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, stocks[i].symbol.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastWriteTime = stocks[i].updateTime;

        FillStockColumnData(&lpFileData[i], lpRDD->hMemHeap, stocks[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSearchDirectory(LPVFSREADDIRDATAW lpRDD, const StockPathInfo& pathInfo) {
    if (pathInfo.searchQuery.empty()) {
        return AddErrorFile(lpRDD,
            L"\u8f93\u5165\u641c\u7d22\u5173\u952e\u8bcd.txt",
            L"\u8bf7\u5728\u8def\u5f84\u4e2d\u8f93\u5165\u80a1\u7968\u4ee3\u7801\u6216\u540d\u79f0\u8fdb\u884c\u641c\u7d22");
    }

    StockSearchResult result = StockClient::SearchStock(pathInfo.searchQuery);
    if (result.stocks.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u672a\u627e\u5230\u7ed3\u679c.txt",
            L"\u641c\u7d22 \"" + pathInfo.searchQuery + L"\" \u672a\u627e\u5230\u5339\u914d\u7684\u80a1\u7968");
    }

    std::vector<std::wstring> symbols;
    for (const auto& s : result.stocks) {
        symbols.push_back(s.symbol);
    }
    std::vector<StockQuote> quotes = StockClient::GetQuotes(symbols);

    std::unordered_map<std::wstring, StockQuote> quoteMap;
    for (const auto& q : quotes) {
        quoteMap[q.symbol] = q;
    }

    int numItems = (int)result.stocks.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, result.stocks[i].symbol.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        auto it = quoteMap.find(result.stocks[i].symbol);
        if (it != quoteMap.end()) {
            FillStockColumnData(&lpFileData[i], lpRDD->hMemHeap, it->second);
        } else {
            StockQuote emptyQuote;
            emptyQuote.symbol = result.stocks[i].symbol;
            emptyQuote.name = result.stocks[i].name;
            emptyQuote.code = result.stocks[i].code;
            emptyQuote.market = result.stocks[i].market;
            FillStockColumnData(&lpFileData[i], lpRDD->hMemHeap, emptyQuote);
        }
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadIndexDirectory(LPVFSREADDIRDATAW lpRDD) {
    std::vector<IndexQuote> indices = StockClient::ListIndices();

    if (indices.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u65e0\u6cd5\u83b7\u53d6\u6307\u6570\u5217\u8868.txt",
            L"\u8bf7\u68c0\u67e5\u7f51\u7edc\u8fde\u63a5\u6216\u7a0d\u540e\u91cd\u8bd5");
    }

    int numItems = (int)indices.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        std::wstring displayName = indices[i].name;
        if (indices[i].currentPoint > 0) {
            WCHAR buf[16];
            StringCchPrintfW(buf, 16, L"%.2f", indices[i].changePercent);
            std::wstring sign = indices[i].changePercent >= 0 ? L"+" : L"";
            displayName += L" [" + sign + std::wstring(buf) + L"%]";
        }

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, indices[i].symbol.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastWriteTime = indices[i].updateTime;

        FillIndexColumnData(&lpFileData[i], lpRDD->hMemHeap, indices[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSectorDirectory(LPVFSREADDIRDATAW lpRDD) {
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 3);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    AddVirtualDir(&lpFileData[0], L"\u884c\u4e1a");
    AddVirtualDir(&lpFileData[1], L"\u6982\u5ff5");
    AddVirtualDir(&lpFileData[2], L"\u5730\u57df");

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSectorList(LPVFSREADDIRDATAW lpRDD, SectorType type) {
    std::vector<SectorInfo> sectors = StockClient::ListSectors(type);

    if (sectors.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u65e0\u6cd5\u83b7\u53d6\u677f\u5757\u5217\u8868.txt",
            L"\u8bf7\u68c0\u67e5\u7f51\u7edc\u8fde\u63a5\u6216\u7a0d\u540e\u91cd\u8bd5");
    }

    int numItems = (int)sectors.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, sectors[i].code.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftLastWriteTime);

        FillSectorColumnData(&lpFileData[i], lpRDD->hMemHeap, sectors[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadSectorStocks(LPVFSREADDIRDATAW lpRDD, const std::wstring& sectorCode) {
    std::vector<StockQuote> stocks = StockClient::ListSectorStocks(sectorCode);

    if (stocks.empty()) {
        return AddErrorFile(lpRDD,
            L"\u26a0 \u65e0\u6cd5\u83b7\u53d6\u677f\u5757\u6210\u5206\u80a1.txt",
            L"\u8bf7\u68c0\u67e5\u7f51\u7edc\u8fde\u63a5\u6216\u7a0d\u540e\u91cd\u8bd5");
    }

    int numItems = (int)stocks.size();
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
    for (int i = 0; i < numItems; i++) {
        if (lpRDD->hAbortEvent && WaitForSingleObject(lpRDD->hAbortEvent, 0) == WAIT_OBJECT_0) break;

        StringCchCopyW(lpFileData[i].wfdData.cFileName, MAX_PATH, stocks[i].symbol.c_str());
        lpFileData[i].wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData[i].wfdData.ftCreationTime);
        lpFileData[i].wfdData.ftLastWriteTime = stocks[i].updateTime;

        FillStockColumnData(&lpFileData[i], lpRDD->hMemHeap, stocks[i]);
    }

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadStockDirectory(LPVFSREADDIRDATAW lpRDD, const StockPathInfo& pathInfo) {
    if (pathInfo.isIndex) {
        LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 1);
        if (!lpFDH) return FALSE;

        LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);
        AddVirtualFile(&lpFileData[0], L"\u5b9e\u65f6\u884c\u60c5.txt");

        lpRDD->lpFileData = lpFDH;
        return TRUE;
    }

    StockQuote quote = StockClient::GetQuote(pathInfo.symbol);

    StockConfig& cfg = StockClient::GetConfig();
    std::wstring metaPfx = cfg.metaPrefix.empty() ? L".st" : cfg.metaPrefix;

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, 2);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    std::wstring quoteFileName = L"\u5b9e\u65f6\u884c\u60c5.txt";
    if (quote.currentPrice > 0 && cfg.showChangeColor) {
        std::wstring sign = quote.changePercent >= 0 ? L"\u2191" : L"\u2193";
        quoteFileName = sign + L" " + quoteFileName;
    }
    AddVirtualFile(&lpFileData[0], quoteFileName);
    AddVirtualDir(&lpFileData[1], metaPfx);

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int ReadMetadataDirectory(LPVFSREADDIRDATAW lpRDD, const StockPathInfo& pathInfo) {
    int numItems = 2;
    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(lpRDD->hMemHeap, numItems);
    if (!lpFDH) return FALSE;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    bool isStarred = StockClient::IsInWatchlist(pathInfo.symbol);
    AddVirtualFile(&lpFileData[0], isStarred ? L"star.signal" : L"star.signal");
    AddVirtualFile(&lpFileData[1], L"info.txt");

    lpRDD->lpFileData = lpFDH;
    return TRUE;
}

static int InternalReadDirectory(LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD || !lpRDD->lpszPath) return FALSE;

    StockPathInfo pathInfo = ParseStockPath(lpRDD->lpszPath);

    if (pathInfo.isRoot) {
        return ReadRootDirectory(lpRDD);
    }
    if (pathInfo.isWatchlist && !pathInfo.isStockDir) {
        return ReadWatchlistDirectory(lpRDD);
    }
    if (pathInfo.isMarket && !pathInfo.isMarketList && pathInfo.symbol.empty()) {
        return ReadMarketDirectory(lpRDD);
    }
    if (pathInfo.isMarketList && pathInfo.symbol.empty()) {
        return ReadMarketStockList(lpRDD, pathInfo);
    }
    if (pathInfo.isIndexList) {
        return ReadIndexDirectory(lpRDD);
    }
    if (pathInfo.isSector && !pathInfo.isSectorList && !pathInfo.isSectorStocks) {
        return ReadSectorDirectory(lpRDD);
    }
    if (pathInfo.isSectorList) {
        return ReadSectorList(lpRDD, pathInfo.sectorType);
    }
    if (pathInfo.isSectorStocks) {
        return ReadSectorStocks(lpRDD, pathInfo.sectorCode);
    }
    if (pathInfo.isSearch && pathInfo.symbol.empty()) {
        return ReadSearchDirectory(lpRDD, pathInfo);
    }
    if (pathInfo.isStockDir && !pathInfo.isQuoteFile && !pathInfo.isMetadata) {
        return ReadStockDirectory(lpRDD, pathInfo);
    }
    if (pathInfo.isMetadata && !pathInfo.isSignalFile && pathInfo.metaType == STOCK_META_ROOT) {
        return ReadMetadataDirectory(lpRDD, pathInfo);
    }

    return AddErrorFile(lpRDD,
        L"\u26a0 \u65e0\u6cd5\u89e3\u6790\u8def\u5f84.txt",
        std::wstring(lpRDD->lpszPath) + L" \u8def\u5f84\u65e0\u6548");
}

extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData) {
    StockClient::SetRefreshCallback([]() {
        if (g_hWndNotify) {
            PostMessageW(g_hWndNotify, DVFSPLUGINMSG_REINITIALIZE, 0, g_dwNotifyData);
        }
    });
    StockClient::Init();
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_Uninit() {
    g_PluginUnloading.store(true);
    while (g_ActiveThreads.load() > 0) Sleep(50);
    StockClient::Cleanup();
}

extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow) {
    if (hwndMsgWindow && !g_hWndNotify) {
        g_hWndNotify = hwndMsgWindow;
    }
    return (HANDLE)1;
}

extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData) {
}

extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo) {
    if (!lpVFSInfo) return FALSE;

    lpVFSInfo->idPlugin = GUIDPlugin_Stock;
    lpVFSInfo->dwVersionHigh = MAKELONG(0, 1);
    lpVFSInfo->dwVersionLow = MAKELONG(0, 0);
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_CASESENSITIVE | VFSCAPABILITY_RANDOMSEEK;

    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, STOCK_VFS_PREFIX);

    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"Stock");

    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax,
                       L"\u80a1\u7968\u865a\u62df\u6587\u4ef6\u7cfb\u7edf");

    if (lpVFSInfo->lpszCopyright)
        StringCchCopyW(lpVFSInfo->lpszCopyright, lpVFSInfo->cchCopyrightMax, L"(c) 2026");

    if (lpVFSInfo->lpszURL)
        StringCchCopyW(lpVFSInfo->lpszURL, lpVFSInfo->cchURLMax, L"");

    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    lpVFSInfo->dwInitFlags = 0;

    HICON hIconLarge = NULL, hIconSmall = NULL;
    ExtractIconExW(L"shell32.dll", 14, &hIconLarge, &hIconSmall, 1);
    lpVFSInfo->hIconLarge = hIconLarge;
    lpVFSInfo->hIconSmall = hIconSmall;

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax) {
    if (!lpszPrefix) return FALSE;
    if (cchPrefixMax < 10) return FALSE;
    memset(lpszPrefix, 0, cchPrefixMax * sizeof(WCHAR));
    StringCchCopyW(lpszPrefix, cchPrefixMax, STOCK_VFS_PREFIX);
    return TRUE;
}

extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hVFSData) {
    static VFSCUSTOMCOLUMNW columns[19];

    columns[0].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[0].lpNext = &columns[1];
    columns[0].lpszLabel = L"\u80a1\u7968\u540d\u79f0";
    columns[0].lpszKey = L"stname";
    columns[0].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[0].iID = 1;

    columns[1].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[1].lpNext = &columns[2];
    columns[1].lpszLabel = L"\u5f53\u524d\u4ef7";
    columns[1].lpszKey = L"stprice";
    columns[1].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[1].iID = 2;

    columns[2].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[2].lpNext = &columns[3];
    columns[2].lpszLabel = L"\u6da8\u8dcc\u989d";
    columns[2].lpszKey = L"stchgamt";
    columns[2].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[2].iID = 3;

    columns[3].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[3].lpNext = &columns[4];
    columns[3].lpszLabel = L"\u6da8\u8dcc\u5e45";
    columns[3].lpszKey = L"stchgpct";
    columns[3].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[3].iID = 4;

    columns[4].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[4].lpNext = &columns[5];
    columns[4].lpszLabel = L"\u4eca\u5f00";
    columns[4].lpszKey = L"stopen";
    columns[4].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[4].iID = 5;

    columns[5].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[5].lpNext = &columns[6];
    columns[5].lpszLabel = L"\u6700\u9ad8";
    columns[5].lpszKey = L"sthigh";
    columns[5].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[5].iID = 6;

    columns[6].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[6].lpNext = &columns[7];
    columns[6].lpszLabel = L"\u6700\u4f4e";
    columns[6].lpszKey = L"stlow";
    columns[6].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[6].iID = 7;

    columns[7].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[7].lpNext = &columns[8];
    columns[7].lpszLabel = L"\u6628\u6536";
    columns[7].lpszKey = L"stprevclose";
    columns[7].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[7].iID = 8;

    columns[8].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[8].lpNext = &columns[9];
    columns[8].lpszLabel = L"\u6210\u4ea4\u91cf";
    columns[8].lpszKey = L"stvol";
    columns[8].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[8].iID = 9;

    columns[9].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[9].lpNext = &columns[10];
    columns[9].lpszLabel = L"\u6210\u4ea4\u989d";
    columns[9].lpszKey = L"stamt";
    columns[9].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[9].iID = 10;

    columns[10].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[10].lpNext = &columns[11];
    columns[10].lpszLabel = L"\u6362\u624b\u7387";
    columns[10].lpszKey = L"stturnover";
    columns[10].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[10].iID = 11;

    columns[11].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[11].lpNext = &columns[12];
    columns[11].lpszLabel = L"\u5e02\u573a";
    columns[11].lpszKey = L"stmarket";
    columns[11].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[11].iID = 12;

    columns[12].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[12].lpNext = &columns[13];
    columns[12].lpszLabel = L"\u5e02\u76c8\u7387";
    columns[12].lpszKey = L"stpe";
    columns[12].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[12].iID = 13;

    columns[13].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[13].lpNext = &columns[14];
    columns[13].lpszLabel = L"\u5e02\u51c0\u7387";
    columns[13].lpszKey = L"stpb";
    columns[13].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[13].iID = 14;

    columns[14].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[14].lpNext = &columns[15];
    columns[14].lpszLabel = L"\u603b\u5e02\u503c";
    columns[14].lpszKey = L"stmktcap";
    columns[14].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[14].iID = 15;

    columns[15].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[15].lpNext = &columns[16];
    columns[15].lpszLabel = L"\u6d41\u901a\u5e02\u503c";
    columns[15].lpszKey = L"stnmc";
    columns[15].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[15].iID = 16;

    columns[16].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[16].lpNext = &columns[17];
    columns[16].lpszLabel = L"\u80a1\u7968\u6570";
    columns[16].lpszKey = L"ststockcount";
    columns[16].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[16].iID = 17;

    columns[17].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[17].lpNext = &columns[18];
    columns[17].lpszLabel = L"\u9886\u6da8\u80a1";
    columns[17].lpszKey = L"stleadstock";
    columns[17].dwFlags = VFSCCF_LEFTJUSTIFY;
    columns[17].iID = 18;

    columns[18].cbSize = sizeof(VFSCUSTOMCOLUMNW);
    columns[18].lpNext = NULL;
    columns[18].lpszLabel = L"\u9886\u6da8\u5e45";
    columns[18].lpszKey = L"stleadchg";
    columns[18].dwFlags = VFSCCF_RIGHTJUSTIFY;
    columns[18].iID = 19;

    return columns;
}

extern "C" __declspec(dllexport) int VFS_ReadDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                         LPVFSREADDIRDATAW lpRDD) {
    if (!lpRDD) return FALSE;

    switch (lpRDD->vfsReadOp) {
    case VFSREAD_FREEDIRCLOSE:
    case VFSREAD_FREEDIR:
    case VFSREAD_CHANGEDIR:
        return TRUE;

    case VFSREAD_NORMAL:
    case VFSREAD_REFRESH:
    case VFSREAD_PARENT:
    case VFSREAD_ROOT:
    case VFSREAD_BACK:
    case VFSREAD_FORWARD:
    case VFSREAD_PRINTDIR:
        return InternalReadDirectory(lpRDD);

    default:
        return TRUE;
    }
}

extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                                              LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags) {
    if (!lpszPath || !hHeap || hHeap == INVALID_HANDLE_VALUE) return NULL;

    StockPathInfo pathInfo = ParseStockPath(lpszPath);

    LPVFSFILEDATAHEADER lpFDH = AllocateFileDataHeader(hHeap, 1);
    if (!lpFDH) return NULL;

    LPVFSFILEDATAW lpFileData = (LPVFSFILEDATAW)(lpFDH + 1);

    if (pathInfo.isRoot) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Stock");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isWatchlist && !pathInfo.isStockDir) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u81ea\u9009\u80a1");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isMarket && !pathInfo.isMarketList && pathInfo.symbol.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u5e02\u573a");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isMarketList && pathInfo.symbol.empty()) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH,
                        StockClient::GetMarketName(pathInfo.market).c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isIndexList) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u6307\u6570");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isIndex && pathInfo.isStockDir && !pathInfo.symbol.empty()) {
        std::vector<IndexQuote> indices = StockClient::ListIndices();
        for (const auto& idx : indices) {
            if (idx.symbol == pathInfo.symbol) {
                std::wstring displayName = idx.name;
                if (idx.currentPoint > 0) {
                    WCHAR buf[16];
                    StringCchPrintfW(buf, 16, L"%.2f", idx.changePercent);
                    std::wstring sign = idx.changePercent >= 0 ? L"+" : L"";
                    displayName += L" [" + sign + std::wstring(buf) + L"%]";
                }
                StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, displayName.c_str());
                lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
                lpFileData->wfdData.ftLastWriteTime = idx.updateTime;
                FillIndexColumnData(lpFileData, hHeap, idx);
                return lpFDH;
            }
        }
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.symbol.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        return lpFDH;
    }

    if (pathInfo.isSector && !pathInfo.isSectorList && !pathInfo.isSectorStocks) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u677f\u5757");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isSectorList) {
        std::wstring typeName;
        switch (pathInfo.sectorType) {
        case SECTOR_TYPE_INDUSTRY: typeName = L"\u884c\u4e1a"; break;
        case SECTOR_TYPE_CONCEPT: typeName = L"\u6982\u5ff5"; break;
        case SECTOR_TYPE_AREA: typeName = L"\u5730\u57df"; break;
        default: typeName = L"\u677f\u5757"; break;
        }
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, typeName.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isSectorStocks) {
        std::vector<SectorInfo> sectors = StockClient::ListSectors(pathInfo.sectorType);
        for (const auto& sec : sectors) {
            if (sec.code == pathInfo.sectorCode) {
                StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, sec.name.c_str());
                lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
                FillSectorColumnData(lpFileData, hHeap, sec);
                return lpFDH;
            }
        }
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, pathInfo.sectorCode.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        return lpFDH;
    }

    if (pathInfo.isSearch && pathInfo.symbol.empty()) {
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u641c\u7d22");
        } else {
            StringCchPrintfW(lpFileData->wfdData.cFileName, MAX_PATH,
                             L"\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        return lpFDH;
    }

    if (pathInfo.isQuoteFile) {
        std::string utf8;
        if (pathInfo.isIndex) {
            std::vector<IndexQuote> indices = StockClient::ListIndices();
            for (const auto& idx : indices) {
                if (idx.symbol == pathInfo.symbol) {
                    std::wstring content = FormatIndexQuoteContent(idx);
                    utf8 = StockClient::WideToUtf8(content);
                    break;
                }
            }
        } else {
            StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
            std::wstring content = FormatQuoteContent(quote);
            utf8 = StockClient::WideToUtf8(content);
        }

        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"\u5b9e\u65f6\u884c\u60c5.txt");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData->wfdData.nFileSizeLow = (DWORD)utf8.length();
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
        return lpFDH;
    }

    if (pathInfo.isSignalFile) {
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"star.signal");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        lpFileData->wfdData.nFileSizeLow = 0;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
        return lpFDH;
    }

    if (pathInfo.isVirtualFile && pathInfo.metaItem == L"info") {
        StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
        std::wstring content = FormatInfoContent(quote);
        std::string utf8 = StockClient::WideToUtf8(content);

        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"info.txt");
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY;
        lpFileData->wfdData.nFileSizeLow = (DWORD)utf8.length();
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftLastWriteTime);
        return lpFDH;
    }

    if (pathInfo.isStockDir) {
        StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
        std::wstring displayName = GetStockDisplayName(quote);
        StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, displayName.c_str());
        lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        GetSystemTimeAsFileTime(&lpFileData->wfdData.ftCreationTime);
        lpFileData->wfdData.ftLastWriteTime = quote.updateTime;
        FillStockColumnData(lpFileData, hHeap, quote);
        return lpFDH;
    }

    StringCchCopyW(lpFileData->wfdData.cFileName, MAX_PATH, L"Unknown");
    lpFileData->wfdData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    return lpFDH;
}

extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                         LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr,
                                                         DWORD dwFlags, LPFILETIME lpFT) {
    if (!IsStockVfsPath(lpszFile)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return NULL;
    }

    StockPathInfo pathInfo = ParseStockPath(lpszFile);

    bool isWrite = (dwMode & GENERIC_WRITE) != 0;
    StockContentInfo* ctx = new StockContentInfo();
    ctx->readPos = 0;
    ctx->isWrite = isWrite;
    ctx->symbol = pathInfo.symbol;

    if (!isWrite) {
        std::wstring content;

        if (pathInfo.isQuoteFile) {
            if (pathInfo.isIndex) {
                std::vector<IndexQuote> indices = StockClient::ListIndices();
                bool found = false;
                for (const auto& idx : indices) {
                    if (idx.symbol == pathInfo.symbol) {
                        content = FormatIndexQuoteContent(idx);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    content = L"\u65e0\u6cd5\u83b7\u53d6\u6307\u6570\u6570\u636e\r\n";
                }
            } else {
                StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
                content = FormatQuoteContent(quote);
            }
            ctx->fileType = L"quote";
        } else if (pathInfo.isSignalFile) {
            content = L"";
            ctx->fileType = L"signal";
        } else if (pathInfo.isVirtualFile && pathInfo.metaItem == L"info") {
            StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
            content = FormatInfoContent(quote);
            ctx->fileType = L"info";
        } else {
            delete ctx;
            SetLastError(ERROR_FILE_NOT_FOUND);
            return NULL;
        }

        std::string utf8 = StockClient::WideToUtf8(content);
        ctx->data.assign(utf8.begin(), utf8.end());
    }

    return (HANDLE)ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                    HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize) {
    StockContentInfo* ctx = (StockContentInfo*)hFile;
    if (!ctx || ctx->isWrite) return FALSE;

    if (lpdwReadSize) *lpdwReadSize = 0;
    if (ctx->readPos >= ctx->data.size()) return TRUE;

    DWORD bytesToRead = min(dwSize, (DWORD)(ctx->data.size() - ctx->readPos));
    if (bytesToRead > 0 && lpData) {
        CopyMemory(lpData, ctx->data.data() + ctx->readPos, bytesToRead);
        ctx->readPos += bytesToRead;
        if (lpdwReadSize) *lpdwReadSize = bytesToRead;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush,
                                                     LPDWORD lpdwWriteSize) {
    StockContentInfo* ctx = (StockContentInfo*)hFile;
    if (!ctx || !ctx->isWrite) return FALSE;

    if (lpdwWriteSize) *lpdwWriteSize = 0;
    if (dwSize == 0 || !lpData) return TRUE;

    size_t newSize = ctx->readPos + dwSize;
    if (newSize > ctx->data.size()) ctx->data.resize(newSize);
    CopyMemory(ctx->data.data() + ctx->readPos, lpData, dwSize);
    ctx->readPos += dwSize;
    if (lpdwWriteSize) *lpdwWriteSize = dwSize;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                    HANDLE hFile, __int64 iPos, DWORD dwMethod,
                                                    DWORD dwFlags, unsigned __int64* piNewPos) {
    StockContentInfo* ctx = (StockContentInfo*)hFile;
    if (!ctx) return FALSE;

    __int64 newPos = 0;
    switch (dwMethod) {
    case FILE_BEGIN: newPos = iPos; break;
    case FILE_CURRENT: newPos = ctx->readPos + iPos; break;
    case FILE_END: newPos = ctx->data.size() + iPos; break;
    default: return FALSE;
    }

    if (newPos < 0) return FALSE;

    if (ctx->isWrite) {
        if ((size_t)newPos > ctx->data.size()) ctx->data.resize((size_t)newPos);
        ctx->readPos = (size_t)newPos;
    } else {
        ctx->readPos = (size_t)min((size_t)newPos, ctx->data.size());
    }

    if (piNewPos) *piNewPos = (unsigned __int64)ctx->readPos;
    return TRUE;
}

extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile) {
    StockContentInfo* ctx = (StockContentInfo*)hFile;
    if (!ctx) return;

    if (ctx->isWrite && ctx->fileType == L"signal" && !ctx->symbol.empty()) {
        StockClient::AddToWatchlist(ctx->symbol, L"");
    }

    delete ctx;
}

extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile) {
    if (!IsStockVfsPath(lpszFile)) return FALSE;

    StockPathInfo pathInfo = ParseStockPath(lpszFile);
    if (pathInfo.isSignalFile && pathInfo.metaItem == L"star" && !pathInfo.symbol.empty()) {
        return StockClient::RemoveFromWatchlist(pathInfo.symbol);
    }

    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                             LPWSTR lpszPath, DWORD dwFlags) {
    if (!IsStockVfsPath(lpszPath)) return FALSE;

    StockPathInfo pathInfo = ParseStockPath(lpszPath);
    if (pathInfo.isSignalFile && pathInfo.metaItem == L"star" && !pathInfo.symbol.empty()) {
        return StockClient::AddToWatchlist(pathInfo.symbol, L"");
    }

    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                       LPWSTR lpszOldName, LPWSTR lpszNewName) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                     LPWSTR lpszOldName, LPWSTR lpszNewName, DWORD dwFlags) {
    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hVFSData, LPWSTR lpszPath,
                                                               LPWSTR lpszDisplayName, int cchDisplayNameMax) {
    if (!lpszPath || !lpszDisplayName) return FALSE;

    StockPathInfo pathInfo = ParseStockPath(lpszPath);

    if (pathInfo.isRoot) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"Stock");
        return TRUE;
    }
    if (pathInfo.isWatchlist && !pathInfo.isStockDir) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u81ea\u9009\u80a1");
        return TRUE;
    }
    if (pathInfo.isMarket && !pathInfo.isMarketList && pathInfo.symbol.empty()) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u5e02\u573a");
        return TRUE;
    }
    if (pathInfo.isMarketList && pathInfo.symbol.empty()) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax,
                        StockClient::GetMarketName(pathInfo.market).c_str());
        return TRUE;
    }
    if (pathInfo.isIndexList) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u6307\u6570");
        return TRUE;
    }
    if (pathInfo.isIndex && pathInfo.isStockDir) {
        std::vector<IndexQuote> indices = StockClient::ListIndices();
        for (const auto& idx : indices) {
            if (idx.symbol == pathInfo.symbol) {
                StringCchCopyW(lpszDisplayName, cchDisplayNameMax, idx.name.c_str());
                return TRUE;
            }
        }
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, pathInfo.symbol.c_str());
        return TRUE;
    }
    if (pathInfo.isSector && !pathInfo.isSectorList && !pathInfo.isSectorStocks) {
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u677f\u5757");
        return TRUE;
    }
    if (pathInfo.isSectorList) {
        switch (pathInfo.sectorType) {
        case SECTOR_TYPE_INDUSTRY: StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u884c\u4e1a"); return TRUE;
        case SECTOR_TYPE_CONCEPT: StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u6982\u5ff5"); return TRUE;
        case SECTOR_TYPE_AREA: StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u5730\u57df"); return TRUE;
        default: break;
        }
    }
    if (pathInfo.isSectorStocks) {
        std::vector<SectorInfo> sectors = StockClient::ListSectors(pathInfo.sectorType);
        for (const auto& sec : sectors) {
            if (sec.code == pathInfo.sectorCode) {
                StringCchCopyW(lpszDisplayName, cchDisplayNameMax, sec.name.c_str());
                return TRUE;
            }
        }
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, pathInfo.sectorCode.c_str());
        return TRUE;
    }
    if (pathInfo.isSearch && pathInfo.symbol.empty()) {
        if (pathInfo.searchQuery.empty()) {
            StringCchCopyW(lpszDisplayName, cchDisplayNameMax, L"\u641c\u7d22");
        } else {
            StringCchPrintfW(lpszDisplayName, cchDisplayNameMax,
                             L"\u641c\u7d22: %s", pathInfo.searchQuery.c_str());
        }
        return TRUE;
    }
    if (pathInfo.isStockDir) {
        StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
        std::wstring displayName = GetStockDisplayName(quote);
        StringCchCopyW(lpszDisplayName, cchDisplayNameMax, displayName.c_str());
        return TRUE;
    }

    return FALSE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hVFSData, LPWSTR lpszPath, BOOL fRoot,
                                                              LPWSTR lpszParentRoot, int cchParentRootMax) {
    if (!lpszPath || !lpszParentRoot) return FALSE;
    StringCchCopyW(lpszParentRoot, cchParentRootMax, STOCK_VFS_PREFIX);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                           LPWSTR lpszPath, LPVFSFUNCDATA lpListData,
                                                           LPVFSCONTEXTMENUDATAW lpContextMenuData) {
    if (!lpContextMenuData) return FALSE;
    lpContextMenuData->fAllowContextMenu = TRUE;
    lpContextMenuData->fDefaultContextMenu = TRUE;
    return TRUE;
}

extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                       LPVFSCONTEXTVERBDATAW lpContextVerbData) {
    if (!lpContextVerbData) return VFSCVRES_DEFAULT;
    return VFSCVRES_DEFAULT;
}

extern "C" __declspec(dllexport) HWND VFS_PropertiesW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                       LPWSTR lpszPath, HWND hWndParent) {
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hVFSData, vfsProperty propId, LPVOID lpPropData,
                                                    DWORD dwPropDataSize) {
    if (!lpPropData) return FALSE;

    switch (propId) {
    case VFSPROP_FUNCAVAILABILITY: {
        unsigned __int64* pAvail = (unsigned __int64*)lpPropData;
        *pAvail = VFSFUNCAVAIL_COPY | VFSFUNCAVAIL_DELETE | VFSFUNCAVAIL_MAKEDIR |
                  VFSFUNCAVAIL_PROPERTIES | VFSFUNCAVAIL_SELECTALL | VFSFUNCAVAIL_SELECTNONE;
        return TRUE;
    }
    case VFSPROP_GETFOLDERICON: {
        HICON* phIcon = (HICON*)lpPropData;
        ExtractIconExW(L"shell32.dll", 14, phIcon, NULL, 1);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

extern "C" __declspec(dllexport) BOOL VFS_GetFileSizeW(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData,
                                                        LPWSTR lpszPath, unsigned __int64* piSize) {
    if (!lpszPath || !piSize) return FALSE;

    StockPathInfo pathInfo = ParseStockPath(lpszPath);

    if (pathInfo.isQuoteFile) {
        if (pathInfo.isIndex) {
            std::vector<IndexQuote> indices = StockClient::ListIndices();
            for (const auto& idx : indices) {
                if (idx.symbol == pathInfo.symbol) {
                    std::wstring content = FormatIndexQuoteContent(idx);
                    std::string utf8 = StockClient::WideToUtf8(content);
                    *piSize = utf8.length();
                    return TRUE;
                }
            }
            *piSize = 0;
            return TRUE;
        }
        StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
        std::wstring content = FormatQuoteContent(quote);
        std::string utf8 = StockClient::WideToUtf8(content);
        *piSize = utf8.length();
        return TRUE;
    }

    if (pathInfo.isSignalFile) {
        *piSize = 0;
        return TRUE;
    }

    if (pathInfo.isVirtualFile && pathInfo.metaItem == L"info") {
        StockQuote quote = StockClient::GetQuote(pathInfo.symbol);
        std::wstring content = FormatInfoContent(quote);
        std::string utf8 = StockClient::WideToUtf8(content);
        *piSize = utf8.length();
        return TRUE;
    }

    return FALSE;
}

extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hVFSData) {
    return (long)GetLastError();
}

// ============================================================================
// Configuration Dialog
// ============================================================================

static const int PAGE_GENERAL = 0;
static const int PAGE_API     = 1;
static const int PAGE_PROXY   = 2;

// Controls belonging to each page (for show/hide)
static const int g_page0Ids[] = {
    IDC_STOCK_DATASOURCE, IDC_STOCK_LBL_SOURCE,
    IDC_STOCK_LBL_CACHE, IDC_STOCK_CACHE_TIMEOUT,
    IDC_STOCK_LBL_REFRESH, IDC_STOCK_REFRESH,
    IDC_STOCK_CHK_COLOR, IDC_STOCK_CHK_VOLUME,
    IDC_STOCK_LBL_PERPAGE, IDC_STOCK_ITEMS_PER_PAGE,
    IDC_STOCK_LBL_METAPFX, IDC_STOCK_METAPREFIX
};
static const int g_page1Ids[] = {
    IDC_STOCK_LBL_TUSHARE, IDC_STOCK_TUSHARE_TOKEN,
    IDC_STOCK_LBL_ALPHA, IDC_STOCK_ALPHA_KEY
};
static const int g_page2Ids[] = {
    IDC_STOCK_PROXY_TYPE, IDC_STOCK_LBL_PROXY,
    IDC_STOCK_LBL_HOST, IDC_STOCK_PROXY_HOST,
    IDC_STOCK_LBL_PORT, IDC_STOCK_PROXY_PORT
};

static void ShowConfigPage(HWND hDlg, int page) {
    const int* pages[] = { g_page0Ids, g_page1Ids, g_page2Ids };
    const int counts[] = { _countof(g_page0Ids), _countof(g_page1Ids), _countof(g_page2Ids) };

    for (int p = 0; p < 3; p++) {
        int cmdShow = (p == page) ? SW_SHOW : SW_HIDE;
        for (int i = 0; i < counts[p]; i++) {
            HWND hCtl = GetDlgItem(hDlg, pages[p][i]);
            if (hCtl) ShowWindow(hCtl, cmdShow);
        }
    }
}

static void SetStockChineseText(HWND hDlg) {
    SetWindowTextW(hDlg, L"Stock VFS \u914d\u7f6e");

    SetDlgItemTextW(hDlg, IDOK, L"\u786e\u5b9a");
    SetDlgItemTextW(hDlg, IDCANCEL, L"\u53d6\u6d88");
    SetDlgItemTextW(hDlg, IDC_STOCK_APPLY, L"\u5e94\u7528");
    SetDlgItemTextW(hDlg, IDC_STOCK_RESET, L"\u91cd\u7f6e");

    // Page 0 labels
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_SOURCE,  L"\u6570\u636e\u6e90:");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_CACHE,   L"\u7f13\u5b58\u8d85\u65f6 (\u79d2):");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_REFRESH, L"\u81ea\u52a8\u5237\u65b0\u95f4\u9694 (\u79d2, 0=\u5173\u95ed):");
    SetDlgItemTextW(hDlg, IDC_STOCK_CHK_COLOR,   L"\u663e\u793a\u6da8\u8dcc\u989c\u8272");
    SetDlgItemTextW(hDlg, IDC_STOCK_CHK_VOLUME,  L"\u540d\u79f0\u4e2d\u663e\u793a\u6210\u4ea4\u91cf");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_PERPAGE, L"\u6bcf\u9875\u663e\u793a\u6570:");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_METAPFX, L"\u5143\u6570\u636e\u524d\u7f00:");

    // Page 1 labels
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_TUSHARE, L"Tushare Token:");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_ALPHA,   L"Alpha Vantage API Key:");

    // Page 2 labels
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_PROXY,   L"\u4ee3\u7406\u7c7b\u578b:");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_HOST,    L"\u4ee3\u7406\u5730\u5740:");
    SetDlgItemTextW(hDlg, IDC_STOCK_LBL_PORT,    L"\u4ee3\u7406\u7aef\u53e3:");
}

static void InitStockDialogControls(HWND hDlg) {
    StockClient::LoadConfig();
    StockConfig& cfg = StockClient::GetConfig();

    // Nav listbox (owner-draw, add empty items)
    HWND hNav = GetDlgItem(hDlg, IDC_STOCK_NAV_LIST);
    SendMessageW(hNav, LB_SETITEMHEIGHT, 0, 24);
    SendMessageW(hNav, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 3; i++) SendMessageW(hNav, LB_ADDSTRING, 0, (LPARAM)L"");
    SendMessageW(hNav, LB_SETCURSEL, 0, 0);

    // Data source combo
    HWND hDS = GetDlgItem(hDlg, IDC_STOCK_DATASOURCE);
    SendMessageW(hDS, CB_ADDSTRING, 0, (LPARAM)L"\u65b0\u6d6a\u8d22\u7ecf");
    SendMessageW(hDS, CB_ADDSTRING, 0, (LPARAM)L"\u4e1c\u65b9\u8d22\u5bcc");
    SendMessageW(hDS, CB_ADDSTRING, 0, (LPARAM)L"Tushare");
    SendMessageW(hDS, CB_ADDSTRING, 0, (LPARAM)L"AlphaVantage");
    SendMessageW(hDS, CB_SETCURSEL, (WPARAM)cfg.dataSource, 0);

    // Cache timeout
    SetDlgItemInt(hDlg, IDC_STOCK_CACHE_TIMEOUT, cfg.cacheTimeout, FALSE);
    // Refresh interval
    SetDlgItemInt(hDlg, IDC_STOCK_REFRESH, cfg.refreshInterval, FALSE);
    // Checkboxes
    CheckDlgButton(hDlg, IDC_STOCK_CHK_COLOR, cfg.showChangeColor ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_STOCK_CHK_VOLUME, cfg.showVolumeInName ? BST_CHECKED : BST_UNCHECKED);
    // Items per page
    SetDlgItemInt(hDlg, IDC_STOCK_ITEMS_PER_PAGE, cfg.itemsPerPage, FALSE);
    // Meta prefix
    SetDlgItemTextW(hDlg, IDC_STOCK_METAPREFIX, cfg.metaPrefix.c_str());

    // Page 1: API keys
    SetDlgItemTextW(hDlg, IDC_STOCK_TUSHARE_TOKEN, cfg.tushareToken.c_str());
    SetDlgItemTextW(hDlg, IDC_STOCK_ALPHA_KEY, cfg.alphaVantageKey.c_str());

    // Page 2: Proxy
    HWND hPT = GetDlgItem(hDlg, IDC_STOCK_PROXY_TYPE);
    SendMessageW(hPT, CB_ADDSTRING, 0, (LPARAM)L"\u65e0\u4ee3\u7406");
    SendMessageW(hPT, CB_ADDSTRING, 0, (LPARAM)L"HTTP");
    SendMessageW(hPT, CB_ADDSTRING, 0, (LPARAM)L"SOCKS5");
    SendMessageW(hPT, CB_SETCURSEL, (WPARAM)cfg.proxyType, 0);
    SetDlgItemTextW(hDlg, IDC_STOCK_PROXY_HOST, cfg.proxyHost.c_str());
    SetDlgItemInt(hDlg, IDC_STOCK_PROXY_PORT, cfg.proxyPort, FALSE);

    // Show page 0
    ShowConfigPage(hDlg, PAGE_GENERAL);
}

static void ResetStockDialogControls(HWND hDlg) {
    StockConfig defaults;
    defaults.dataSource = STOCK_SOURCE_SINA;
    defaults.tushareToken = L"";
    defaults.alphaVantageKey = L"";
    defaults.cacheTimeout = 60;
    defaults.refreshInterval = 0;
    defaults.showChangeColor = true;
    defaults.showVolumeInName = false;
    defaults.itemsPerPage = 50;
    defaults.metaPrefix = L".st";
    defaults.proxyType = STOCK_PROXY_NONE;
    defaults.proxyHost = L"";
    defaults.proxyPort = 0;

    SendDlgItemMessageW(hDlg, IDC_STOCK_DATASOURCE, CB_SETCURSEL, (WPARAM)defaults.dataSource, 0);
    SetDlgItemInt(hDlg, IDC_STOCK_CACHE_TIMEOUT, defaults.cacheTimeout, FALSE);
    SetDlgItemInt(hDlg, IDC_STOCK_REFRESH, defaults.refreshInterval, FALSE);
    CheckDlgButton(hDlg, IDC_STOCK_CHK_COLOR, defaults.showChangeColor ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_STOCK_CHK_VOLUME, defaults.showVolumeInName ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_STOCK_ITEMS_PER_PAGE, defaults.itemsPerPage, FALSE);
    SetDlgItemTextW(hDlg, IDC_STOCK_METAPREFIX, defaults.metaPrefix.c_str());
    SetDlgItemTextW(hDlg, IDC_STOCK_TUSHARE_TOKEN, defaults.tushareToken.c_str());
    SetDlgItemTextW(hDlg, IDC_STOCK_ALPHA_KEY, defaults.alphaVantageKey.c_str());
    SendDlgItemMessageW(hDlg, IDC_STOCK_PROXY_TYPE, CB_SETCURSEL, (WPARAM)defaults.proxyType, 0);
    SetDlgItemTextW(hDlg, IDC_STOCK_PROXY_HOST, defaults.proxyHost.c_str());
    SetDlgItemInt(hDlg, IDC_STOCK_PROXY_PORT, defaults.proxyPort, FALSE);
}

static bool SaveStockDialogControls(HWND hDlg) {
    StockConfig& cfg = StockClient::GetConfig();

    // Data source
    cfg.dataSource = (StockDataSource)SendDlgItemMessageW(hDlg, IDC_STOCK_DATASOURCE, CB_GETCURSEL, 0, 0);

    // Cache timeout
    BOOL translated = FALSE;
    int val = GetDlgItemInt(hDlg, IDC_STOCK_CACHE_TIMEOUT, &translated, FALSE);
    if (translated && val >= 0) cfg.cacheTimeout = val;

    // Refresh interval
    val = GetDlgItemInt(hDlg, IDC_STOCK_REFRESH, &translated, FALSE);
    if (translated && val >= 0) cfg.refreshInterval = val;

    // Checkboxes
    cfg.showChangeColor = (IsDlgButtonChecked(hDlg, IDC_STOCK_CHK_COLOR) == BST_CHECKED);
    cfg.showVolumeInName = (IsDlgButtonChecked(hDlg, IDC_STOCK_CHK_VOLUME) == BST_CHECKED);

    // Items per page
    val = GetDlgItemInt(hDlg, IDC_STOCK_ITEMS_PER_PAGE, &translated, FALSE);
    if (translated && val > 0) cfg.itemsPerPage = val;

    // Meta prefix
    WCHAR bufMeta[64] = {};
    GetDlgItemTextW(hDlg, IDC_STOCK_METAPREFIX, bufMeta, _countof(bufMeta));
    cfg.metaPrefix = bufMeta;

    // API keys
    WCHAR bufToken[256] = {};
    GetDlgItemTextW(hDlg, IDC_STOCK_TUSHARE_TOKEN, bufToken, _countof(bufToken));
    cfg.tushareToken = bufToken;

    WCHAR bufKey[256] = {};
    GetDlgItemTextW(hDlg, IDC_STOCK_ALPHA_KEY, bufKey, _countof(bufKey));
    cfg.alphaVantageKey = bufKey;

    // Proxy
    cfg.proxyType = (StockProxyType)SendDlgItemMessageW(hDlg, IDC_STOCK_PROXY_TYPE, CB_GETCURSEL, 0, 0);

    WCHAR bufHost[256] = {};
    GetDlgItemTextW(hDlg, IDC_STOCK_PROXY_HOST, bufHost, _countof(bufHost));
    cfg.proxyHost = bufHost;

    val = GetDlgItemInt(hDlg, IDC_STOCK_PROXY_PORT, &translated, FALSE);
    if (translated && val >= 0) cfg.proxyPort = val;

    return StockClient::SaveConfig();
}

INT_PTR CALLBACK StockConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SetStockChineseText(hDlg);
        InitStockDialogControls(hDlg);
        return TRUE;

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlType == ODT_LISTBOX && lpmis->CtlID == IDC_STOCK_NAV_LIST)
            lpmis->itemHeight = 24;
    }
    return (INT_PTR)TRUE;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlType == ODT_LISTBOX && lpdis->CtlID == IDC_STOCK_NAV_LIST) {
            static const WCHAR* navLabels[] = {
                L"\u5e38\u89c4\u8bbe\u7f6e",
                L"API\u5bc6\u94a5",
                L"\u4ee3\u7406\u8bbe\u7f6e"
            };
            int idx = (int)lpdis->itemID;
            if (idx >= 0 && idx < 3) {
                BOOL selected = (lpdis->itemState & ODS_SELECTED);
                HBRUSH hBrush = selected ? CreateSolidBrush(RGB(0, 120, 215)) : GetSysColorBrush(COLOR_WINDOW);
                FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
                if (selected) DeleteObject(hBrush);
                SetTextColor(lpdis->hDC, selected ? RGB(255, 255, 255) : GetSysColor(COLOR_WINDOWTEXT));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RECT rcText = lpdis->rcItem;
                rcText.left += 4;
                rcText.right -= 4;
                DrawTextW(lpdis->hDC, navLabels[idx], -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }
    return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            SaveStockDialogControls(hDlg);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        case IDC_STOCK_APPLY:
            SaveStockDialogControls(hDlg);
            return TRUE;

        case IDC_STOCK_RESET:
            ResetStockDialogControls(hDlg);
            return TRUE;

        case IDC_STOCK_NAV_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                int sel = (int)SendDlgItemMessageW(hDlg, IDC_STOCK_NAV_LIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel <= 2) {
                    ShowConfigPage(hDlg, sel);
                }
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

extern "C" __declspec(dllexport) HWND VFS_Configure(HWND hWndParent, HWND hWndNotify, DWORD dwNotifyData) {
    g_hWndNotify = hWndNotify;
    g_dwNotifyData = dwNotifyData;
    DialogBoxParamW(g_hModule, MAKEINTRESOURCEW(IDD_STOCK_CONFIG), hWndParent, StockConfigProc, 0);
    return NULL;
}

extern "C" __declspec(dllexport) HWND VFS_About(HWND hWndParent) {
    MessageBoxW(hWndParent,
                L"Stock VFS \u63d2\u4ef6 v1.0.0\n\n"
                L"(c) 2026\n\n"
                L"\u80a1\u7968\u865a\u62df\u6587\u4ef6\u7cfb\u7edf\n\n"
                L"\u529f\u80fd\u7279\u6027:\n"
                L"  \u2022 \u81ea\u9009\u80a1\u6d4f\u89c8\n"
                L"  \u2022 \u5b9e\u65f6\u884c\u60c5\u67e5\u770b\n"
                L"  \u2022 \u80a1\u7968\u641c\u7d22\n"
                L"  \u2022 \u591a\u5e02\u573a\u652f\u6301 (A\u80a1/\u6e2f\u80a1/\u7f8e\u80a1)\n"
                L"  \u2022 \u81ea\u5b9a\u4e49\u5217\u663e\u793a\n"
                L"  \u2022 \u7f13\u5b58\u652f\u6301",
                L"\u5173\u4e8e Stock VFS",
                MB_OK | MB_ICONINFORMATION);
    return NULL;
}

extern "C" __declspec(dllexport) BOOL VFS_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData) {
    if (pUSBSafeData) {
        pUSBSafeData->pszOtherExports[0] = L'\0';
    }
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
