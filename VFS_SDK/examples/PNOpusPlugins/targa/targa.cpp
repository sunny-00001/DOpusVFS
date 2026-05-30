/*
   TARGA.DLL - Directory Opus Sample Viewer Plugin

   (c) Copyright 2009 GP Software
   All Rights Reserved

  Version 1.02, modified October 5, 2001:
    - Now supports TGA files that don't have the identification 'footer'
    - Handles 32 bit TGA files (same as 24 bit really)

  Version 1.03/1.04 - Unreleased

  Version 1.05, modified June 5, 2007:
    - Added sanity check to try to identify files which aren't really TGA
      Sanity check based on TGA 2.0 Specification

  Version 1.06, modified April 17, 2009 (Leo Davidson):
    - Alpha channel in 32 bit TGA files is now used instead of thrown away.
	- Fixed red/blue channels being swapped with 16 bit TGA files.

  Version 1.07, modified May 10, 2009 (Leo Davidson):
	- Much of the code has been re-written.
    - Pre-multiplied alpha support.
	- Alpha cannel is now ignored if it is completely zero and the image does not specify its meaning.
	- Indexed color support.
	- Greyscale support.
	- Fixed memory leak when files contain a colormap.
	- Better error/input checking.
*/

#include "StdAfx.h"
#include "../common/LeoHelpers.h"
#include "../common/Win32IOWrapper.h"
#include "targa.h"
#include "TGAFile.h"

// Every plugin should have its own GUID
// {0FB3AA93-A6C7-470f-8984-6C63F0D5327A}
static const GUID GUIDPlugin_TGA = 
{ 0xfb3aa93, 0xa6c7, 0x470f, { 0x89, 0x84, 0x6c, 0x63, 0xf0, 0xd5, 0x32, 0x7a } };

HINSTANCE g_hModuleInstance = 0;
CRITICAL_SECTION g_cs;
int g_refCount = 0;
wchar_t *g_szTgaImage = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain may not be called again after such a failure (depending on the compiler).
		InitializeCriticalSection(&g_cs);
		g_hModuleInstance = hModule;
	//	DisableThreadLibraryCalls(g_hModuleInstance); // Leo 03/Aug/2009: Removed. Doing this is wrong (and most likely the call failed) when we use the static CRT.
		break;

	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&g_cs);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	default:
		break;
	}

	return TRUE;
}


BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData)
{
	LeoHelpers::CriticalSectionScoper css(&g_cs);

	if (0 == g_refCount++)
	{
		// Load translated strings.
		DOpusPluginHelperUtil helper;

		// Strings will be allocated with LocalAlloc. We need to free them with LocalFree in DVP_Uninit.
		helper.GetString(STR_TARGA_IMAGE, &g_szTgaImage, 0);
	}

	return TRUE;
}


void DVP_Uninit(void)
{
	LeoHelpers::CriticalSectionScoper css(&g_cs);

	if (0 < g_refCount)
	{
		if (0 == --g_refCount)
		{
			::LocalFree(g_szTgaImage);
			g_szTgaImage = 0;
		}
	}
}


// Tell Opus we're safe to export to USB
BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
	return TRUE;
}


// Identify the viewer plugin to DOpus
BOOL DVP_IdentifyW(LPVIEWERPLUGININFOW lpVPInfo)
{
	BOOL bResult = FALSE;

	// We only accept plugin API version 4
	if (lpVPInfo->cbSize >= VIEWERPLUGININFO_V4_SIZE)
	{
		// Don't assume that DVP_Init/DVP_InitEx have been called, or that the plugin support API is available.
		// DVP_Identify may be called by the USB export code which may not have called DVP_Init, and may also be
		// running a 32-bit version of us in a proxy process on behalf of 64-bit Opus, in which case the Opus helper
		// functions won't be available. Plugins that care can check VPINITF_NOINIT and VPINITF_NOSUPPORT.

		// Plugin flags. We can handle streams (e.g. files in Zip archives archives and FTP sites).
		// For fast, random-seeking files (e.g. in local directories) we identify based on file contents.
		// For slow files (e.g. FTP) and non-random-seeking files (e.g. within a Zip) we only identify based on file extension.
		// (This is because identifying a Targa file based on its contents requires seeking to the very end of the file to find the footer signature,
		// which won't be there in older files anyway. For local files, if the footer is missing we'll still recognise them if they have .tga extension.)
		
		lpVPInfo->dwFlags = DVPFIF_CanHandleStreams
						  | DVPFIF_ExtensionsOnlyIfSlow
						  | DVPFIF_ExtensionsOnlyIfNoRndSeek
					//	  | DVPFIF_ExtensionsOnlyForThumbnails
						  | DVPFIF_UseVersionResource;

		// Preferred filename extension is .tga
		LeoHelpers::StringCopy(lpVPInfo->lpszHandleExts,  lpVPInfo->cchHandleExtsMax,  L".tga");

		wchar_t *szTgaPluginDescription = 0;

		// Even if VPINITF_NOSUPPORT is set, GetString may be available. We should still be able
		// to fall back when it is not available and doesn't give us a string, however.
		DOpusPluginHelperUtil helper;
		helper.GetString(STR_TARGA_PLUGIN_DESCRIPTION, &szTgaPluginDescription, 0);

		// Plugin information
		LeoHelpers::StringCopy(lpVPInfo->lpszName,        lpVPInfo->cchNameMax,        L"TARGA");
		LeoHelpers::StringCopy(lpVPInfo->lpszDescription, lpVPInfo->cchDescriptionMax, szTgaPluginDescription ? szTgaPluginDescription : L"Truevision TGA (TARGA) Viewer Plugin");
		LeoHelpers::StringCopy(lpVPInfo->lpszCopyright,   lpVPInfo->cchCopyrightMax,   L"(c) Copyright 2009 GP Software");
		LeoHelpers::StringCopy(lpVPInfo->lpszURL,         lpVPInfo->cchURLMax,         L"http://www.gpsoft.com.au");

		if (szTgaPluginDescription)
		{
			::LocalFree(szTgaPluginDescription);
			szTgaPluginDescription = 0;
		}

		// Min file size is used to speed up identification - files smaller than 45 bytes probably aren't TGA.
		lpVPInfo->dwlMinFileSize = 45; // A 1x1 TGA file could be as small as 45 bytes.

		// Major type of file we handle is images.
		// The major type we specify can affect which of our functions Opus calls and which structures it passes to them.
		lpVPInfo->uiMajorFileType = DVPMajorType_Image;

		// Our GUID to uniquely identify us to DOpus.
		lpVPInfo->idPlugin = GUIDPlugin_TGA;

		bResult = TRUE;
	}

	return bResult;
}

// FileAndStream packages up the file or stream arguments into an object which can turn either into the other.
// This (along with the Win32IOWrapper class in some cases) allows us to write one piece of code which works
// with either type of input, or which only works with one type of input but can automatically convert whatever
// it's given into whatever it needs.

BOOL IdentifyFAS(LeoHelpers::FileAndStream &fas, LPVIEWERPLUGINFILEINFOW lpVPFileInfo)
{
	BOOL bResult = FALSE;

	TGAFile tgaFile(&fas);

	if (tgaFile.Identify())
	{
		// Fill out file information and return success
		lpVPFileInfo->dwFlags        = DVPFIF_CanReturnBitmap | DVPFIF_CanReturnThumbnail;
		lpVPFileInfo->wMajorType     = DVPMajorType_Image;
		lpVPFileInfo->wMinorType     = 0;
		lpVPFileInfo->szImageSize.cx = tgaFile.GetWidth();
		lpVPFileInfo->szImageSize.cy = tgaFile.GetHeight();
		lpVPFileInfo->iNumBits       = tgaFile.GetDepth();

		LeoHelpers::WriteFileInfoInfoLine(lpVPFileInfo, g_szTgaImage ? g_szTgaImage : L"TGA Image");

		if (lpVPFileInfo->cbSize > VIEWERPLUGINFILEINFO_V1_SIZE)
		{
			// TODO: Return resolution?
		}

		bResult = TRUE;
	}

	return bResult;
}

// Identify a local disk-based file
BOOL DVP_IdentifyFileW(HWND hWnd, LPWSTR lpszName, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, HANDLE hAbortEvent)
{
	if (g_refCount == 0) { return FALSE; } // Prevent call from very old versions of Opus which don't call DVP_InitEx. (Critical section not required to test int zero.)

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	return IdentifyFAS(fas, lpVPFileInfo);
}

// Identify a stream-based file
BOOL DVP_IdentifyFileStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,DWORD dwStreamFlags)
{
	if (g_refCount == 0) { return FALSE; } // Prevent call from very old versions of Opus which don't call DVP_InitEx. (Critical section not required to test int zero.)

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	return IdentifyFAS(fas, lpVPFileInfo);
}

// Create a bitmap from a disk-based TGA file
HBITMAP DVP_LoadBitmapW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszSize,HANDLE hAbortEvent)
{
	if (g_refCount == 0) { return FALSE; } // Prevent call from very old versions of Opus which don't call DVP_InitEx. (Critical section not required to test int zero.)

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	TGAFile tgaFile(&fas);
	return tgaFile.LoadBitmap(hWnd, lpVPFileInfo, lpszSize);
}

// Create a bitmap from a stream-based TGA file
HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszSize,DWORD dwStreamFlags)
{
	if (g_refCount == 0) { return FALSE; } // Prevent call from very old versions of Opus which don't call DVP_InitEx. (Critical section not required to test int zero.)

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	TGAFile tgaFile(&fas);
	return tgaFile.LoadBitmap(hWnd, lpVPFileInfo, lpszSize);
}
