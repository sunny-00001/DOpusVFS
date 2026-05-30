#include "StdAfx.h"
#include "LeoHelpers.h"
#include "FlatImage.h"
#include "EmptyImage.h"
#include "FramedImage.h"
#include "ImageInImage.h"

CFlatImage::CFlatImage(CAbstractImageList *pFrames, int iNumberOfColumns, int iScaleMode, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper)
: CAbstractImage(iScaleMode)
, m_pEmptyImage(NULL)
, m_pSourceFrames(pFrames)
, m_pBorderType2(NULL)
, m_pTopBorder(NULL)
, m_pLeftBorder(NULL)
, m_pRightBorder(NULL)
, m_pBottomBorder(NULL)
, m_pOutsideTopBorder(NULL)
, m_pOutsideLeftBorder(NULL)
, m_pOutsideRightBorder(NULL)
, m_pOutsideBottomBorder(NULL)
, m_pOutsideTopLeftBorder(NULL)
, m_pOutsideTopRightBorder(NULL)
, m_pOutsideBottomLeftBorder(NULL)
, m_pOutsideBottomRightBorder(NULL)
, m_numColumns(0)
, m_numRows(0)
{
	m_bBorders = false;
	m_pFrames = pFrames;
	m_bOwnImageList = false;

	m_bInitOK = m_pSourceFrames->InitOK();

	SetNumberOfColumns(iNumberOfColumns, bFrameImage, pPluginHelper);
}

CFlatImage::~CFlatImage()
{
	delete m_pEmptyImage;
	m_pEmptyImage = NULL;

	if (m_bOwnImageList)
	{
		delete m_pFrames;
		m_pFrames = NULL;
	}

	deleteBorders();
}

void CFlatImage::deleteBorders()
{
	delete m_pBorderType2; m_pBorderType2 = NULL;

	delete m_pTopBorder;    m_pTopBorder = NULL;
	delete m_pLeftBorder;   m_pLeftBorder = NULL;
	delete m_pRightBorder;  m_pRightBorder = NULL;
	delete m_pBottomBorder; m_pBottomBorder = NULL;

	delete m_pOutsideTopBorder;    m_pOutsideTopBorder = NULL;
	delete m_pOutsideLeftBorder;   m_pOutsideLeftBorder = NULL;
	delete m_pOutsideRightBorder;  m_pOutsideRightBorder = NULL;
	delete m_pOutsideBottomBorder; m_pOutsideBottomBorder = NULL;

	delete m_pOutsideTopLeftBorder;     m_pOutsideTopLeftBorder = NULL;
	delete m_pOutsideTopRightBorder;    m_pOutsideTopRightBorder = NULL;
	delete m_pOutsideBottomLeftBorder;  m_pOutsideBottomLeftBorder = NULL;
	delete m_pOutsideBottomRightBorder; m_pOutsideBottomRightBorder = NULL;
}

void CFlatImage::SetNumberOfColumns(CAbstractImageList::size_type numColumns, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper)
{
	if (InitOK())
	{
		delete m_pEmptyImage;
		m_pEmptyImage = NULL;

		if (numColumns < 1)
		{
			numColumns = 1;
		}

		if (m_pFrames == NULL || m_pFrames->IsEmpty())
		{
			m_iWidth  = 0;
			m_iHeight = 0;

			m_numColumns = 0;
			m_numRows = 0;
		}
		else
		{
			CAbstractImageList::size_type numFrames = m_pSourceFrames->GetNumberOfFrames();

			CAbstractImageList::size_type oldNumColumns = m_numColumns;
			CAbstractImageList::size_type oldNumRows = m_numRows;

			m_numColumns = numColumns;
			m_numRows = ((numFrames + (m_numColumns - 1)) / m_numColumns);

			bool bOldBorders = m_bBorders;

			if (!m_bBorders && m_pSourceFrames->GetFirstFrame()->WantBorders())
			{
				// Want borders now. Either this is the first call or they were off but have been turned on by the user.
				m_bBorders = true;
				m_pFrames = new CAbstractImageList();
				m_bOwnImageList = true;
			}

			if (m_bBorders)
			{
				if (!m_pSourceFrames->GetFirstFrame()->WantBorders())
				{
					// No longer want borders. User must have turned them off.
					m_bBorders = false;
				}
				else if (!bOldBorders
				     ||  0 == oldNumColumns
				     ||  0 == oldNumRows
				     ||  m_pSourceFrames->GetFirstFrame()->NeedNewBorders(oldNumColumns, oldNumRows, m_numColumns, m_numRows))
				{
					// We need new borders.

					assert(m_bOwnImageList);

					m_pFrames->Clear();

					deleteBorders();

					bool bTopSpans = false;

					RECT innerRect = {0};

//					if (m_pSourceFrames->GetFirstFrame()->CreateBorders(m_numColumns, m_numRows, false,
//						&m_pTopBorder, &m_pLeftBorder, &m_pRightBorder, &m_pBottomBorder, &bTopSpans,
//						&m_pOutsideTopBorder, &m_pOutsideLeftBorder, &m_pOutsideRightBorder, &m_pOutsideBottomBorder,
//						&m_pOutsideTopLeftBorder, &m_pOutsideTopRightBorder, &m_pOutsideBottomLeftBorder, &m_pOutsideBottomRightBorder))
//					{
//						for(CAbstractImageList::size_type i = 0; i < numFrames; ++i)
//						{
//							m_pFrames->AddFrame(new CFramedImage(m_pSourceFrames->GetFrame(i), m_pTopBorder, m_pLeftBorder, m_pRightBorder, m_pBottomBorder, bTopSpans), true);
//						}
//					}
//					else
					
					if (m_pSourceFrames->GetFirstFrame()->CreateBorders(false, &m_pBorderType2, &innerRect, bFrameImage, pPluginHelper) && m_pBorderType2 != NULL)
					{
						for(CAbstractImageList::size_type i = 0; i < numFrames; ++i)
						{
							m_pFrames->AddFrame(new CImageInImage(m_pBorderType2, m_pSourceFrames->GetFrame(i), innerRect), true);
						}
					}
					else
					{
						m_bBorders = false;
					}
				}

				if (!m_bBorders)
				{
					deleteBorders();
					delete m_pFrames;
					m_pFrames = m_pSourceFrames;
					m_bOwnImageList = false;
				}
			}

			int iFrameWidth  = m_pFrames->GetFirstFrame()->GetWidth();
			int iFrameHeight = m_pFrames->GetFirstFrame()->GetHeight();

			CAbstractImageList::size_type wastedSpaces = (m_numColumns - (numFrames % m_numColumns)) % m_numColumns;

			m_iWidth  = m_numColumns * iFrameWidth;
			m_iHeight = m_numRows * iFrameHeight;

			if (m_pOutsideLeftBorder   != NULL) { m_iWidth  += m_pOutsideLeftBorder->GetWidth();    }
			if (m_pOutsideRightBorder  != NULL) { m_iWidth  += m_pOutsideRightBorder->GetWidth();   }
			if (m_pOutsideTopBorder    != NULL) { m_iHeight += m_pOutsideTopBorder->GetHeight();    }
			if (m_pOutsideBottomBorder != NULL) { m_iHeight += m_pOutsideBottomBorder->GetHeight(); }

			if (wastedSpaces > 0)
			{
				RGBQUAD rgbBackground;
				COLORREF cr2 = GetCurrentBackgroundColor();
				rgbBackground.rgbRed   = GetRValue(cr2);
				rgbBackground.rgbGreen = GetGValue(cr2);
				rgbBackground.rgbBlue  = GetBValue(cr2);
				rgbBackground.rgbReserved = 0;

				m_pEmptyImage = new CEmptyImage(wastedSpaces * iFrameWidth, iFrameHeight, GetScaleMode(), &rgbBackground);
			}
		}
	}
}

COLORREF CFlatImage::GetCurrentBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return /*m_pFrames->GetHasUsedTransparency() ?*/ m_pFrames->GetFirstFrame()->GetCurrentBackgroundColor() /*: m_pFrames->GetFirstFrame()->GetAutomaticBackgroundColor()*/;
}

COLORREF CFlatImage::GetAutomaticBackgroundColor() const
{
	if (!InitOK())
	{
		return RGB(0,0,0);
	}

	return m_pFrames->GetFirstFrame()->GetAutomaticBackgroundColor();
}

void CFlatImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	if (InitOK())
	{
		DeleteCachedPaintBitmap();

		if (m_bOwnImageList)          { m_pFrames->UpdateViewerBackgroundColor(crNewBackground); }

		if (m_pEmptyImage   != NULL)  { m_pEmptyImage->UpdateViewerBackgroundColor( crNewBackground ); } // GetCurrentBackgroundColor()

		if (m_pBorderType2  != NULL)  { m_pBorderType2->UpdateViewerBackgroundColor( crNewBackground ); }

		if (m_pTopBorder    != NULL)  { m_pTopBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pLeftBorder   != NULL)  { m_pLeftBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pRightBorder  != NULL)  { m_pRightBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pBottomBorder != NULL)  { m_pBottomBorder->UpdateViewerBackgroundColor( crNewBackground ); }

		if (m_pOutsideTopBorder    != NULL)  { m_pOutsideTopBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideLeftBorder   != NULL)  { m_pOutsideLeftBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideRightBorder  != NULL)  { m_pOutsideRightBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideBottomBorder != NULL)  { m_pOutsideBottomBorder->UpdateViewerBackgroundColor( crNewBackground ); }

		if (m_pOutsideTopLeftBorder     != NULL)  { m_pOutsideTopLeftBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideTopRightBorder    != NULL)  { m_pOutsideTopRightBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideBottomLeftBorder  != NULL)  { m_pOutsideBottomLeftBorder->UpdateViewerBackgroundColor( crNewBackground ); }
		if (m_pOutsideBottomRightBorder != NULL)  { m_pOutsideBottomRightBorder->UpdateViewerBackgroundColor( crNewBackground ); }
	}
}

void CFlatImage::HideAlphaChannel()
{
	if (InitOK())
	{
		DeleteCachedPaintBitmap();

		if (m_bOwnImageList)          { m_pFrames->HideAlphaChannel(); }

		// Don't change the empty image or borders.
	}
}

bool CFlatImage::RotateImage(int rotationAmount)
{
	bool bResult = false;

	if (InitOK())
	{
		DeleteCachedPaintBitmap();

		// We expect to be thrown away if the images are rotated so don't bother to do anything else.
	}

	return bResult;
}

bool CFlatImage::InitOK() const
{
	return m_bInitOK;
}

int CFlatImage::GetWidth() const
{
	return m_iWidth;
}

int CFlatImage::GetHeight() const
{
	return m_iHeight;
}

const TCHAR *CFlatImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return NULL;
}

bool CFlatImage::GetHasUsedTransparency() const
{
	return (m_pFrames != NULL && !m_pFrames->IsEmpty() && m_pFrames->GetHasUsedTransparency());
}

int CFlatImage::GetDelayTime() const
{
	return 0;
}

void CFlatImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	CopyRGBQPixelRect(pDest, NULL, NULL, true, NULL, 0, NULL, false, NULL, NULL);
}

void CFlatImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

	CAbstractImageList::size_type totalFrames = m_pFrames->GetNumberOfFrames();
	CAbstractImageList::size_type frame = 0;
	CAbstractImageList::size_type col = 0;
	CAbstractImageList::size_type row = 0;

	const int frameWidth  = m_pFrames->GetFirstFrame()->GetWidth();
	const int frameHeight = m_pFrames->GetFirstFrame()->GetHeight();

	RECT frameRect;
	frameRect.left   = 0;
	frameRect.top    = 0;
	frameRect.right  = frameWidth;
	frameRect.bottom = frameHeight;

	SIZE frameDestOffset;
	RECT wantedFrameRelFrameRect;

	RECT borderFrameRect;
	int outLeftWidth    = (m_pOutsideLeftBorder   == NULL ? 0 : m_pOutsideLeftBorder->GetWidth());
	int outRightWidth   = (m_pOutsideRightBorder  == NULL ? 0 : m_pOutsideRightBorder->GetWidth());
	int outTopHeight    = (m_pOutsideTopBorder    == NULL ? 0 : m_pOutsideTopBorder->GetHeight());
	int outBottomHeight = (m_pOutsideBottomBorder == NULL ? 0 : m_pOutsideBottomBorder->GetHeight());

	if (outTopHeight)
	{
		borderFrameRect = frameRect;
		borderFrameRect.bottom = borderFrameRect.top + outTopHeight;

		if (translateRects1(borderFrameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
		{
			m_pOutsideTopBorder->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
									  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
									  bGammaEnable, pdGammaValue, GammaTable);
		}

		frameRect.top += outTopHeight;
		frameRect.bottom += outTopHeight;
	}

	while (frame < totalFrames)
	{
		if (col == 0 && outLeftWidth != 0)
		{
			borderFrameRect = frameRect;
			borderFrameRect.right = borderFrameRect.left + outLeftWidth;

			if (translateRects1(borderFrameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
			{
				m_pOutsideLeftBorder->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
										  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
										  bGammaEnable, pdGammaValue, GammaTable);
			}

			frameRect.left += outLeftWidth;
			frameRect.right += outLeftWidth;
		}

		if (translateRects1(frameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
		{
			m_pFrames->GetFrame(frame)->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
									  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
									  bGammaEnable, pdGammaValue, GammaTable);
		}

		++frame;

		frameRect.left  = frameRect.right;
		frameRect.right += frameWidth;

		if (++col == m_numColumns)
		{
			if (outRightWidth != 0)
			{
				borderFrameRect = frameRect;
				borderFrameRect.right = borderFrameRect.left + outRightWidth;

				if (translateRects1(borderFrameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
				{
					m_pOutsideRightBorder->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
											  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
											  bGammaEnable, pdGammaValue, GammaTable);
				}
			}

			col = 0;
			++row;

			frameRect.left   = 0;
			frameRect.right  = frameWidth;
			frameRect.top    = frameRect.bottom;
			frameRect.bottom += frameHeight;
		}
	}

	if (col != 0)
	{
		if (outRightWidth != 0)
		{
			borderFrameRect = frameRect;
			borderFrameRect.right = borderFrameRect.left + outRightWidth;

			if (translateRects1(borderFrameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
			{
				m_pOutsideRightBorder->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
										  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
										  bGammaEnable, pdGammaValue, GammaTable);
			}

			frameRect.left  += outRightWidth;
			frameRect.right += outRightWidth;
		}

		frameRect.right = pWantedRect->right;

		if (translateRects1(frameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
		{
			RGBQUAD rgbtempfill;
			const RGBQUAD *pRGBFill = pRGBForTransparent;

			if (pRGBFill == NULL)
			{
				COLORREF crFill = GetCurrentBackgroundColor();

				pRGBFill = &rgbtempfill;
				rgbtempfill.rgbRed   = GetRValue(crFill);
				rgbtempfill.rgbGreen = GetGValue(crFill);
				rgbtempfill.rgbBlue  = GetBValue(crFill);
				rgbtempfill.rgbReserved = 255;
			}

			StaticFillRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown, &wantedFrameRelFrameRect, pRGBFill);
		}
	}

	if (outBottomHeight)
	{
		borderFrameRect = frameRect;
		borderFrameRect.bottom = h;

		if (translateRects1(borderFrameRect, pDestOffset, pWantedRect, &frameDestOffset, &wantedFrameRelFrameRect))
		{
			m_pOutsideBottomBorder->CopyRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown,
									  &wantedFrameRelFrameRect, iDitherOffset, pRGBForTransparent,
									  bGammaEnable, pdGammaValue, GammaTable);
		}
	}
}

RGBQUAD *CFlatImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

void CFlatImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

void CFlatImage::Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// *pOffset: top-left image to top-left window.

	if (!InitOK())
	{
		return;
	}

	const int w = GetWidth();
	const int h = GetHeight();

	RECT windowRect;
	windowRect.left   = 0;
	windowRect.top    = 0;
	windowRect.right  = pWindowSize->cx;
	windowRect.bottom = pWindowSize->cy;

	CAbstractImageList::size_type totalFrames = m_pFrames->GetNumberOfFrames();
	CAbstractImageList::size_type frame = 0;
	CAbstractImageList::size_type col = 0;
	CAbstractImageList::size_type row = 0;

	const int frameWidth  = m_pFrames->GetFirstFrame()->GetWidth();
	const int frameHeight = m_pFrames->GetFirstFrame()->GetHeight();

	// The +1i64 in a few cases below is so that the RHS is calculated using 64-bit numbers.
	// This is required for flattened images which become extremely wide, where the multiplication part
	// overflows the limit of a 32-bit number but the final result, after the division, still fits within
	// 32-bits.

	RECT frameRectFull;
	RECT frameRectScaled;

	SIZE frameWindowOffset;
	SIZE frameSize;

	RECT borderFrameRectFull;
	RECT borderFrameRectScaled;

	int outLeftWidthFull    = (m_pOutsideLeftBorder  == NULL ? 0 : m_pOutsideLeftBorder->GetWidth());
	int outRightWidthFull   = (m_pOutsideRightBorder == NULL ? 0 : m_pOutsideRightBorder->GetWidth());
	int outLeftWidthScaled = LeoHelpers::MulDivRoundDown(outLeftWidthFull, pImageSize->cx, w);

	int outTopHeightFull    = (m_pOutsideTopBorder    == NULL ? 0 : m_pOutsideTopBorder->GetHeight());
	int outBottomHeightFull = (m_pOutsideBottomBorder == NULL ? 0 : m_pOutsideBottomBorder->GetHeight());

	if (outTopHeightFull != 0)
	{
		borderFrameRectFull.left   = 0;
		borderFrameRectFull.right  = w;
		borderFrameRectFull.top    = 0;
		borderFrameRectFull.bottom = outTopHeightFull;

		borderFrameRectScaled.left   = -pOffset->cx;
		borderFrameRectScaled.right  = -pOffset->cx + pImageSize->cx;
		borderFrameRectScaled.top    = -pOffset->cy;
		borderFrameRectScaled.bottom = -pOffset->cy + LeoHelpers::MulDivRoundDown(outTopHeightFull, pImageSize->cy, h);

		if (translateRects2(borderFrameRectScaled, borderFrameRectFull, windowRect, &frameWindowOffset, &frameSize))
		{
			m_pOutsideTopBorder->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
		}

		frameRectScaled.top = borderFrameRectScaled.bottom;
	}
	else
	{
		frameRectScaled.top = -pOffset->cy;
	}
	frameRectFull.left   = 0;
	frameRectFull.right  = frameWidth;
	frameRectFull.top    = outTopHeightFull;
	frameRectFull.bottom = outTopHeightFull + frameHeight;

	frameRectScaled.left   = -pOffset->cx;
	frameRectScaled.right  = 0;
	frameRectScaled.bottom = -pOffset->cy + (((row + 1i64) * frameHeight + outTopHeightFull) * pImageSize->cy) / h; // +1i64 instead of +1 to force a 64-bit calculation and avoid overflows during the intermediate steps.

	while (frame < totalFrames)
	{
		if (col == 0)
		{
			borderFrameRectFull.left   = 0;
			borderFrameRectFull.right  = outLeftWidthFull;
			borderFrameRectFull.top    = frameRectFull.top;
			borderFrameRectFull.bottom = frameRectFull.bottom;

			borderFrameRectScaled.left   = -pOffset->cx;
			borderFrameRectScaled.right  = -pOffset->cx + outLeftWidthScaled;
			borderFrameRectScaled.top    = frameRectScaled.top;
			borderFrameRectScaled.bottom = frameRectScaled.bottom;

			if (outLeftWidthFull != 0)
			{
				if (translateRects2(borderFrameRectScaled, borderFrameRectFull, windowRect, &frameWindowOffset, &frameSize))
				{
					m_pOutsideLeftBorder->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
				}
			}

			frameRectFull.left  = borderFrameRectFull.right;
			frameRectFull.right = borderFrameRectFull.right + frameWidth;

			frameRectScaled.left  = borderFrameRectScaled.right;
		}

		frameRectScaled.right = -pOffset->cx + (((col + 1i64) * frameWidth + outLeftWidthFull) * pImageSize->cx) / w; // +1i64 instead of +1 to force a 64-bit calculation and avoid overflows during the intermediate steps.

		if (!translateRects2(frameRectScaled, frameRectFull, windowRect, &frameWindowOffset, &frameSize))
		{
			m_pFrames->GetFrame(frame)->DeleteCachedPaintBitmap();
		}
		else
		{
			m_pFrames->GetFrame(frame)->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
		}

		++frame;

		frameRectFull.left  = frameRectFull.right;
		frameRectFull.right += frameWidth;

		frameRectScaled.left  = frameRectScaled.right;

		if (++col == m_numColumns)
		{
			if (outRightWidthFull != 0)
			{
				borderFrameRectFull = frameRectFull;
				borderFrameRectFull.right = w;

				borderFrameRectScaled = frameRectScaled;
				borderFrameRectScaled.right = -pOffset->cx + pImageSize->cx;

				if (translateRects2(borderFrameRectScaled, borderFrameRectFull, windowRect, &frameWindowOffset, &frameSize))
				{
					m_pOutsideRightBorder->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
				}
			}

			col = 0;
			++row;

			frameRectFull.top    = frameRectFull.bottom;
			frameRectFull.bottom += frameHeight;

			frameRectScaled.top    = frameRectScaled.bottom;
			frameRectScaled.bottom = -pOffset->cy + (((row + 1i64) * frameHeight + outTopHeightFull) * pImageSize->cy) / h; // +1i64 instead of +1 to force a 64-bit calculation and avoid overflows during the intermediate steps.
		}
	}

	bool bUsedEmpty = false;

	if (col != 0 && m_pEmptyImage != NULL)
	{
		if (outRightWidthFull != 0)
		{
			borderFrameRectFull = frameRectFull;
			borderFrameRectFull.right = borderFrameRectFull.left + outRightWidthFull;

			borderFrameRectScaled = frameRectScaled;
			borderFrameRectScaled.right = -pOffset->cx + static_cast<LONG>( (((col + 0i64) * frameWidth + outLeftWidthFull * 2i64) * pImageSize->cx) / w ); // +0i64 instead of +0 to force a 64-bit calculation and avoid overflows during the intermediate steps.

			if (translateRects2(borderFrameRectScaled, borderFrameRectFull, windowRect, &frameWindowOffset, &frameSize))
			{
				m_pOutsideRightBorder->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
			}

			frameRectFull.left = borderFrameRectFull.right;
		
			frameRectScaled.left = borderFrameRectScaled.right;
		}

		frameRectFull.right = w;
		frameRectScaled.right = -pOffset->cx + pImageSize->cx;

		if (translateRects2(frameRectScaled, frameRectFull, windowRect, &frameWindowOffset, &frameSize))
		{
			m_pEmptyImage->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
			bUsedEmpty = true;
		}
	}

	if (!bUsedEmpty && m_pEmptyImage != NULL)
	{
		m_pEmptyImage->DeleteCachedPaintBitmap();
	}

	if (outBottomHeightFull != 0)
	{
		borderFrameRectFull.left   = 0;
		borderFrameRectFull.right  = w;
		borderFrameRectFull.top    = frameRectFull.top;
		borderFrameRectFull.bottom = h;

		borderFrameRectScaled.left   = -pOffset->cx;
		borderFrameRectScaled.right  = -pOffset->cx + pImageSize->cx;
		borderFrameRectScaled.top    = frameRectScaled.top;
		borderFrameRectScaled.bottom = -pOffset->cy + pImageSize->cy;

		if (translateRects2(borderFrameRectScaled, borderFrameRectFull, windowRect, &frameWindowOffset, &frameSize))
		{
			m_pOutsideBottomBorder->Paint(hDC, &frameWindowOffset, pWindowSize, &frameSize, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);
		}
	}
}

void CFlatImage::DeleteCachedPaintBitmap()
{
	if (m_pEmptyImage != NULL)
	{
		m_pEmptyImage->DeleteCachedPaintBitmap();
	}
}

void CFlatImage::GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	// No Op.
}

void CFlatImage::PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
{
	// Normally Paint calls PaintUnmodified if needed but we've overridden Paint and implemented everything in there.

	if (InitOK())
	{
		Paint(hDC, pOffset, pWindowSize, pImageSize, 0, false, NULL, NULL);
	}
}
