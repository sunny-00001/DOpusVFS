#include "StdAfx.h"
#include "LeoHelpers.h"
#include "AbstractImage.h"

CAbstractImageList::CAbstractImageList()
: m_currentFrameIndex(0)
, m_bHasUsedTransparency(false)
{
}

CAbstractImageList::~CAbstractImageList()
{
	Clear();
}

void CAbstractImageList::Clear()
{
	while(!m_frames.empty())
	{
		std::pair< CAbstractImage *, bool > fbp = m_frames.back();
		m_frames.pop_back();

		if (fbp.second)
		{
			delete fbp.first;
		}
	}

	m_currentFrameIndex = 0;
	m_bHasUsedTransparency = false;
}

bool CAbstractImageList::IsEmpty() const
{
	return m_frames.empty();
}

CAbstractImageList::size_type CAbstractImageList::GetNumberOfFrames() const
{
	return static_cast<size_type>(m_frames.size());
}

CAbstractImageList::size_type CAbstractImageList::GetCurrentIndex() const
{
	return m_currentFrameIndex;
}

void CAbstractImageList::SetCurrentIndex(size_type frameIndex)
{
	if (0 > frameIndex || frameIndex >= static_cast<size_type>(m_frames.size()))
	{
		frameIndex = 0;
	}

	m_currentFrameIndex = frameIndex;
}

void CAbstractImageList::IncrementCurrentIndex(int iDelta)
{
	size_type s = static_cast<size_type>(m_frames.size());

	if (s < 2)
	{
		m_currentFrameIndex = 0;
	}
	else
	{
		if (iDelta < 0)
		{
			size_type stDelta = -iDelta;

			while(m_currentFrameIndex < stDelta)
			{
				m_currentFrameIndex += s;
			}

			m_currentFrameIndex -= stDelta;
		}
		else
		{
			m_currentFrameIndex += iDelta;
		}

		m_currentFrameIndex %= s;
	}
}

CAbstractImage *CAbstractImageList::GetFirstFrame() const
{
	if (m_frames.empty())
	{
		return NULL;
	}

	return m_frames[ 0 ].first;
}

CAbstractImage *CAbstractImageList::GetCurrentFrame() const
{
	if (m_frames.empty())
	{
		return NULL;
	}

	return m_frames[ m_currentFrameIndex ].first;
}

CAbstractImage *CAbstractImageList::GetRelativeFrame(int iDelta) const
{
	if (m_frames.empty())
	{
		return NULL;
	}

	size_type s = static_cast<size_type>(m_frames.size());

	CAbstractImageList::size_type wantedFrameIndex = m_currentFrameIndex;

	if (s < 2)
	{
		wantedFrameIndex = 0;
	}
	else
	{
		if (iDelta < 0)
		{
			size_type stDelta = -iDelta;

			while(wantedFrameIndex < stDelta)
			{
				wantedFrameIndex += s;
			}

			wantedFrameIndex -= stDelta;
		}
		else
		{
			wantedFrameIndex += iDelta;
		}

		wantedFrameIndex %= s;
	}

	return m_frames[ wantedFrameIndex ].first;
}

CAbstractImage *CAbstractImageList::GetFrame(size_type frameIndex) const
{
	if (m_frames.empty())
	{
		return NULL;
	}

	if (0 > frameIndex || frameIndex >= static_cast<size_type>(m_frames.size()))
	{
		frameIndex = 0;
	}

	return m_frames[ frameIndex ].first;
}

void CAbstractImageList::AddFrame(CAbstractImage *pFrame, bool bTakeOwnership)
{
	m_frames.push_back( std::make_pair(pFrame, bTakeOwnership) );

	if (pFrame->GetHasUsedTransparency())
	{
		m_bHasUsedTransparency = true;
	}
}

bool CAbstractImageList::InitOK() const
{
	bool bResult = false;

	for(std::vector< std::pair< CAbstractImage *, bool > >::const_iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		bResult = (*iter).first->InitOK();

		if (!bResult)
		{
			break;
		}
	}

	return bResult;
}

bool CAbstractImageList::GetHasUsedTransparency() const
{
	return m_bHasUsedTransparency;
}

void CAbstractImageList::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		(*iter).first->UpdateViewerBackgroundColor(crNewBackground);
	}
}

void CAbstractImageList::HideAlphaChannel()
{
	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		(*iter).first->HideAlphaChannel();
	}
}

bool CAbstractImageList::RotateImages(int rotationAmount)
{
	bool bResult = true;

	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		if ( ! (*iter).first->RotateImage(rotationAmount) )
		{
			bResult = false;
		}
	}

	return(bResult);
}

void CAbstractImageList::DeleteCachedPaintBitmaps(CAbstractImage *pFrameNoDelete1, CAbstractImage *pFrameNoDelete2)
{
	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		CAbstractImage *pFrame = (*iter).first;

		if (pFrame != pFrameNoDelete1 && pFrame != pFrameNoDelete2)
		{
			pFrame->DeleteCachedPaintBitmap();
		}
	}
}

/*
void CAbstractImageList::GeneratePaintCaches(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		(*iter).first->GeneratePaintCache(hDC, pImageSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
	}
}
*/

bool CAbstractImageList::SetImageSize(const SIZE *pSize)
{
	bool bResult = true;

	for(std::vector< std::pair< CAbstractImage *, bool > >::iterator iter = m_frames.begin(); iter != m_frames.end(); ++iter)
	{
		if (!(*iter).first->SetSize(pSize))
		{
			bResult = false;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CAbstractImage::CAbstractImage(int iScaleMode)
: m_iScaleMode(iScaleMode)
, m_hbmCachedImage(NULL)
, m_bCachedImageIsNotReallyScaled(false)
, m_iCachedImageDitherOffset(0)
, m_bCachedImageGammaEnable(false)
, m_dCachedImageGammaValue(0.0)
{
	m_sizeCachedImage.cx = 0;
	m_sizeCachedImage.cy = 0;

}

// virtual
CAbstractImage::~CAbstractImage()
{
	DeleteCachedPaintBitmap();
}

bool CAbstractImage::SetSize(const SIZE *pSize)
{
	return false;
}

void CAbstractImage::GetRGBQPixel(RGBQUAD *pPixel, int x, int y)
{
	RECT wantedRect;
	wantedRect.left = x;
	wantedRect.right = x + 1;
	wantedRect.top = y;
	wantedRect.bottom = y + 1;

	CopyRGBQPixelRect(pPixel, NULL, NULL, true, &wantedRect, 0, NULL, false, NULL, NULL);
}

void CAbstractImage::DeleteCachedPaintBitmap()
{
	if (NULL != m_hbmCachedImage)
	{
		DeleteObject(m_hbmCachedImage);
		m_hbmCachedImage = NULL;
	}

	m_sizeCachedImage.cx = 0;
	m_sizeCachedImage.cy = 0;
	m_bCachedImageIsNotReallyScaled = false;
	m_iCachedImageDitherOffset = 0;
	m_bCachedImageGammaEnable = false;
	m_dCachedImageGammaValue = 0.0;
}

void CAbstractImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	if (InitOK())
	{
		bool bScaleImage  = (NULL != pImageSize && (pImageSize->cx != GetWidth() || pImageSize->cy != GetHeight()));

		// Delete the cached image if we don't need one or if we need to make a new one.

		if (NULL != m_hbmCachedImage)
		{
			if ((!bScaleImage && 0 == iDitherOffset && !bGammaEnable)
			||	( bScaleImage && (pImageSize->cx != m_sizeCachedImage.cx || pImageSize->cy != m_sizeCachedImage.cy))
			||  (!bScaleImage && (    GetWidth() != m_sizeCachedImage.cx ||    GetHeight() != m_sizeCachedImage.cy))
			||	m_iCachedImageDitherOffset != iDitherOffset
			||	m_bCachedImageGammaEnable != bGammaEnable
			||	(bGammaEnable && pdGammaValue != NULL && (m_dCachedImageGammaValue != *pdGammaValue)))
			{
				DeleteCachedPaintBitmap();
			}
		}

		// Generate a new cached image if required.

		if (NULL == m_hbmCachedImage && (bScaleImage || 0 != iDitherOffset || bGammaEnable))
		{
			bool bDeleteSourcePixels = false;
			RGBQUAD *pSourcePixels = GetRGBQPixels(&bDeleteSourcePixels, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);

			m_iCachedImageDitherOffset = iDitherOffset;
			m_bCachedImageGammaEnable = bGammaEnable;
			m_dCachedImageGammaValue = (pdGammaValue == NULL ? -1.0 : *pdGammaValue);

			if (NULL != pSourcePixels)
			{
				HDC hdcCache = CreateCompatibleDC(hDC);

				if (NULL != hdcCache)
				{
					BITMAPINFO bitmapInfo;
					LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, GetWidth(), GetHeight(), true);

					if (bScaleImage)
					{
						m_hbmCachedImage = CreateCompatibleBitmap(hDC, pImageSize->cx, pImageSize->cy);

						if (NULL != m_hbmCachedImage)
						{
							bool bScaleSuccess = false;

							HGDIOBJ hOldObj = SelectObject(hdcCache, m_hbmCachedImage);

							if (NULL != hOldObj)
							{
								::SetStretchBltMode(hdcCache, m_iScaleMode);
								::SetBrushOrgEx(hdcCache, 0, 0, NULL);

								if (GetHeight() == ::StretchDIBits(hdcCache, 0, 0, pImageSize->cx, pImageSize->cy,
																			 0, 0, GetWidth(), GetHeight(),
																	pSourcePixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY))
								{
									m_sizeCachedImage.cx = pImageSize->cx;
									m_sizeCachedImage.cy = pImageSize->cy;
									bScaleSuccess = true;
								}

								SelectObject(hdcCache, hOldObj);
							}

							if (!bScaleSuccess)
							{
								DeleteObject(m_hbmCachedImage);
								m_hbmCachedImage = NULL;
							}
						}
					}

					if (NULL == m_hbmCachedImage)
					{
						m_hbmCachedImage = CreateCompatibleBitmap(hDC, GetWidth(), GetHeight());

						if (NULL != m_hbmCachedImage)
						{
							bool bCopySuccess = false;

							HGDIOBJ hOldObj = SelectObject(hdcCache, m_hbmCachedImage);

							if (NULL != hOldObj)
							{
								if (GetHeight() == ::SetDIBitsToDevice(hdcCache, 0, 0, GetWidth(), GetHeight(),
															0, 0, 0, GetHeight(), pSourcePixels, &bitmapInfo, DIB_RGB_COLORS))
								{
									m_sizeCachedImage.cx = bScaleImage ? pImageSize->cx : GetWidth();
									m_sizeCachedImage.cy = bScaleImage ? pImageSize->cy : GetHeight();
									m_bCachedImageIsNotReallyScaled = bScaleImage;
									bCopySuccess = true;
								}

								SelectObject(hdcCache, hOldObj);
							}

							if (!bCopySuccess)
							{
								DeleteObject(m_hbmCachedImage);
								m_hbmCachedImage = NULL;
							}
						}
					}

					DeleteDC(hdcCache);
				}

				if (bDeleteSourcePixels)
				{
					DeleteRGBQPixels(pSourcePixels);
					pSourcePixels = NULL;
				}
			}
		}
	}
}

void CAbstractImage::Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// NOTE: GDI scaling functions fail when zooming in a lot on really large images.
	//       Have worked around it to some extent by falling back on scaling onto the window
	//       rather than creating a cached scaled image, but this may still fail for really large images
	//       (and indeed does if the scaling mode is left as default, which will be the case on Win95).
	//       Generating the cached scaled image in tiles would solve the problem but is much harder than it sounds.

	if (InitOK())
	{
		GeneratePaintCache(hDC, pImageSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);

		// Paint the cached or original image.

		if (NULL != m_hbmCachedImage)
		{
			HDC hdcCache = CreateCompatibleDC(hDC);

			if (NULL != hdcCache)
			{
				HGDIOBJ hOldObj = SelectObject(hdcCache, m_hbmCachedImage);

				if (NULL != hOldObj)
				{
					if (m_bCachedImageIsNotReallyScaled)
					{
						::SetStretchBltMode(hDC, m_iScaleMode);
						::SetBrushOrgEx(hDC, 0, 0, NULL);

						// We failed to create a cached scaled image. Scale it directly onto the window now.
						// Note that the cached image may still have dithering or selection done to it, just not scaling.
						if (0 == ::StretchBlt(hDC, -pOffset->cx, -pOffset->cy, m_sizeCachedImage.cx, m_sizeCachedImage.cy,
												hdcCache, 0, 0, GetWidth(), GetHeight(), SRCCOPY))
						{
							// Slightly better than nothing...
							::BitBlt(hDC, -pOffset->cx, -pOffset->cy, GetWidth(), GetHeight(), hdcCache, 0, 0, SRCCOPY);
						}
					}
					else
					{
						::BitBlt(hDC, -pOffset->cx, -pOffset->cy, m_sizeCachedImage.cx, m_sizeCachedImage.cy, hdcCache, 0, 0, SRCCOPY);
					}

					SelectObject(hdcCache, hOldObj);
				}

				DeleteDC(hdcCache);
			}
		}
		else
		{
			PaintUnmodified(hDC, pOffset, pWindowSize, pImageSize);
		}
	}
}

void CAbstractImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	if (InitOK())
	{
		bool bDeleteSourcePixels = false;
		RGBQUAD *pSourcePixels = GetRGBQPixels(&bDeleteSourcePixels, 0, false, NULL, NULL);

		BITMAPINFO bitmapInfo;
		LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, GetWidth(), GetHeight(), true);

		::SetDIBitsToDevice(hDC, -pOffset->cx, -pOffset->cy, GetWidth(), GetHeight(),
							0, 0, 0, GetHeight(), pSourcePixels, &bitmapInfo, DIB_RGB_COLORS);

		if (bDeleteSourcePixels)
		{
			DeleteRGBQPixels(pSourcePixels);
			pSourcePixels = NULL;
		}
	}
}

bool CAbstractImage::CopyToClipboard(HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset)
{
	bool bResult = false;

	RECT selectRect;
	normaliseSelectRect(&selectRect, pSelectRectNotNormalised);

	DWORD dwHeaderAndPadSize = sizeof(BITMAPINFOHEADER);
	DWORD dwSizeImage = sizeof(RGBQUAD) * (selectRect.right - selectRect.left) * (selectRect.bottom - selectRect.top);

	HGLOBAL hDibMemGlobal = GlobalAlloc(GMEM_MOVEABLE, dwHeaderAndPadSize + dwSizeImage);

	if (NULL != hDibMemGlobal)
	{
		BYTE *pDibMemLocked = reinterpret_cast<BYTE *>(GlobalLock(hDibMemGlobal));

		const bool bTopDown = false;

		if (NULL != pDibMemLocked)
		{
			BITMAPINFOHEADER *pHeader = reinterpret_cast<BITMAPINFOHEADER *>(pDibMemLocked);
			RGBQUAD *pPixels = reinterpret_cast<RGBQUAD *>(pDibMemLocked + dwHeaderAndPadSize);

			pHeader->biSize				= sizeof(BITMAPINFOHEADER);
			pHeader->biWidth			= selectRect.right - selectRect.left;
			pHeader->biHeight			= bTopDown ? (selectRect.top - selectRect.bottom) : (selectRect.bottom - selectRect.top); // +ve -> bottom-up bitmap
			pHeader->biPlanes			= 1;
			pHeader->biBitCount			= 32;
			pHeader->biCompression		= BI_RGB;
			pHeader->biSizeImage		= dwSizeImage;
			pHeader->biXPelsPerMeter	= 0;
			pHeader->biYPelsPerMeter	= 0;
			pHeader->biClrUsed			= 0;
			pHeader->biClrImportant		= 0;

			CopyRGBQPixelRect(pPixels, NULL, NULL, bTopDown, &selectRect, iDitherOffset, NULL, false, NULL, NULL);

			GlobalUnlock(hDibMemGlobal);

			if (OpenClipboard(hWnd))
			{
				EmptyClipboard();

				if (NULL != SetClipboardData(CF_DIB, hDibMemGlobal))
				{
					hDibMemGlobal = NULL; // Owned by the system now.
					bResult = true;
				}

				CloseClipboard();
			}
		}

		if (NULL != hDibMemGlobal)
		{
			GlobalFree(hDibMemGlobal);
		}
	}

	if (!bResult)
	{
		MessageBeep(MB_ICONHAND);
	}

	return(bResult);
}

bool CAbstractImage::SetDesktopWallpaper(HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset, DWORD dwWallpaperStyle)
{
	bool bResult = false;

	RECT selectRect;
	normaliseSelectRect(&selectRect, pSelectRectNotNormalised);

	DWORD dwSizeFileHeader = sizeof(BITMAPFILEHEADER);
	DWORD dwSizeInfoHeader = sizeof(BITMAPINFOHEADER);
	DWORD dwSizeImage = sizeof(RGBQUAD) * (selectRect.right - selectRect.left) * (selectRect.bottom - selectRect.top);
	DWORD dwFileSize = dwSizeFileHeader + dwSizeInfoHeader + dwSizeImage;

	BYTE *pBmpFileMem = new(std::nothrow) BYTE[ dwFileSize ];

	if (NULL != pBmpFileMem)
	{
		BITMAPFILEHEADER *pFileHeader = reinterpret_cast<BITMAPFILEHEADER *>(pBmpFileMem);
		BITMAPINFOHEADER *pInfoHeader = reinterpret_cast<BITMAPINFOHEADER *>(pBmpFileMem + dwSizeFileHeader);
		RGBQUAD *pPixels = reinterpret_cast<RGBQUAD *>(pBmpFileMem + dwSizeFileHeader + dwSizeInfoHeader);

		reinterpret_cast<char *>(&pFileHeader->bfType)[0] = 'B';
		reinterpret_cast<char *>(&pFileHeader->bfType)[1] = 'M';
		pFileHeader->bfSize = dwFileSize;
		pFileHeader->bfReserved1 = 0;
		pFileHeader->bfReserved2 = 0;
		pFileHeader->bfOffBits = dwSizeFileHeader + dwSizeInfoHeader;

		pInfoHeader->biSize				= sizeof(BITMAPINFOHEADER);
		pInfoHeader->biWidth			= selectRect.right - selectRect.left;
		pInfoHeader->biHeight			= selectRect.bottom - selectRect.top; // +ve; bottom-up bitmap
		pInfoHeader->biPlanes			= 1;
		pInfoHeader->biBitCount			= 32;
		pInfoHeader->biCompression		= BI_RGB;
		pInfoHeader->biSizeImage		= dwSizeImage;
		pInfoHeader->biXPelsPerMeter	= 0;
		pInfoHeader->biYPelsPerMeter	= 0;
		pInfoHeader->biClrUsed			= 0;
		pInfoHeader->biClrImportant		= 0;

		COLORREF crDesktop = GetSysColor(COLOR_DESKTOP);
		RGBQUAD rgbDesktop;
		rgbDesktop.rgbRed      = GetRValue(crDesktop);
		rgbDesktop.rgbGreen    = GetGValue(crDesktop);
		rgbDesktop.rgbBlue     = GetBValue(crDesktop);
		rgbDesktop.rgbReserved = 0;

		CopyRGBQPixelRect(pPixels, NULL, NULL, false, &selectRect, iDitherOffset, &rgbDesktop, false, NULL, NULL);

		TCHAR szDocumentsPath[_MAX_PATH + 1];

		if (SHGetSpecialFolderPath(hWnd, szDocumentsPath, CSIDL_PERSONAL, TRUE))
		{
			std::wstring strFilePath = szDocumentsPath;
			LeoHelpers::AppendPathString(&strFilePath, L"DOpus_Wallpaper.bmp");

			HANDLE hFile = CreateFile(strFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

            if (NULL != hFile)
			{
				DWORD dwWritten = 0;

				if (WriteFile(hFile, pBmpFileMem, dwFileSize, &dwWritten, NULL)
				&&	dwFileSize == dwWritten)
				{
					CloseHandle(hFile);
					hFile = NULL;

					delete [] pBmpFileMem;
					pBmpFileMem = NULL;

					IActiveDesktop *pActiveDesktop = NULL;
					HRESULT hr = CoCreateInstance(CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER, IID_IActiveDesktop,
													reinterpret_cast<void**>(&pActiveDesktop));

					if (SUCCEEDED(hr) && NULL != pActiveDesktop)
					{
						WALLPAPEROPT wopt;
						wopt.dwSize  = sizeof(WALLPAPEROPT);
						wopt.dwStyle = dwWallpaperStyle;

						if (SUCCEEDED(pActiveDesktop->SetWallpaper(strFilePath.c_str(), 0))
						&&	SUCCEEDED(pActiveDesktop->SetWallpaperOptions(&wopt, 0))
						&&	SUCCEEDED(pActiveDesktop->ApplyChanges(AD_APPLY_REFRESH|AD_APPLY_SAVE)))
						{
							bResult = true;
						}

						pActiveDesktop->Release();
					}
				}

				if (NULL != hFile)
				{
					CloseHandle(hFile);
				}
			}
		}

		delete [] pBmpFileMem;
	}

	if (!bResult)
	{
		MessageBeep(MB_ICONHAND);
	}

	return(bResult);
}

HBITMAP CAbstractImage::CreateDIBSection(HDC hDC, const RECT *pSelectRectNotNormalised, int iDitherOffset)
{
	HBITMAP hResult = NULL;

	if (this->InitOK())
	{
		if (NULL == pSelectRectNotNormalised && 0 == iDitherOffset)
		{
			RGBQUAD *pPixelData = NULL;

			BITMAPINFO bitmapInfo;
			LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, GetWidth(), GetHeight(), true);

			hResult = ::CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pPixelData), NULL, 0);

			if (NULL == hResult)
			{
				MessageBeep(MB_ICONHAND);
			}
			else
			{
				GdiFlush();

				CopyAllRGBQPixels(pPixelData);

				GdiFlush();
			}

			// We used to create a compatible bitmap with CreateDIBitmap but this loses the alpha channel and screws
			// things up on non-32-bit display modes. Returning a DIBSection instead seems to work in all cases.
//			hResult = ::CreateDIBitmap(hDC, &m_bitmapInfo.bmiHeader, CBM_INIT, m_pPixelData, &m_bitmapInfo, DIB_RGB_COLORS);
		}
		else
		{
			RECT selectRect;
			normaliseSelectRect(&selectRect, pSelectRectNotNormalised);

			DWORD dwSizeInfoHeader = sizeof(BITMAPINFOHEADER);
			DWORD dwSizeImage = sizeof(RGBQUAD) * (selectRect.right - selectRect.left) * (selectRect.bottom - selectRect.top);

			BITMAPINFO bitmapInfo;
			bitmapInfo.bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
			bitmapInfo.bmiHeader.biWidth			= selectRect.right - selectRect.left;
			bitmapInfo.bmiHeader.biHeight			= selectRect.bottom - selectRect.top; // +ve; bottom-up bitmap
			bitmapInfo.bmiHeader.biPlanes			= 1;
			bitmapInfo.bmiHeader.biBitCount			= 32;
			bitmapInfo.bmiHeader.biCompression		= BI_RGB;
			bitmapInfo.bmiHeader.biSizeImage		= dwSizeImage;
			bitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
			bitmapInfo.bmiHeader.biYPelsPerMeter	= 0;
			bitmapInfo.bmiHeader.biClrUsed			= 0;
			bitmapInfo.bmiHeader.biClrImportant		= 0;

			RGBQUAD *pPixels = NULL;

			hResult = ::CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pPixels), NULL, 0);

			if (NULL == hResult)
			{
				MessageBeep(MB_ICONHAND);
			}
			else
			{
				GdiFlush();

				CopyRGBQPixelRect(pPixels, NULL, NULL, false, &selectRect, iDitherOffset, NULL, false, NULL, NULL);

				GdiFlush();
			}
		}
	}

	return(hResult);
}

HBITMAP CAbstractImage::CreateThumbnailDIBSection(HDC hDC, const SIZE *pSize, bool bSprockets, bool *pbHasUsedTransparency, bool *pbWantFrame, bool *pbRegenOnResize)
{
	if (!InitOK())
	{
		return NULL;
	}

	if (NULL != pbHasUsedTransparency)
	{
		*pbHasUsedTransparency = false;
	}

	if (NULL != pbWantFrame)
	{
		*pbWantFrame = true;
	}

	if (NULL != pbRegenOnResize)
	{
		*pbRegenOnResize = false;
	}

	HBITMAP hbmResult = NULL;

	SIZE tempSize;

	if (NULL == pSize || 0 == pSize->cx || 0 == pSize->cy)
	{
		tempSize.cx = this->GetWidth();
		tempSize.cy = this->GetHeight();
		pSize = &tempSize;
	}

	SIZE imageSize;
	imageSize.cx = pSize->cx;
	imageSize.cy = pSize->cy;

	int sprocketStripWidth = (5 <= (pSize->cx / 10)) ? (pSize->cx / 10) : 5;
	int sprocketSquare     = sprocketStripWidth / 2;
	int borderHeight       = (2 <= (sprocketStripWidth / 4)) ? (sprocketStripWidth / 4) : 2;

	if (bSprockets)
	{
		imageSize.cx -= (2*sprocketStripWidth);
		imageSize.cy -= (2*borderHeight);
	}

	if (FixAspectRatio(&imageSize))
	{
		RGBQUAD *pSmallPixelData = NULL;
		RGBQUAD *pStretchedPixelData = NULL;
		RGBQUAD *pOriginalPixelData = NULL;
		bool bDeleteOriginalPixelData = false;

		if (imageSize.cx < this->GetWidth() || imageSize.cy < this->GetHeight())
		{
			// Make absolutely sure we don't ever enlarge either dimension, because the enlarging part of stretchPreserveAlpha has been removed.

			if (imageSize.cx > this->GetWidth())
			{
				imageSize.cx = this->GetWidth();
			}

			if (imageSize.cy > this->GetHeight())
			{
				imageSize.cy = this->GetHeight();
			}

			pStretchedPixelData = GetRGBQStretchPreserveAlpha(imageSize.cx, imageSize.cy);
			pSmallPixelData = pStretchedPixelData;
		}
		else
		{
			imageSize.cx = this->GetWidth();
			imageSize.cy = this->GetHeight();

			pOriginalPixelData = GetRGBQPixels(&bDeleteOriginalPixelData, 0, false, NULL, NULL);
			pSmallPixelData = pOriginalPixelData;
		}

		if (NULL != pSmallPixelData)
		{
			if (bSprockets)
			{
				if (NULL != pbHasUsedTransparency)
				{
					*pbHasUsedTransparency = true; // Sprockets always use alpha, even if the image itself doesn't.
				}

				if (NULL != pbWantFrame)
				{
					*pbWantFrame = false;
				}

				if (NULL != pbRegenOnResize)
				{
					*pbRegenOnResize = true;
				}

				RGBQUAD *pDIBPixelData = NULL;

				SIZE thumbSize;
				thumbSize.cx = imageSize.cx + (2*sprocketStripWidth);
				thumbSize.cy = imageSize.cy + (2*borderHeight);

				BITMAPINFO bitmapInfo;
				LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, thumbSize.cx, thumbSize.cy, true);

				hbmResult = ::CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pDIBPixelData), NULL, 0);

				if (NULL == hbmResult)
				{
					MessageBeep(MB_ICONHAND);
				}
				else if (pDIBPixelData != NULL)
				{
					GdiFlush();

					StaticCopyRGBQPixelsWithSprockets(	pDIBPixelData, &thumbSize, pSmallPixelData, &imageSize,
														sprocketStripWidth, sprocketSquare, borderHeight,
														GetAutomaticBackgroundColor(),
														GetHasUsedTransparency() ? 0 : 128);//255);

					GdiFlush();
				}

			}
			else
			{
				if (NULL != pbHasUsedTransparency)
				{
					*pbHasUsedTransparency = GetHasUsedTransparency();
				}

				RGBQUAD *pDIBPixelData = NULL;

				BITMAPINFO bitmapInfo;
				LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, imageSize.cx, imageSize.cy, true);

				hbmResult = ::CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pDIBPixelData), NULL, 0);

				if (NULL == hbmResult)
				{
					MessageBeep(MB_ICONHAND);
				}
				else if (pDIBPixelData != NULL)
				{
					GdiFlush();

					RGBQUAD *pDest = pDIBPixelData;
					const RGBQUAD *pSourcePixelData = pSmallPixelData;

					int iNumPixels = imageSize.cx * imageSize.cy;
					while(iNumPixels-- > 0)
					{
						*pDest++ = *pSourcePixelData++;
					}

					GdiFlush();
				}
			}
		}

		if (NULL != pStretchedPixelData)
		{
			DeleteRGBQStretchedPixels(pStretchedPixelData);
		}

		if (NULL != pOriginalPixelData && bDeleteOriginalPixelData)
		{
			DeleteRGBQPixels(pOriginalPixelData);
		}
	}

	return(hbmResult);
}

bool CAbstractImage::Print(HDC hDCPrint, HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset, LeoHelpers::OpusStringLoader *pSL)
{
	bool bResult = false;

	RECT selectRect;
	normaliseSelectRect(&selectRect, pSelectRectNotNormalised);

	DWORD dwSizeHeader = sizeof(BITMAPINFOHEADER); // no colour table
	DWORD dwSizeImage = sizeof(RGBQUAD) * (selectRect.right - selectRect.left) * (selectRect.bottom - selectRect.top);

	BYTE *pBmpMem = new(std::nothrow) BYTE[ dwSizeHeader + dwSizeImage ];

	if (NULL != pBmpMem)
	{
		BITMAPINFO *pBmInfo = reinterpret_cast<BITMAPINFO *>(pBmpMem);
		RGBQUAD *pPixels = reinterpret_cast<RGBQUAD *>(pBmpMem + dwSizeHeader);

		pBmInfo->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
		pBmInfo->bmiHeader.biWidth			= selectRect.right - selectRect.left;
		pBmInfo->bmiHeader.biHeight			= selectRect.bottom - selectRect.top; // +ve; bottom-up bitmap
		pBmInfo->bmiHeader.biPlanes			= 1;
		pBmInfo->bmiHeader.biBitCount		= 32;
		pBmInfo->bmiHeader.biCompression	= BI_RGB;
		pBmInfo->bmiHeader.biSizeImage		= dwSizeImage;
		pBmInfo->bmiHeader.biXPelsPerMeter	= 0;
		pBmInfo->bmiHeader.biYPelsPerMeter	= 0;
		pBmInfo->bmiHeader.biClrUsed		= 0;
		pBmInfo->bmiHeader.biClrImportant	= 0;

		CopyRGBQPixelRect(pPixels, NULL, NULL, false, &selectRect, iDitherOffset, NULL, false, NULL, NULL);

		int iDevCaps = GetDeviceCaps(hDCPrint, RASTERCAPS);

		if (!(iDevCaps&RC_STRETCHDIB /*&& iDevCaps&RC_BITBLT && iDevCaps&RC_BITMAP64*/))
		{
			MessageBox(hWnd, pSL->Get(STR_GIFANIM_PRINTER_ABILITIES), pSL->Get(STR_GIFANIM_PLUGIN_DESCRIPTION), MB_OK | MB_ICONERROR);
		}
		else
		{
			HDC hDCWindow = GetDC(hWnd);

			if (NULL != hDCWindow)
			{
				int iWindowLogPixX = GetDeviceCaps(hDCWindow,LOGPIXELSX);
				int iWindowLogPixY = GetDeviceCaps(hDCWindow,LOGPIXELSY);
				int iPrintLogPixX  = GetDeviceCaps(hDCPrint, LOGPIXELSX);
				int iPrintLogPixY  = GetDeviceCaps(hDCPrint, LOGPIXELSY);

				int iMultX = iPrintLogPixX;
				int iDivX  = iWindowLogPixX;
				int iMultY = iPrintLogPixY;
				int iDivY  = iWindowLogPixY;

				if (iMultX < iDivX)
				{
					iMultX = iWindowLogPixX;
					iDivX  = iPrintLogPixX;
				}

				if (iMultY < iDivY)
				{
					iMultY = iWindowLogPixY;
					iDivY  = iPrintLogPixY;
				}

				int iPrintResX = GetDeviceCaps(hDCPrint, HORZRES);
				int iPrintResY = GetDeviceCaps(hDCPrint, VERTRES);

				int iScaledX = LeoHelpers::MulDivRoundDown(pBmInfo->bmiHeader.biWidth,  iMultX, iDivX);
				int iScaledY = LeoHelpers::MulDivRoundDown(pBmInfo->bmiHeader.biHeight, iMultY, iDivY);

				if (iScaledX > iPrintResX || iScaledY > iPrintResY)
				{
					if ( LeoHelpers::MulDivRoundDown(iScaledY, iPrintResX, iScaledX) <= iPrintResY )
					{
						iScaledY = LeoHelpers::MulDivRoundDown(iScaledY, iPrintResX, iScaledX);
						iScaledX = LeoHelpers::MulDivRoundDown(iScaledX, iPrintResX, iScaledX);
					}
					else
					{
						iScaledX = LeoHelpers::MulDivRoundDown(iScaledX, iPrintResY, iScaledY);
						iScaledY = LeoHelpers::MulDivRoundDown(iScaledY, iPrintResY, iScaledY);
					}
				}

				int iOffsetX = (iPrintResX - iScaledX) / 2;
				int iOffsetY = (iPrintResY - iScaledY) / 2;

				int iLinesDone = ::StretchDIBits(hDCPrint,
													iOffsetX, iOffsetY, iScaledX,                   iScaledY,
													0,        0,        pBmInfo->bmiHeader.biWidth, pBmInfo->bmiHeader.biHeight,
													pPixels, pBmInfo, DIB_RGB_COLORS, SRCCOPY);

				if (pBmInfo->bmiHeader.biHeight != iLinesDone)
				{
					MessageBox(hWnd, pSL->Get(STR_GIFANIM_PRINTER_ABILITIES), pSL->Get(STR_GIFANIM_PLUGIN_DESCRIPTION), MB_OK | MB_ICONERROR);
				}
				else
				{
					bResult = true;
				}

				ReleaseDC(hWnd, hDCWindow);
			}
		}

		delete [] pBmpMem;
	}

	return(bResult);
}

int CAbstractImage::GetScaleMode() const
{
	return m_iScaleMode;
}

bool CAbstractImage::FixAspectRatio(SIZE *pSize)
{
	bool bResult = false;

	if (NULL != pSize && 0 != pSize->cx && 0 != pSize->cy)
	{
		LONG x = LeoHelpers::MulDivRoundDown(pSize->cy, this->GetWidth(),  this->GetHeight());
		LONG y = LeoHelpers::MulDivRoundDown(pSize->cx, this->GetHeight(), this->GetWidth());

		if (0 < x && x <= pSize->cx)
		{
			pSize->cx = x;
			bResult = true;
		}
		else if (0 < y && y <= pSize->cy)
		{
			pSize->cy = y;
			bResult = true;
		}
	}

	return(bResult);
}

// static
void CAbstractImage::StaticCopyRGBQPixelsWithSprockets(RGBQUAD *pDestRGBQPixels, const SIZE *pDestSize, const RGBQUAD *pSourceRGBQPixels, const SIZE *pSourceSize, int inSprocketStripWidth, int inSprocketSquare, int inBorderHeight, COLORREF bgCol, BYTE fillAlpha)
{
	SprocketData sd;
	sd.oldWidth             = pSourceSize->cx;
	sd.oldHeight            = pSourceSize->cy;
	sd.newWidth				= pDestSize->cx;
	sd.newHeight			= pDestSize->cy;
	sd.sprocketStripWidth   = inSprocketStripWidth;
	sd.sprocketSquare       = inSprocketSquare;
	sd.sprocketGap			= sd.sprocketSquare + sd.sprocketSquare/2;
	sd.sprocketOffsetLeft   = (sd.sprocketStripWidth -  sd.sprocketSquare) / 2;
	sd.sprocketOffsetRight  = (sd.sprocketStripWidth - (sd.sprocketSquare + sd.sprocketOffsetLeft));
	sd.borderHeight         = inBorderHeight;
	sd.imageAndOffsetWidth  = (pDestSize->cx - 2*sd.sprocketStripWidth);
	sd.imageAndOffsetHeight = (pDestSize->cy - 2*sd.borderHeight);
	sd.offsetLeft           = (sd.imageAndOffsetWidth  -  sd.oldWidth ) / 2;
	sd.offsetRight          = (sd.imageAndOffsetWidth  - (sd.oldWidth + sd.offsetLeft));
	sd.offsetTop            = (sd.imageAndOffsetHeight -  sd.oldHeight) / 2;
	sd.offsetBottom         = (sd.imageAndOffsetHeight - (sd.oldHeight + sd.offsetTop));

	sd.rgbBackground.rgbRed      = GetRValue(bgCol);
	sd.rgbBackground.rgbGreen    = GetGValue(bgCol);
	sd.rgbBackground.rgbBlue     = GetBValue(bgCol);
	sd.rgbBackground.rgbReserved = fillAlpha;

	sd.rgbSprocket.rgbRed      = 0;
	sd.rgbSprocket.rgbGreen    = 0;
	sd.rgbSprocket.rgbBlue     = 0;
	sd.rgbSprocket.rgbReserved = 255;

	sd.rgbSprocketGap.rgbRed      = 255;
	sd.rgbSprocketGap.rgbGreen    = 255;
	sd.rgbSprocketGap.rgbBlue     = 255;
	sd.rgbSprocketGap.rgbReserved = 0;

	sd.rgbSprocketShadow1.rgbRed      = 0;
	sd.rgbSprocketShadow1.rgbGreen    = 0;
	sd.rgbSprocketShadow1.rgbBlue     = 0;
	sd.rgbSprocketShadow1.rgbReserved = 100;

	// Leo 16/Apr/2009: Fix divide by zero errors with very wide/short images
	if ((sd.sprocketGap + sd.sprocketSquare) > 0)
	{
		const int numSprockets = sd.newHeight / (sd.sprocketGap + sd.sprocketSquare);

		if (numSprockets > 0)
		{
			sd.sprocketGap = (sd.newHeight - (numSprockets * sd.sprocketSquare)) / numSprockets;
		}
	}

	sd.iSprocketY = -(sd.sprocketGap/2);
	sd.x = 0;
	sd.pOld = NULL;
	sd.pNew = NULL;

	if (NULL != pDestRGBQPixels)
	{
		// Draw new image buffer with sprockets around the old image.
		sd.pOld = pSourceRGBQPixels;
		sd.pNew = pDestRGBQPixels;
		int y;

		for (y = 0; y < sd.borderHeight-1; y++) { drawSprocketBorderLine    (sd); }
		for (y = 0; y < 1;                 y++) { drawSprocketBorderGap     (sd); }
		for (y = 0; y < sd.offsetTop;      y++) { drawSprocketBlankImageLine(sd); }
		for (y = 0; y < sd.oldHeight;      y++) { drawSprocketImageLine     (sd); }
		for (y = 0; y < sd.offsetBottom;   y++) { drawSprocketBlankImageLine(sd); }
		for (y = 0; y < 1;                 y++) { drawSprocketBorderGap     (sd); }
		for (y = 0; y < sd.borderHeight-1; y++) { drawSprocketBorderLine    (sd); }
	}
}

// virtual
void CAbstractImage::DeleteRGBQStretchedPixels(RGBQUAD *pPixels)
{
	delete[] pPixels;
}

// virtual
RGBQUAD *CAbstractImage::GetRGBQStretchPreserveAlpha(int iNewWidth, int iNewHeight)
{
	if (!InitOK())
	{
		return NULL;
	}

	bool bDeletePixels = false;
	RGBQUAD *pPixels = GetRGBQPixels(&bDeletePixels, 0, false, NULL, NULL);

	if (pPixels == NULL)
	{
		return NULL;
	}

	RGBQUAD *pStretchPixels = LeoHelpers::RGBQAllocateStretchPreserveAlpha(iNewWidth,  iNewHeight, GetWidth(), GetHeight(), pPixels);

	if (bDeletePixels)
	{
		DeleteRGBQPixels(pPixels);
	}

	return pStretchPixels;
}

void CAbstractImage::StaticFillRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, const RGBQUAD *prgbFill)
{
	assert(pWantedRect != NULL);

	if (prgbFill != NULL && pWantedRect != NULL)
	{
		SIZE st1;
		SIZE st2;

		if (pDestSize == NULL)
		{
			pDestSize = &st1;
			st1.cx = pWantedRect->right - pWantedRect->left;
			st1.cy = pWantedRect->bottom - pWantedRect->top;
		}

		if (pDestOffset == NULL)
		{
			pDestOffset = &st2;
			st2.cx = 0;
			st2.cy = 0;
		}

		if (bTopDown)
		{
			pDest += (pDestOffset->cx + pDestSize->cx * pDestOffset->cy);
		}
		else
		{
			pDest += (pDestOffset->cx + pDestSize->cx * (pDestSize->cy - (pDestOffset->cy + (pWantedRect->bottom - pWantedRect->top))));
		}

		const int yStart = pWantedRect->top;
		const int yEnd   = pWantedRect->bottom;

		const int xStart = pWantedRect->left;
		const int xEnd   = pWantedRect->right;

		if (yStart < yEnd && xStart < xEnd)
		{
			RGBQUAD *pRow = pDest;

			for (int x = xStart; x < xEnd; ++x)
			{
				*pRow++ = *prgbFill;
			}

			const size_t memSize = (xEnd - xStart) * sizeof(RGBQUAD);

			for (int y = yStart+1; y < yEnd; ++y)
			{
				pRow = pDest;
				pDest += pDestSize->cx;
				memcpy(pDest, pRow, memSize);
			}
		}
	}
}

void CAbstractImage::StaticCopyRGBQPixelRectTopDownToTopDown(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, const RGBQUAD *pSource, const SIZE *pSourceSize)
{
	pDest += (pDestOffset->cx + pDestSize->cx * pDestOffset->cy);

	const int w = pSourceSize->cx;
	const int h = pSourceSize->cy;

	if ((w + pDestOffset->cx) <= pDestSize->cx
	&&	(h + pDestOffset->cy) <= pDestSize->cy)
	{
		for (int y = 0; y < h; ++y)
		{
			memcpy(pDest, pSource, w * sizeof(RGBQUAD));
			pDest   += pDestSize->cx;
			pSource += pSourceSize->cx;
		}
	}
}

// pPixels and pDest may point to the same buffer.
bool CAbstractImage::StaticBlurPreserveAlpha(RGBQUAD *pDest, const RGBQUAD *pSource, int w, int h, const RGBQUAD rgbqBackground)
{
	bool bResult = false;

	const int weights[4] = { 100, 61, 14, 1 };
	const int weightC = 100;
	const int weightT = 1 + 14 + 61 + 100 + 61 + 14 + 1;

	int yoffsets[4];

	yoffsets[0] = 0;
	for (int i = 1; i < 4; ++i)
	{
		yoffsets[i] = yoffsets[i-1] + w;
	}

	int r;
	int g;
	int b;
	int a;

	const RGBQUAD *pInput = pSource;
	RGBQUAD *pOutput;
	const RGBQUAD *pSample;

	RGBQUAD *pTemp = new(std::nothrow) RGBQUAD[ w * h ];

	if (pTemp != NULL)
	{
		pOutput = pTemp;

		for (int y = 0; y < h; ++y)
		{
			for (int x = 0, z = w-1; x < w; ++x, --z, ++pInput, ++pOutput)
			{
				r = pInput->rgbRed      * weightC;
				g = pInput->rgbGreen    * weightC;
				b = pInput->rgbBlue     * weightC;
				a = pInput->rgbReserved * weightC;

				for (int s = 1; s < 4; ++s)
				{
					pSample = (x > s ? pInput - s : &rgbqBackground);
					r += pSample->rgbRed      * weights[s];
					g += pSample->rgbGreen    * weights[s];
					b += pSample->rgbBlue     * weights[s];
					a += pSample->rgbReserved * weights[s];

					pSample = (z > s ? pInput + s : &rgbqBackground);
					r += pSample->rgbRed      * weights[s];
					g += pSample->rgbGreen    * weights[s];
					b += pSample->rgbBlue     * weights[s];
					a += pSample->rgbReserved * weights[s];
				}

				pOutput->rgbRed      = r / weightT;
				pOutput->rgbGreen    = g / weightT;
				pOutput->rgbBlue     = b / weightT;
				pOutput->rgbReserved = a / weightT;
		}
	}

		pInput = pTemp;
		pOutput = pDest;

		for (int y = 0, z = h-1; y < h; ++y, --z)
		{
			for (int x = 0; x < w; ++x, ++pInput, ++pOutput)
			{
				*pOutput = *pInput;

				r = pInput->rgbRed      * weightC;
				g = pInput->rgbGreen    * weightC;
				b = pInput->rgbBlue     * weightC;
				a = pInput->rgbReserved * weightC;

				for (int s = 1; s < 4; ++s)
				{
					pSample = (y > s ? pInput - yoffsets[s] : &rgbqBackground);
					r += pSample->rgbRed      * weights[s];
					g += pSample->rgbGreen    * weights[s];
					b += pSample->rgbBlue     * weights[s];
					a += pSample->rgbReserved * weights[s];

					pSample = (z > s ? pInput + yoffsets[s] : &rgbqBackground);
					r += pSample->rgbRed      * weights[s];
					g += pSample->rgbGreen    * weights[s];
					b += pSample->rgbBlue     * weights[s];
					a += pSample->rgbReserved * weights[s];
				}

				pOutput->rgbRed      = r / weightT;
				pOutput->rgbGreen    = g / weightT;
				pOutput->rgbBlue     = b / weightT;
				pOutput->rgbReserved = a / weightT;
			}
				}

		bResult = true;
	}

	delete [] pTemp;

	return bResult;
}
