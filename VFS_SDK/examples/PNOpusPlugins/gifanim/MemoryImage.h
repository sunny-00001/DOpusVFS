#pragma once

#include "AbstractImage.h"

class CMemoryImage : public CAbstractImage
{
public:
	CMemoryImage(int iWidth, int iHeight, int iScaleMode);

	CMemoryImage(CAbstractImage *pImageToCopy, RECT *pRectToCopy, const wchar_t *szImageTypeName);

	virtual ~CMemoryImage();

private:
	CMemoryImage(const CMemoryImage &rhs); // disallow
	CMemoryImage &operator=(const CMemoryImage &rhs); // disallow

public:
		virtual COLORREF GetCurrentBackgroundColor() const;
		virtual COLORREF GetAutomaticBackgroundColor() const;
		virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground);
		virtual void HideAlphaChannel();
		virtual bool RotateImage(int rotationAmount);

		virtual bool InitOK() const						{ return(NULL != m_pPixelData);			}
		virtual int GetWidth() const					{ return(m_iWidth);						}
		virtual int GetHeight() const					{ return(m_iHeight);					}
		virtual bool GetHasUsedTransparency() const		{ return(m_bTransparencyActuallyUsed);	}
		virtual int GetDelayTime() const				{ return(0);							}

		virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const { return m_szImageTypeName; }

		virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

		virtual void GetRGBQPixel(RGBQUAD *pPixel, int x, int y);

		virtual void CopyAllRGBQPixels(RGBQUAD *pDest);

		virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
		virtual void DeleteRGBQPixels(RGBQUAD *pPixels);

		virtual RGBQUAD *GetRGBQStretchPreserveAlpha(int iNewWidth, int iNewHeight);
		virtual void DeleteRGBQStretchedPixels(RGBQUAD *pPixels);

protected:
	int      m_iWidth;
	int      m_iHeight;

	RGBQUAD *m_pPixelData;
	bool     m_bTransparencyActuallyUsed;
	COLORREF m_crCurrentBackgroundColor;
	COLORREF m_crAutomaticBackgroundColor;

	wchar_t *m_szImageTypeName;


protected:

	static inline bool rotate270(RGBQUAD **ppPixelData, int origX, int origY)
	{
		bool bResult = true;
		if (NULL != ppPixelData && NULL != *ppPixelData)
		{
			DWORD dwPixelDataSize = origX * origY;
			RGBQUAD *pNewPixelData = new(std::nothrow) RGBQUAD[ dwPixelDataSize ];
			if (NULL == pNewPixelData)
			{
				bResult = false;
			}
			else
			{
				DWORD dwPixelXY, dwPixelIJ;
				for (int x = 0; x < origX; x++)
				{
					for (int y = 0; y < origY; y++)
					{
						dwPixelXY = origX * y + x;
						dwPixelIJ = origY * (origX - (x+1)) + y;
						pNewPixelData[ dwPixelIJ ] = (*ppPixelData)[ dwPixelXY ];
					}
				}
				delete [] (*ppPixelData);
				(*ppPixelData) = pNewPixelData;
			}
		}
		return(bResult);
	}

	static inline bool rotate180(RGBQUAD **ppPixelData, int origX, int origY)
	{
		bool bResult = true;
		if (NULL != ppPixelData && NULL != *ppPixelData)
		{
			DWORD dwPixelDataSize = origX * origY;
			RGBQUAD *pNewPixelData = new(std::nothrow) RGBQUAD[ dwPixelDataSize ];
			if (NULL == pNewPixelData)
			{
				bResult = false;
			}
			else
			{
				DWORD dwPixelXY, dwPixelIJ;
				for (int x = 0; x < origX; x++)
				{
					for (int y = 0; y < origY; y++)
					{
						dwPixelXY = origX * y + x;
						dwPixelIJ = dwPixelDataSize - (dwPixelXY + 1);
						pNewPixelData[ dwPixelIJ ] = (*ppPixelData)[ dwPixelXY ];
					}
				}
				delete [] (*ppPixelData);
				(*ppPixelData) = pNewPixelData;
			}
		}
		return(bResult);
	}

	static inline bool rotate90(RGBQUAD **ppPixelData, int origX, int origY)
	{
		bool bResult = true;
		if (NULL != ppPixelData && NULL != *ppPixelData)
		{
			DWORD dwPixelDataSize = origX * origY;
			RGBQUAD *pNewPixelData = new(std::nothrow) RGBQUAD[ dwPixelDataSize ];
			if (NULL == pNewPixelData)
			{
				bResult = false;
			}
			else
			{
				DWORD dwPixelXY, dwPixelIJ;
				for (int x = 0; x < origX; x++)
				{
					for (int y = 0; y < origY; y++)
					{
						dwPixelXY = origX * y + x;
						dwPixelIJ = origY * x + (origY - (y+1));
						pNewPixelData[ dwPixelIJ ] = (*ppPixelData)[ dwPixelXY ];
					}
				}
				delete [] (*ppPixelData);
				(*ppPixelData) = pNewPixelData;
			}
		}
		return(bResult);
	}

	static inline void addPixelToTotals(int &t, int &r, int &g, int &b, int &a, const RGBQUAD *pSource, const RGBQUAD *pRGBForTransparent)
	{
		const RGBQUAD *pSourcePixelColour = (NULL != pRGBForTransparent && 0 == pSource->rgbReserved) ? pRGBForTransparent : pSource;

		t++;
		r += pSourcePixelColour->rgbRed;
		g += pSourcePixelColour->rgbGreen;
		b += pSourcePixelColour->rgbBlue;
		a += pSourcePixelColour->rgbReserved;
	}

	static inline void copyPixelWithDither(RGBQUAD *pDest, const RGBQUAD *pSource, int sourceX, int sourceY, int sourceWidth, int sourceHeight, int iDitherOffset, const RGBQUAD *pRGBForTransparent, const BYTE *GammaTable)
	{
		const RGBQUAD *pSourcePixelColour = (NULL != pRGBForTransparent && 0 == pSource->rgbReserved) ? pRGBForTransparent : pSource;

		if (0 == iDitherOffset || (iDitherOffset%2 == (sourceX+sourceY)%2) )
		{
			*pDest = *pSourcePixelColour;
		}
		else
		{
			// Dither offset replaces every 2nd pixel with the average of its neighbours.

			int t=0;
			int r=0;
			int g=0;
			int b=0;
			int a=0;

			if (sourceX > 0               ) { addPixelToTotals(t,r,g,b,a, pSource - 1          , pRGBForTransparent); }
			if (sourceX < (sourceWidth -1)) { addPixelToTotals(t,r,g,b,a, pSource + 1          , pRGBForTransparent); }
			if (sourceY > 0               ) { addPixelToTotals(t,r,g,b,a, pSource - sourceWidth, pRGBForTransparent); }
			if (sourceY < (sourceHeight-1)) { addPixelToTotals(t,r,g,b,a, pSource + sourceWidth, pRGBForTransparent); }

			pDest->rgbRed      = (t==0 ? pSourcePixelColour->rgbRed      : r / t);
			pDest->rgbGreen    = (t==0 ? pSourcePixelColour->rgbGreen    : g / t);
			pDest->rgbBlue     = (t==0 ? pSourcePixelColour->rgbBlue     : b / t);
			pDest->rgbReserved = (t==0 ? pSourcePixelColour->rgbReserved : a / t);
		}

		// Adjust gamma for pixels that aren't transparent.
		if (NULL != GammaTable && 0 != pDest->rgbReserved)
		{
			pDest->rgbRed   = GammaTable[ pDest->rgbRed   ];
			pDest->rgbGreen = GammaTable[ pDest->rgbGreen ];
			pDest->rgbBlue  = GammaTable[ pDest->rgbBlue  ];
		}
	}
};
