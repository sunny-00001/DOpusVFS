// dcrawrap.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "resource.h"
#include "../common/plugindialogs.h"
#include "LeoHelpers.h"
#include "Win32IOWrapper.h"
#include "dcraw_settings.h"
#include "dcrawrap_config.h"
#include "dcrawrap_config_dlg.h"
#include "ReaderPPM.h"
#include "dcraw_interface.h"
#include "dcrawrap.h"

// {14B3B1B0-39A6-43b7-9552-A6E0709EF162}
static const GUID GUIDPlugin_dcrawrap =
{ 0x14b3b1b0, 0x39a6, 0x43b7, { 0x95, 0x52, 0xa6, 0xe0, 0x70, 0x9e, 0xf1, 0x62 } };

static HMODULE s_hDllModule = NULL;
static CRITICAL_SECTION s_cs;
static CRITICAL_SECTION s_csLcms;
static CRITICAL_SECTION s_csTempFOpen;
static int s_refCount = 0;
static DOpusPluginHelperDCRawPlugin *s_pPluginHelper = NULL;
static DCRawConfig *s_pConfig = NULL;
static DWORD s_dwStartTime = 0;
static std::vector< std::wstring > s_vecCameras;

// DOpusViewerPluginFileInfo::dwPrivateData index definitions
#define DPDI_MAGIC 0
#define DPDI_STARTTIME 1
#define DPDI_ROTATION 3
#define DPDI_CAMERA 4

#define DPDI_MAGIC_VALUE 0x44524157 

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case(DLL_PROCESS_ATTACH):
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain may not be called again after such a failure (depending on the compiler).
		InitializeCriticalSection(&s_cs);
		InitializeCriticalSection(&s_csLcms);
		InitializeCriticalSection(&s_csTempFOpen);
		s_hDllModule = reinterpret_cast<HMODULE>(hModule);
	//	DisableThreadLibraryCalls(s_hDllModule); // Leo 03/Aug/2009: Removed. Doing this is wrong (and most likely the call failed) when we use the static CRT.
		break;
	case(DLL_PROCESS_DETACH):
		DeleteCriticalSection(&s_csTempFOpen);
		DeleteCriticalSection(&s_csLcms);
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
		s_dwStartTime = ::GetTickCount();

		DWORD64 dw64OpusVersion = pInitExData->dwOpusVerMajor;
		dw64OpusVersion <<= 32;
		dw64OpusVersion += pInitExData->dwOpusVerMinor;

		s_pPluginHelper = new DOpusPluginHelperDCRawPlugin();
		s_pConfig = new DCRawConfig(s_hDllModule, dw64OpusVersion, s_pPluginHelper);

		if (s_pConfig->CheckOpusAbility(DCRawConfig::DCRA_RUN))
		{
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
			s_dwStartTime = 0;
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

		// The Raw identify function is prone to false positives so we must match using file extensions.
		// People can modify the list of extensions via the configuration dialog.

		// DVPFIF_OverrideInternal is required because some raw camera images are in TIF containers which
		// Opus's internal TIFF code will attempt (and fail) to view unless we get to them first.

		lpVPInfo->dwFlags = DVPFIF_CanHandleStreams
						  | DVPFIF_ExtensionsOnly
						  | DVPFIF_CanConfigure
						  | DVPFIF_OverrideInternal;

		std::vector< std::wstring > vecExtensions;
		std::wstring strExtensions;

		// We must not use the config if we're not sure the plugin has been initialized. In cases where
		// the NOINIT flag is set the caller does not care about the list of extensions anyway.

		assert((lpVPInfo->dwInitFlags&VPINITF_NOINIT) || s_pConfig != NULL); // If init has happened then the config should not be null.

		if (!(lpVPInfo->dwInitFlags&VPINITF_NOINIT) && s_pConfig != NULL)
		{
			s_pConfig->GetExtensions(&vecExtensions, true, false, true, false);

			for(std::vector< std::wstring >::const_iterator pExt = vecExtensions.begin(); pExt != vecExtensions.end(); pExt++)
			{
				if (lpVPInfo->cchHandleExtsMax > (strExtensions.length() + pExt->length() + 2)) // +1 for '.' and +1 for ';'
				{
					if (!strExtensions.empty())
					{
						strExtensions += L';';
					}

					strExtensions += L".";
					strExtensions += *pExt;
				}
			}
		}

		LeoHelpers::StringCopy(lpVPInfo->lpszHandleExts, lpVPInfo->cchHandleExtsMax, strExtensions.c_str());

		lpVPInfo->dwlMinFileSize = 8;
		lpVPInfo->dwlMaxFileSize = 0;
		lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
		lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
		lpVPInfo->uiMajorFileType = DVPMajorType_Image;
		lpVPInfo->idPlugin = GUIDPlugin_dcrawrap;

		if (LeoHelpers::CopyVersionResourceToViewerPluginInfo(s_hDllModule, IDI_MAIN, STR_DCRAW_PLUGIN_NAME, STR_DCRAW_PLUGIN_DESCRIPTION, lpVPInfo))
		{
			// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
			bResult = TRUE;
		}
	}

	return(bResult);
}

bool GetCameraByIndex(std::wstring *pstrSafeName, DWORD dwStartTime, DWORD dwIndex)
{
	pstrSafeName->clear();

	LeoHelpers::CriticalSectionScoper css(&s_cs);

	if (s_dwStartTime != dwStartTime
	||	dwIndex >= s_vecCameras.size())
	{
		assert(false);
		return false;
	}

	*pstrSafeName = s_vecCameras.at(dwIndex);

	return true;
}

DWORD GetIndexOfCamera(const wchar_t *szMake, const wchar_t *szModel)
{
	std::wstring strSafeName = DCRawConfig::DCR_ProfileMap::GenerateSafeName(szMake, szModel);

	LeoHelpers::CriticalSectionScoper css(&s_cs);

	// Slow linear search with the assumption the vector will not be very long.
	// The vector only contains the cameras which the user actually uses so building
	// a two-way lookup to speed this up seems more like a waste of memory than
	// something which will actually improve speed. The important thing is that
	// this look-up be significantly faster than DCRaw having to open and identify
	// a file an extra time.

	for(std::vector< std::wstring >::size_type i = 0; i < s_vecCameras.size(); ++i)
	{
		if (0 == wcscmp(strSafeName.c_str(), s_vecCameras.at(i).c_str()))
		{
			return static_cast<DWORD>(i);
		}
	}

	s_vecCameras.push_back(strSafeName);
	return static_cast<DWORD>(s_vecCameras.size()-1);
}

HWND DVP_Configure(HWND hWndParent,HWND hWndNotify,DWORD dwNotifyData)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return NULL;
	}

	// Structure's data will be copied before we return, so creating it on the stack is fine, and if the create fails we don't leak anything.
	DCRawConfigDlg::DCRawConfigDlg_CreationData cd(s_pConfig, hWndNotify, dwNotifyData);

	return(s_pConfig->GetOpusPluginHelper()->CreateLangDlg(s_pConfig->GetInstance(), MAKEINTRESOURCE(IDD_RAW_CONFIG), hWndParent, DCRawConfigDlg::configDlgProc, reinterpret_cast<LPARAM>(&cd)));
}

DCRawResult *CallDCRaw(LeoHelpers::FileAndStream &fas, bool bFromFile, DCRawResult::DCRAW_OPERATION dcrOper, const DCR_RawSettings *prs)
{
	// TODO: Can we hook up some kind of abort event (or equiv) in the stream case?

	assert((dcrOper==DCRawResult::DCRO_IDENTIFY && prs==NULL) || (dcrOper!=DCRawResult::DCRO_IDENTIFY && prs!=NULL));

	DCRawResult *pRawResult = NULL;

	std::wstring strInputFilePath;

	const char *szInputFileNameIfAscii = NULL;
	std::wstring strInputFileNameWide;
	std::string strInputFileNameAscii;

	const wchar_t *szInputDirIfReal = NULL;
	std::wstring strParentPath;

	if (fas.GetFilePath(&strInputFilePath))
	{
		if (fas.GetFileName(&strInputFileNameWide)
		&&	LeoHelpers::IsAscii(strInputFileNameWide.c_str())
		&&	LeoHelpers::LeetWCtoMB(&strInputFileNameAscii, strInputFileNameWide.c_str()))
		{
			szInputFileNameIfAscii = strInputFileNameAscii.c_str();
		}

		if (bFromFile
		&&	NULL != LeoHelpers::GetParentPathString(&strParentPath, strInputFilePath.c_str()))
		{
			szInputDirIfReal = strParentPath.c_str();
		}

		pRawResult = DCRawResult::DoFile(&s_csLcms, &s_csTempFOpen, fas.GetAbortEvent(), dcrOper,
										 strInputFilePath.c_str(), szInputFileNameIfAscii, szInputDirIfReal, prs, NULL);
	}
	
	return pRawResult;
}

BOOL IdentifyFASRaw(LeoHelpers::FileAndStream &fas, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, bool bFromFile, bool bForceFullIdentify)
{
	BOOL bResult = FALSE;

	lpVPFileInfo->dwFlags = DVPFIF_CanReturnThumbnail | DVPFIF_CanReturnBitmap;
	lpVPFileInfo->iColorSpace = DVPColorSpace_Unknown;
	lpVPFileInfo->wMajorType = DVPMajorType_Image;
	lpVPFileInfo->wMinorType = 0;
	lpVPFileInfo->iNumBits = 0;

	if (!bForceFullIdentify && (!bFromFile || !fas.HasFastRandomSeek()))
	{
		// A file extension match is good enough for media that is slow or cannot seek.
		// DCRaw won't work on non-seekable streams and it isn't worth extracting a huge Raw image from
		// an archive just to make one. Also, the identify function is very slow if seeking is slow since it
		// has to move around the file a lot. Archives have slow seeking but network drives do not.
		lpVPFileInfo->szImageSize.cx = 0;
		lpVPFileInfo->szImageSize.cy = 0;
		LeoHelpers::WriteFileInfoInfoLine(lpVPFileInfo, s_pConfig->GetString(STR_DCRAW_RAW_IMAGE)); // "Raw Image"

		bResult = TRUE;
	}
	else
	{
		DCRawResult *pRawResult = CallDCRaw(fas, bFromFile, DCRawResult::DCRO_IDENTIFY, NULL);

		if (pRawResult && pRawResult->GetHeight() > 0 && pRawResult->GetWidth() > 0)
		{
			std::wstring strImageType;
			std::wstring strMake;
			std::wstring strModel;
			if (!pRawResult->GetMake().empty()
			&&	LeoHelpers::LeetMBtoWC(&strMake, pRawResult->GetMake().c_str())
			&&	!pRawResult->GetModel().empty()
			&&	LeoHelpers::LeetMBtoWC(&strModel, pRawResult->GetModel().c_str()))
			{
				strImageType += strMake;
				strImageType += L" ";
				strImageType += strModel;
				strImageType += L" ";
				strImageType += s_pConfig->GetString(STR_DCRAW_RAW_IMAGE); // "Raw Image"

				lpVPFileInfo->szImageSize.cx = pRawResult->GetWidth();
				lpVPFileInfo->szImageSize.cy = pRawResult->GetHeight();
				LeoHelpers::WriteFileInfoInfoLine(lpVPFileInfo, strImageType.c_str());

				// Cache some data about the image in case it's useful for extraction.
				lpVPFileInfo->dwPrivateData[ DPDI_MAGIC     ] = DPDI_MAGIC_VALUE;
				lpVPFileInfo->dwPrivateData[ DPDI_STARTTIME ] = s_dwStartTime; // Protects against this data being used across plugin instances.
				lpVPFileInfo->dwPrivateData[ DPDI_ROTATION  ] = pRawResult->GetRotation();
				lpVPFileInfo->dwPrivateData[ DPDI_CAMERA    ] = GetIndexOfCamera(strMake.c_str(), strModel.c_str());

				bResult = TRUE;
			}
		}

		delete pRawResult;
	}

	return bResult;
}


BOOL IdentifyFAS(LeoHelpers::FileAndStream &fas, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, bool bFromFile)
{
	BOOL bResult = FALSE;

	std::wstring strExtNoDotLower;

	if (fas.GetFileExtension(&strExtNoDotLower, false))
	{
		DCRawConfig::ProcessExtension(&strExtNoDotLower,false);

		switch(s_pConfig->GetExtensionType(strExtNoDotLower, true, false, true, false))
		{
		case DCRawConfig::DCRET_RAW:
			bResult = IdentifyFASRaw(fas, lpVPFileInfo, bFromFile, false);
			break;
		case DCRawConfig::DCRET_PNM:
			{
				Win32IOWrapper *pIO = Win32IOWrapper::CreateFromFAS(&fas, false); // We don't need to be able to seek in the stream for PPM.

				if (pIO != NULL)
				{
					bResult = ReaderPNM::Process(pIO, NULL, lpVPFileInfo, NULL);

					delete pIO;
				}
			}
			break;
		default:
		case DCRawConfig::DCRET_UNHANDLED:
			break;
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

BOOL LoadBitmapFASRaw(LeoHelpers::FileAndStream &fas, HBITMAP *phBitmap, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, bool bFromFile, LPSIZE lpszDesiredSize)
{
	BOOL bResult = FALSE;

	bool bThumbnail = (lpszDesiredSize != NULL
					&& lpszDesiredSize->cx != 0
					&& lpszDesiredSize->cy != 0);

	bool bConverter = (lpVPFileInfo->dwFlags&DVPFIF_ForImageConverter) ? true : false;

	DCR_RawSettings::Purpose purp = DCR_RawSettings::DP_VIEWERS;

	if (bThumbnail)
	{
		purp = DCR_RawSettings::DP_THUMBS;
	}
	else if (bConverter)
	{
		purp = DCR_RawSettings::DP_CONVERTER;
	}

	if (DPDI_MAGIC_VALUE != lpVPFileInfo->dwPrivateData[ DPDI_MAGIC     ]
	||	s_dwStartTime    != lpVPFileInfo->dwPrivateData[ DPDI_STARTTIME ])
	{
		// We can't rely on the cached data, or there is no cached data at all.
		// Either the plugin has been restarted since it was generated (Opus shouldn't do that AFAIK but it doesn't
		// hurt to check) or we never did a full identification of this file. The latter will happen with files in
		// zips since full identification of them is slow (due to requiring them to be fully unpacked). The file is
		// unpacked now and we need the info, so identify it.

		VIEWERPLUGINFILEINFOW fileInfoTemp = {0};
		fileInfoTemp.cbSize = sizeof(fileInfoTemp);
		fileInfoTemp.lpszInfo   = lpVPFileInfo->lpszInfo;
		fileInfoTemp.cchInfoMax = lpVPFileInfo->cchInfoMax;

		if (IdentifyFASRaw(fas, &fileInfoTemp, bFromFile, true))
		{
			lpVPFileInfo->szImageSize.cx = fileInfoTemp.szImageSize.cx;
			lpVPFileInfo->szImageSize.cy = fileInfoTemp.szImageSize.cy;
			lpVPFileInfo->iNumBits = fileInfoTemp.iNumBits;
			lpVPFileInfo->lpszInfo   = fileInfoTemp.lpszInfo;
			lpVPFileInfo->cchInfoMax = fileInfoTemp.cchInfoMax;

			for (size_t i = 0; i < _countof(lpVPFileInfo->dwPrivateData); ++i)
			{
				lpVPFileInfo->dwPrivateData[ i ] = fileInfoTemp.dwPrivateData[ i ];
			}
		}
	}

	std::wstring strSafeName;

	if (DPDI_MAGIC_VALUE != lpVPFileInfo->dwPrivateData[ DPDI_MAGIC     ]
	||	s_dwStartTime    != lpVPFileInfo->dwPrivateData[ DPDI_STARTTIME ]
	||	!GetCameraByIndex(&strSafeName, s_dwStartTime, lpVPFileInfo->dwPrivateData[ DPDI_CAMERA ]))
	{
		strSafeName.clear(); // Get the default settings if we don't know the camera make.
	}

	DCR_RawSettings rs = s_pConfig->GetRawSettings(strSafeName, purp);

	DCRawResult::DCRAW_OPERATION dcrOp = DCRawResult::DCRO_UNKNOWN;

	for(int modeNum = 0; !bResult && modeNum < 2; ++modeNum)
	{
		if (modeNum == 0)
		{
			if (!rs.bTryPreview) { continue; }
			dcrOp = DCRawResult::DCRO_PREVIEW;
		}
		else if (modeNum == 1)
		{
			if (!rs.bTryFull) { continue; }
			dcrOp = DCRawResult::DCRO_DECODE;
		}
		else
		{
			assert(false);
			break;
		}

		DCRawResult *pRawResult = CallDCRaw(fas, bFromFile, dcrOp, &rs);

		if (pRawResult)
		{
			std::wstring strOutputFilePath;
			const wchar_t *szOutputExtension;

			if (pRawResult->GetOutputFilePath(&strOutputFilePath)
			&&	NULL != (szOutputExtension= LeoHelpers::GetExtensionPart(false, strOutputFilePath.c_str())))
			{
				std::wstring strExtNoDotLower = szOutputExtension;
				DCRawConfig::ProcessExtension(&strExtNoDotLower,false);

				// DCRaw can output JPG and PNM formats, depending on what happened.
				// Preview images are often JPG but not always. Full decodes are probably/almost always PNM.

				if (0 == wcscmp(strExtNoDotLower.c_str(), L"jpg") && NULL != lpVPFileInfo)
				{
					// In Opus 9.1.1.8 and earlier DVPFIF_JPEGStream worked when we were called via DVP_LoadBitmap but
					// did not work when we were called via DVP_LoadBitmapStream. Instead it would fail and leak the memory
					// we allocated. The plugin now requires 9.1.1.9 to run at all (for dialog translation) so there's no
					// longer a need to check the version.
					//if (bFromFile || s_pConfig->CheckOpusAbility(DCRawConfig::DCRA_STREAM_JPEG_STREAM))
					{
						LeoHelpers::BinaryData fileBuffer(true); // Uses LocalAlloc(LMEM_FIXED)

						if (fileBuffer.Load(strOutputFilePath.c_str()))
						{
							*phBitmap = reinterpret_cast< HBITMAP >( fileBuffer.GetBuffer() );

							if (phBitmap != NULL)
							{
								fileBuffer.Forget();
								lpVPFileInfo->dwFlags |= DVPFIF_JPEGStream;
								bResult = true;
							}
						}
					}
				}
				// DCRaw can output .PAM as well, if the image data has four channels, but we should never make it do that
				// and don't support loading such files.
				else if (DCRawConfig::DCRET_PNM == s_pConfig->GetExtensionType(strExtNoDotLower, false, false, false, true))
				{
					Win32IOFileWrapper io = Win32IOFileWrapper(strOutputFilePath.c_str(), fas.GetAbortEvent());

					bResult = ReaderPNM::Process(&io, NULL, NULL, phBitmap);
				}
				else
				{
					assert(false);
				}
			}

			delete pRawResult;
		}
	}

	if (!bResult)
	{
		dcrOp = DCRawResult::DCRO_UNKNOWN;
	}

	if (bResult && dcrOp == DCRawResult::DCRO_PREVIEW)
	{
		// Tell Opus to rotate the image, but only if it's a preview image.
		// Full decodes will already be rotated.
		// Retrieve the amount of rotation that we stored in the DVP_IdentifyFile stage.
		// Do not call pRawResult->GetRotation() as it will be wrong here! The rotation amount isn't calculated by DCRaw when
		// extracting a thumbnail. (That's why we stored the rotation earlier, when we asked DCRaw to identify the file.)

		if (DPDI_MAGIC_VALUE == lpVPFileInfo->dwPrivateData[ DPDI_MAGIC     ]
		&&	s_dwStartTime    == lpVPFileInfo->dwPrivateData[ DPDI_STARTTIME ])
		{
			int iRotation = lpVPFileInfo->dwPrivateData[ DPDI_ROTATION ];

			switch( iRotation )
			{
			default:
				break;
			case 90:
				lpVPFileInfo->dwFlags |= DVPFIF_Rotate90;
				break;
			case 180:
				lpVPFileInfo->dwFlags |= DVPFIF_Rotate180;
				break;
			case 270:
				lpVPFileInfo->dwFlags |= DVPFIF_Rotate270;
				break;
			}
		}
	}

	if (bResult
	&&	!bThumbnail
	&&	!bConverter // Not that it matters as the converter reports the string from IdentifyFile and ignores the string returned by LoadBitmap
	&&	lpVPFileInfo->cchInfoMax > 0
	&&	lpVPFileInfo->lpszInfo
	&&	iswdigit(lpVPFileInfo->lpszInfo[0])) // The current string should start with the dimensions; if not then don't mess with it.
	{
		const wchar_t *szPrefix = NULL;

		if (dcrOp == DCRawResult::DCRO_PREVIEW)
		{
			szPrefix = s_pConfig->GetString(STR_DCRAW_PREFIX_QUICKPREVIEW); // "Quick Preview / "
		}
		else if (dcrOp == DCRawResult::DCRO_DECODE)
		{
			if (rs.bHalfSizeColor)
			{
				szPrefix = s_pConfig->GetString(STR_DCRAW_PREFIX_QUARTERDECODING); // "Quarter-Size Decoding / "
			}
			else
			{
				szPrefix = s_pConfig->GetString(STR_DCRAW_PREFIX_FULLDECODING); // "Full Decoding / "
			}
		}

		if (szPrefix)
		{
			std::wstring strNewInfo = szPrefix;
			strNewInfo += lpVPFileInfo->lpszInfo;
			if (strNewInfo.length() < lpVPFileInfo->cchInfoMax)
			{
				LeoHelpers::StringCopy(lpVPFileInfo->lpszInfo, strNewInfo.c_str(), lpVPFileInfo->cchInfoMax);
			}
		}
	}

	return bResult;
}

BOOL LoadBitmapFAS(LeoHelpers::FileAndStream &fas, HBITMAP *phBitmap, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, bool bFromFile, LPSIZE lpszDesiredSize)
{
	BOOL bResult = FALSE;

	std::wstring strExtNoDotLower;

	if (fas.GetFileExtension(&strExtNoDotLower, false))
	{
		DCRawConfig::ProcessExtension(&strExtNoDotLower,false);

		switch(s_pConfig->GetExtensionType(strExtNoDotLower, true, false, true, false))
		{
		case DCRawConfig::DCRET_RAW:
			bResult = LoadBitmapFASRaw(fas, phBitmap, lpVPFileInfo, bFromFile, lpszDesiredSize);
			break;
		case DCRawConfig::DCRET_PNM:
			{
				Win32IOWrapper *pIO = Win32IOWrapper::CreateFromFAS(&fas, false); // We don't need to be able to seek in the stream for PPM.

				if (pIO != NULL)
				{
					bResult = ReaderPNM::Process(pIO, NULL, NULL, phBitmap);

					delete pIO;
				}
			}
			break;
		default:
		case DCRawConfig::DCRET_UNHANDLED:
			break;
		}
	}

	return bResult;
}

HBITMAP DVP_LoadBitmapW(HWND hWnd,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return NULL;
	}

	LeoHelpers::FileAndStream fas(lpszName, hAbortEvent, false);

	HBITMAP hbm = NULL;

	if (!LoadBitmapFAS(fas, &hbm, lpVPFileInfo, true, lpszDesiredSize))
	{
		return NULL;
	}

	return hbm;
}

HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPWSTR lpszName,LPVIEWERPLUGINFILEINFOW lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags)
{
	if (s_pConfig == NULL)
	{
		assert(false);
		return NULL;
	}

	LeoHelpers::FileAndStream fas(lpszName, lpStream, (dwStreamFlags&DVPSF_NoRandomSeek)?true:false, (dwStreamFlags&DVPSF_Slow)?true:false);

	HBITMAP hbm = NULL;

	if (!LoadBitmapFAS(fas, &hbm, lpVPFileInfo, false, lpszDesiredSize))
	{
		return NULL;
	}

	return hbm;
}
