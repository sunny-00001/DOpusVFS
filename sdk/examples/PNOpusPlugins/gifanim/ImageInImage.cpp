#include "StdAfx.h"
#include "LeoHelpers.h"
#include "ImageInImage.h"

CImageInImage::CImageInImage(CAbstractImage *pOuter, CAbstractImage *pInner, const RECT &rectInnerInOuter)
: CAbstractImage(pInner->GetScaleMode())
, m_pOuter(pOuter)
, m_pInner(pInner)
, m_bInitOK(false)
, m_rectInner(rectInnerInOuter)
{
	if (m_pOuter != NULL && m_pOuter->InitOK()
	&&	m_pInner != NULL && m_pInner->InitOK()
	&&	m_rectInner.left   >= 0
	&&	m_rectInner.top    >= 0
	&&	m_rectInner.right  <= m_pOuter->GetWidth()
	&&	m_rectInner.bottom <= m_pOuter->GetHeight()
	&&	(m_rectInner.right - m_rectInner.left) == m_pInner->GetWidth()
	&&	(m_rectInner.bottom - m_rectInner.top) == m_pInner->GetHeight())
	{
		m_bInitOK = true;
	}
}

CImageInImage::~CImageInImage(void)
{
}

COLORREF CImageInImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pInner->GetCurrentBackgroundColor();
}

COLORREF CImageInImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pInner->GetAutomaticBackgroundColor();
}

void CImageInImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();
}

void CImageInImage::HideAlphaChannel()
{
	DeleteCachedPaintBitmap();
}

bool CImageInImage::RotateImage(int rotationAmount)
{
	DeleteCachedPaintBitmap();
	return false;
}

const TCHAR *CImageInImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return (m_pInner == NULL ? NULL : m_pInner->GetImageTypeName(pSL));
}

bool CImageInImage::GetHasUsedTransparency() const
{
	// Ignore border transparency; this is used to see if there's an alpha channel to hide.
	return (m_pInner != NULL && m_pInner->GetHasUsedTransparency());
}

int CImageInImage::GetDelayTime() const
{
	return (m_pInner == NULL ? 0 : m_pInner->GetDelayTime());
}

void CImageInImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	CopyRGBQPixelRect(pDest, NULL, NULL, true, NULL, 0, NULL, false, NULL, NULL);
}

void CImageInImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

	RECT *pRectSubImage;
	CAbstractImage *pSubImage;

	RECT wantedFrameRect;
	RECT wantedFrameRelFrameRect;
	SIZE frameDestOffset;

	RECT rectTop;
	RECT rectLeft;
	RECT rectRight;
	RECT rectBottom;
	::SetRect(&rectTop,    0,                  0,                  m_pOuter->GetWidth(), m_rectInner.top);
	::SetRect(&rectLeft,   0,                  m_rectInner.top,    m_rectInner.left,     m_rectInner.bottom);
	::SetRect(&rectRight,  m_rectInner.right,  m_rectInner.top,    m_pOuter->GetWidth(), m_rectInner.bottom);
	::SetRect(&rectBottom, 0,                  m_rectInner.bottom, m_pOuter->GetWidth(), m_pOuter->GetHeight());

	for (int subImageIdx = 0; subImageIdx < 5; ++subImageIdx)
	{
		switch(subImageIdx)
		{
		default: pRectSubImage = NULL;          pSubImage = NULL;     break;
		case 0:  pRectSubImage = &rectTop;      pSubImage = m_pOuter; break;
		case 1:  pRectSubImage = &rectLeft;     pSubImage = m_pOuter; break;
		case 2:  pRectSubImage = &rectRight;    pSubImage = m_pOuter; break;
		case 3:  pRectSubImage = &rectBottom;   pSubImage = m_pOuter; break;
		case 4:  pRectSubImage = &m_rectInner;  pSubImage = m_pInner; break;
		}

		if (pRectSubImage == NULL || pSubImage == NULL)
		{
			break;
		}

		if (::IntersectRect(&wantedFrameRect, pWantedRect, pRectSubImage))
		{
			frameDestOffset.cx = pDestOffset->cx + (wantedFrameRect.left - pWantedRect->left);
			frameDestOffset.cy = pDestOffset->cy + (wantedFrameRect.top  - pWantedRect->top);

			if (pSubImage == m_pInner)
			{
				wantedFrameRelFrameRect.left   = wantedFrameRect.left   - pRectSubImage->left;
				wantedFrameRelFrameRect.top    = wantedFrameRect.top    - pRectSubImage->top;
				wantedFrameRelFrameRect.right  = wantedFrameRect.right  - pRectSubImage->left;
				wantedFrameRelFrameRect.bottom = wantedFrameRect.bottom - pRectSubImage->top;
			}
			else
			{
				wantedFrameRelFrameRect = wantedFrameRect;
			}

			pSubImage->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
										 &wantedFrameRelFrameRect, (pSubImage == m_pInner) ? iDitherOffset : 0, pRGBForTransparent,
										 (pSubImage == m_pInner) ? bGammaEnable : false, pdGammaValue, GammaTable);
		}
	}
}

RGBQUAD *CImageInImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

void CImageInImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

// static
void CImageInImage::ScaleFrames(const RECT &rectOuter, const RECT &rectInner, const SIZE *pSizeOuterScaled, RECT &rectOuterScaled, RECT &rectInnerScaled)
{
	assert(rectOuter.left == 0 && rectOuter.top == 0);

	const int outerWidth  = rectOuter.right - rectOuter.left;
	const int outerHeight = rectOuter.bottom - rectOuter.top;
	const int innerWidth  = rectInner.right - rectInner.left;
	const int innerHeight = rectInner.bottom - rectInner.top;

	if (NULL == pSizeOuterScaled || (pSizeOuterScaled->cx == outerWidth && pSizeOuterScaled->cy == outerHeight))
	{
		rectOuterScaled  = rectOuter;
		rectInnerScaled  = rectInner;
	}
	else
	{
		rectOuterScaled.left   = 0;
		rectOuterScaled.top    = 0;
		rectOuterScaled.right  = pSizeOuterScaled->cx;
		rectOuterScaled.bottom = pSizeOuterScaled->cy;

		// The right and bottom are scaled by their offsets from the edge, so that all the edges are scaled
		// consistently and even borders remain even.
		rectInnerScaled.left   =                        MulDiv(              rectInner.left,   pSizeOuterScaled->cx, outerWidth);
		rectInnerScaled.top    =                        MulDiv(              rectInner.top,    pSizeOuterScaled->cy, outerHeight);
		rectInnerScaled.right  = pSizeOuterScaled->cx - MulDiv(outerWidth  - rectInner.right,  pSizeOuterScaled->cx, outerWidth);
		rectInnerScaled.bottom = pSizeOuterScaled->cy - MulDiv(outerHeight - rectInner.bottom, pSizeOuterScaled->cy, outerHeight);

		if (rectInnerScaled.left >= rectInnerScaled.right
		||	rectInnerScaled.top >= rectInnerScaled.bottom)
		{
			// The border is using up all the space of the image. We must be zoomed very small. Give the
			// image the entire rect, or as much as it will fill at 100% zoom.
			if (pSizeOuterScaled->cx <= innerWidth)
			{
				rectInnerScaled.left = 0;
				rectInnerScaled.right = rectOuterScaled.right;
			}
			else
			{
				rectInnerScaled.left  = (rectOuterScaled.right - innerWidth) / 2;
				rectInnerScaled.right = rectOuterScaled.right - rectInnerScaled.left;
			}

			if (pSizeOuterScaled->cy <= innerHeight)
			{
				rectInnerScaled.top    = 0;
				rectInnerScaled.bottom = rectOuterScaled.bottom;
			}
			else
			{
				rectInnerScaled.top    = (rectOuterScaled.bottom - innerHeight) / 2;
				rectInnerScaled.bottom = rectOuterScaled.bottom - rectInnerScaled.top;
			}
		}
	}
}

void CImageInImage::Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// *pOffset: top-left image to top-left window.

	if (!InitOK())
	{
		return;
	}

	const int w = GetWidth();
	const int h = GetHeight();

	RECT rectOuter;
	::SetRect(&rectOuter, 0, 0, w, h);

	RECT rectOuterScaled;
	RECT rectInnerScaled;
	ScaleFrames(rectOuter, m_rectInner, pImageSize, rectOuterScaled, rectInnerScaled);

	RECT windowRect;
	windowRect.left   = 0;
	windowRect.top    = 0;
	windowRect.right  = pWindowSize->cx;
	windowRect.bottom = pWindowSize->cy;

	RECT wantedFrameRectRelWindow;
	SIZE frameWindowOffset;

	SIZE frameSizeScaled;
	RECT frameRectRelWindow;

	RECT *pRectSubImage;
	RECT *pRectSubImageScaled;
	CAbstractImage *pSubImage;

	for (int subImageIdx = 0; subImageIdx < 2; ++subImageIdx)
	{
		switch(subImageIdx)
		{
		default: pRectSubImage = NULL;         pRectSubImageScaled = NULL;             pSubImage = NULL;     break;
		case 0:  pRectSubImage = &rectOuter;   pRectSubImageScaled = &rectOuterScaled; pSubImage = m_pOuter; break;
		case 1:  pRectSubImage = &m_rectInner; pRectSubImageScaled = &rectInnerScaled; pSubImage = m_pInner; break;
		}

		if (pRectSubImage == NULL || pRectSubImageScaled == NULL || pSubImage == NULL)
		{
			break;
		}

		frameSizeScaled.cx = pRectSubImageScaled->right - pRectSubImageScaled->left;
		frameSizeScaled.cy = pRectSubImageScaled->bottom - pRectSubImageScaled->top;

		frameRectRelWindow = *pRectSubImageScaled;
		::OffsetRect(&frameRectRelWindow, -pOffset->cx, -pOffset->cy);

		if (::IntersectRect(&wantedFrameRectRelWindow, &windowRect, &frameRectRelWindow))
		{
			frameWindowOffset.cx = windowRect.left - frameRectRelWindow.left;
			frameWindowOffset.cy = windowRect.top  - frameRectRelWindow.top;

			if (pSubImage == m_pInner)
			{
				pSubImage->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSizeScaled, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
			}
			else
			{
				bool bOriginalRegionIsNull = true;

				HRGN hrgnClipOld = ::CreateRectRgn(0,0,1,1);

				if (1 == ::GetClipRgn(hDC, hrgnClipOld))
				{
					bOriginalRegionIsNull = false;
				}

				ExcludeClipRect(hDC,
					rectInnerScaled.left   - pOffset->cx, 
					rectInnerScaled.top    - pOffset->cy,
					rectInnerScaled.right  - pOffset->cx,
					rectInnerScaled.bottom - pOffset->cy);

				pSubImage->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSizeScaled, 0, false, NULL, NULL);

				SelectClipRgn(hDC, bOriginalRegionIsNull ? NULL : hrgnClipOld);

				DeleteObject(hrgnClipOld);
				hrgnClipOld = NULL;
			}
		}
	}
}

void CImageInImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	// Normally Paint calls PaintUnmodified if needed but we've overridden Paint and implemented everything in there.

	if (InitOK())
	{
		Paint(hDC, pOffset, pWindowSize, pImageSize, 0, false, NULL, NULL);
	}
}

void CImageInImage::DeleteCachedPaintBitmap()
{
	// No Op.
	// We could speed up painting of multiple framed images (with the same frames) that are also selected
	// by having a cache of differently selected ProxyImage wrappers around the frame images, similar to
	// the way TiledImage keeps a cache. This cache would have to be shared by the different ImageInImage
	// instances. For now this doesn't seem worth writing because ImageInImage isn't used for animation and
	// so it shouldn't hurt to do a bit of extra calculation when rendering the frames with a selection
	// rectangle.
}

void CImageInImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// No Op.
	// See comments for DeleteCachedPaintBitmap, above.
}
