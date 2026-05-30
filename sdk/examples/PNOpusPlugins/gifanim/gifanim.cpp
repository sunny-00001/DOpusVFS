#include "stdafx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "GifConfig.h"
#include "GifConfigDlg.h"
#include "GifDecoder.h"
#include "ImageViewer.h"
#include "GifTlsData.h"
#include "gifanim.h"
#include "ViewerToolbarProxy.h"

// {617E4181-FB31-495f-BD90-33FEA1EE9BAB}
static const GUID GUIDPlugin_gifanim =
{ 0x617e4181, 0xfb31, 0x495f, { 0xbd, 0x90, 0x33, 0xfe, 0xa1, 0xee, 0x9b, 0xab } };

static DWORD s_dwGifViewerTlsDataIndex = TLS_OUT_OF_INDEXES;
static HMODULE s_hDllModule = NULL;
static CRITICAL_SECTION s_cs;
static int s_refCount = 0;
static std::set<CGifTlsData *> s_tlsDataSet;
static DOpusPluginHelperGifPlugin *s_pPluginHelper = NULL;
static CGifConfig *s_pConfig = NULL;
static ATOM s_atomViewer = 0;
static ATOM s_atomProxy = 0;

static void S_RegisterWindowClasses()
{
	if (s_atomViewer == 0)
	{
		s_atomViewer = CImageViewer::RegisterWindowClass(s_hDllModule);
	}

	if (s_atomProxy  == 0)
	{
		s_atomProxy  = CViewerToolbarProxy::RegisterWindowClass(s_hDllModule);
	}
}

static void S_FreeWindowClasses()
{
	if (s_atomViewer != 0)
	{
		CImageViewer::UnregisterWindowClass(s_hDllModule);
		s_atomViewer = 0;
	}

	if (s_atomProxy != 0)
	{
		CViewerToolbarProxy::UnregisterWindowClass(s_hDllModule);
		s_atomProxy = 0;
	}
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case(DLL_PROCESS_ATTACH):
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain won't be called again after such a failure.
		InitializeCriticalSection(&s_cs);
		s_hDllModule = reinterpret_cast<HMODULE>(hModule);
		s_dwGifViewerTlsDataIndex = TlsAlloc();
		break;

	case(DLL_PROCESS_DETACH):
		// If the process is terminated or FreeLibrary is called on us
		// we don't get thread detach calls and must clean-up our TLS stuff here instead.
		if (TLS_OUT_OF_INDEXES != s_dwGifViewerTlsDataIndex)
		{
			TlsFree(s_dwGifViewerTlsDataIndex);
			s_dwGifViewerTlsDataIndex = TLS_OUT_OF_INDEXES;
		}
		DeleteCriticalSection(&s_cs);
		break;

	case(DLL_THREAD_ATTACH):
		// Note: We don't call TlsSetValue on DLL_THREAD_ATTACH since it's only called
		// for threads created after our DLL is loaded. This also means we don't create
		// objects for threads which will never need them.
		break;

	case(DLL_THREAD_DETACH):
		if (TLS_OUT_OF_INDEXES != s_dwGifViewerTlsDataIndex)
		{
			CGifTlsData *pTlsData = reinterpret_cast<CGifTlsData *>(TlsGetValue(s_dwGifViewerTlsDataIndex));

			if (NULL != pTlsData)
			{
				LeoHelpers::CriticalSectionScoper css(&s_cs);
				s_tlsDataSet.erase(pTlsData);
			}
			delete pTlsData;
		}
		break;

	default:
		break;
	}

    return(TRUE);
}

CGifTlsData *getGifViewerTlsData()
{
	CGifTlsData *pResult = NULL;

	{
		LeoHelpers::CriticalSectionScoper css(&s_cs);

		if (TLS_OUT_OF_INDEXES != s_dwGifViewerTlsDataIndex)
		{
			pResult = reinterpret_cast<CGifTlsData *>(TlsGetValue(s_dwGifViewerTlsDataIndex));

			if (pResult == NULL && ::GetLastError() == ERROR_SUCCESS)
			{
				pResult = new CGifTlsData();

				if (NULL != pResult)
				{
					TlsSetValue(s_dwGifViewerTlsDataIndex, pResult);

					s_tlsDataSet.insert(pResult);
				}
			}
		}
	}

	return(pResult);
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
		S_RegisterWindowClasses();

		if (s_atomViewer != 0 && s_atomProxy != 0)
		{
			DWORD64 dw64OpusVersion = pInitExData->dwOpusVerMajor;
			dw64OpusVersion <<= 32;
			dw64OpusVersion += pInitExData->dwOpusVerMinor;

			s_pPluginHelper = new DOpusPluginHelperGifPlugin();
			s_pConfig = new CGifConfig(s_hDllModule, dw64OpusVersion, s_pPluginHelper);

			if (s_pConfig->CheckOpusAbility(CGifConfig::GAA_RUN))
			{
				s_pConfig->Load(); // It is not worth delaying config load until needed. Doing so would also complicate registry->XML conversion and backup/USB-export.

				bInitSuccess = TRUE;
			}
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
			{
				LeoHelpers::CriticalSectionScoper css(&s_cs);

				for(std::set<CGifTlsData *>::iterator iter = s_tlsDataSet.begin(); iter != s_tlsDataSet.end(); ++iter)
				{
					delete *iter;
				}
				s_tlsDataSet.clear();
			}

			delete s_pConfig;       s_pConfig       = NULL;
			delete s_pPluginHelper; s_pPluginHelper = NULL;

			S_FreeWindowClasses();
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

		lpVPInfo->dwFlags = DVPFIF_CanHandleBytes
						  | DVPFIF_CanHandleStreams
						  | DVPFIF_OverrideInternal
						  | DVPFIF_ExtensionsOnlyIfSlow
						  | DVPFIF_ExtensionsOnlyForThumbnails
						  | DVPFIF_CanConfigure
						  | DVPFIF_TrueThumbnailSize;

		lpVPInfo->lpszHandleExts = _T(".gif");
		lpVPInfo->dwlMinFileSize = 16;
		lpVPInfo->dwlMaxFileSize = 0;
		lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
		lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
		lpVPInfo->uiMajorFileType = DVPMajorType_Image;
		lpVPInfo->idPlugin = GUIDPlugin_gifanim;

		if (LeoHelpers::CopyVersionResourceToViewerPluginInfo(s_hDllModule, IDI_MAIN, STR_GIFANIM_PLUGIN_NAME, STR_GIFANIM_PLUGIN_DESCRIPTION, lpVPInfo))
		{
			// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
			bResult = TRUE;
		}
	}

	return(bResult);
}

BOOL DVP_IdentifyFileBytes(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags)
{
	BOOL bResult = FALSE;

	if (s_pConfig != NULL
	&&	13 <= uiDataSize // size of GIF header + logical screen descriptor
	&&  (0 == strncmp(reinterpret_cast<char *>(lpData), "GIF87a", 6)
	||   0 == strncmp(reinterpret_cast<char *>(lpData), "GIF89a", 6)))
	{
		lpVPFileInfo->dwFlags = DVPFIF_CanReturnThumbnail | DVPFIF_CanReturnViewer;
		lpVPFileInfo->iColorSpace = DVPColorSpace_RGB;
		lpVPFileInfo->wMajorType = DVPMajorType_Image;
		lpVPFileInfo->wMinorType = 0;
		lpVPFileInfo->szImageSize.cx = NGifDecoder::LM_to_uint(lpData[6], lpData[7]);
		lpVPFileInfo->szImageSize.cy = NGifDecoder::LM_to_uint(lpData[8], lpData[9]);

		int iNumColours = (1<<((lpData[10]&0x07)+1));

		if (iNumColours <= 2)
		{
			lpVPFileInfo->iNumBits = 1;
		}
		else if (iNumColours <= 4)
		{
			lpVPFileInfo->iNumBits = 2;
		}
		else if (iNumColours <= 16)
		{
			lpVPFileInfo->iNumBits = 4;
		}
		else
		{
			lpVPFileInfo->iNumBits = 8;
		}

		LeoHelpers::WriteFileInfoInfoLine(lpVPFileInfo, s_pConfig->CacheString(STR_GIFANIM_GIF_IMAGE));

		bResult = TRUE;
	}

	return(bResult);
}

HBITMAP loadBitmapGifFile(HWND hWnd, NGifDecoder::CGifFile *pGifFile, LPVIEWERPLUGINFILEINFO lpVPFileInfo, LPSIZE lpszDesiredSize)
{
	HBITMAP hbmResult = NULL;

	if (s_pConfig != NULL)
	{
		HDC hDC = GetDC(hWnd);

		if (NULL != hDC)
		{
			HDC hDCCompat = CreateCompatibleDC(hDC); // Create a compatible DC so that our thread doesn't keep the main DC locked while loading.

			ReleaseDC(hWnd, hDC);
			hDC = NULL;

			if (NULL != hDCCompat)
			{
				bool bHasTransparency = false;
				bool bWantFrame = true;
				bool bRegenOnResize = false;
				bool bThumbnailSprockets = s_pConfig->GetThumbnailSprockets();

				hbmResult = NGifDecoder::CGifImage::LoadGifToDIBSection(hDCCompat, pGifFile, &bHasTransparency, &bWantFrame, &bRegenOnResize, lpszDesiredSize, bThumbnailSprockets);

				if (bHasTransparency)
				{
					lpVPFileInfo->dwFlags |= DVPFIF_HasAlphaChannel;
				}
				if (!bWantFrame)
				{
					lpVPFileInfo->dwFlags |= DVPFIF_NoThumbnailBorder;
				}
				if (bRegenOnResize)
				{
					lpVPFileInfo->dwFlags |= DVPFIF_RegenerateOnResize;
				}

				DeleteDC(hDCCompat);
			}
		}
	}

	return(hbmResult);
}

HBITMAP DVP_LoadBitmap(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent)
{
	NGifDecoder::CGifFile gifFile(lpszName, hAbortEvent);

	return(loadBitmapGifFile(hWnd, &gifFile, lpVPFileInfo, lpszDesiredSize));
}

HBITMAP DVP_LoadBitmapStream(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags)
{
	NGifDecoder::CGifFile gifFile(lpStream);

	return(loadBitmapGifFile(hWnd, &gifFile, lpVPFileInfo, lpszDesiredSize));
}

HWND DVP_CreateViewer(HWND hWnd,LPRECT lpRc,DWORD dwFlags)
{
	if (s_pConfig == NULL)
	{
		return NULL;
	}

	return(CViewerToolbarProxy::CreateViewerToolbarProxyWindow(hWnd, lpRc, dwFlags, s_pConfig));
}

HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData)
{
	if (s_pConfig == NULL)
	{
		return NULL;
	}

	NGifConfigDlg::ConfigDlgData *pcd = new NGifConfigDlg::ConfigDlgData(hWndNotify, dwNotifyData, s_pConfig);

	HWND hwndResult = CreateDialogParam(s_hDllModule, MAKEINTRESOURCE(IDD_CONFIG), hWndParent, NGifConfigDlg::configDlgProc, reinterpret_cast<LPARAM>(pcd));

	if (hwndResult == NULL)
	{
		delete pcd;
	}

	return hwndResult;
}
