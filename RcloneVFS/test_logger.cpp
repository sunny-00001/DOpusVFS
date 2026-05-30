#include <windows.h>
#include <string>
#include <iostream>
#include "../RcloneVFS/Logger.h"

int main() {
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Logger Functionality Test" << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << std::endl;

    InitLogger(LOG_DEBUG);

    std::wcout << L"Testing LogError..." << std::endl;
    LogError(L"This is an error message: %d", 1001);

    std::wcout << L"Testing LogWarn..." << std::endl;
    LogWarn(L"This is a warning message: %s", L"test");

    std::wcout << L"Testing LogInfo..." << std::endl;
    LogInfo(L"This is an info message");

    std::wcout << L"Testing LogDebug..." << std::endl;
    LogDebug(L"This is a debug message with number: %.2f", 3.14159);

    std::wcout << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"All log functions executed successfully" << std::endl;
    std::wcout << L"Please check the log file for output" << std::endl;
    std::wcout << L"========================================" << std::endl;

    return 0;
}
