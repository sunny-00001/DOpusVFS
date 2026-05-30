#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    std::wstring wide(len, '\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

static std::wstring GbkToWide(const std::string& gbk) {
    if (gbk.empty()) return L"";
    int len = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), NULL, 0);
    std::wstring wide(len, '\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.length(), &wide[0], len);
    return wide;
}

static std::string SendHttpGet(const std::wstring& host, const std::wstring& path, int port = 80, bool https = false) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
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

    std::wstring headers = L"Referer: https://finance.sina.com.cn\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResult = WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

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
    std::string numStr;
    bool hasDot = false;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-' || (json[pos] == '.' && !hasDot))) {
        if (json[pos] == '.') hasDot = true;
        numStr += json[pos];
        pos++;
    }
    return numStr.empty() ? 0.0 : atof(numStr.c_str());
}

int main() {
    FILE* out = nullptr;
    fopen_s(&out, "d:\\vfs\\test_api_verify.txt", "wb");
    if (!out) { printf("Cannot open output file\n"); return 1; }

    fprintf(out, "=== API Verification Test ===\n\n");

    // Test 1: A股列表API - 检查per/pb/mktcap/nmc字段
    fprintf(out, "--- Test 1: A股列表API额外字段 ---\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node=hs_a&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            size_t arrStart = response.find('[');
            if (arrStart == std::string::npos) {
                fprintf(out, "ERROR: No array in response\n");
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
                    double turnoverratio = ExtractJsonDouble(obj, "turnoverratio");

                    fprintf(out, "  [%d] symbol=%s name=%s\n", count, symbol.c_str(), name.c_str());
                    fprintf(out, "       per(PE)=%.2f pb=%.2f mktcap=%.0f nmc=%.0f turnoverratio=%.2f\n",
                        per, pb, mktcap, nmc, turnoverratio);

                    pos = objEnd + 1;
                    count++;
                }
            }
        }
    }

    // Test 2: 国内指数行情
    fprintf(out, "\n--- Test 2: 国内指数行情 ---\n");
    {
        std::string response = SendHttpGet(L"hq.sinajs.cn", L"/list=s_sh000001,s_sz399001,s_sz399006,s_sh000016,s_sh000300,s_sh000905", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;
                std::string varName = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                if (value.size() >= 2 && value[0] == '"') {
                    value = value.substr(1);
                    if (!value.empty() && value.back() == '"') value.pop_back();
                    if (!value.empty() && value.back() == ';') value.pop_back();
                }
                fprintf(out, "  %s = %s\n", varName.c_str(), value.c_str());
            }
        }
    }

    // Test 3: 全球指数行情
    fprintf(out, "\n--- Test 3: 全球指数行情 ---\n");
    {
        std::string response = SendHttpGet(L"hq.sinajs.cn", L"/list=int_dji,int_nasdaq,int_sp500,int_ftse,int_nikkei,int_hangseng", 80, false);
        if (response.empty()) {
            fprintf(out, "ERROR: Empty response\n");
        } else {
            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;
                std::string varName = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                if (value.size() >= 2 && value[0] == '"') {
                    value = value.substr(1);
                    if (!value.empty() && value.back() == '"') value.pop_back();
                    if (!value.empty() && value.back() == ';') value.pop_back();
                }
                fprintf(out, "  %s = %s\n", varName.c_str(), value.c_str());
            }
        }
    }

    // Test 4: 行业板块列表
    fprintf(out, "\n--- Test 4: 行业板块列表API ---\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=5&sort=changepercent&asc=0&node=hangye_zjh&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "  hangye_zjh response length: %zu\n", response.length());
        if (!response.empty() && response.find('[') != std::string::npos) {
            fprintf(out, "  First 500 chars: %.500s\n", response.c_str());
        } else {
            fprintf(out, "  Response: %.300s\n", response.c_str());
        }
    }

    // Test 5: 新浪行业分类列表页面
    fprintf(out, "\n--- Test 5: 新浪行业分类页面 ---\n");
    {
        std::wstring path = L"/quotes/view/newflhy.php";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "  Response length: %zu\n", response.length());
        if (!response.empty()) {
            size_t pos = 0;
            int found = 0;
            while (found < 10) {
                size_t nodePos = response.find("node=", pos);
                if (nodePos == std::string::npos) break;
                size_t endPos = response.find('"', nodePos);
                if (endPos == std::string::npos) break;
                std::string nodeStr = response.substr(nodePos, endPos - nodePos);
                fprintf(out, "  Found: %s\n", nodeStr.c_str());
                pos = endPos + 1;
                found++;
            }
        }
    }

    // Test 6: 申万行业节点列表
    fprintf(out, "\n--- Test 6: 申万行业节点 ---\n");
    {
        const char* swNodes[] = {
            "new_swzz", "new_swjt", "new_swfd", "new_swyh",
            "new_swjsj", "new_swfz", "new_swgf", "new_swjz"
        };
        for (const char* node : swNodes) {
            std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeData?page=1&num=3&sort=changepercent&asc=0&node="
                + Utf8ToWide(node) + L"&_s_r_a=auto";
            std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
            bool hasData = !response.empty() && response.find('[') != std::string::npos && response.find("\"symbol\"") != std::string::npos;
            fprintf(out, "  node=%s: %s\n", node, hasData ? "HAS DATA" : "EMPTY");
        }
    }

    // Test 7: 新浪板块分类树API
    fprintf(out, "\n--- Test 7: 板块分类树API ---\n");
    {
        std::wstring path = L"/quotes_service/api/json_v2.php/Market_Center.getHQNodeTree&_s_r_a=auto";
        std::string response = SendHttpGet(L"vip.stock.finance.sina.com.cn", path, 80, false);
        fprintf(out, "  getHQNodeTree response length: %zu\n", response.length());
        if (!response.empty()) {
            fprintf(out, "  First 1000 chars: %.1000s\n", response.c_str());
        }
    }

    // Test 8: 完整指数行情(32字段格式)
    fprintf(out, "\n--- Test 8: 完整指数行情(32字段) ---\n");
    {
        std::string response = SendHttpGet(L"hq.sinajs.cn", L"/list=sh000001,sz399001,sz399006", 80, false);
        if (!response.empty()) {
            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;
                std::string varName = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                if (value.size() >= 2 && value[0] == '"') {
                    value = value.substr(1);
                    if (!value.empty() && value.back() == '"') value.pop_back();
                    if (!value.empty() && value.back() == ';') value.pop_back();
                }
                int commaCount = 0;
                for (char c : value) if (c == ',') commaCount++;
                fprintf(out, "  %s: %d fields, value=%.200s\n", varName.c_str(), commaCount + 1, value.c_str());
            }
        }
    }

    fclose(out);
    printf("Test complete. Output: d:\\vfs\\test_api_verify.txt\n");
    return 0;
}
