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

int main() {
    StockClient::Init();

    std::wstring path = L"/api/qt/clist/get?pn=1&pz=10&po=1&np=1&fltt=2&invt=2&fid=f3&fs=m:90+t:2+f:!50&fields=f2,f3,f4,f12,f14,f104,f105,f128,f136,f140,f141";
    std::string response = StockClient::SendHttpGet(L"push2.eastmoney.com", path, 80, false);

    FILE* fout = fopen("test_em_sector_via_client.txt", "wb");
    if (fout) {
        fwrite(response.c_str(), 1, response.length(), fout);
        fclose(fout);
    }

    std::wstring wresp = StockClient::Utf8ToWide(response);

    FILE* fout2 = fopen("test_em_sector_via_client_u8.txt", "w, ccs=UTF-8");
    if (fout2) {
        fputws(wresp.c_str(), fout2);
        fclose(fout2);
    }

    printf("Response length: %zu bytes\n", response.length());

    StockClient::Cleanup();
    return 0;
}
