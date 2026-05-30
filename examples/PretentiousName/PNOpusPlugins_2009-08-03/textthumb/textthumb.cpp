// textthumb.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "TextThumbConfig.h"
#include "TextFile.h"
#include "textthumb.h"
#include "TextThumbGenerator.h"
#include "TextThumbConfigDlg.h"
#include "CodePageEnumerator.h"

// {DFEC0911-DAA2-47ec-AB19-B11B25AB898A}
static const GUID GUIDPlugin_textthumb =
{ 0xdfec0911, 0xdaa2, 0x47ec, { 0xab, 0x19, 0xb1, 0x1b, 0x25, 0xab, 0x89, 0x8a } };

static bool s_bDestroyStatic = false;
static HMODULE s_hDllModule = NULL;
static CRITICAL_SECTION s_cs;
static int s_refCount = 0;
static CTextThumbGenerator *s_pTextThumbGenerator = NULL;
static DOpusPluginHelperTextThumbPlugin *s_pPluginHelper = NULL;
static CTextThumbConfig *s_pConfig = NULL;


BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case(DLL_PROCESS_ATTACH):
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain won't be called again after such a failure.
		InitializeCriticalSection(&s_cs);
		s_hDllModule = reinterpret_cast<HMODULE>(hModule);
	//	DisableThreadLibraryCalls(s_hDllModule); // Leo 03/Aug/2009: Removed. Doing this is wrong (and most likely the call failed) when we use the static CRT.
		break;
	case(DLL_PROCESS_DETACH):
		DeleteCriticalSection(&s_cs);
		break;
	case(DLL_THREAD_ATTACH):
	case(DLL_THREAD_DETACH):
		break;
	default:
		break;
	}

    return(TRUE);
}

BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData)
{
	// Note: DVP_InitEx won't even be called before Opus 9, which is the minimum supported version.

	// Note: We assume that Opus has initialized the common controls and COM for all
	// threads which call our code.

	// DVP_InitEx and DVP_Uninit must keep a refcount as Opus may init and uninit us multiple times.
	// (e.g. Click the Refresh button in the plugins list to trigger an additional Init/Uninit pair.)

	LeoHelpers::CriticalSectionScoper css(&s_cs);

	BOOL bInitSuccess = FALSE;

	if (0 < s_refCount++)
	{
		bInitSuccess = TRUE;
	}
	else
	{
		DWORD64 dw64OpusVersion = pInitExData->dwOpusVerMajor;
		dw64OpusVersion <<= 32;
		dw64OpusVersion += pInitExData->dwOpusVerMinor;

		s_pPluginHelper = new DOpusPluginHelperTextThumbPlugin();
		s_pConfig = new CTextThumbConfig(s_hDllModule, dw64OpusVersion, s_pPluginHelper);

		if (s_pConfig->CheckOpusAbility(CTextThumbConfig::TTA_RUN)
		&&	CCodePageEnumerator::StaticInitialize())
		{
			s_bDestroyStatic = true;

			s_pTextThumbGenerator = new CTextThumbGenerator();

			s_pConfig->Load(); // It is not worth delaying config load until needed. Doing so would also complicate registry->XML conversion and backup/USB-export.

			bInitSuccess = TRUE;
		}

		if (!bInitSuccess)
		{
			DVP_Uninit(); // Opus does not call Uninit if Init fails. Clean ourselves up.
		}
	}

	return(bInitSuccess);
}

void DVP_Uninit(void)
{
	LeoHelpers::CriticalSectionScoper css(&s_cs);

	if (0 < s_refCount)
	{
		if (0 == --s_refCount)
		{
			if (s_bDestroyStatic)
			{
				s_bDestroyStatic = false;
				CCodePageEnumerator::StaticDestroy();
			}

			delete s_pTextThumbGenerator; s_pTextThumbGenerator = NULL;
			delete s_pConfig;             s_pConfig             = NULL;
			delete s_pPluginHelper;       s_pPluginHelper       = NULL;
		}
	}
}

BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
	return TRUE;
}

BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo)
{
	BOOL bResult = FALSE;

	// We only accept plugin API version 4
	if (lpVPInfo->cbSize >= VIEWERPLUGININFO_V4_SIZE)
	{
		// Don't check the config in here as it's called by the USB export code which may not have called DVP_Init,
		// and may also be running a 32-bit version of us on behalf of 64-bit Opus, in which case the Opus helper
		// functions won't be available. Plugins that care can check VPINITF_NOINIT and VPINITF_NOSUPPORT.

		lpVPInfo->dwFlags = DVPFIF_CanHandleStreams
					//	  | DVPFIF_ExtensionsOnlyIfSlow
						  | DVPFIF_CanHandleBytes
						  | DVPFIF_ZeroBytesOk
						  | DVPFIF_TrueThumbnailSize
						  | DVPFIF_CanConfigure
						  | DVPFIF_CatchAll
						  | DVPFIF_OnlyThumbnails;

		lpVPInfo->lpszHandleExts = _T("*.*");
		lpVPInfo->dwlMinFileSize = 0;
		lpVPInfo->dwlMaxFileSize = 0;
		lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
		lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
		lpVPInfo->uiMajorFileType = DVPMajorType_Text;
		lpVPInfo->idPlugin = GUIDPlugin_textthumb;

		if (LeoHelpers::CopyVersionResourceToViewerPluginInfo(s_hDllModule, IDI_MAIN, STR_TEXTTHUMB_PLUGIN_NAME, STR_TEXTTHUMB_PLUGIN_DESCRIPTION, lpVPInfo))
		{
			// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
			bResult = TRUE;
		}
	}
	return(bResult);
}

BOOL DVP_IdentifyFileBytes(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags)
{
	if (s_pConfig == NULL)
	{
		return FALSE;
	}

	// if lpszName is NULL then szExtWithDot will be NULL as well.
	const TCHAR *szExtWithDot = LeoHelpers::GetExtensionPart(true, lpszName);

	// If szExtWithDot is NULL, or not found, then the default config will be returned.
	CTextThumbConfig::CTypeConfig typeConfig = s_pConfig->GetTypeConfig(szExtWithDot != NULL ? szExtWithDot : _T(""));

	// Check if we're being called for a sub-image of a folder thumbnail and the user has turned them off for this plugin.
	// Also, if it's a folder sub-image and we can detect the file is hidden, exclude it always.
	// (i.e. So that folder thumbs don't end up full of boring/hidden text files.)

	if (lpVPFileInfo->dwFlags&DVPFIF_InFolderThumbnail)
	{
		if (typeConfig.dwFlags&CTextThumbConfig::TTF_FOLDERTHUMBNAILS_OFF)
		{
			return FALSE;
		}

		DWORD dwAttribs = ::GetFileAttributes(lpszName); // This may fail if lpszName isn't a real file path (e.g. we're in a zip file).

		if (dwAttribs != INVALID_FILE_ATTRIBUTES && (dwAttribs&FILE_ATTRIBUTE_HIDDEN))
		{
			return FALSE; // It's a hidden file so exclude it from folder thumbnails.
		}
	}

	// Filter out unwanted file extensions.

	if (NULL != lpszName)
	{
		size_t lenName = _tcslen(lpszName);

		if (0 < lenName)
		{
			std::vector< std::basic_string<TCHAR> > vecExcludedExtensions;
			s_pConfig->GetExcludedExtensions(&vecExcludedExtensions);

			for (std::vector< std::basic_string<TCHAR> >::const_iterator pStr = vecExcludedExtensions.begin(); pStr != vecExcludedExtensions.end(); ++pStr)
			{
				if (pStr->length() <= lenName
				&&	0 == _tcsicmp(pStr->c_str(), lpszName + (lenName - pStr->length())))
				{
					return FALSE;
				}
			}
		}
	}

	// If the file is empty, only display a thumbnail for it if its PerceivedType is text.

	if (0 == uiDataSize && NULL != szExtWithDot)
	{
		std::basic_string<TCHAR> strPerceivedType;

		if (!LeoHelpers::LeetRegQueryStringValue(HKEY_CLASSES_ROOT, szExtWithDot, _T("PerceivedType"), 0, &strPerceivedType)
		||	0 != _tcsicmp(strPerceivedType.c_str(), _T("text")))
		{
			return FALSE;
		}
	}

	// Filter out file headers for things like PDF files that often appear as text but aren't wanted.

	if (0 != uiDataSize)
	{
		std::vector< std::basic_string<TCHAR> > vecExcludedHeaders;
		s_pConfig->GetExcludedHeaders(&vecExcludedHeaders);

		for (std::vector< std::basic_string<TCHAR> >::const_iterator pStr = vecExcludedHeaders.begin(); pStr != vecExcludedHeaders.end(); ++pStr)
		{
			bool bMatches = false;

			char *szExclHead = NULL;
#ifdef UNICODE
			if (0 != LeoHelpers::WCtoMB(&szExclHead, pStr->c_str(), -1, typeConfig.dwCodePage))
			{
#else
			szExclHead = pStr->c_str()
#endif
				size_t slen = strlen(szExclHead);

				if (uiDataSize >= slen
				&&	0 == _strnicmp(szExclHead, reinterpret_cast<const char *>(lpData), slen))
				{
					bMatches = true;
				}
#ifdef UNICODE
				delete[] szExclHead;
			}
#endif

			if (bMatches)
			{
				return FALSE;
			}
		}
	}

	// Okay, let's try to load the start of the file which Opus has given to us in memory.

	CMemoryTextFile textFile(lpData, uiDataSize);

	// Check the size (given to use by newer versions of Opus) to tell whether we have been given all of the file or just part of it.
	// This is used to decide whether to add "..." to the end of the description when everything we have can fit.
	DWORDLONG dwlDataSize = uiDataSize;
	bool bAlwaysTruncate = (dwlDataSize < lpVPFileInfo->dwlFileSize);

	lpVPFileInfo->dwFlags |= (DVPFIF_CanReturnThumbnail | DVPFIF_NoThumbnailBorder | DVPFIF_RegenerateOnResize | DVPFIF_NoCache);

	if (typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_ON)
	{
		lpVPFileInfo->dwFlags |= DVPFIF_ShowThumbnailIcon; // Force it on.
	}
	else if (typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_OFF)
	{
		lpVPFileInfo->dwFlags |= DVPFIF_NoShowThumbnailIcon; // Force it off. (Else leave it up to thumbnail prefs.)
	}

	lpVPFileInfo->wMajorType = DVPMajorType_Text;
	lpVPFileInfo->wMinorType = 0;

	if (NULL != lpVPFileInfo->lpszInfo && 0 < lpVPFileInfo->cchInfoMax)
	{
		const std::basic_string<TCHAR> strHeading = s_pConfig->CacheString(STR_TEXTTHUMB_CONTENTS_PREFIX);

		std::wstring wstrText;

		unsigned int lMaxLength = static_cast<unsigned int>(lpVPFileInfo->cchInfoMax - strHeading.length());

		if (!textFile.ReadLines(&wstrText, 0, lMaxLength, L' ', true, true, bAlwaysTruncate, typeConfig.dwCodePage))
		{
			return FALSE;
		}
		else if ((typeConfig.dwFlags&CTextThumbConfig::TTF_DESCRIPTION_ON) && !wstrText.empty())
		{
#ifdef UNICODE
			wstrText = strHeading + wstrText;
			LeoHelpers::StringCopy(lpVPFileInfo->lpszInfo, wstrText.c_str(), lpVPFileInfo->cchInfoMax);
#else
			char *szConverted = NULL;

			if (0 != LeoHelpers::WCtoMB(&szConverted, wstrText.c_str(), -1, dwCodePage)
			{
				strText = strHeading + szConverted;
				LeoHelpers::StringCopy(lpVPFileInfo->lpszInfo, strText.c_str(), lpVPFileInfo->cchInfoMax);
				delete [] szConverted;
			}
#endif
		}
	}

	return TRUE;
}

HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData)
{
	if (s_pConfig == NULL)
	{
		return NULL;
	}

	NTextThumbConfigDlg::ConfigDlgData *pcd =
		new NTextThumbConfigDlg::ConfigDlgData(hWndNotify, dwNotifyData, GetTickCount()/1000, s_pTextThumbGenerator, s_pConfig);

	HWND hwndResult = CreateDialogParam(s_hDllModule, MAKEINTRESOURCE(IDD_CONFIG), hWndParent, NTextThumbConfigDlg::configDlgProc, reinterpret_cast<LPARAM>(pcd));

	if (hwndResult == NULL)
	{
		delete pcd;
	}

	return hwndResult;
}

HBITMAP loadBitmapCommon(HWND hWnd, CBaseTextFile *pTextFile, LPTSTR lpszName, LPVIEWERPLUGINFILEINFO lpVPFileInfo, LPSIZE lpszDesiredSize, bool bStream)
{
	if (NULL == s_pTextThumbGenerator
	||	NULL == s_pConfig)
	{
		return NULL;
	}

	// if lpszName is NULL then szExtWithDot will be NULL as well.
	const TCHAR *szExtWithDot = LeoHelpers::GetExtensionPart(true, lpszName);

	// If szExtWithDot is NULL, or not found, then the default config will be returned.
	CTextThumbConfig::CTypeConfig typeConfig = s_pConfig->GetTypeConfig(szExtWithDot != NULL ? szExtWithDot : _T(""));

	if (lpVPFileInfo->dwFlags&DVPFIF_InFolderThumbnail)
	{
		if (typeConfig.dwFlags&CTextThumbConfig::TTF_FOLDERTHUMBNAILS_OFF)
		{
			return NULL;
		}

		if (!bStream)
		{
			DWORD dwAttribs = ::GetFileAttributes(lpszName);

			if (dwAttribs != INVALID_FILE_ATTRIBUTES && (dwAttribs&FILE_ATTRIBUTE_HIDDEN))
			{
				return NULL; // It's a hidden file so exclude it from folder thumbnails.
			}
		}
	}

	HBITMAP hbmResult = s_pTextThumbGenerator->GenerateThumbnail(*s_pConfig, hWnd, pTextFile, lpVPFileInfo, typeConfig,
																 false, false, lpszDesiredSize->cx, lpszDesiredSize->cy);

	return(hbmResult);
}

HBITMAP DVP_LoadBitmap(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent)
{
	CFileTextFile textFile(lpszName);

	return(loadBitmapCommon(hWnd, &textFile, lpszName, lpVPFileInfo, lpszDesiredSize, false));
}

HBITMAP DVP_LoadBitmapStream(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags)
{
	CStreamTextFile textFile(lpStream);

	return(loadBitmapCommon(hWnd, &textFile, lpszName, lpVPFileInfo, lpszDesiredSize, true));
}
