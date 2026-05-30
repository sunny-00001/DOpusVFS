// audiotags.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "Win32IOWrapper.h"
#include "audiotags_config.h"
#include "audiotags_oggvorbis.h"
#include "audiotags_flac.h"
#include "audiotags.h"

// {87E75C3E-6612-4105-9B51-616437727185}
static const GUID GUIDPlugin_audiotags =
{ 0x87e75c3e, 0x6612, 0x4105, { 0x9b, 0x51, 0x61, 0x64, 0x37, 0x72, 0x71, 0x85 } };

static HMODULE s_hDllModule = NULL;
static CRITICAL_SECTION s_cs;
static int s_refCount = 0;
static DOpusPluginHelperAudioTagsPlugin *s_pPluginHelper = NULL;
static AudioTagsConfig *s_pConfig = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case(DLL_PROCESS_ATTACH):
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain may not be called again after such a failure (depending on the compiler).
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

		s_pPluginHelper = new DOpusPluginHelperAudioTagsPlugin();
		s_pConfig = new AudioTagsConfig(s_hDllModule, dw64OpusVersion, s_pPluginHelper);

		if (s_pConfig->CheckOpusAbility(AudioTagsConfig::ATA_RUN))
		{
//			s_pConfig->Load();

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
			delete s_pConfig;       s_pConfig       = NULL;
			delete s_pPluginHelper; s_pPluginHelper = NULL;
		}
	}
}

BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
	return TRUE;
}

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

		// We match using extensions only, not contents, so that the plugin can be unloaded by Opus when not in use.
		// Since the formats we handle rarely have other extensions, and most users will not have any files that
		// benefit from them, it is a good trade-off to let Opus avoid keeping us loaded on most machines.

		lpVPInfo->dwFlags = DVPFIF_CanHandleStreams
						  | DVPFIF_ExtensionsOnly
						  | DVPFIF_ProvideFileInfo;
					//	  | DVPFIF_NoSlowFiles

		_tcsncpy_s(lpVPInfo->lpszHandleExts, lpVPInfo->cchHandleExtsMax, L".ogg;.oga;.flac;.fla", _TRUNCATE);

		lpVPInfo->dwlMinFileSize = 128;
		lpVPInfo->dwlMaxFileSize = 0;
		lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
		lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
		lpVPInfo->uiMajorFileType = DVPMajorType_Sound;
		lpVPInfo->idPlugin = GUIDPlugin_audiotags;

		if (LeoHelpers::CopyVersionResourceToViewerPluginInfo(s_hDllModule, IDI_MAIN, STR_AUDIOTAGS_PLUGIN_NAME, STR_AUDIOTAGS_PLUGIN_DESCRIPTION, lpVPInfo))
		{
			// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
			bResult = TRUE;
		}
	}

	return(bResult);
}

enum AudioTags_Type
{
	ATT_UNKNOWN,
	ATT_OGG,
	ATT_FLAC,
};

AudioTags_Type getAudioTagsTypeFromExtension(const wchar_t *szLowerExtNoDot)
{
	if (0 == wcscmp(szLowerExtNoDot, L"ogg")
	||	0 == wcscmp(szLowerExtNoDot, L"oga"))
	{
		// While FLAC is usually put into its own container format, it can also appear in .ogg containers. Those ogg-FLAC files will go down this path too.
		return ATT_OGG;
	}

	if (0 == wcscmp(szLowerExtNoDot, L"flac")
	||	0 == wcscmp(szLowerExtNoDot, L"fla"))
	{
		return ATT_FLAC;
	}

	return ATT_UNKNOWN;
}

BOOL IdentifyFAS(LeoHelpers::FileAndStream &fas, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, bool bFromFile)
{
	// The old Ogg/FLAC plugin has code to identify those formats by the file header but
	// doing that means Opus has to keep our plugin loaded an it doesn't seem worth it
	// given how uncommon it is for these files to have the wrong extension, and how uncommon
	// all of the formats this plugin handles are in general (and thus how it'd be a waste of
	// memory to keep the plugin in memory on most people's machines).

	BOOL bResult = FALSE;

	std::wstring strExtNoDotLower;

	if (fas.GetFileExtension(&strExtNoDotLower, false))
	{
		LeoHelpers::ToLower(&strExtNoDotLower);

		AudioTags_Type att = getAudioTagsTypeFromExtension(strExtNoDotLower.c_str());

		if (att != ATT_UNKNOWN)
		{
			bResult = TRUE;
		}

		if (bResult)
		{
			lpVPFileInfo->dwFlags=DVPFIF_CanReturnFileInfo|DVPFIF_CanReturnThumbnail|DVPFIF_NoThumbnailDimensions;
			lpVPFileInfo->wMajorType=DVPMajorType_Sound; // Video as well or instead in some cases. Hmm...
		}
	}

	return bResult;
}

BOOL DVP_IdentifyFileW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,HANDLE hAbortEvent)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return FALSE;
	}

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	return IdentifyFAS(fas, lpVPFileInfo, true);
}

BOOL DVP_IdentifyFileStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,DWORD dwStreamFlags)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return FALSE;
	}

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	return IdentifyFAS(fas, lpVPFileInfo, false);
}

BOOL ParseTagsFAS(LeoHelpers::FileAndStream &fas, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, LPDVPFILEINFOHEADER lpFIH, HBITMAP *phBitmap, bool bFromFile)
{
	LPDVPFILEINFOMUSICW pMusicInfo = NULL;

	if (lpFIH != NULL)
	{
		if (lpFIH->uiMajorType != DVPMajorType_Sound
		||	lpFIH->cbSize < DVPFILEINFOMUSICW_V1_SIZE)
		{
			return FALSE;
		}

		pMusicInfo = reinterpret_cast< LPDVPFILEINFOMUSICW >(lpFIH);
	}

	phBitmap = NULL;

	BOOL bResult = FALSE;

	if (pMusicInfo != NULL || phBitmap != NULL)
	{
		std::wstring strExtNoDotLower;

		if (fas.GetFileExtension(&strExtNoDotLower, false))
		{
			LeoHelpers::ToLower(&strExtNoDotLower);

			AudioTags_Type att = getAudioTagsTypeFromExtension(strExtNoDotLower.c_str());

			switch (att)
			{
			default:
			case ATT_UNKNOWN:
				break;

			case ATT_OGG:
				bResult = AudioTags_OggVorbis::GetFileInfo(*s_pConfig, fas, bFromFile, lpVPFileInfo, pMusicInfo, phBitmap);
				break;

			case ATT_FLAC:
				bResult = AudioTags_FLAC::GetFileInfo(*s_pConfig, fas, bFromFile, lpVPFileInfo, pMusicInfo, phBitmap);
				break;
			}
		}
	}

	if (bResult && lpVPFileInfo && phBitmap && *phBitmap)
	{
		lpVPFileInfo->dwFlags |= DVPFIF_ShowThumbnailIcon; // Opus's internal MP3 and WMA thumbnail handling forces the icon on so we should follow suit.
	}

	return bResult;
}

BOOL DVP_GetFileInfoFileW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPDVPFILEINFOHEADER lpFIH,HANDLE hAbortEvent)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return FALSE;
	}

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	return ParseTagsFAS(fas, lpVPFileInfo, lpFIH, NULL, true);
}

BOOL DVP_GetFileInfoFileStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPDVPFILEINFOHEADER lpFIH,DWORD dwStreamFlags)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return FALSE;
	}

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	return ParseTagsFAS(fas, lpVPFileInfo, lpFIH, NULL, false);
}

HBITMAP DVP_LoadBitmapW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return FALSE;
	}

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	HBITMAP hBitmap = NULL;

	if (!ParseTagsFAS(fas, lpVPFileInfo, NULL, &hBitmap, true))
	{
		hBitmap = NULL;
	}

	return hBitmap;
}

HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return NULL;
	}

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	HBITMAP hBitmap = NULL;

	if (!ParseTagsFAS(fas, lpVPFileInfo, NULL, &hBitmap, true))
	{
		hBitmap = NULL;
	}

	return hBitmap;
}
