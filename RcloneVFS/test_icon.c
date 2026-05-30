#include <windows.h>
#include <stdio.h>

int main() {
    HICON hLarge = NULL, hSmall = NULL;
    UINT result = ExtractIconExW(L"shell32.dll", 14, &hLarge, &hSmall, 1);
    printf("ExtractIconExW(shell32.dll, 14) returned: %u\n", result);
    printf("hLarge: %p\n", (void*)hLarge);
    printf("hSmall: %p\n", (void*)hSmall);
    if (hLarge) DestroyIcon(hLarge);
    if (hSmall) DestroyIcon(hSmall);

    hLarge = NULL; hSmall = NULL;
    result = ExtractIconExW(L"shell32.dll", 3, &hLarge, &hSmall, 1);
    printf("\nExtractIconExW(shell32.dll, 3) returned: %u\n", result);
    printf("hLarge: %p\n", (void*)hLarge);
    printf("hSmall: %p\n", (void*)hSmall);
    if (hLarge) DestroyIcon(hLarge);
    if (hSmall) DestroyIcon(hSmall);

    SHFILEINFOW sfi;
    ZeroMemory(&sfi, sizeof(sfi));
    DWORD_PTR r = SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON);
    printf("\nSHGetFileInfoW(SHGFI_SMALLICON) returned: %llu\n", (unsigned long long)r);
    printf("hIcon: %p\n", (void*)sfi.hIcon);
    if (sfi.hIcon) DestroyIcon(sfi.hIcon);

    ZeroMemory(&sfi, sizeof(sfi));
    r = SHGetFileInfoW(L"", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON);
    printf("\nSHGetFileInfoW(SHGFI_LARGEICON) returned: %llu\n", (unsigned long long)r);
    printf("hIcon: %p\n", (void*)sfi.hIcon);
    if (sfi.hIcon) DestroyIcon(sfi.hIcon);

    return 0;
}
