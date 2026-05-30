#include "StdAfx.h"
#include "LeoHelpers.h"
#include "ProxyImage.h"

CProxyImage::CProxyImage(CAbstractImage *pImage)
: CAbstractImage(pImage->GetScaleMode())
, m_pImage(pImage)
{
}

CProxyImage::~CProxyImage()
{
}

COLORREF CProxyImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pImage->GetCurrentBackgroundColor();
}

COLORREF CProxyImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pImage->GetAutomaticBackgroundColor();
}

void CProxyImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();
}

void CProxyImage::HideAlphaChannel()
{
	DeleteCachedPaintBitmap();
}

bool CProxyImage::RotateImage(int rotationAmount)
{
	DeleteCachedPaintBitmap();
	return true;
}

bool CProxyImage::InitOK() const
{
	return (m_pImage != NULL && m_pImage->InitOK());
}

int CProxyImage::GetWidth() const
{
	return (m_pImage == NULL ? 0 : m_pImage->GetWidth());
}

int CProxyImage::GetHeight() const
{
	return (m_pImage == NULL ? 0 : m_pImage->GetHeight());
}

const TCHAR *CProxyImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return (m_pImage == NULL ? NULL : m_pImage->GetImageTypeName(pSL));
}

bool CProxyImage::GetHasUsedTransparency() const
{
	return (m_pImage != NULL && m_pImage->GetHasUsedTransparency());
}

int CProxyImage::GetDelayTime() const
{
	return (m_pImage == NULL ? 0 : m_pImage->GetDelayTime());
}

void CProxyImage::GetRGBQPixel(RGBQUAD *pPixel, int x, int y)
{
	if (m_pImage != NULL)
	{
		m_pImage->GetRGBQPixel(pPixel, x, y);
	}
}

void CProxyImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	if (m_pImage != NULL)
	{
		m_pImage->CopyAllRGBQPixels(pDest);
	}
}

void CProxyImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	if (m_pImage != NULL)
	{
		m_pImage->CopyRGBQPixelRect(pDest, pDestSize, pDestOffset, bTopDown, pWantedRect, iDitherOffset, pRGBForTransparent, bGammaEnable, pdGammaValue, GammaTable);
	}
}

RGBQUAD *CProxyImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	*pbDelete = false;
	RGBQUAD *pPixels = NULL;

	if (m_pImage != NULL)
	{
		pPixels = m_pImage->GetRGBQPixels(pbDelete, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
	}

	return pPixels;
}

void CProxyImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	if (m_pImage != NULL)
	{
		m_pImage->DeleteRGBQPixels(pPixels);
	}
	else
	{
		// WTF!? :-)
		delete [] pPixels;
	}
}

void CProxyImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	if (m_pImage != NULL)
	{
		m_pImage->PaintUnmodified(hDC, pOffset, pWindowSize, pImageSize);
	}
}
