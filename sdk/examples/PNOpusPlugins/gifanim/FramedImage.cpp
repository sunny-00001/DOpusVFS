#include "StdAfx.h"

#if 0

#include "LeoHelpers.h"
#include "FramedImage.h"

CFramedImage::CFramedImage(CAbstractImage *pCentre, CAbstractImage *pTop, CAbstractImage *pLeft, CAbstractImage *pRight, CAbstractImage *pBottom, bool bTopSpans)
: CAbstractImage(pCentre->GetScaleMode())
, m_pCentre(pCentre)
, m_pTop(pTop)
, m_pLeft(pLeft)
, m_pRight(pRight)
, m_pBottom(pBottom)
, m_bInitOK(false)
, m_bTopSpans(bTopSpans)
, m_width(0)
, m_height(0)
{
	if (m_pCentre != NULL && m_pCentre->InitOK()
	&&	m_pTop    != NULL && m_pTop->InitOK()
	&&	m_pLeft   != NULL && m_pLeft->InitOK()
	&&	m_pRight  != NULL && m_pRight->InitOK()
	&&	m_pBottom != NULL && m_pBottom->InitOK())
	{
		if (m_bTopSpans)
		{
			m_rectTop.left   = 0;
			m_rectTop.right  = m_pTop->GetWidth();
			m_rectTop.top    = 0;
			m_rectTop.bottom = m_pTop->GetHeight();

			m_rectLeft.left   = 0;
			m_rectLeft.right  = m_pLeft->GetWidth();
			m_rectLeft.top    = m_rectTop.bottom;
			m_rectLeft.bottom = m_rectTop.bottom + m_pLeft->GetHeight();

			m_rectCentre.left   = m_rectLeft.right;
			m_rectCentre.right  = m_rectLeft.right + m_pCentre->GetWidth();
			m_rectCentre.top    = m_rectTop.bottom;
			m_rectCentre.bottom = m_rectTop.bottom + m_pCentre->GetHeight();

			m_rectRight.left   = m_rectCentre.right;
			m_rectRight.right  = m_rectCentre.right + m_pRight->GetWidth();
			m_rectRight.top    = m_rectTop.bottom;
			m_rectRight.bottom = m_rectTop.bottom + m_pRight->GetHeight();

			m_rectBottom.left   = 0;
			m_rectBottom.right  = m_pBottom->GetWidth();
			m_rectBottom.top    = m_rectLeft.bottom;
			m_rectBottom.bottom = m_rectLeft.bottom + m_pBottom->GetHeight();

			m_width  = m_rectRight.right;
			m_height = m_rectBottom.bottom;

			if (m_rectTop.right == m_width
			&&	m_rectBottom.right == m_width
			&&	m_rectLeft.bottom == m_rectCentre.bottom
			&&	m_rectLeft.bottom == m_rectRight.bottom)
			{
				m_bInitOK = true;
			}
		}
		else
		{
			m_rectLeft.left   = 0;
			m_rectLeft.right  = m_pLeft->GetWidth();
			m_rectLeft.top    = 0;
			m_rectLeft.bottom = m_pLeft->GetHeight();

			m_rectTop.left   = m_rectLeft.right;
			m_rectTop.right  = m_rectLeft.right + m_pTop->GetWidth();
			m_rectTop.top    = 0;
			m_rectTop.bottom = m_pTop->GetHeight();

			m_rectCentre.left   = m_rectLeft.right;
			m_rectCentre.right  = m_rectLeft.right + m_pCentre->GetWidth();
			m_rectCentre.top    = m_rectTop.bottom;
			m_rectCentre.bottom = m_rectTop.bottom + m_pCentre->GetHeight();

			m_rectRight.left   = m_rectCentre.right;
			m_rectRight.right  = m_rectCentre.right + m_pRight->GetWidth();
			m_rectRight.top    = 0;
			m_rectRight.bottom = m_pRight->GetHeight();

			m_rectBottom.left   = m_rectLeft.right;
			m_rectBottom.right  = m_rectLeft.right + m_pBottom->GetWidth();
			m_rectBottom.top    = m_rectCentre.bottom;
			m_rectBottom.bottom = m_rectCentre.bottom + m_pBottom->GetHeight();

			m_width  = m_rectRight.right;
			m_height = m_rectBottom.bottom;

			if (m_rectLeft.bottom == m_height
			&&	m_rectRight.bottom == m_height
			&&	m_rectTop.right == m_rectCentre.right
			&&	m_rectTop.right == m_rectBottom.right)
			{
				m_bInitOK = true;
			}
		}
	}
}

CFramedImage::~CFramedImage()
{
}

COLORREF CFramedImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pCentre->GetCurrentBackgroundColor();
}

COLORREF CFramedImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pCentre->GetAutomaticBackgroundColor();
}

void CFramedImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();
}

void CFramedImage::HideAlphaChannel()
{
	DeleteCachedPaintBitmap();
}

bool CFramedImage::RotateImage(int rotationAmount)
{
	DeleteCachedPaintBitmap();
	return false;
}

const TCHAR *CFramedImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return (m_pCentre == NULL ? NULL : m_pCentre->GetImageTypeName(pSL));
}

bool CFramedImage::GetHasUsedTransparency() const
{
	// Ignore border transparency; this is used to see if there's an alpha channel to hide.
	return (m_pCentre != NULL && m_pCentre->GetHasUsedTransparency());
}

int CFramedImage::GetDelayTime() const
{
	return (m_pCentre == NULL ? 0 : m_pCentre->GetDelayTime());
}

void CFramedImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	CopyRGBQPixelRect(pDest, NULL, NULL, true, NULL, 0, NULL, false, NULL, NULL);
}

void CFramedImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

	for (int subImageIdx = 0; subImageIdx < 5; ++subImageIdx)
	{
		switch(subImageIdx)
		{
		default: pRectSubImage = NULL;          pSubImage = NULL;      break;
		case 0:  pRectSubImage = &m_rectTop;    pSubImage = m_pTop;    break;
		case 1:  pRectSubImage = &m_rectLeft;   pSubImage = m_pLeft;   break;
		case 2:  pRectSubImage = &m_rectCentre; pSubImage = m_pCentre; break;
		case 3:  pRectSubImage = &m_rectRight;  pSubImage = m_pRight;  break;
		case 4:  pRectSubImage = &m_rectBottom; pSubImage = m_pBottom; break;
		}

		if (pRectSubImage == NULL || pSubImage == NULL)
		{
			break;
		}

		if (::IntersectRect(&wantedFrameRect, pWantedRect, pRectSubImage))
		{
			frameDestOffset.cx = pDestOffset->cx + (wantedFrameRect.left - pWantedRect->left);
			frameDestOffset.cy = pDestOffset->cy + (wantedFrameRect.top  - pWantedRect->top);

			wantedFrameRelFrameRect.left   = wantedFrameRect.left   - pRectSubImage->left;
			wantedFrameRelFrameRect.top    = wantedFrameRect.top    - pRectSubImage->top;
			wantedFrameRelFrameRect.right  = wantedFrameRect.right  - pRectSubImage->left;
			wantedFrameRelFrameRect.bottom = wantedFrameRect.bottom - pRectSubImage->top;

			pSubImage->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
										 &wantedFrameRelFrameRect, (pSubImage == m_pCentre) ? iDitherOffset : 0, pRGBForTransparent,
										 (pSubImage == m_pCentre) ? bGammaEnable : false, pdGammaValue, GammaTable);
		}
	}
}

RGBQUAD *CFramedImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

void CFramedImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

void CFramedImage::Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// *pOffset: top-left image to top-left window.

	if (!InitOK())
	{
		return;
	}

	const int w = GetWidth();
	const int h = GetHeight();

	RECT rectTopScaled;
	RECT rectBottomScaled;
	RECT rectLeftScaled;
	RECT rectRightScaled;
	RECT rectCentreScaled;

	if (NULL == pImageSize || (pImageSize->cx == w && pImageSize->cy == h))
	{
		rectTopScaled    = m_rectTop;
		rectBottomScaled = m_rectBottom;
		rectLeftScaled   = m_rectLeft;
		rectRightScaled  = m_rectRight;
		rectCentreScaled = m_rectCentre;
	}
	else if (m_bTopSpans)
	{
		rectTopScaled.left   = 0;
		rectTopScaled.top    = 0;
		rectTopScaled.right  = pImageSize->cx;
		rectTopScaled.bottom = LeoHelpers::MulDivRoundDown(m_rectTop.bottom, pImageSize->cy, h);

		rectLeftScaled.left   = 0;
		rectLeftScaled.top    = rectTopScaled.bottom;
		rectLeftScaled.right  = LeoHelpers::MulDivRoundDown(m_rectLeft.right, pImageSize->cx, w);
		rectLeftScaled.bottom = LeoHelpers::MulDivRoundDown(m_rectLeft.bottom, pImageSize->cy, h);

		rectCentreScaled.left   = rectLeftScaled.right;
		rectCentreScaled.top    = rectTopScaled.bottom;
		rectCentreScaled.right  = LeoHelpers::MulDivRoundDown(m_rectCentre.right, pImageSize->cx, w);
		rectCentreScaled.bottom = rectLeftScaled.bottom;

		rectRightScaled.left   = rectCentreScaled.right;
		rectRightScaled.top    = rectTopScaled.bottom;
		rectRightScaled.right  = pImageSize->cx;
		rectRightScaled.bottom = rectLeftScaled.bottom;

		rectBottomScaled.left   = 0;
		rectBottomScaled.top    = rectLeftScaled.bottom;
		rectBottomScaled.right  = pImageSize->cx;
		rectBottomScaled.bottom = pImageSize->cy;
	}
	else
	{
		rectLeftScaled.left   = 0;
		rectLeftScaled.top    = 0;
		rectLeftScaled.right  = LeoHelpers::MulDivRoundDown(m_rectLeft.right, pImageSize->cx, w);
		rectLeftScaled.bottom = pImageSize->cy;

		rectTopScaled.left   = rectLeftScaled.right;
		rectTopScaled.top    = 0;
		rectTopScaled.right  = LeoHelpers::MulDivRoundDown(m_rectTop.right, pImageSize->cx, w);
		rectTopScaled.bottom = LeoHelpers::MulDivRoundDown(m_rectTop.bottom, pImageSize->cy, h);

		rectCentreScaled.left   = rectLeftScaled.right;
		rectCentreScaled.top    = rectTopScaled.bottom;
		rectCentreScaled.right  = rectTopScaled.right;
		rectCentreScaled.bottom = LeoHelpers::MulDivRoundDown(m_rectCentre.bottom, pImageSize->cy, h);

		rectBottomScaled.left   = rectLeftScaled.right;
		rectBottomScaled.top    = rectCentreScaled.bottom;
		rectBottomScaled.right  = rectTopScaled.right;
		rectBottomScaled.bottom = pImageSize->cy;

		rectRightScaled.left   = rectTopScaled.right;
		rectRightScaled.top    = 0;
		rectRightScaled.right  = pImageSize->cx;
		rectRightScaled.bottom = pImageSize->cy;
	}

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

	for (int subImageIdx = 0; subImageIdx < 5; ++subImageIdx)
	{
		switch(subImageIdx)
		{
		default: pRectSubImage = NULL;          pRectSubImageScaled = NULL;              pSubImage = NULL;      break;
		case 0:  pRectSubImage = &m_rectTop;    pRectSubImageScaled = &rectTopScaled;    pSubImage = m_pTop;    break;
		case 1:  pRectSubImage = &m_rectLeft;   pRectSubImageScaled = &rectLeftScaled;   pSubImage = m_pLeft;   break;
		case 2:  pRectSubImage = &m_rectCentre; pRectSubImageScaled = &rectCentreScaled; pSubImage = m_pCentre; break;
		case 3:  pRectSubImage = &m_rectRight;  pRectSubImageScaled = &rectRightScaled;  pSubImage = m_pRight;  break;
		case 4:  pRectSubImage = &m_rectBottom; pRectSubImageScaled = &rectBottomScaled; pSubImage = m_pBottom; break;
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

			pSubImage->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSizeScaled, (pSubImage == m_pCentre) ? iDitherOffset : 0, (pSubImage == m_pCentre) ? bGammaEnable : false, pdGammaValue, GammaTable);
		}
	}
}

void CFramedImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	// Normally Paint calls PaintUnmodified if needed but we've overridden Paint and implemented everything in there.

	if (InitOK())
	{
		Paint(hDC, pOffset, pWindowSize, pImageSize, 0, false, NULL, NULL);
	}
}

void CFramedImage::DeleteCachedPaintBitmap()
{
	// No Op.
	// We could speed up painting of multiple framed images (with the same frames) that are also selected
	// by having a cache of differently selected ProxyImage wrappers around the frame images, similar to
	// the way TiledImage keeps a cache. This cache would have to be shared by the different FramedImage
	// instances. For now this doesn't seem worth writing because FramedImage isn't used for animation and
	// so it shouldn't hurt to do a bit of extra calculation when rendering the frames with a selection
	// rectangle.
}

void CFramedImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// No Op.
	// See comments for DeleteCachedPaintBitmap, above.
}

#endif
