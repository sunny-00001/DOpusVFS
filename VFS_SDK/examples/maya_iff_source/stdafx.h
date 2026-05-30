#pragma once

#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif

#define WINVER         0x0500	// Win2k and above
#define _WIN32_WINNT   0x0500	// Win2k and above
#define _WIN32_WINDOWS 0x0410	// Win98 and above
#define _WIN32_IE      0x0600	// IE 6.0 and above

#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <Shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(__cplusplus)

#include <string>
#include <vector>
#include <map>
#include <list>
#include <algorithm>

#define VIEWERPLUGINVERSION 4
#define DVPFIF_CanHandleStreams				(1<<0)
#define DVPFIF_CanHandleBytes				(1<<1)
#define DVPFIF_ExtensionsOnly				(1<<4)
#define DVPFIF_OverrideInternal				(1<<15)

#define DVPMajorType_Image 0

typedef struct DOpusViewerPluginInfo
{
	UINT		cbSize;
	DWORD		dwFlags;
	DWORD		dwVersionHigh;
	DWORD		dwVersionLow;
	LPWSTR		lpszHandleExts;
	LPWSTR		lpszName;
	LPWSTR		lpszDescription;
	LPWSTR		lpszCopyright;
	LPWSTR		lpszURL;
	UINT		cchHandleExtsMax;
	UINT		cchNameMax;
	UINT		cchDescriptionMax;
	UINT		cchCopyrightMax;
	UINT		cchURLMax;
	DWORDLONG	dwlMinFileSize;
	DWORDLONG	dwlMaxFileSize;
	DWORDLONG	dwlMinPreviewFileSize;
	DWORDLONG	dwlMaxPreviewFileSize;
	UINT		uiMajorFileType;
	GUID		idPlugin;
	DWORD		dwOpusVerMajor;
	DWORD		dwOpusVerMinor;
	DWORD		dwInitFlags;
	HICON		hIconSmall;
	HICON		hIconLarge;
} VIEWERPLUGININFO, * LPVIEWERPLUGININFO;

#define DVPFIF_CanReturnBitmap			(1<<0)
#define DVPFIF_CanReturnThumbnail		(1<<2)
#define DVPFIF_HasAlphaChannel			(1<<8)
#define DVPFIF_CanReturnFileInfo		(1<<11)

#define DVPColorSpace_RGB 2

typedef struct DOpusViewerPluginFileInfo
{
	UINT		cbSize;
	DWORD		dwFlags;
	WORD		wMajorType;
	WORD		wMinorType;
	SIZE		szImageSize;
	int			iNumBits;
	LPWSTR		lpszInfo;
	UINT		cchInfoMax;
	DWORD		dwPrivateData[8];
	SIZE		szResolution;
	int			iTypeHint;
	COLORREF	crTransparentColor;
	WORD		wThumbnailQuality;
	DWORDLONG	dwlFileSize;
	int			iColorSpace;
} VIEWERPLUGINFILEINFO, * LPVIEWERPLUGINFILEINFO;

typedef struct OpusUSBSafeData
{
	UINT		cbSize;
	LPWSTR		pszOtherExports;
	UINT		cchOtherExports;
} * LPOPUSUSBSAFEDATA;

#define DVPSF_Slow					(1<<0)
#define DVPSF_NoRandomSeek			(1<<1)

typedef struct DVPInitExData
{
	UINT			cbSize;
	HWND			hwndDOpusMsgWindow;
	DWORD			dwOpusVerMajor;
	DWORD			dwOpusVerMinor;
	LPWSTR			pszLanguageName;
} DVPINITEXDATA, * LPDVPINITEXDATA;

#endif // defined(__cplusplus)
