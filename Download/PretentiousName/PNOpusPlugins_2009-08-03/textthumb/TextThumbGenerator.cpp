#include "stdafx.h"
#include "LeoHelpers.h"
#include "TextThumbConfig.h"
#include "resource.h"
#include "TextFile.h"
#include "TextThumbConfigDlg.h"
#include "TextThumbGenerator.h"

CTextThumbGenerator::CTextThumbGenerator()
{
	InitializeCriticalSection(&m_cs);

	m_pCacheCleanupThread = new CCacheCleanupThread(this);
}

CTextThumbGenerator::~CTextThumbGenerator()
{
	delete m_pCacheCleanupThread;

	DeleteCriticalSection(&m_cs);
}

HBITMAP CTextThumbGenerator::GenerateThumbnail(	const CTextThumbConfig &config, HWND hWnd, CBaseTextFile *pTextFile, LPVIEWERPLUGINFILEINFO lpVPFileInfo,
											    const CTextThumbConfig::CTypeConfig &typeConfig,
												const bool bFillAlphaForDlg, const bool bSimulateIcon, LONG lWidth, LONG lHeight)
{
	HBITMAP hbmResult = NULL;

	if (lWidth  < 32) { lWidth  = 32; }
	if (lHeight < 32) { lHeight = 32; }

	HDC hdcWindow = GetDC(hWnd);

	if (NULL != hdcWindow)
	{
		HDC hDC = CreateCompatibleDC(hdcWindow);

		ReleaseDC(hWnd, hdcWindow);
		hdcWindow = NULL;

		if (NULL != hDC)
		{
			SetMapMode(hDC, MM_TEXT); // Make sure the logical units are pixels.

			const RGBQUAD *pRGBBackground = CacheBackground(hDC, lWidth, lHeight, typeConfig.crBackground);

			if (NULL != pRGBBackground)
			{
				CBackgroundReleaser backgroundReleaser(this, lWidth, lHeight, typeConfig.crBackground);

				BITMAPINFO bitmapInfo;
				RGBQUAD *pPixels;

				ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
				bitmapInfo.bmiHeader.biSize				= sizeof(bitmapInfo.bmiHeader);
				bitmapInfo.bmiHeader.biWidth			= lWidth;
				bitmapInfo.bmiHeader.biHeight			= -lHeight;
				bitmapInfo.bmiHeader.biPlanes			= 1;
				bitmapInfo.bmiHeader.biBitCount			= 32;
				bitmapInfo.bmiHeader.biCompression		= BI_RGB;
				bitmapInfo.bmiHeader.biSizeImage		= 0;
				bitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
				bitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
				bitmapInfo.bmiHeader.biYPelsPerMeter	= 0;
				bitmapInfo.bmiHeader.biClrUsed			= 0;
				bitmapInfo.bmiHeader.biClrImportant		= 0;

				hbmResult = CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pPixels), NULL, 0);

				if (NULL != hbmResult)
				{
					GdiFlush();

					// Copy the background pixels to our new bitmap.
					const LONG lTotalPixels = lWidth * lHeight;
					memcpy(pPixels, pRGBBackground, lTotalPixels * sizeof(RGBQUAD));

					HGDIOBJ hbmOriginal = SelectObject(hDC, hbmResult);

					HRGN hRgn = GenerateRegion(lWidth, lHeight, 1, true);

					if (NULL != hRgn)
					{
						SelectClipRgn(hDC, hRgn); // Clip drawing to inside the jagged region.
					}

					DrawTextIntoCurrentRegion(hDC, lWidth, lHeight, typeConfig, pTextFile);

					if (NULL != hRgn)
					{
						SelectClipRgn(hDC, NULL); // Remove the clipping region.
						DeleteObject(hRgn); // No longer need the region.
					}

					GdiFlush();

					// Restore the alpha channel that was messed up by drawing the text.

					for (LONG i = 0; i < lTotalPixels; i++)
					{
						pPixels[ i ].rgbReserved = pRGBBackground[ i ].rgbReserved;
					}

					if (bFillAlphaForDlg)
					{
						// Draw the image on top of the window's background brush using the alpha channel.
						FillBackgroundForDialog(hDC, hWnd, lWidth, lHeight, pPixels);
					}

					if (bSimulateIcon)
					{
						// Draw an example icon in the corner of the thumbnail.
						DrawSampleIcon(hDC, config.GetDllModule(), pPixels, lWidth, lHeight, typeConfig.strExt);
					}

					SelectObject(hDC, hbmOriginal);
				}
			
				// backgroundReleaser destructor releases cached background here so it can be cleaned up by housekeeping thread.
			}

			DeleteDC(hDC);
		}
	}

	if (NULL != hbmResult && NULL != lpVPFileInfo)
	{
		lpVPFileInfo->dwFlags |= DVPFIF_HasAlphaChannel;
	}

	return(hbmResult);
}

// static
const BYTE *CTextThumbGenerator::GetShadowArray(LONG &lShadow)
{
	static const BYTE shadowArray[ 4 ] = { 255, 100, 33, 0 };
//	static const BYTE shadowArray[ 6 ] = { 255, 125, 100, 75, 33, 0 };
//	static const BYTE shadowArray[ 10 ] = { 255, 200, 175, 150, 125, 100, 75, 50, 33, 0 };
	lShadow = sizeof(shadowArray)/sizeof(shadowArray[0]);
	return shadowArray;
}

// static
HRGN CTextThumbGenerator::GenerateRegion(const LONG lWidth, const LONG lHeight, const LONG lAASize, bool bForText)
{
	std::vector<POINT> vecPoints;

	LONG lShadow;
	GetShadowArray(lShadow);

	const LONG lStepDiv  = 10;
	const LONG lDepthDiv = 25;

	const LONG lRgnLeft = lAASize;
	const LONG lRgnTop  = lAASize;

	const LONG lRgnWidth  = (lWidth - lRgnLeft) - ((lShadow-2)*lAASize + (lAASize > 1 ? 1 : 0));
	const LONG lRgnHeight = (lHeight - lRgnTop) - ((lShadow-2)*lAASize + (lAASize > 1 ? 1 : 0));

	LONG lXY = lRgnHeight / lDepthDiv;
	LONG lYX = lRgnWidth  / lDepthDiv;

	if (lXY < 3) { lXY = 3; }
	if (lYX < 3) { lYX = 3; }

	POINT pt;

	pt.x = lRgnLeft;
	pt.y = lRgnTop;
	vecPoints.push_back( pt );

	for (LONG i = 0; i < lStepDiv; i++)
	{
		pt.x = lRgnLeft + (i * lRgnWidth) / lStepDiv;
		pt.y = lRgnTop + (i%2 ? lRgnHeight : lRgnHeight - lXY);
		vecPoints.push_back( pt );
	}

	for (LONG i = lStepDiv; i >= 0; i--)
	{
		pt.y = lRgnTop + (i * lRgnHeight) / lStepDiv;
		pt.x = lRgnLeft + (i%2 ? lRgnWidth  : lRgnWidth  - lYX);
		vecPoints.push_back( pt );
	}

	// Assumption: &vecPoints[0] is valid. See "C++ FAQ-Lite" for justification.
	HRGN polyRgn = CreatePolygonRgn(&vecPoints[0], static_cast<int>(vecPoints.size()), WINDING);

	if (polyRgn == NULL || !bForText)
	{
		return polyRgn;
	}

	const int textTrim = 1;

	HRGN tempRgn = CreateRectRgn(1,1,1,1);

	if (tempRgn != NULL)
	{
		if (ERROR != CombineRgn(tempRgn, polyRgn, tempRgn, RGN_COPY))
		{
			OffsetRgn(tempRgn, -textTrim, 0);
			CombineRgn(polyRgn, polyRgn, tempRgn, RGN_AND);
			OffsetRgn(tempRgn, +textTrim, -textTrim);
			CombineRgn(polyRgn, polyRgn, tempRgn, RGN_AND);
		}

		DeleteObject(tempRgn);
	}

	return polyRgn;
}

// static
RGBQUAD *CTextThumbGenerator::GenerateBackground(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground)
{
	RGBQUAD *pRGBResult = NULL;

	const LONG lAASize = 4;
	const LONG lAAWidth  = lAASize * lWidth;
	const LONG lAAHeight = lAASize * lHeight;

	// Create the bitmap.
	RGBQUAD *pAAPixels = NULL;

	BITMAPINFO bitmapInfoAA;

	ZeroMemory(&bitmapInfoAA, sizeof(bitmapInfoAA));
	bitmapInfoAA.bmiHeader.biSize			= sizeof(bitmapInfoAA.bmiHeader);
	bitmapInfoAA.bmiHeader.biWidth			= lAAWidth;
	bitmapInfoAA.bmiHeader.biHeight			= -lAAHeight;
	bitmapInfoAA.bmiHeader.biPlanes			= 1;
	bitmapInfoAA.bmiHeader.biBitCount		= 32;
	bitmapInfoAA.bmiHeader.biCompression	= BI_RGB;
	bitmapInfoAA.bmiHeader.biSizeImage		= 0;
	bitmapInfoAA.bmiHeader.biXPelsPerMeter	= 0;
	bitmapInfoAA.bmiHeader.biXPelsPerMeter	= 0;
	bitmapInfoAA.bmiHeader.biYPelsPerMeter	= 0;
	bitmapInfoAA.bmiHeader.biClrUsed		= 0;
	bitmapInfoAA.bmiHeader.biClrImportant	= 0;

	HBITMAP hbmAAResult = CreateDIBSection(hDC, &bitmapInfoAA, DIB_RGB_COLORS, reinterpret_cast<void**>(&pAAPixels), NULL, 0);

	if (NULL != hbmAAResult)
	{
		HGDIOBJ hbmOriginal = SelectObject(hDC, hbmAAResult);

		// Create region representing the jagged page.
		HRGN hAARgn = GenerateRegion(lAAWidth, lAAHeight, lAASize, false);

		if (NULL != hAARgn)
		{
			RECT rectAll;
			SetRect(&rectAll, 0, 0, lAAWidth, lAAHeight);

			// Fill everything with black. We'll draw over this in some places and reduce the alpha in others to create the shadow effect.
			FillRect(hDC, &rectAll, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

			// Clip drawing to inside the jagged region.
			SelectClipRgn(hDC, hAARgn);

			// Gradient-fill the jagged page.
			GradientFillAllWithCurrentRegion(hDC, lAAWidth, lAAHeight, crBackground);

			// Frame the jagged page.
			const int iBrushSize = (lAASize/2);
			FrameRgn(hDC, hAARgn, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), iBrushSize, iBrushSize);

			// Remove clipping region.
			SelectClipRgn(hDC, NULL);

			GdiFlush();

			// Set the alpha channel to make the body opaque, shadows translucent and background transparent.

			RGBQUAD *pAAPixelsShadow = NULL;
			HBITMAP hBmpAAShadow = GenerateShadowMaskDIB(&pAAPixelsShadow, hDC, hAARgn, lAAWidth, lAAHeight, lAASize);

			if (NULL != hBmpAAShadow)
			{
				const LONG lAATotalPixels = lAAWidth * lAAHeight;

				for (LONG i = 0; i < lAATotalPixels; i++)
				{
					pAAPixels[ i ].rgbReserved = pAAPixelsShadow[ i ].rgbBlue;
				}

				DeleteObject(hBmpAAShadow);
			}

			// Scale the image down to the correct size. This gives us antialiasing.
			pRGBResult = LeoHelpers::RGBQAllocateStretchPreserveAlpha(	static_cast<WORD>(lWidth),
																		static_cast<WORD>(lHeight),
																		static_cast<WORD>(lAAWidth),
																		static_cast<WORD>(lAAHeight),
																		pAAPixels );

			DeleteObject(hAARgn);
		}

		SelectObject(hDC, hbmOriginal);
		DeleteObject(hbmAAResult);
	}

	return(pRGBResult);
}

// static
HBITMAP CTextThumbGenerator::GenerateShadowMaskDIB(RGBQUAD **ppPixels, HDC hDC, HRGN hRgn, const LONG lWidth, const LONG lHeight, const LONG lAASize)
{
	(*ppPixels) = NULL;

	LONG lShadow;
	const BYTE *shadowArray = GetShadowArray(lShadow);

	BITMAPINFO bitmapInfo;
	ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
	bitmapInfo.bmiHeader.biSize				= sizeof(bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth			= lWidth;
	bitmapInfo.bmiHeader.biHeight			= -lHeight;
	bitmapInfo.bmiHeader.biPlanes			= 1;
	bitmapInfo.bmiHeader.biBitCount			= 32;
	bitmapInfo.bmiHeader.biCompression		= BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage		= 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter	= 0;
	bitmapInfo.bmiHeader.biClrUsed			= 0;
	bitmapInfo.bmiHeader.biClrImportant		= 0;

	HBITMAP hBitmap = CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(ppPixels), NULL, 0);

	if (NULL != hBitmap && NULL != hRgn && lShadow > 2 && lAASize > 0 && lWidth >= 32 && lHeight >= 32)
	{
		HGDIOBJ hbmOriginal = SelectObject(hDC, hBitmap);

		HBRUSH hBrush = NULL;

		// Start by filling everything with the last shadow alpha value, which should be 0 (transparent).

		RECT rectAll;
		SetRect(&rectAll, 0, 0, lWidth, lHeight);
		hBrush = CreateSolidBrush(RGB(shadowArray[lShadow - 1], shadowArray[lShadow - 1], shadowArray[lShadow - 1]));
		if (NULL != hBrush)
		{
			FillRect(hDC, &rectAll, hBrush);
			DeleteObject(hBrush); hBrush = NULL;
		}

		// Offset the region towards the bottom-right and draw each intermediate shadow level lAASize times, offsetting back towards
		// the original region each time, then draw the first shadow level, which should be 255 (opaque), once in the original position.
		LONG lFullOffset = (lShadow-2) * lAASize; // -2 is for the first and last levels.
		int iRgnResult = OffsetRgn(hRgn, lFullOffset, lFullOffset);

		for (LONG lShadowIndex = (lShadow - 2); lShadowIndex >= 0; --lShadowIndex)
		{
			// "1 : lAASize" is so the opaque level is only drawn once.
			for (LONG lAAIndex = 0; ERROR != iRgnResult && lAAIndex < (lShadowIndex == 0 ? 1 : lAASize); lAAIndex++)
			{
				BYTE alphaValue;
				if (lAAIndex == 0) // Includes the only case where (lShadowIndex == 0).
				{
					alphaValue = shadowArray[lShadowIndex];
				}
				else
				{
					alphaValue = static_cast<BYTE>((shadowArray[lShadowIndex - 1] * lAAIndex + shadowArray[lShadowIndex] * (lAASize - lAAIndex)) / lAASize);
				}

				hBrush = CreateSolidBrush(RGB(alphaValue, alphaValue, alphaValue));
				if (NULL != hBrush)
				{
					FillRgn(hDC, hRgn, hBrush);
					DeleteObject(hBrush); hBrush = NULL;
				}

				if (lShadowIndex != 0)
				{
					iRgnResult = OffsetRgn(hRgn, -1, -1);
				}
			}
		}

		GdiFlush();
		SelectObject(hDC, hbmOriginal);
	}

	return hBitmap;
}

// static 
bool CTextThumbGenerator::DrawTextIntoCurrentRegion(HDC hDC, const LONG lWidth, const LONG lHeight, const CTextThumbConfig::CTypeConfig &typeConfig, CBaseTextFile *pTextFile)
{
	bool bResult = false;

	HFONT hFont = CreateFontIndirect(&typeConfig.logFont);
	HGDIOBJ hfontOriginal = NULL;
	if (NULL != hFont)
	{
		hfontOriginal = SelectObject(hDC, hFont);
	}

	// Get height of each line to work out how many lines we need to read.
	TEXTMETRIC textMetric;
	ZeroMemory(&textMetric, sizeof(textMetric));
	std::wstring wstrText;

	if (GetTextMetrics(hDC, &textMetric)
	&&	pTextFile->ReadLines(&wstrText, lHeight/textMetric.tmHeight+1, 0, L'\n', false, (typeConfig.dwFlags&CTextThumbConfig::TTF_REMOVE_BLANK_LINES_ON ? true : false), false, typeConfig.dwCodePage))
	{
		// Draw text over the background.
		RECT rectText;
		SetRect(&rectText, 4, 2, lWidth, lHeight);
		SetTextColor(hDC, typeConfig.crText);
		SetBkMode(hDC, TRANSPARENT);
		UINT uFormat = DT_LEFT|DT_TOP|DT_NOPREFIX|DT_EXPANDTABS;//|DT_TABSTOP|4<<8;
		if (typeConfig.dwFlags&CTextThumbConfig::TTF_WRAP_ON)
		{
			uFormat |= DT_WORDBREAK;
		}

		if (0 != DrawTextW(hDC, wstrText.c_str(), -1, &rectText, uFormat))
		{
			bResult = true;
		}
	}

	if (NULL != hFont)
	{
		SelectObject(hDC, hfontOriginal);
		DeleteObject(hFont);
	}

	return bResult;
}

// static
bool CTextThumbGenerator::GradientFillAllWithCurrentRegion(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground)
{
	// Gradient fill the page.
	TRIVERTEX gradVerts[4];

	const int r = 0xFF & GetRValue(crBackground);
	const int g = 0xFF & GetGValue(crBackground);
	const int b = 0xFF & GetBValue(crBackground);

	gradVerts[0].x      = 0;
	gradVerts[0].y      = 0;
	gradVerts[0].Red    = ((255*4+r)/5)<<8;
	gradVerts[0].Green  = ((255*4+g)/5)<<8;
	gradVerts[0].Blue   = ((253*4+b)/5)<<8;
	gradVerts[0].Alpha  = 0;

	gradVerts[1].x      = lWidth;
	gradVerts[1].y      = 0;
	gradVerts[1].Red    = ((245*3+r)/4)<<8;
	gradVerts[1].Green  = ((245*3+g)/4)<<8;
	gradVerts[1].Blue   = ((240*3+b)/4)<<8;
	gradVerts[1].Alpha  = 0;

	gradVerts[2].x      = 0;
	gradVerts[2].y      = lHeight;
	gradVerts[2].Red    = ((245*2+r)/3)<<8;
	gradVerts[2].Green  = ((245*2+g)/3)<<8;
	gradVerts[2].Blue   = ((240*2+b)/3)<<8;
	gradVerts[2].Alpha  = 0;

	gradVerts[3].x      = lWidth;
	gradVerts[3].y      = lHeight;
	gradVerts[3].Red    = r<<8;
	gradVerts[3].Green  = g<<8;
	gradVerts[3].Blue   = b<<8;
	gradVerts[3].Alpha  = 0;

	GRADIENT_TRIANGLE gradTris[2];

	gradTris[0].Vertex1 = 0;
	gradTris[0].Vertex2 = 1;
	gradTris[0].Vertex3 = 2;

	gradTris[1].Vertex1 = 1;
	gradTris[1].Vertex2 = 2;
	gradTris[1].Vertex3 = 3;

	if (!LeoHelpers::GradientFillIfColor(hDC, gradVerts, 4, &gradTris, 2, GRADIENT_FILL_TRIANGLE))
	{
		RECT r;
		SetRect(&r, 0, 0, lWidth, lHeight);
		HBRUSH hBrushBackground = CreateSolidBrush(crBackground);

		FillRect(hDC, &r, hBrushBackground != NULL ? hBrushBackground : reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

		if (NULL != hBrushBackground)
		{
			DeleteObject(hBrushBackground);
		}
	}

	return true;
}

// static
bool CTextThumbGenerator::FillBackgroundForDialog(HDC hDC, HWND hWnd, const LONG lWidth, const LONG lHeight, RGBQUAD *pPixels)
{
	bool bResult = false;

	// Get the dialog window's background brush, create a bitmap filled with it
	// and blend that with our bitmap. Note that we shouldn't dispose of this brush.
	HBRUSH hBrush = reinterpret_cast<HBRUSH>(SendMessage(hWnd, WM_CTLCOLORDLG,
								reinterpret_cast<WPARAM>(hDC), reinterpret_cast<LPARAM>(hWnd)));

	if (NULL != hBrush)
	{
		RGBQUAD *pPixelsBackground = NULL;

		BITMAPINFO bitmapInfoBackground;

		ZeroMemory(&bitmapInfoBackground, sizeof(bitmapInfoBackground));
		bitmapInfoBackground.bmiHeader.biSize			= sizeof(bitmapInfoBackground.bmiHeader);
		bitmapInfoBackground.bmiHeader.biWidth			= lWidth;
		bitmapInfoBackground.bmiHeader.biHeight			= -lHeight;
		bitmapInfoBackground.bmiHeader.biPlanes			= 1;
		bitmapInfoBackground.bmiHeader.biBitCount		= 32;
		bitmapInfoBackground.bmiHeader.biCompression	= BI_RGB;
		bitmapInfoBackground.bmiHeader.biSizeImage		= 0;
		bitmapInfoBackground.bmiHeader.biXPelsPerMeter	= 0;
		bitmapInfoBackground.bmiHeader.biXPelsPerMeter	= 0;
		bitmapInfoBackground.bmiHeader.biYPelsPerMeter	= 0;
		bitmapInfoBackground.bmiHeader.biClrUsed		= 0;
		bitmapInfoBackground.bmiHeader.biClrImportant	= 0;

		HBITMAP hbmBackground = CreateDIBSection(hDC, &bitmapInfoBackground, DIB_RGB_COLORS, reinterpret_cast<void**>(&pPixelsBackground), NULL, 0);

		if (NULL != hbmBackground)
		{
			HGDIOBJ hbmOriginalBackground = SelectObject(hDC, hbmBackground);

			RECT r;
			SetRect(&r, 0, 0, lWidth, lHeight);
			FillRect(hDC, &r, hBrush);
			GdiFlush();
			BYTE alpha;

			const LONG lTotalPixels = lWidth * lHeight;

			for (LONG i = 0; i < lTotalPixels; i++)
			{
				if (pPixels[ i ].rgbReserved != 255)
				{
					alpha = pPixels[ i ].rgbReserved;
					pPixels[ i ].rgbRed   = (alpha * pPixels[ i ].rgbRed   + (255 - alpha) * pPixelsBackground[ i ].rgbRed)   / 255;
					pPixels[ i ].rgbGreen = (alpha * pPixels[ i ].rgbGreen + (255 - alpha) * pPixelsBackground[ i ].rgbGreen) / 255;
					pPixels[ i ].rgbBlue  = (alpha * pPixels[ i ].rgbBlue  + (255 - alpha) * pPixelsBackground[ i ].rgbBlue)  / 255;
					pPixels[ i ].rgbReserved = 255;
				}
			}

			bResult = true;

			SelectObject(hDC, hbmOriginalBackground);
			DeleteObject(hbmBackground);
		}
	}

	return bResult;
}

// static
bool CTextThumbGenerator::DrawSampleIcon(HDC hDC, HINSTANCE hInstance, RGBQUAD *pPixels, LONG lWidth, LONG lHeight, const std::wstring &strExt)
{
	bool bResult = false;

	SHFILEINFO sfi = {0};

	if (SHGetFileInfo(strExt.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON|SHGFI_SMALLICON|SHGFI_USEFILEATTRIBUTES)
	||	sfi.hIcon != NULL)
	{
		SIZE sizeIcon;
		sizeIcon.cx = GetSystemMetrics(SM_CXSMICON); // 16
		sizeIcon.cy = GetSystemMetrics(SM_CYSMICON); // 16

		SIZE sizeIconPosn;
		sizeIconPosn.cx = lWidth  - (sizeIcon.cx + 2); // + (16 - sizeIcon.cx)/2;
		sizeIconPosn.cy = lHeight - (sizeIcon.cy + 2); // + (16 - sizeIcon.cy)/2;

		if (DrawIconEx(hDC, sizeIconPosn.cx, sizeIconPosn.cy, sfi.hIcon, 0, 0, 0, NULL, DI_NORMAL))
		{
			GdiFlush();

			for (LONG y = sizeIconPosn.cy; y < sizeIconPosn.cy + sizeIcon.cy; y++)
			{
				for (LONG x = sizeIconPosn.cx; x < sizeIconPosn.cx + sizeIcon.cx; x++)
				{
					pPixels[ y * lWidth + x ].rgbReserved = 255; // Make icon opaque;
				}
			}

			bResult = true;
		}

		DestroyIcon(sfi.hIcon);
	}

	if (!bResult)
	{
		HDC hdcIcon = CreateCompatibleDC(hDC);
		if (NULL != hdcIcon)
		{
			HBITMAP hbmpIcon = reinterpret_cast<HBITMAP>(LoadImage(hInstance, MAKEINTRESOURCE(IDB_HEADTINY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));

			if (NULL != hbmpIcon)
			{
				HGDIOBJ hbmpOriginalIcon = SelectObject(hdcIcon, hbmpIcon);

				SIZE sizeIcon;		// Assumption: Bitmap we loaded is 13x16 and has no transparency.
				sizeIcon.cx = 13;
				sizeIcon.cy = 16;

				SIZE sizeIconPosn;
				sizeIconPosn.cx = lWidth  - (16 + 2) + (16 - sizeIcon.cx)/2;
				sizeIconPosn.cy = lHeight - (16 + 2) + (16 - sizeIcon.cy)/2;

				if (BitBlt(hDC, sizeIconPosn.cx, sizeIconPosn.cy, sizeIcon.cx, sizeIcon.cy, hdcIcon, 0, 0, SRCCOPY))
				{
					GdiFlush();
					for (LONG y = sizeIconPosn.cy; y < sizeIconPosn.cy + sizeIcon.cy; y++)
					{
						for (LONG x = sizeIconPosn.cx; x < sizeIconPosn.cx + sizeIcon.cx; x++)
						{
							pPixels[ y * lWidth + x ].rgbReserved = 255; // Make icon opaque;
						}
					}

					bResult = true;
				}

				SelectObject(hdcIcon, hbmpOriginalIcon);
				DeleteObject(hbmpIcon);
			}

			DeleteDC(hdcIcon);
		}
	}

	return bResult;
}

const RGBQUAD *CTextThumbGenerator::CacheBackground(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground)
{
	const RGBQUAD *pResult = NULL;

	ColorCache *pColorCache = NULL;
	LeoHelpers::CriticalObject< RGBQUAD * > * pPixelObject = NULL;

	// releaser will do nothing until SetGenerator(this) is called. Then it will FlagDone the colorCache if the function exits
	// prematurely or due to error without calling SetGenerator(NULL).
	CBackgroundReleaser backgroundReleaser(NULL, lWidth, lHeight, crBackground);

	{
		LeoHelpers::CriticalSectionScoper css(&m_cs);

		// Creates a new ColorCache for the dimensions, if there wasn't one already.
		pColorCache = &( m_mapSizeToColors[ std::make_pair( lWidth, lHeight ) ] );

		// Flag this width/height as in use so that the ColorCache and CriticalObject/RGBQUAD* objects below it will not be deleted.
		pColorCache->FlagInUse();
		backgroundReleaser.SetGenerator(this); // Release the background on error.

		std::map< COLORREF, LeoHelpers::CriticalObject< RGBQUAD * > * >::iterator pixelIter = pColorCache->m_mapColorToPixels.find(crBackground);

		if (pixelIter != pColorCache->m_mapColorToPixels.end())
		{
			pPixelObject = pixelIter->second;
		}
		else
		{
			pPixelObject = new LeoHelpers::CriticalObject< RGBQUAD * >(NULL);
			pColorCache->m_mapColorToPixels.insert( std::make_pair( crBackground, pPixelObject ) );
		}
	}

	// We've unlocked the main critical section so that multiple backgrounds can be generated at once.
	// Now we lock just the background we're interested in to see if it has been generated and, if not, we generate it.

	{
		// This should be the only place where the pixelObject is locked or changed, except during cleanup.
		// In particular, the pixelObject should only be locked when its parent colorCache is flagged in use,
		// else the housekeeper may delete it regardless of the lock.
		LeoHelpers::CriticalObjectScoper< RGBQUAD * > cos(pPixelObject);

		if (NULL == pPixelObject->object)
		{
			pPixelObject->object = GenerateBackground(hDC, lWidth, lHeight, crBackground);
		}

		pResult = pPixelObject->object;

		if (NULL != pResult)
		{
			backgroundReleaser.SetGenerator(NULL); // Leave the background locked for the caller.
		}
	}

	return pResult;
}

void CTextThumbGenerator::ReleaseBackground(const LONG lWidth, const LONG lHeight, const COLORREF crBackground)
{
	// Ensure the cache cleanup thread is running. If it is stopped or stopping it will be restarted.
	m_pCacheCleanupThread->Start();

	{
		LeoHelpers::CriticalSectionScoper css(&m_cs);

		std::map< std::pair< LONG, LONG >, ColorCache >::iterator iterColorCache = m_mapSizeToColors.find( std::make_pair( lWidth, lHeight ) );

		if (iterColorCache != m_mapSizeToColors.end())
		{
			// Flag that the cacheCleanupThread may reclaim this part of the cache, assuming nothing else has flagged it as in use.
			iterColorCache->second.FlagDone();
		}
	}
}

CTextThumbGenerator::CCacheCleanupThread::CCacheCleanupThread(CTextThumbGenerator *pGenerator)
: LeoHelpers::HousekeepingThread(2000) // Clean up the cache every 2 seconds until it is empty.
, m_pGenerator(pGenerator)
{
}

// virtual
CTextThumbGenerator::CCacheCleanupThread::~CCacheCleanupThread()
{
}

// Once the thread has started, MainTask is called every m_dwThreadIntervalMS until Stop or RequestStop has been called.
// virtual
void CTextThumbGenerator::CCacheCleanupThread::MainTask()
{
	LeoHelpers::CriticalSectionScoper css(&m_pGenerator->m_cs);

	// Delete any colorCache objects in the cache which are no longer in use.

	std::map< std::pair< LONG, LONG >, ColorCache >::iterator iterColorCache = m_pGenerator->m_mapSizeToColors.begin();
	
	while (iterColorCache != m_pGenerator->m_mapSizeToColors.end())
	{
		if (iterColorCache->second.IsExpired(2000))
		{
			iterColorCache = m_pGenerator->m_mapSizeToColors.erase(iterColorCache);
		}
		else
		{
			++iterColorCache;
		}
	}

	// It is essential that m_pGenerator->m_cs remain locked while we check for empty and until after we call RequestStop.

	if (m_pGenerator->m_mapSizeToColors.empty())
	{
		// The cache is now empty and there's no need for a housekeeping thread until new items are added.
		RequestStop(); 
	}
}
