#include "StdAfx.h"

#if 0

#include "LeoHelpers.h"
#include "SprocketBorderImage.h"

CSprocketBorderImage::CSprocketBorderImage(BORDER_TYPE t, int iWidth, int iHeight, int iOtherWidth, int iOtherHeight, int iScaleMode, bool bUseAlphaChannel, const RGBQUAD *prgbBackground, const RGBQUAD *prgbFill, const int iFillAlpha)
: CAbstractImage(iScaleMode)
, m_type(t)
, m_iWidth(iWidth)
, m_iHeight(iHeight)
, m_iOtherWidth(iOtherWidth)
, m_iOtherHeight(iOtherHeight)
, m_bUseAlphaChannel(bUseAlphaChannel)
, m_rgbFillFromConstructor(*prgbFill)
, m_iFillAlpha(iFillAlpha)
{
	generateColors(prgbBackground, m_rgbqBlack, m_rgbqSpace, m_rgbqFill, m_rgbqShadow);
}

CSprocketBorderImage::~CSprocketBorderImage()
{
}

void CSprocketBorderImage::UpdateViewerBackgroundColor(COLORREF crNewBackground)
{
	DeleteCachedPaintBitmap();

	RGBQUAD rgbNewBackground;
	rgbNewBackground.rgbRed   = GetRValue(crNewBackground);
	rgbNewBackground.rgbGreen = GetGValue(crNewBackground);
	rgbNewBackground.rgbBlue  = GetBValue(crNewBackground);
	rgbNewBackground.rgbReserved = 255;

	generateColors(&rgbNewBackground, m_rgbqBlack, m_rgbqSpace, m_rgbqFill, m_rgbqShadow);
}

void CSprocketBorderImage::generateColors(const RGBQUAD *prgbNewBackground, RGBQUAD &rgbqBlack, RGBQUAD &rgbqSpace, RGBQUAD &rgbqFill, RGBQUAD (&rgbqShadow)[6]) const
{
	int iTotalBack = prgbNewBackground->rgbRed;
	iTotalBack += prgbNewBackground->rgbGreen;
	iTotalBack += prgbNewBackground->rgbBlue;

	bool bInverse = (iTotalBack < 300);

	rgbqBlack.rgbRed   = (bInverse ? 255 : 0);
	rgbqBlack.rgbGreen = (bInverse ? 255 : 0);
	rgbqBlack.rgbBlue  = (bInverse ? 255 : 0);
	rgbqBlack.rgbReserved = 255;

	rgbqSpace = *prgbNewBackground;
	rgbqSpace.rgbReserved = (m_bUseAlphaChannel ? 0 : 255);

//	int iShadowAlpha = (bInverse ? 100 : 25);

	int iShadowAlpha[6] = { 115, 71, 29, 15, 9, 0 };
	
	int iBlackAlpha = 200;

	if (m_bUseAlphaChannel)
	{
		rgbqFill = m_rgbFillFromConstructor;
		rgbqFill.rgbReserved = m_iFillAlpha;

		for (int i = 0; i < 6; ++i)
		{
			rgbqShadow[i] = rgbqBlack;
			rgbqShadow[i].rgbReserved = iShadowAlpha[i];
		}

		rgbqBlack.rgbReserved = iBlackAlpha;
	}
	else
	{
		rgbqFill.rgbRed   = (m_iFillAlpha * m_rgbFillFromConstructor.rgbRed   + (255 - m_iFillAlpha) * rgbqSpace.rgbRed)   / 255;
		rgbqFill.rgbGreen = (m_iFillAlpha * m_rgbFillFromConstructor.rgbGreen + (255 - m_iFillAlpha) * rgbqSpace.rgbGreen) / 255;
		rgbqFill.rgbBlue  = (m_iFillAlpha * m_rgbFillFromConstructor.rgbBlue  + (255 - m_iFillAlpha) * rgbqSpace.rgbBlue)  / 255;
		rgbqFill.rgbReserved = 255;

		for (int i = 0; i < 6; ++i)
		{
			rgbqShadow[i].rgbRed   = (iShadowAlpha[i] * rgbqBlack.rgbRed   + (255 - iShadowAlpha[i]) * rgbqSpace.rgbRed)   / 255;
			rgbqShadow[i].rgbGreen = (iShadowAlpha[i] * rgbqBlack.rgbGreen + (255 - iShadowAlpha[i]) * rgbqSpace.rgbGreen) / 255;
			rgbqShadow[i].rgbBlue  = (iShadowAlpha[i] * rgbqBlack.rgbBlue  + (255 - iShadowAlpha[i]) * rgbqSpace.rgbBlue)  / 255;
			rgbqShadow[i].rgbReserved = 255;
		}

		rgbqBlack.rgbRed   = (iBlackAlpha * rgbqBlack.rgbRed   + (255 - iBlackAlpha) * rgbqSpace.rgbRed)   / 255;
		rgbqBlack.rgbGreen = (iBlackAlpha * rgbqBlack.rgbGreen + (255 - iBlackAlpha) * rgbqSpace.rgbGreen) / 255;
		rgbqBlack.rgbBlue  = (iBlackAlpha * rgbqBlack.rgbBlue  + (255 - iBlackAlpha) * rgbqSpace.rgbBlue)  / 255;
	}

	for (int i = 0; i < 6; ++i)
	{
		rgbqShadow[i] = rgbqSpace;
	}
}

void CSprocketBorderImage::HideAlphaChannel()
{
	// No-op
}

bool CSprocketBorderImage::RotateImage(int rotationAmount)
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

			switch(m_type)
			{
			case TOP_THICK:		m_type = LEFT_THICK;	break;
			case LEFT_THICK:	m_type = BOTTOM_THICK;	break;
			case RIGHT_THICK:	m_type = TOP_THICK;		break;
			case BOTTOM_THICK:	m_type = RIGHT_THICK;	break;
			case TOP_THIN:		m_type = LEFT_THIN;		break;
			case LEFT_THIN:		m_type = BOTTOM_THIN;	break;
			case RIGHT_THIN:	m_type = TOP_THIN;		break;
			case BOTTOM_THIN:	m_type = RIGHT_THIN;	break;
			default: break;
			}

			bResult = true;
		}
		else if (180 <= rotationAmount)
		{
			switch(m_type)
			{
			case TOP_THICK:		m_type = BOTTOM_THICK;	break;
			case LEFT_THICK:	m_type = RIGHT_THICK;	break;
			case RIGHT_THICK:	m_type = LEFT_THICK;	break;
			case BOTTOM_THICK:	m_type = TOP_THICK;		break;
			case TOP_THIN:		m_type = BOTTOM_THIN;	break;
			case LEFT_THIN:		m_type = RIGHT_THIN;	break;
			case RIGHT_THIN:	m_type = LEFT_THIN;		break;
			case BOTTOM_THIN:	m_type = TOP_THIN;		break;
			default: break;
			}

			bResult = true;
		}
		else if (90 <= rotationAmount)
		{
			// Transpose width and height.
			m_iWidth  = origGlobalY;
			m_iHeight = origGlobalX;

			switch(m_type)
			{
			case TOP_THICK:		m_type = RIGHT_THICK;	break;
			case LEFT_THICK:	m_type = TOP_THICK;		break;
			case RIGHT_THICK:	m_type = BOTTOM_THICK;	break;
			case BOTTOM_THICK:	m_type = LEFT_THICK;	break;
			case TOP_THIN:		m_type = RIGHT_THIN;	break;
			case LEFT_THIN:		m_type = TOP_THIN;		break;
			case RIGHT_THIN:	m_type = BOTTOM_THIN;	break;
			case BOTTOM_THIN:	m_type = LEFT_THIN;		break;
			default: break;
			}

			bResult = true;
		}
		else if (0 <= rotationAmount)
		{
			bResult = true;
		}
	}

	return bResult;
}

bool CSprocketBorderImage::InitOK() const
{
	return true;
}

int CSprocketBorderImage::GetWidth() const
{
	return m_iWidth;
}

int CSprocketBorderImage::GetHeight() const
{
	return m_iHeight;
}

const TCHAR *CSprocketBorderImage::GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
{
	return NULL;
}

bool CSprocketBorderImage::GetHasUsedTransparency() const
{
	return true;
}

int CSprocketBorderImage::GetDelayTime() const
{
	return 0;
}

COLORREF CSprocketBorderImage::GetCurrentBackgroundColor() const
{
	return RGB(m_rgbqSpace.rgbRed, m_rgbqSpace.rgbGreen, m_rgbqSpace.rgbBlue);
}

COLORREF CSprocketBorderImage::GetAutomaticBackgroundColor() const
{
	return GetCurrentBackgroundColor();
}

void CSprocketBorderImage::CopyAllRGBQPixels(RGBQUAD *pDest)
{
	CopyRGBQPixelRect(pDest, NULL, NULL, true, NULL, 0, NULL, false, NULL, NULL);
}

void CSprocketBorderImage::CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	if (InitOK())
	{
		RGBQUAD rgbqBlack;
		RGBQUAD rgbqSpace;
		RGBQUAD rgbqFill;
		RGBQUAD rgbqShadow[6];

		if (NULL != pRGBForTransparent)
		{
			generateColors(pRGBForTransparent, rgbqBlack, rgbqSpace, rgbqFill, rgbqShadow);
		}
		else
		{
			rgbqBlack  = m_rgbqBlack;
			rgbqSpace  = m_rgbqSpace;
			rgbqFill   = m_rgbqFill;
			
			for (int i = 0; i < 6; ++i)
			{
				rgbqShadow[i] = m_rgbqShadow[i];
			}
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

		if (!bGammaEnable)
		{
			GammaTable = NULL;
		}

		RECT r;
		std::vector< std::pair< RECT, RGBQUAD > > vecRecs(50);
		vecRecs.push_back( std::pair< RECT, RGBQUAD >( *pWantedRect, rgbqSpace ) );

		switch(m_type)
		{
		case BOTTOM_THICK:
		case TOP_THICK:
			{
				bool bTop = (m_type == TOP_THICK);

				const int topGap = bTop ? 3 : 1;
				const int bottomGap = bTop ? 1 : 3;
				const int frameSize = max(h - 4, 2);
				const int solidSize = max(frameSize / 4, 1);
				const int holeSize  = max(frameSize - 2 * solidSize, 0);
				const int halfHoleSize = holeSize / 2;
				const int otherHalfHoleSize = holeSize - halfHoleSize;
				const int holeAndGapSize = static_cast<int>(holeSize * 2.75);
				const int holeTop = topGap + solidSize;
				const int holeBottom = h - (bottomGap + solidSize);


				if (bTop)
				{
					SetRect(&r, 0, topGap, w, (h - bottomGap) + 1);
					vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );

					SetRect(&r, m_iOtherWidth - 1, h - bottomGap, w - (m_iOtherWidth - 1), (h - bottomGap) + 1);
					vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqShadow[1] ) );

					addShadowHoleRect(r, vecRecs, 2, rgbqShadow, 0, 0, w, topGap, 0, 0, 0, -1);
				}
				else
				{
					SetRect(&r, 0, topGap - 1, w, h - bottomGap);
					vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );

					SetRect(&r, m_iOtherWidth - 1, topGap - 1, w - (m_iOtherWidth - 1), topGap);
					vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqShadow[1] ) );

					addShadowHoleRect(r, vecRecs, 1, rgbqShadow, 0, h - bottomGap, w, h, 0, 1, 0, 0);
				}

				if (holeSize > 0)
				{
					const int numHoles = w / holeAndGapSize;

					if (numHoles > 0)
					{
						int x;

						for (int i = 0; i <= numHoles; ++i)
						{
							x = (i * w) / numHoles;
							addShadowHoleRect(r, vecRecs, 1, rgbqShadow, x - halfHoleSize, holeTop, x + otherHalfHoleSize, holeBottom, 1, 1, 0, 0);
						}
					}
				}
			}
			break;
		case LEFT_THIN:
			SetRect(&r, 0, 0, w-1, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );
			SetRect(&r, w-1, 0, w, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqShadow[1] ) );
			break;
		case RIGHT_THIN:
			SetRect(&r, 1, 0, w, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );
			SetRect(&r, 0, 0, 1, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqShadow[1] ) );
			break;
		case LEFT_THICK:
			break;
		case RIGHT_THICK:
			break;
		case TOP_THIN:
			break;
		case BOTTOM_THIN:
			break;
		case LEFT_OUTER:
			SetRect(&r, 0, 0, w, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );
			break;
		case RIGHT_OUTER:
			addShadowHoleRect(r, vecRecs, 1, rgbqShadow, 0, 0, w, h, 1, 1, 0, 0);
			break;
		case TOP_OUTER:
			SetRect(&r, 0, 0, w, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );
			break;
		case BOTTOM_OUTER:
			SetRect(&r, 0, 0, w, h);
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqBlack  ) );
			break;
		default:
			break;
		}

		RECT wantedFrameRect;
		RECT wantedFrameRelFrameRect;
		SIZE frameDestOffset;

		for(std::vector< std::pair< RECT, RGBQUAD > >::const_iterator vecIter = vecRecs.begin(); vecIter != vecRecs.end(); ++vecIter)
		{
			const RECT &frameRect = vecIter->first;
			const RGBQUAD &rgbq   = vecIter->second;

			if (IntersectRect(&wantedFrameRect, pWantedRect, &frameRect))
			{
				frameDestOffset.cx = pDestOffset->cx + (wantedFrameRect.left - pWantedRect->left);
				frameDestOffset.cy = pDestOffset->cy + (wantedFrameRect.top  - pWantedRect->top);

				wantedFrameRelFrameRect.left   = wantedFrameRect.left   - frameRect.left;
				wantedFrameRelFrameRect.top    = wantedFrameRect.top    - frameRect.top;
				wantedFrameRelFrameRect.right  = wantedFrameRect.right  - frameRect.left;
				wantedFrameRelFrameRect.bottom = wantedFrameRect.bottom - frameRect.top;

				FillRGBQPixelRect(pDest, pDestSize, &frameDestOffset, bTopDown, &wantedFrameRelFrameRect, &rgbq);
			}
		}

//		blurPreserveAlpha(pDest, pDest, w, h, rgbqSpace);
	}
}

RGBQUAD *CSprocketBorderImage::GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
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

void CSprocketBorderImage::DeleteRGBQPixels(RGBQUAD *pPixels)
{
	delete [] pPixels;
}

#endif
