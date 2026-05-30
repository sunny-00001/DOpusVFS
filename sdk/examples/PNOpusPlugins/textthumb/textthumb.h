#pragma once

// If DVP_IdentifyFile is implemented then it will be called instead of DVP_IdentifyFileBytes.
// DVP_IdentifyFileStream isn't used if DVP_IdentifyFileBytes is available instead.

extern "C"
{
	__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	__declspec(dllexport) void DVP_Uninit(void);
	__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
	__declspec(dllexport) HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData);

	__declspec(dllexport) BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo);
	__declspec(dllexport) BOOL DVP_IdentifyFileBytes(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags);
	__declspec(dllexport) HBITMAP DVP_LoadBitmap(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapStream(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags);
};
