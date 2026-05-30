#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, NULL, NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (https) {
        DWORD optFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &optFlags, sizeof(optFlags));
    }

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResult = WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "ERROR_WINHTTP:" + std::to_string(err);
    }

    DWORD statusCode = 0;
    DWORD szStatusCode = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &szStatusCode, NULL);

    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead);
        if (bytesRead > 0) response.append(buffer.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    pos += searchKey.length();
    size_t endPos = json.find('"', pos);
    if (endPos == std::string::npos) return "";
    return json.substr(pos, endPos - pos);
}

static double ExtractJsonDouble(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0.0;
    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    char* end = nullptr;
    double val = strtod(json.c_str() + pos, &end);
    if (end == json.c_str() + pos) return 0.0;
    return val;
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

int main() {
    FILE* out = nullptr;
    fopen_s(&out, "d:\\vfs\\test_index_sector.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    fprintf(out, "=== Test 1: A\u80a1\u5217\u8868\u989d\u5916\u5b57\u6bb5 (per/pb/mktcap/nmc) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=hs_a&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            size_t arrStart = response.find('[');
            if (arrStart == std::string::npos) {
                fprintf(out, "ERROR: No array found\n");
                fprintf(out, "Response: %.500s\n", response.c_str());
            } else {
                size_t pos = arrStart + 1;
                int count = 0;
                while (pos < response.length() && count < 3) {
                    size_t objStart = response.find('{', pos);
                    if (objStart == std::string::npos) break;
                    size_t objEnd = response.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = response.substr(objStart, objEnd - objStart + 1);
                    std::string symbol = ExtractJsonString(obj, "symbol");
                    std::string name = ExtractJsonString(obj, "name");
                    double per = ExtractJsonDouble(obj, "per");
                    double pb = ExtractJsonDouble(obj, "pb");
                    double mktcap = ExtractJsonDouble(obj, "mktcap");
                    double nmc = ExtractJsonDouble(obj, "nmc");

                    std::wstring wname = DecodeJsonUnicode(name);
                    std::string nameUtf8;
                    nameUtf8.reserve(wname.size() * 3);
                    for (wchar_t c : wname) {
                        if (c < 0x80) nameUtf8 += (char)c;
                        else if (c < 0x800) { nameUtf8 += (char)(0xC0|(c>>6)); nameUtf8 += (char)(0x80|(c&0x3F)); }
                        else { nameUtf8 += (char)(0xE0|(c>>12)); nameUtf8 += (char)(0x80|((c>>6)&0x3F)); nameUtf8 += (char)(0x80|(c&0x3F)); }
                    }

                    fprintf(out, "  [%d] symbol=%s name=%s\n", count, symbol.c_str(), nameUtf8.c_str());
                    fprintf(out, "       per(PE)=%.2f pb=%.2f mktcap=%.0f nmc=%.0f\n", per, pb, mktcap, nmc);

                    pos = objEnd + 1;
                    count++;
                }
            }
        }
    }

    fprintf(out, "\n=== Test 2: \u6307\u6570\u5217\u8868 (gs_s node) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=20&sort=changepercent&asc=0&node=gs_s&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            size_t arrStart = response.find('[');
            if (arrStart == std::string::npos) {
                fprintf(out, "ERROR: No array found\n");
                fprintf(out, "Response: %.2000s\n", response.c_str());
            } else {
                size_t pos = arrStart + 1;
                int count = 0;
                while (pos < response.length() && count < 20) {
                    size_t objStart = response.find('{', pos);
                    if (objStart == std::string::npos) break;
                    size_t objEnd = response.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = response.substr(objStart, objEnd - objStart + 1);
                    std::string symbol = ExtractJsonString(obj, "symbol");
                    std::string name = ExtractJsonString(obj, "name");
                    std::string trade = ExtractJsonString(obj, "trade");
                    double changepercent = ExtractJsonDouble(obj, "changepercent");
                    double pricechange = ExtractJsonDouble(obj, "pricechange");

                    std::wstring wname = DecodeJsonUnicode(name);
                    std::string nameUtf8;
                    nameUtf8.reserve(wname.size() * 3);
                    for (wchar_t c : wname) {
                        if (c < 0x80) nameUtf8 += (char)c;
                        else if (c < 0x800) { nameUtf8 += (char)(0xC0|(c>>6)); nameUtf8 += (char)(0x80|(c&0x3F)); }
                        else { nameUtf8 += (char)(0xE0|(c>>12)); nameUtf8 += (char)(0x80|((c>>6)&0x3F)); nameUtf8 += (char)(0x80|(c&0x3F)); }
                    }

                    fprintf(out, "  [%d] symbol=%s name=%s trade=%s chg%%=%.2f chg=%.2f\n",
                        count, symbol.c_str(), nameUtf8.c_str(), trade.c_str(), changepercent, pricechange);

                    pos = objEnd + 1;
                    count++;
                }
                fprintf(out, "Total items parsed: %d\n", count);
            }
        }
    }

    fprintf(out, "\n=== Test 3: \u6307\u6570\u5b9e\u65f6\u884c\u60c5 (hq.sinajs.cn) ===\n");
    {
        std::wstring path = L"/list=sh000001,sh000300,sz399001,sz399006";
        std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            fprintf(out, "First 2000 chars:\n%.2000s\n", response.c_str());
        }
    }

    fprintf(out, "\n=== Test 4: \u884c\u4e1a\u677f\u5757\u5217\u8868 (newSinaHy.php) ===\n");
    {
        std::string response = SendHttpGet(L"finance.sina.com.cn", L"/q/view/newSinaHy.php", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            std::wstring wresp = GbkToWide(response);
            std::string utf8Resp;
            utf8Resp.reserve(wresp.size() * 3);
            for (wchar_t c : wresp) {
                if (c < 0x80) utf8Resp += (char)c;
                else if (c < 0x800) { utf8Resp += (char)(0xC0|(c>>6)); utf8Resp += (char)(0x80|(c&0x3F)); }
                else { utf8Resp += (char)(0xE0|(c>>12)); utf8Resp += (char)(0x80|((c>>6)&0x3F)); utf8Resp += (char)(0x80|(c&0x3F)); }
            }
            fprintf(out, "First 3000 chars:\n%.3000s\n", utf8Resp.c_str());
        }
    }

    fprintf(out, "\n=== Test 5: \u6982\u5ff5\u677f\u5757\u5217\u8868 (newFLJK.php) ===\n");
    {
        std::string response = SendHttpGet(L"money.finance.sina.com.cn", L"/q/view/newFLJK.php?param=class", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            std::wstring wresp = GbkToWide(response);
            std::string utf8Resp;
            utf8Resp.reserve(wresp.size() * 3);
            for (wchar_t c : wresp) {
                if (c < 0x80) utf8Resp += (char)c;
                else if (c < 0x800) { utf8Resp += (char)(0xC0|(c>>6)); utf8Resp += (char)(0x80|(c&0x3F)); }
                else { utf8Resp += (char)(0xE0|(c>>12)); utf8Resp += (char)(0x80|((c>>6)&0x3F)); utf8Resp += (char)(0x80|(c&0x3F)); }
            }
            fprintf(out, "First 3000 chars:\n%.3000s\n", utf8Resp.c_str());
        }
    }

    fprintf(out, "\n=== Test 6: \u5730\u57df\u677f\u5757\u5217\u8868 (newFLJK.php?param=area) ===\n");
    {
        std::string response = SendHttpGet(L"money.finance.sina.com.cn", L"/q/view/newFLJK.php?param=area", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            std::wstring wresp = GbkToWide(response);
            std::string utf8Resp;
            utf8Resp.reserve(wresp.size() * 3);
            for (wchar_t c : wresp) {
                if (c < 0x80) utf8Resp += (char)c;
                else if (c < 0x800) { utf8Resp += (char)(0xC0|(c>>6)); utf8Resp += (char)(0x80|(c&0x3F)); }
                else { utf8Resp += (char)(0xE0|(c>>12)); utf8Resp += (char)(0x80|((c>>6)&0x3F)); utf8Resp += (char)(0x80|(c&0x3F)); }
            }
            fprintf(out, "First 3000 chars:\n%.3000s\n", utf8Resp.c_str());
        }
    }

    fprintf(out, "\n=== Test 7: \u677f\u5757\u6210\u5206\u80a1 (getHQNodeData node=new_hy) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=new_hy&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            fprintf(out, "First 2000 chars:\n%.2000s\n", response.c_str());
        }
    }

    fprintf(out, "\n=== Test 8: \u677f\u5757\u6210\u5206\u80a1 (getHQNodeData node=hangye_new) ===\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=hangye_new&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else if (response.find("ERROR_WINHTTP:") == 0) {
            fprintf(out, "ERROR: %s\n", response.c_str());
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            fprintf(out, "First 2000 chars:\n%.2000s\n", response.c_str());
        }
    }

    fprintf(out, "\n=== Test 9: \u677f\u5757\u884c\u60c5 (bk_\u524d\u7f00) ===\n");
    {
        std::wstring path = L"/list=bk0428,bk0477,bk0733";
        std::string response = SendHttpGet(L"hq.sinajs.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            fprintf(out, "Response length: %zu\n", response.length());
            fprintf(out, "First 2000 chars:\n%.2000s\n", response.c_str());
        }
    }

    fclose(out);
    printf("Done. Output: d:\\vfs\\test_index_sector.txt\n");
    return 0;
}
