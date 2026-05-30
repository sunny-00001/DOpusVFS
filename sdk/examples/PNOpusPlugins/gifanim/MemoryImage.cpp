#include "StdAfx.h"
#include "LeoHelpers.h"
#include "MemoryImage.h"

CMemoryImage::CMemoryImage(int iWidth, int iHeight, int iScaleMode)
: CAbstractImage(iScaleMode)
, m_pPixelData(NULL)
, m_iWidth(iWidth)
, m_iHeight(iHeight)
, m_bTransparencyActuallyUsed(false)
, m_crCurrentBackgroundColor(RGB(0,0,0))
, m_crAutomaticBackgroundColor(RGB(0,0,0))
, m_szImageTypeName(NULL)
{
	if (0 < m_iWidth && 0 < m_iHeight)
	{
		m_pPixelData = new(std::nothrow) RGBQUAD[ m_iWidth * m_iHeight ];
	}
}

CMemoryImage::CMemoryImage(CAbstractImage *pImageToCopy, RECT *pRectToCopy, const wchar_t *szImageTypeName)
: CAbstractImage(pImageToCopy->GetScaleMode())
, m_pPixelData(NULL)
, m_iWidth(0)
, m_iHeight(0)
, m_bTransparencyActuallyUsed( pImageToCopy->GetHasUsedTransparency())
, m_crCurrentBackgroundColor(  pImageToCopy->GetCurrentBackgroundColor())
, m_crAutomaticBackgroundColor(pImageToCopy->GetAutomaticBackgroundColor())
, m_szImageTypeName(NULL)
{	
	assert(szImageTypeName != NULL);

	if (szImageTypeName != NULL)
	{
		m_szImageTypeName = new wchar_t[wcslen(szImageTypeName) + 1];
		wcscpy_s(m_szImageTypeName, wcslen(szImageTypeName) + 1, szImageTypeName);
	}

	RECT rectFullImage;
	::SetRect(&rectFullImage, 0, 0, pImageToCopy->GetWidth(), pImageToCopy->GetHeight());

	if (pRectToCopy == NULL)
	{
		pRectToCopy = &rectFullImage;
	}

	m_iWidth  = pRectToCopy->right - pRectToCopy->left;
	m_iHeight = pRectToCopy->bottom - pRectToCopy->top;

	if (pRectToCopy->left >= 0
	&&	pRectToCopy->top  >= 0
	&&	m_iWidth  >  0
	&&	m_iHeight >  0
	&&	m_iWidth  <= rectFullImage.right
	&&	m_iHeight <= rectFullImage.bottom)
	{
		m_pPixelData = new(std::nothrow) RGBQUAD[ m_iWidth * m_iHeight ];

		if (m_pPixelData != NULL)
		{
			pImageToCopy->CopyRGBQPixelRect(m_pPixelData, NULL, NULL, true, pRectToCopy, 0, NULL, false, NULL, NULL);

			if (m_bTransparencyActuallyUsed)
			{
				m_bTransparencyActuallyUsed = false;

				// Confirm that there are still transparent pixels in the region we have copied.
				int iTotalPixels = m_iWidth * m_iHeight;

				for(int i = 0; i < iTotalPixels; ++i)
				{
					if (m_pPixelData[i].rgbReserved == 0)
					{
						m_bTransparencyActuallyUsed = true;
						break;
					}
				}
			}
		}
	}
}

CMemoryImage::~CMemoryImage(void)
{
	delete [] m_pPixelData;
	m_pPixelData = NULL;

	delete [] m_szImageTypeName;
	m_szImageTypeName = NULL;
}

void CMemoryImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	if (InitOK())
	{
		int iNumPixels = m_iWidth * m_iHeight;

		RGBQUAD *pSource = m_pPixelData;

		while(iNumPixels-- > 0)
		{
			*pDest++ = *pSource++;
		}
	}
}

void CMemoryImage::GetRGBQPixel(RGBQUAD *pPixel, int x, int y)
{
	if (InitOK() && x >= 0 && y >= 0 && x < m_iWidth && y < m_iHeight)
	{
		*pPixel = m_pPixelData[ x + y * m_iHeight ];
	}
}

void CMemoryImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	if (InitOK())
	{
		const int w = m_iWidth;
		const int h = m_iHeight;

		RECT rt1;
		SIZE st1;
		SIZE st2;

		if (pWantedRect == NULL)
		{
			pWantedRect = &rt1;
			rt1.left   = 0;
			rt1.top    = 0;
			rt1.right  = w;
			rt1.bottom = h;
		}

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

		const int iDestLineOffset = pDestSize->cx - (pWantedRect->right - pWantedRect->left);

		if (bTopDown)
		{
			pDest += (pDestOffset->cx + pDestSize->cx * pDestOffset->cy);
		}
		else
		{
			pDest += (pDestOffset->cx + pDestSize->cx * (pDestSize->cy - (pDestOffset->cy + (pWantedRect->bottom - pWantedRect->top))));
		}

		if (!bGammaEnable)
		{
			GammaTable = NULL;
		}

		if (bTopDown)
		{
			const int yStart = pWantedRect->top;
			const int yEnd   = pWantedRect->bottom;

			const int xStart = pWantedRect->left;
			const int xEnd   = pWantedRect->right;

			const int iSourceLineOffset = w - (xEnd - xStart);

			const RGBQUAD *pSource = m_pPixelData + xStart + yStart * w;

			for (int y = yStart; y < yEnd; ++y)
			{
				for (int x = xStart; x < xEnd; ++x)
				{
					copyPixelWithDither(pDest++, pSource++, x, y, w, h, iDitherOffset, pRGBForTransparent, GammaTable);
				}

				pDest   += iDestLineOffset;
				pSource += iSourceLineOffset;
			}
		}
		else
		{
			const int yStart = pWantedRect->bottom;
			const int yEnd   = pWantedRect->top;

			const int xStart = pWantedRect->left;
			const int xEnd   = pWantedRect->right;

			const int iSourceLineOffset = -(w + (xEnd - xStart));

			const RGBQUAD *pSource = m_pPixelData + xStart + (yStart - 1) * w;

			for (int y = yStart - 1; y >= yEnd; --y)
			{
				for (int x = xStart; x < xEnd; ++x)
				{
					copyPixelWithDither(pDest++, pSource++, x, y, w, h, iDitherOffset, pRGBForTransparent, GammaTable);
				}

				pDest   += iDestLineOffset;
				pSource += iSourceLineOffset;
			}
		}
	}
}

RGBQUAD *CMemoryImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	RGBQUAD *pRGBQPixels = NULL;
	*pbDelete = false;

	if (InitOK())
	{
		if (0 == iDitherOffset && !bGammaEnable)
		{
			pRGBQPixels = m_pPixelData;
		}
		else
		{
			const int iNumPixels = m_iWidth * m_iHeight;

			pRGBQPixels = new(std::nothrow) RGBQUAD[ iNumPixels ];

			if (NULL != pRGBQPixels)
			{
				*pbDelete = true;

				CopyRGBQPixelRect(pRGBQPixels, NULL, NULL, true, NULL, iDitherOffset, NULL, bGammaEnable, pdGammaValue, GammaTable);
			}
		}
	}

	return pRGBQPixels;
}

void CMemoryImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

RGBQUAD *CMemoryImage::GetRGBQStretchPreserveAlpha(int iNewWidth, int iNewHeight)
{
	if (!InitOK())
	{
		return NULL;
	}

	return LeoHelpers::RGBQAllocateStretchPreserveAlpha(iNewWidth, iNewHeight, m_iWidth, m_iHeight, m_pPixelData);
}

void CMemoryImage::DeleteRGBQStretchedPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

COLORREF CMemoryImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_crCurrentBackgroundColor;
}

COLORREF CMemoryImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_crAutomaticBackgroundColor;
}

void CMemoryImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	if (InitOK())
	{
		DeleteCachedPaintBitmap();

		m_crCurrentBackgroundColor = crNewBackground;

		if (m_bTransparencyActuallyUsed)
		{
			const int totalPixels = m_iWidth * m_iHeight;

			RGBQUAD transPixel;
			transPixel.rgbRed   = GetRValue(crNewBackground);
			transPixel.rgbGreen = GetGValue(crNewBackground);
			transPixel.rgbBlue  = GetBValue(crNewBackground);
			transPixel.rgbReserved = 0;

			for (int i = 0; i < totalPixels; ++i)
			{
				if ( 0 == m_pPixelData[ i ].rgbReserved )
				{
					// This pixel is transparent. Set it to the new background color.
					m_pPixelData[ i ] = transPixel;
				}
			}
		}
	}
}

void CMemoryImage::HideAlphaChannel()
{
	if (InitOK() && GetHasUsedTransparency())
	{
		UpdateViewerBackgroundColor(GetAutomaticBackgroundColor());
	}
}

bool CMemoryImage::RotateImage(int rotationAmount)
{
	bool bResult = false;

	rotationAmount %= 360;

	if (0 > rotationAmount)
	{
		rotationAmount += 360;
	}

	if (InitOK() && 0 < rotationAmount)
	{
		DeleteCachedPaintBitmap();

		int origGlobalX = m_iWidth;
		int origGlobalY = m_iHeight;

		if (270 <= rotationAmount)
		{
			if (rotate270( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				// Transpose width and height.
				m_iWidth  = origGlobalY;
				m_iHeight = origGlobalX;

				bResult = true;
			}
		}
		else if (180 <= rotationAmount)
		{
			if (rotate180( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				bResult = true;
			}
		}
		else if (90 <= rotationAmount)
		{
			if (rotate90( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				// Transpose width and height.
				m_iWidth  = origGlobalY;
				m_iHeight = origGlobalX;

				bResult = true;
			}
		}
		else if (0 <= rotationAmount)
		{
			bResult = true;
		}
	}

	return(bResult);
}
