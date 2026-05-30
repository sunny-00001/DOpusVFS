#pragma once

// If DVP_IdentifyFileA is implemented then it will be called instead of DVP_IdentifyFileBytesA.
//  Since DVP_IdentifyFileBytesA makes the most sense for us we don't provide DVP_IdentifyFileA.

// DVP_IdentifyFileStreamA isn't used if DVP_IdentifyFileBytesA is available instead.

extern "C"
{
	__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	__declspec(dllexport) void DVP_Uninit(void);
	__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
	__declspec(dllexport) HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData);
	__declspec(dllexport) HWND DVP_CreateViewer(HWND hWnd,LPRECT lpRc,DWORD dwFlags);

	__declspec(dllexport) BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo);
//	__declspec(dllexport) BOOL DVP_IdentifyFile(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,HANDLE hAbortEvent);
//	__declspec(dllexport) BOOL DVP_IdentifyFileStream(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags);
	__declspec(dllexport) BOOL DVP_IdentifyFileBytes(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags);
	__declspec(dllexport) HBITMAP DVP_LoadBitmap(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapStream(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags);
};

class DOpusPluginHelper;
class CGifTlsData;

CGifTlsData *getGifViewerTlsData();
