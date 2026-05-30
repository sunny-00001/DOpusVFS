#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define BUFFER_SIZE 4096

int main() {
    HANDLE hSharedBuffer = NULL;
    LPVOID pSharedBuffer = NULL;
    HANDLE hEventBufferReady = NULL;
    HANDLE hEventDataReady = NULL;

    hSharedBuffer = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BUFFER_SIZE, "DBWIN_BUFFER");
    if (!hSharedBuffer) {
        printf("CreateFileMapping failed: %lu\n", GetLastError());
        return 1;
    }

    pSharedBuffer = MapViewOfFile(hSharedBuffer, FILE_MAP_READ, 0, 0, BUFFER_SIZE);
    if (!pSharedBuffer) {
        printf("MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(hSharedBuffer);
        return 1;
    }

    hEventBufferReady = CreateEventA(NULL, FALSE, FALSE, "DBWIN_BUFFER_READY");
    hEventDataReady = CreateEventA(NULL, FALSE, FALSE, "DBWIN_DATA_READY");

    if (!hEventBufferReady || !hEventDataReady) {
        printf("CreateEvent failed: %lu\n", GetLastError());
        return 1;
    }

    printf("Listening for OutputDebugString messages... Press ESC to stop.\n");
    printf("Looking for [RcloneVFS] messages...\n\n");

    while (1) {
        if (_kbhit() && _getch() == 27) break;

        SetEvent(hEventBufferReady);

        DWORD waitResult = WaitForSingleObject(hEventDataReady, 500);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD* pid = (DWORD*)pSharedBuffer;
            char* msg = (char*)pSharedBuffer + sizeof(DWORD);
            if (strstr(msg, "RcloneVFS") || strstr(msg, "rclone")) {
                printf("[PID:%lu] %s", *pid, msg);
            }
        }
    }

    UnmapViewOfFile(pSharedBuffer);
    CloseHandle(hSharedBuffer);
    CloseHandle(hEventBufferReady);
    CloseHandle(hEventDataReady);

    return 0;
}
