/* Maya IFF plugin interface
 * Copyright (C) 2008-2011 Leo Davidson
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

#include "stdafx.h"
#include "LeoUtils.h"
#include "resource.h"
#include "maya_iff.h"
#include "iffimage.h"

// {FA959248-AB91-4363-BDB8-059C5D848897}
static const GUID GUIDPlugin_mayaiff = 
{ 0xfa959248, 0xab91, 0x4363, { 0xbd, 0xb8, 0x5, 0x9c, 0x5d, 0x84, 0x88, 0x97 } };

static HMODULE s_hDllModule = NULL;
//static CRITICAL_SECTION s_cs;
//static int s_refCount = 0;

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// If DLL_PROCESS_ATTACH fails it should clean up everything it did
		// as DllMain may or may not be called again after such a failure (depending on compiler version).
	//	InitializeCriticalSection(&s_cs);
		s_hDllModule = reinterpret_cast<HMODULE>(hModule);
	//	DisableThreadLibraryCalls(s_hDllModule); // Leo 03/Aug/2009: Removed. Doing this is wrong (and most likely the call failed) when we use the static CRT.
		break;

	case DLL_PROCESS_DETACH:
	//	DeleteCriticalSection(&s_cs);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	default:
		break;
	}

    return(TRUE);
}

BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData)
{
	DWORD64 dw64OpusVersion = pInitExData->dwOpusVerMajor;
	dw64OpusVersion <<= 32;
	dw64OpusVersion += pInitExData->dwOpusVerMinor;

	return(dw64OpusVersion >= MAKE64BITVERSIONNUMBER(9,1,3,0));
}

void DVP_Uninit(void)
{
}

BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData)
{
	// Note: Opus 9 calls DVP_USBSafe but Opus 10 does not. Instead, Opus 10 looks for the "OPUS" XML resource.
	//       By providing both we support USB export on both versions.
	return TRUE;
}

BOOL DVP_IdentifyW(LPVIEWERPLUGININFO lpVPInfo)
{
	BOOL bResult = FALSE;

	if (lpVPInfo->cbSize >= sizeof(VIEWERPLUGININFO))
	{
		lpVPInfo->dwFlags = DVPFIF_CanHandleStreams
						  | DVPFIF_CanHandleBytes
						  | DVPFIF_ExtensionsOnly
						  | DVPFIF_OverrideInternal; // Since Opus handles IFF ILBM files internally we must set this so that we get first shot at .iff files.

		lpVPInfo->lpszHandleExts = _T(".iff");
		lpVPInfo->dwlMinFileSize = 32;
		lpVPInfo->dwlMaxFileSize = 0;
		lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
		lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
		lpVPInfo->uiMajorFileType = DVPMajorType_Image;
		lpVPInfo->idPlugin = GUIDPlugin_mayaiff;

		if (LeoUtils::CopyVersionResourceToViewerPluginInfo(s_hDllModule, IDI_MAIN, lpVPInfo))
		{
			// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
			bResult = TRUE;
		}
	}

	return(bResult);
}

struct IffIoFileWrapperData
{
	const wchar_t *szFilePath;
	FILE *pFile;
	HANDLE hAbortEvent;
};

struct IffIoStreamWrapperData
{
	IStream *pStream;
};

extern "C" int    IffIoFileWrapperOpen(void *pData)
{
	IffIoFileWrapperData *pRealData = reinterpret_cast<IffIoFileWrapperData *>(pData);

	if (!pRealData->pFile
	&&	(pRealData->hAbortEvent==NULL || WAIT_TIMEOUT==WaitForSingleObject(pRealData->hAbortEvent,0)))
	{
		if (0 != _wfopen_s(&pRealData->pFile, pRealData->szFilePath, L"rb"))
		{
			pRealData->pFile = NULL;
		}
	}

	return (pRealData->pFile != NULL);
}

extern "C" int    IffIoFileWrapperClose(void *pData)
{
	IffIoFileWrapperData *pRealData = reinterpret_cast<IffIoFileWrapperData *>(pData);

	if (pRealData->pFile)
	{
		fclose(pRealData->pFile);
		pRealData->pFile = NULL;
	}

	return TRUE;
}

extern "C" size_t IffIoFileWrapperRead(void *pData, void *pBuffer, size_t elementSize, size_t elementCount)
{
	IffIoFileWrapperData *pRealData = reinterpret_cast<IffIoFileWrapperData *>(pData);

	if (!pRealData->pFile
	||	(pRealData->hAbortEvent!=NULL && WAIT_OBJECT_0==WaitForSingleObject(pRealData->hAbortEvent,0)))
	{
		ZeroMemory(pBuffer, elementSize * elementCount);
		return 0;
	}

	size_t res = fread(pBuffer, elementSize, elementCount, pRealData->pFile);

	if (res == 0)
	{
		ZeroMemory(pBuffer, elementSize * elementCount);
	}
	else if (res < elementCount)
	{
		ZeroMemory(reinterpret_cast<BYTE *>(pBuffer) + res * elementSize, (elementCount - res) * elementSize);
	}

	return res;
}

extern "C" int    IffIoFileWrapperSeek(void *pData, long offset, int origin)
{
	IffIoFileWrapperData *pRealData = reinterpret_cast<IffIoFileWrapperData *>(pData);

	if (!pRealData->pFile)
	{
		return -1;
	}

	return fseek(pRealData->pFile, offset, origin);
}

extern "C" long   IffIoFileWrapperTell(void *pData)
{
	IffIoFileWrapperData *pRealData = reinterpret_cast<IffIoFileWrapperData *>(pData);

	if (!pRealData->pFile)
	{
		return -1L;
	}

	return ftell(pRealData->pFile);
}

extern "C" int    IffIoStreamWrapperOpen(void *pData)
{
	IffIoStreamWrapperData *pRealData = reinterpret_cast<IffIoStreamWrapperData *>(pData);

	return (pRealData->pStream != NULL);
}

extern "C" int    IffIoStreamWrapperClose(void *pData)
{
	return TRUE;
}

extern "C" size_t IffIoStreamWrapperRead(void *pData, void *pBuffer, size_t elementSize, size_t elementCount)
{
	IffIoStreamWrapperData *pRealData = reinterpret_cast<IffIoStreamWrapperData *>(pData);

	if (!pRealData->pStream)
	{
		ZeroMemory(pBuffer, elementSize * elementCount);
		return 0;
	}

	size_t sizeTBytesToRead = elementSize * elementCount;

	if (sizeTBytesToRead > ULONG_MAX)
	{
		ZeroMemory(pBuffer, sizeTBytesToRead);
		return 0;
	}

	ULONG ulBytesToRead = static_cast<ULONG>(sizeTBytesToRead);

	ULONG ulBytesRead = 0;

	if (S_OK != pRealData->pStream->Read(pBuffer, ulBytesToRead, &ulBytesRead))
	{
		ulBytesRead = 0;
	}

	size_t elementsRead = ulBytesRead / elementSize;

	if (elementsRead == 0)
	{
		ZeroMemory(pBuffer, sizeTBytesToRead);
	}
	else if (elementsRead < elementCount)
	{
		ZeroMemory(reinterpret_cast<BYTE *>(pBuffer) + elementsRead * elementSize, (elementCount - elementsRead) * elementSize);
	}

	return elementsRead;
}

extern "C" int    IffIoStreamWrapperSeek(void *pData, long offset, int origin)
{
	IffIoStreamWrapperData *pRealData = reinterpret_cast<IffIoStreamWrapperData *>(pData);

	if (!pRealData->pStream)
	{
		return -1;
	}

	LARGE_INTEGER liOffset;
	liOffset.QuadPart = offset;

	DWORD dwOrigin;

	switch(origin)
	{
	default:
	case SEEK_SET:	dwOrigin = STREAM_SEEK_SET;	break;
	case SEEK_CUR:	dwOrigin = STREAM_SEEK_CUR;	break;
	case SEEK_END:	dwOrigin = STREAM_SEEK_END;	break;
	}

	if (S_OK == pRealData->pStream->Seek(liOffset, dwOrigin, NULL))
	{
		return 0; // success
	}

	return -1; // failure
}

extern "C" long   IffIoStreamWrapperTell(void *pData)
{
	IffIoStreamWrapperData *pRealData = reinterpret_cast<IffIoStreamWrapperData *>(pData);

	if (!pRealData->pStream)
	{
		return -1L;
	}

	LARGE_INTEGER liSeek;
	liSeek.QuadPart = 0;

	ULARGE_INTEGER uliPos;
	uliPos.QuadPart = 0;

	if (S_OK == pRealData->pStream->Seek(liSeek, STREAM_SEEK_CUR, &uliPos))
	{
		return uliPos.LowPart; // success
	}

	return -1L; // failure
}

BOOL DVP_IdentifyFileBytesW(HWND hWnd,LPTSTR lpszName,LPBYTE lpData,UINT uiDataSize,LPVIEWERPLUGINFILEINFO lpVPFileInfo,DWORD dwStreamFlags)
{
	BOOL bResult = FALSE;

	// Create a stream on the memory buffer.
	LeoUtils::CFileAndStream fas(lpszName, lpData, uiDataSize);

	IffIoStreamWrapperData ioData = {0};

	if (fas.GetStream(&ioData.pStream))
	{
		iff_file_io ioWrapper = {0};
		ioWrapper.pData = &ioData;
		ioWrapper.open  = IffIoStreamWrapperOpen;
		ioWrapper.close = IffIoStreamWrapperClose;
		ioWrapper.read  = IffIoStreamWrapperRead;
		ioWrapper.seek  = IffIoStreamWrapperSeek;
		ioWrapper.tell  = IffIoStreamWrapperTell;

		int fWas16Bit = FALSE;

		iff_image *pImage = iff_load(&ioWrapper, NULL, &fWas16Bit, FALSE, FALSE, FALSE, FALSE);

		if (pImage)
		{
			lpVPFileInfo->dwFlags = DVPFIF_CanReturnThumbnail | DVPFIF_CanReturnBitmap | DVPFIF_CanReturnFileInfo;

			if (pImage->depth == 4)
			{
				lpVPFileInfo->dwFlags |= DVPFIF_HasAlphaChannel;
			}

			lpVPFileInfo->iColorSpace = DVPColorSpace_RGB;
			lpVPFileInfo->wMajorType = DVPMajorType_Image;
			lpVPFileInfo->wMinorType = 0;
			lpVPFileInfo->szImageSize.cx = pImage->width;
			lpVPFileInfo->szImageSize.cy = pImage->height;
			lpVPFileInfo->iNumBits = (fWas16Bit ? 16 : 8) * pImage->depth;

			writeFileInfoInfoLine(lpVPFileInfo, L"Maya IFF Image");

			bResult = TRUE;

			iff_free(pImage);
		}
	}

	return bResult;
}

HBITMAP LoadMain(iff_file_io &ioWrapper, LPVIEWERPLUGINFILEINFO lpVPFileInfo)
{
	HBITMAP hbmResult = NULL;

	iff_image *pImage = iff_load(&ioWrapper, NULL, NULL, TRUE, FALSE, FALSE, FALSE);

	if (pImage)
	{
		if (pImage->width > 0
		&&	pImage->height > 0
		&&	(pImage->depth == 1 || pImage->depth == 3 || pImage->depth == 4)
		&&	pImage->rgba != NULL)
		{
			if (pImage->depth == 4)
			{
				lpVPFileInfo->dwFlags |= DVPFIF_HasAlphaChannel;
			}

			HDC hdc = CreateCompatibleDC(0);

			if (hdc)
			{
				BYTE *pBits = NULL;

				BITMAPINFO bmi = {0};
				bmi.bmiHeader.biSize         = sizeof(bmi.bmiHeader);
				bmi.bmiHeader.biWidth        = pImage->width;
				bmi.bmiHeader.biHeight       = pImage->height;
				bmi.bmiHeader.biPlanes       = 1;
				bmi.bmiHeader.biBitCount     = (pImage->depth==4) ? 32 : 24;
				bmi.bmiHeader.biCompression  = BI_RGB;
				bmi.bmiHeader.biSizeImage    = 0;
				bmi.bmiHeader.biClrUsed      = 0;
				bmi.bmiHeader.biClrImportant = 0;

				hbmResult = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBits), NULL, 0);

				if (hbmResult != NULL)
				{
					GdiFlush();

					// Re-order the channels since RGBQUADs are not in BGRA order, not RGBA order.
					const size_t cPixels = pImage->width * pImage->height;

					const BYTE *pInput = pImage->rgba;

					if (pImage->depth == 4)
					{
						for (size_t i = 0; i < cPixels; ++i)
						{
							pBits[2] = *pInput++;
							pBits[1] = *pInput++;
							pBits[0] = *pInput++;
							pBits[3] = *pInput++;
							pBits+=4;
						}
					}
					else if (pImage->depth == 3)
					{
						for (size_t i = 0; i < cPixels; ++i)
						{
							pBits[2] = *pInput++;
							pBits[1] = *pInput++;
							pBits[0] = *pInput++;
							pBits+=3;
						}
					}
					else if (pImage->depth == 1)
					{
						for (size_t i = 0; i < cPixels; ++i)
						{
							pBits[0] = *pInput;
							pBits[1] = *pInput;
							pBits[2] = *pInput++;
							pBits+=3;
						}
					}
				}

				DeleteDC(hdc);
			}
		}

		iff_free(pImage);
	}

	ioWrapper.close(ioWrapper.pData); // Ensure the file was closed.

	return hbmResult;
}

HBITMAP DVP_LoadBitmapW(HWND hWnd,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,HANDLE hAbortEvent)
{
	IffIoFileWrapperData ioData = {0};

	ioData.szFilePath = lpszName;
	ioData.pFile = NULL;
	ioData.hAbortEvent = hAbortEvent;

	iff_file_io ioWrapper = {0};
	ioWrapper.pData = &ioData;
	ioWrapper.open  = IffIoFileWrapperOpen;
	ioWrapper.close = IffIoFileWrapperClose;
	ioWrapper.read  = IffIoFileWrapperRead;
	ioWrapper.seek  = IffIoFileWrapperSeek;
	ioWrapper.tell  = IffIoFileWrapperTell;

	return LoadMain(ioWrapper,lpVPFileInfo);
}

HBITMAP DVP_LoadBitmapStreamW(HWND hWnd,LPSTREAM lpStream,LPTSTR lpszName,LPVIEWERPLUGINFILEINFO lpVPFileInfo,LPSIZE lpszDesiredSize,DWORD dwStreamFlags)
{
	// Use the CFileAndStream wrapper to ensure the stream is seekable.
	LeoUtils::CFileAndStream fas(lpszName, lpStream, (dwStreamFlags & (DVPSF_NoRandomSeek|DVPSF_Slow)) ? true : false);

	IffIoStreamWrapperData ioData = {0};

	if (!fas.GetStream(&ioData.pStream))
	{
		return NULL;
	}

	iff_file_io ioWrapper = {0};
	ioWrapper.pData = &ioData;
	ioWrapper.open  = IffIoStreamWrapperOpen;
	ioWrapper.close = IffIoStreamWrapperClose;
	ioWrapper.read  = IffIoStreamWrapperRead;
	ioWrapper.seek  = IffIoStreamWrapperSeek;
	ioWrapper.tell  = IffIoStreamWrapperTell;

	return LoadMain(ioWrapper,lpVPFileInfo);
}
