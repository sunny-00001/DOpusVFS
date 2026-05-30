#include "StdAfx.h"
#include "LeoHelpers.h"
#include "TiledImage.h"
#include "ProxyImage.h"

CTiledImage::CTiledImage(CAbstractImage *pImage)
: CAbstractImage(pImage->GetScaleMode())
, m_pImage(pImage)
, m_iWidth(pImage == NULL ? 0 : pImage->GetWidth())
, m_iHeight(pImage == NULL ? 0 : pImage->GetHeight())
{
}

CTiledImage::~CTiledImage()
{
}

bool CTiledImage::SetSize(const SIZE *pSize)
{
	bool bResult = false;

	if (InitOK() && pSize != NULL && pSize->cx > 0 && pSize->cy > 0)
	{
		m_iWidth  = pSize->cx;
		m_iHeight = pSize->cy;
		bResult = true;
	}

	return bResult;
}

COLORREF CTiledImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pImage->GetCurrentBackgroundColor();
}

COLORREF CTiledImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pImage->GetAutomaticBackgroundColor();
}

void CTiledImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();
}

void CTiledImage::HideAlphaChannel()
{
	DeleteCachedPaintBitmap();
}

bool CTiledImage::RotateImage(int rotationAmount)
{
	DeleteCachedPaintBitmap();
	return true;
}

bool CTiledImage::InitOK() const
{
	return (m_pImage != NULL && m_pImage->InitOK());
}

int CTiledImage::GetWidth() const
{
	return m_iWidth;
}

int CTiledImage::GetHeight() const
{
	return m_iHeight;
}

const TCHAR *CTiledImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return NULL;
}

bool CTiledImage::GetHasUsedTransparency() const
{
	return (m_pImage != NULL && m_pImage->GetHasUsedTransparency());
}

int CTiledImage::GetDelayTime() const
{
	return (m_pImage == NULL ? 0 : m_pImage->GetDelayTime());
}

void CTiledImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	CopyRGBQPixelRect(pDest, NULL, NULL, true, NULL, 0, NULL, false, NULL, NULL);
}

void CTiledImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	if (!InitOK())
	{
		return;
	}

	const int w = GetWidth();
	const int h = GetHeight();

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

	const int frameWidth  = m_pImage->GetWidth();
	const int frameHeight = m_pImage->GetHeight();

	// Assumption: pWantedRect is never negative. If it was we'd need to subtract 1 from start/end row/col involving negative coordinates.
	// Since pWantedRect is relative to the image it should never be negative.
	const int startCol = pWantedRect->left   / frameWidth;
	const int endCol   = pWantedRect->right  / frameWidth;
	const int startRow = pWantedRect->top    / frameHeight;
	const int endRow   = pWantedRect->bottom / frameHeight;

	RECT frameRect;
	frameRect.left   = startCol * frameWidth;
	frameRect.top    = startRow * frameHeight;
	frameRect.right  = frameRect.left + frameWidth;
	frameRect.bottom = frameRect.top  + frameHeight;

	RECT wantedFrameRect;
	RECT wantedFrameRelFrameRect;
	SIZE frameDestOffset;

	for (int r = startRow; r <= endRow; ++r)
	{
		for(int c = startCol; c <= endCol; ++c)
		{
			if (::IntersectRect(&wantedFrameRect, pWantedRect, &frameRect))
			{
				frameDestOffset.cx = pDestOffset->cx + (wantedFrameRect.left - pWantedRect->left);
				frameDestOffset.cy = pDestOffset->cy + (wantedFrameRect.top  - pWantedRect->top);

				wantedFrameRelFrameRect.left   = wantedFrameRect.left   - frameRect.left;
				wantedFrameRelFrameRect.top    = wantedFrameRect.top    - frameRect.top;
				wantedFrameRelFrameRect.right  = wantedFrameRect.right  - frameRect.left;
				wantedFrameRelFrameRect.bottom = wantedFrameRect.bottom - frameRect.top;

				m_pImage->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
											&wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
											bGammaEnable, pdGammaValue, GammaTable);
			}

			frameRect.left  = frameRect.right;
			frameRect.right += frameWidth;
		}

		frameRect.left   = startCol * frameWidth;
		frameRect.top    = frameRect.bottom;
		frameRect.right  = frameRect.left + frameWidth;
		frameRect.bottom += frameHeight;
	}
}

RGBQUAD *CTiledImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	*pbDelete = false;
	RGBQUAD *pPixels = NULL;
	
	if (InitOK())
	{
		pPixels = new(std::nothrow) RGBQUAD[ GetWidth() * GetHeight() ];

		if (pPixels != NULL)
		{
			*pbDelete = true;

			CopyRGBQPixelRect(pPixels, NULL, NULL, true, NULL, iDitherOffset, NULL, bGammaEnable, pdGammaValue, GammaTable);
		}
	}

	return pPixels;
}

void CTiledImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

void CTiledImage::Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// *pOffset: top-left image to top-left window.

	if (!InitOK())
	{
		return;
	}

	RECT imageRect;
	imageRect.left = 0;
	imageRect.top  = 0;
	imageRect.right  = (pImageSize != NULL ? pImageSize->cx : GetWidth());
	imageRect.bottom = (pImageSize != NULL ? pImageSize->cy : GetHeight());

	RECT windowRect;
	windowRect.left   = 0;
	windowRect.top    = 0;
	windowRect.right  = pWindowSize->cx;
	windowRect.bottom = pWindowSize->cy;

	SIZE frameSize;
	frameSize.cx = m_pImage->GetWidth();
	frameSize.cy = m_pImage->GetHeight();

	int startCol = 0;
	int endCol   = imageRect.right / frameSize.cx;
	int startRow = 0;
	int endRow   = imageRect.bottom / frameSize.cy;

	RECT frameRectRelImage;
	frameRectRelImage.left   = 0;
	frameRectRelImage.top    = 0;
	frameRectRelImage.right  = frameSize.cx;
	frameRectRelImage.bottom = frameSize.cy;

	RECT frameRectRelWindow = frameRectRelImage;
	::OffsetRect(&frameRectRelWindow, -pOffset->cx, -pOffset->cy);

	RECT wantedFrameRectRelImage;
	SIZE frameWindowOffset;

	HRGN hrgnOriginal = ::CreateRectRgn(0,0,1,1);

	if (hrgnOriginal != NULL)
	{
		bool bRemoveRegion = false;

		if (1 != ::GetClipRgn(hDC, hrgnOriginal))
		{
			bRemoveRegion = true;
		}

		::IntersectClipRect(hDC, imageRect.left  - pOffset->cx, imageRect.top    - pOffset->cy,
								 imageRect.right - pOffset->cx, imageRect.bottom - pOffset->cy);

		for (int r = startRow; r <= endRow; ++r)
		{
			for(int c = startCol; c <= endCol; ++c)
			{
				if (::IntersectRect(&wantedFrameRectRelImage, &imageRect, &frameRectRelImage))
				{
					frameWindowOffset.cx = windowRect.left - frameRectRelWindow.left;
					frameWindowOffset.cy = windowRect.top  - frameRectRelWindow.top;

					m_pImage->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
				}

				frameRectRelImage.left  = frameRectRelImage.right;
				frameRectRelImage.right += frameSize.cx;

				frameRectRelWindow.left  = frameRectRelWindow.right;
				frameRectRelWindow.right += frameSize.cx;
			}

			frameRectRelImage.left   = startCol * frameSize.cx;
			frameRectRelImage.top    = frameRectRelImage.bottom;
			frameRectRelImage.right  = frameRectRelImage.left + frameSize.cx;
			frameRectRelImage.bottom += frameSize.cy;

			frameRectRelWindow = frameRectRelImage;
			::OffsetRect(&frameRectRelWindow, -pOffset->cx, -pOffset->cy);
		}

		::SelectClipRgn(hDC, bRemoveRegion ? NULL : hrgnOriginal);

		::DeleteObject(hrgnOriginal);
		hrgnOriginal = NULL;
	}
}

void CTiledImage::DeleteCachedPaintBitmap()
{
	// No Op.
}

void CTiledImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// No Op.
}


void CTiledImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	// Normally Paint calls PaintUnmodified if needed but we've overridden Paint and implemented everything in there.

	if (InitOK())
	{
		Paint(hDC, pOffset, pWindowSize, pImageSize, 0, false, NULL, NULL);
	}
}
