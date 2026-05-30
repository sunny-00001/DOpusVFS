#pragma once
#include "ImageInImage.h"
#include "AbstractImage.h"

class CPictureFrameImage : public CAbstractImage
{
private:
	RGBQUAD *m_pCachePixels;
	int m_iWidth;
	int m_iHeight;
	COLORREF m_crCurrentBackgroundColor;
	RECT m_rectFrame;
	DOpusPluginHelperUtil *m_pPluginHelper;

private:

	CPictureFrameImage(const CPictureFrameImage &rhs); // disallow
	CPictureFrameImage &operator=(const CPictureFrameImage &rhs); // disallow

public:
	CPictureFrameImage(int iWidth, int iHeight, const RECT &rectFrame, int iScaleMode, COLORREF crBackground, DOpusPluginHelperUtil *pPluginHelper)
	: CAbstractImage(iScaleMode)
	, m_pCachePixels(NULL)
	, m_iWidth(iWidth)
	, m_iHeight(iHeight)
	, m_crCurrentBackgroundColor(crBackground)
	, m_rectFrame(rectFrame)
	, m_pPluginHelper(pPluginHelper)
	{
//		if (0 < m_iWidth && 0 < m_iHeight)
//		{
//			m_pPixelData = new(std::nothrow) RGBQUAD[ m_iWidth * m_iHeight ];
//		}

		UpdateViewerBackgroundColor(crBackground);
	}
		
	virtual ~CPictureFrameImage(void)
	{
		// DeleteCachedPaintBitmap is called by ~CAbstractImage
	}

	virtual COLORREF GetCurrentBackgroundColor() const
	{
		if (!InitOK())
		{
			return RGB(0,0,0);
		}

		return m_crCurrentBackgroundColor;
	}

	virtual COLORREF GetAutomaticBackgroundColor() const
	{
		return GetCurrentBackgroundColor();
	}

	virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground)
	{
		DeleteCachedPaintBitmap();

		m_crCurrentBackgroundColor = crNewBackground;
	}

	virtual void HideAlphaChannel()
	{
		// No-op. Frames always keep their alpha channel.
	}

	virtual bool RotateImage(int rotationAmount)
	{
		DeleteCachedPaintBitmap();
		// This is easy enough to implement but isn't needed for now.
		return false;
	}

	virtual bool InitOK() const						{ return(true);			}
	virtual int GetWidth() const					{ return(m_iWidth);		}
	virtual int GetHeight() const					{ return(m_iHeight);	}
	virtual bool GetHasUsedTransparency() const		{ return(true);			}
	virtual int GetDelayTime() const				{ return(0);			}
	virtual bool AllowFrameAroundImage() const		{ return false;			}

	virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const { return NULL; }

	virtual void DeleteCachedPaintBitmap()
	{
		if (NULL != m_hbmCachedImage)
		{
			DeleteObject(m_hbmCachedImage);
			m_hbmCachedImage = NULL;
		}

		m_pCachePixels = NULL;

		m_sizeCachedImage.cx = 0;
		m_sizeCachedImage.cy = 0;
		m_bCachedImageIsNotReallyScaled = false;
		m_iCachedImageDitherOffset = 0;
		m_bCachedImageGammaEnable = false;
		m_dCachedImageGammaValue = 0.0;
	}

	virtual void GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
	{
		if (InitOK())
		{
			SIZE altSize;

			if (NULL == pImageSize)
			{
				altSize.cx = GetWidth();
				altSize.cy = GetHeight();
				pImageSize = &altSize;
			}

			if (m_sizeCachedImage.cx != pImageSize->cx
			||	m_sizeCachedImage.cy != pImageSize->cy)
			{
				DeleteCachedPaintBitmap();
			}

			if (NULL == m_hbmCachedImage)
			{
				bool bReleaseDC = false;

				if (NULL == hDC
				&&	NULL != (hDC = ::GetDC(NULL)))
				{
					bReleaseDC = true;
				}

				if (NULL != hDC)
				{
					BITMAPINFO bitmapInfo;
					LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, pImageSize->cx, pImageSize->cy, true);

					m_hbmCachedImage = ::CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_pCachePixels), NULL, 0);

					if (NULL != m_hbmCachedImage
					&&	NULL != m_pCachePixels)
					{
						const int w = GetWidth();
						const int h = GetHeight();

						RECT rectOuter;
						::SetRect(&rectOuter, 0, 0, GetWidth(), GetHeight());

						RECT rectOuterScaled;
						RECT rectInnerScaled;
						CImageInImage::ScaleFrames(rectOuter, m_rectFrame, pImageSize, rectOuterScaled, rectInnerScaled);

						const int numPixels = pImageSize->cx * pImageSize->cy;

						RGBQUAD rgbBackground;
						rgbBackground.rgbRed      = GetRValue(m_crCurrentBackgroundColor);
						rgbBackground.rgbGreen    = GetGValue(m_crCurrentBackgroundColor);
						rgbBackground.rgbBlue     = GetBValue(m_crCurrentBackgroundColor);
						rgbBackground.rgbReserved = 255;

						RGBQUAD *pDest = m_pCachePixels;

						for(int i = 0; i < numPixels; ++i)
						{
							*pDest++ = rgbBackground;
						}

						int iMinGap = rectInnerScaled.left - rectOuterScaled.left;

						if (iMinGap > (rectInnerScaled.top - rectOuterScaled.top))
						{
							iMinGap = (rectInnerScaled.top - rectOuterScaled.top);
						}
						if (iMinGap > (rectOuterScaled.right - rectInnerScaled.right))
						{
							iMinGap = (rectOuterScaled.right - rectInnerScaled.right);
						}
						if (iMinGap > (rectOuterScaled.bottom - rectInnerScaled.bottom))
						{
							iMinGap = (rectOuterScaled.bottom - rectInnerScaled.bottom);
						}

						int iFrameSize = VIEWPIC_FRAMESIZE;
						int iShadowSize = VIEWPIC_SHADOWSIZE;

						if (iMinGap < (iFrameSize + iShadowSize))
						{
							iFrameSize  = (VIEWPIC_FRAMESIZE  * iMinGap) / (VIEWPIC_FRAMESIZE + VIEWPIC_SHADOWSIZE);
							iShadowSize = (VIEWPIC_SHADOWSIZE * iMinGap) / (VIEWPIC_FRAMESIZE + VIEWPIC_SHADOWSIZE);

							while (iMinGap > (iFrameSize + iShadowSize) && iFrameSize < VIEWPIC_FRAMESIZE)
							{
								++iFrameSize;
							}

							while (iFrameSize < 2 && iShadowSize > 0)
							{
								++iFrameSize;
								--iShadowSize;
							}

							if (iFrameSize  <  2) { iFrameSize  = 2; } // Else it will trigger the default size, or be 1 which looks rubbish.
							if (iShadowSize == 0) { iShadowSize = 1; } // Else it will trigger the default size.
						}

					//	LeoHelpers::OutputDebugFormat(L"[GIFAnim] ", L"CPictureFrameImage", L"GeneratePaintCache", L"iMinGap = %d, iFrameSize = %d, iShadowSize = %d", iMinGap, iFrameSize, iShadowSize);

						m_pPluginHelper->DrawPictureFrameInDIB(&bitmapInfo, m_pCachePixels, &rectInnerScaled, iFrameSize, iShadowSize);

						m_sizeCachedImage.cx = pImageSize->cx;
						m_sizeCachedImage.cy = pImageSize->cy;
					}
				}

				if (bReleaseDC)
				{
					::ReleaseDC(NULL, hDC);
					hDC = NULL;
				}
			}
		}

	}

	virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
	{
		GeneratePaintCache(NULL, NULL, iDitherOffset, bGammaEnable, pdGammaValue, GammaTable);

		if (!InitOK()
		||	m_hbmCachedImage == NULL
		||	m_pCachePixels == NULL)
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

			const RGBQUAD *pSource = m_pCachePixels + xStart + yStart * w;

			for (int y = yStart; y < yEnd; ++y)
			{
				for (int x = xStart; x < xEnd; ++x)
				{
				//	copyPixelWithDither(pDest++, pSource++, x, y, w, h, iDitherOffset, pRGBForTransparent, GammaTable);
					*pDest++ = *pSource++;
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

			const RGBQUAD *pSource = m_pCachePixels + xStart + (yStart - 1) * w;

			for (int y = yStart - 1; y >= yEnd; --y)
			{
				for (int x = xStart; x < xEnd; ++x)
				{
				//	copyPixelWithDither(pDest++, pSource++, x, y, w, h, iDitherOffset, pRGBForTransparent, GammaTable);
					*pDest++ = *pSource++;
				}

				pDest   += iDestLineOffset;
				pSource += iSourceLineOffset;
			}
		}
	}

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest)
	{
		GeneratePaintCache(NULL, NULL, 0, false, NULL, NULL);

		if (!InitOK()
		||	m_hbmCachedImage == NULL
		||	m_pCachePixels == NULL)
		{
			return;
		}

		int iNumPixels = GetWidth() * GetHeight();

		RGBQUAD *pSource = m_pCachePixels;

		while(iNumPixels-- > 0)
		{
			*pDest++ = *pSource++;
		}
	}

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
	{
		RGBQUAD *pRGBQPixels = NULL;
		*pbDelete = false;

		if (InitOK())
		{
			const int iNumPixels = GetWidth() * GetHeight();

			pRGBQPixels = new(std::nothrow) RGBQUAD[ iNumPixels ];

			if (NULL != pRGBQPixels)
			{
				*pbDelete = true;

				CopyRGBQPixelRect(pRGBQPixels, NULL, NULL, true, NULL, iDitherOffset, NULL, bGammaEnable, pdGammaValue, GammaTable);
			}
		}

		return pRGBQPixels;
	}

	virtual void DeleteRGBQPixels(RGBQUAD *pPixels)
	{
		delete [] pPixels;
	}

	virtual void PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize)
	{
		assert(false);
	}
};
