/* Maya IFF plugin interface
 * Copyright (C) 2008 Leo Davidson
 * (email: leo@ox.compsoc.net, WWW: http://www.pretentiousname.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#pragma once

extern "C"
{
	__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
	__declspec(dllexport) void DVP_Uninit(void);
	__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);
	__declspec(dllexport) BOOL DVP_IdentifyW(LPVIEWERPLUGININFO lpVPInfo);
	__declspec(dllexport) BOOL DVP_IdentifyFileBytesW(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapW(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent);
	__declspec(dllexport) HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags);
};

inline void writeFileInfoInfoLine(VIEWERPLUGINFILEINFO *pInfo, const wchar_t *szTypeName)
{
	if (NULL != pInfo && NULL != pInfo->lpszInfo)
	{
		_sntprintf_s(pInfo->lpszInfo, pInfo->cchInfoMax, _TRUNCATE, _T("%ld x %ld x %d %s"), pInfo->szImageSize.cx, pInfo->szImageSize.cy, pInfo->iNumBits, szTypeName);
	}
}
