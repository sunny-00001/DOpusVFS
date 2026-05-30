#include "StdAfx.h"
#include "LeoHelpers.h"
#include "EmptyImage.h"

CEmptyImage::CEmptyImage(int iWidth, int iHeight, int iScaleMode, const RGBQUAD *prgbBackground)
: CAbstractImage(iScaleMode)
, m_iWidth(iWidth)
, m_iHeight(iHeight)
{
	m_crBackgroundColor = RGB(prgbBackground->rgbRed, prgbBackground->rgbGreen, prgbBackground->rgbBlue);
}

CEmptyImage::~CEmptyImage()
{
}

void CEmptyImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();

	m_crBackgroundColor = crNewBackground;
}

void CEmptyImage::HideAlphaChannel()
{
	// No-op
}

bool CEmptyImage::RotateImage(int rotationAmount)
{
	bool bResult = false;

	rotationAmount %= 360;

	if (0 > rotationAmount) 
	{
		rotationAmount += 360;
	}

	if (0 < rotationAmount)
	{
		DeleteCachedPaintBitmap();

		int origGlobalX = m_iWidth;
		int origGlobalY = m_iHeight;

		if (270 <= rotationAmount)
		{
			// Transpose width and height.
			m_iWidth  = origGlobalY;
			m_iHeight = origGlobalX;
			bResult = true;
		}
		else if (180 <= rotationAmount)
		{
			bResult = true;
		}
		else if (90 <= rotationAmount)
		{
			// Transpose width and height.
			m_iWidth  = origGlobalY;
			m_iHeight = origGlobalX;
			bResult = true;
		}
		else if (0 <= rotationAmount)
		{
			bResult = true;
		}
	}

	return bResult;
}

bool CEmptyImage::InitOK() const
{
	return true;
}

int CEmptyImage::GetWidth() const
{
	return m_iWidth;
}

int CEmptyImage::GetHeight() const
{
	return m_iHeight;
}

const TCHAR *CEmptyImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return NULL;
}

bool CEmptyImage::GetHasUsedTransparency() const
{
	return true;
}

int CEmptyImage::GetDelayTime() const
{
	return 0;
}

COLORREF CEmptyImage::GetCurrentBackgroundColor() const
{
	return m_crBackgroundColor;
}

COLORREF CEmptyImage::GetAutomaticBackgroundColor() const
{
	return m_crBackgroundColor;
}

void CEmptyImage::GetRGBQPixel(RGBQUAD *pPixel, int x, int y)
{
	pPixel->rgbRed   = GetRValue(m_crBackgroundColor);
	pPixel->rgbGreen = GetGValue(m_crBackgroundColor);
	pPixel->rgbBlue  = GetBValue(m_crBackgroundColor);
	pPixel->rgbReserved = 0;
}

void CEmptyImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	RGBQUAD rgb;

	rgb.rgbRed   = GetRValue(m_crBackgroundColor);
	rgb.rgbGreen = GetGValue(m_crBackgroundColor);
	rgb.rgbBlue  = GetBValue(m_crBackgroundColor);
	rgb.rgbReserved = 0;

	const int numPixels = GetWidth() * GetHeight();

	for(int i = 0; i < numPixels; ++i)
	{
		*pDest++ = rgb;
	}
}

void CEmptyImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	RGBQUAD rgb;

	rgb.rgbRed   = GetRValue(m_crBackgroundColor);
	rgb.rgbGreen = GetGValue(m_crBackgroundColor);
	rgb.rgbBlue  = GetBValue(m_crBackgroundColor);
	rgb.rgbReserved = 0;

	RECT rt1;

	if (pWantedRect == NULL)
	{
		pWantedRect = &rt1;
		rt1.left   = 0;
		rt1.top    = 0;
		rt1.right  = GetWidth();
		rt1.bottom = GetHeight();
	}

	StaticFillRGBQPixelRect(pDest, pDestSize, pDestOffset, bTopDown, pWantedRect, &rgb);
}

RGBQUAD *CEmptyImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	*pbDelete = false;
	RGBQUAD *pPixels = NULL;
	
	pPixels = new(std::nothrow) RGBQUAD[ GetWidth() * GetHeight() ];

	if (pPixels != NULL)
	{
		*pbDelete = true;

		CopyRGBQPixelRect(pPixels, NULL, NULL, true, NULL, iDitherOffset, NULL, bGammaEnable, pdGammaValue, GammaTable);
	}

	return pPixels;
}

void CEmptyImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

void CEmptyImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	DeleteCachedPaintBitmap();
}

void CEmptyImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	// *pOffset: top-left image to top-left window.

	const int w = (pImageSize == NULL ? GetWidth()  : pImageSize->cx);
	const int h = (pImageSize == NULL ? GetHeight() : pImageSize->cy);

	RECT r;
	r.left   = -pOffset->cx;
	r.top    = -pOffset->cy;
	r.right  = r.left + w;
	r.bottom = r.top  + h;

	HBRUSH hbr = CreateSolidBrush( m_crBackgroundColor );

	if (NULL != hbr)
	{
		::FillRect(hDC, &r, hbr);

		DeleteObject(hbr);
	}
}
