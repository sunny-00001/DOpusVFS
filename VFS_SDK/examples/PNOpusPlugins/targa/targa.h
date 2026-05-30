#pragma once
/*
   TARGA.DLL - Directory Opus Sample Viewer Plugin

   (c) Copyright 2009 GP Software
   All Rights Reserved
*/

// Define the exported DLL functions
extern "C"
{
	__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	__declspec(dllexport) void DVP_Uninit(void);
	__declspec(dllexport) BOOL DVP_IdentifyW(LPVIEWERPLUGININFOW lpVPInfo);
	__declspec(dllexport) BOOL DVP_IdentifyFileW(HWND hWnd, LPWSTR lpszName, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, HANDLE hAbortEvent);
	__declspec(dllexport) BOOL DVP_IdentifyFileStreamW(HWND hWnd, LPSTREAM lpStream, LPWSTR lpszName, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, DWORD dwStreamFlags);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapW(HWND hWnd, LPWSTR lpszName, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, LPSIZE lpszSize, HANDLE hAbortEvent);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapStreamW(HWND hWnd, LPSTREAM lpStream, LPWSTR lpszName, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, LPSIZE lpszSize, DWORD dwStreamFlags);
	__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
};

