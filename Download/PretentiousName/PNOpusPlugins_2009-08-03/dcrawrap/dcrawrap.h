#pragma once

// If DVP_IdentifyFile is implemented then it will be called instead of DVP_IdentifyFileBytes.
// DVP_IdentifyFileStream isn't used if DVP_IdentifyFileBytes is available instead.

extern "C"
{
	__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	__declspec(dllexport) void DVP_Uninit(void);
	__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
	__declspec(dllexport) BOOL DVP_IdentifyW(LPVIEWERPLUGININFOW lpVPInfo);
	__declspec(dllexport) HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData);

	__declspec(dllexport) BOOL DVP_IdentifyFileW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,HANDLE hAbortEvent);
	__declspec(dllexport) BOOL DVP_IdentifyFileStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,DWORD dwStreamFlags);

	__declspec(dllexport) HBITMAP DVP_LoadBitmapW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags);
};

extern DCRawResult *CallDCRaw(LeoHelpers::FileAndStream &fas, bool bFromFile, DCRawResult::DCRAW_OPERATION dcrOper, const DCR_RawSettings *prs);
