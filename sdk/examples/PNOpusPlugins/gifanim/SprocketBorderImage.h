#pragma once


#if 0


#include "AbstractImage.h"

class CSprocketBorderImage : public CAbstractImage
{
public:

	enum BORDER_TYPE
	{
		TOP_THICK,
		LEFT_THICK,
		RIGHT_THICK,
		BOTTOM_THICK,
		TOP_THIN,
		LEFT_THIN,
		RIGHT_THIN,
		BOTTOM_THIN,
		LEFT_OUTER,
		RIGHT_OUTER,
		TOP_OUTER,
		BOTTOM_OUTER
	};

	CSprocketBorderImage(BORDER_TYPE t, int iWidth, int iHeight, int iOtherWidth, int iOtherHeight, int iScaleMode, bool bUseAlphaChannel, const RGBQUAD *prgbBackground, const RGBQUAD *prgbFill, const int iFillAlpha);
	virtual ~CSprocketBorderImage();

private:

	CSprocketBorderImage(const CSprocketBorderImage &rhs); // disallow
	CSprocketBorderImage &operator=(const CSprocketBorderImage &rhs); // disallow

public:
	virtual bool AlwaysUsePaintCache() const { return true; } <-- this was removed from the base class.

	virtual COLORREF GetCurrentBackgroundColor() const;
	virtual COLORREF GetAutomaticBackgroundColor() const;
	virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground);
	virtual void HideAlphaChannel();
	virtual bool RotateImage(int rotationAmount);

	virtual bool InitOK() const;
	virtual int GetWidth() const;
	virtual int GetHeight() const;
	virtual bool GetHasUsedTransparency() const;
	virtual int GetDelayTime() const;
	virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const;

	virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest);

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void DeleteRGBQPixels(RGBQUAD *pPixels);

protected:

	void generateColors(const RGBQUAD *prgbNewBackground, RGBQUAD &rgbqBlack, RGBQUAD &rgbqSpace, RGBQUAD &rgbqFill, RGBQUAD (&rgbqShadow)[6]) const;

	static void addShadowHoleRect(RECT &r, std::vector< std::pair< RECT, RGBQUAD > > &vecRecs, int startShadow, const RGBQUAD (&rgbqShadow)[6],
								  const int left, const int top, const int right, const int bottom,
								  const int deltaLeft, const int deltaTop, const int deltaRight, const int deltaBottom)
	{
		SetRect(&r, left, top, right, bottom);

		for (int i = startShadow; i < 6 && !IsRectEmpty(&r); ++i)
		{
			vecRecs.push_back( std::pair< RECT, RGBQUAD >( r, rgbqShadow[i] ) );
			r.left   += deltaLeft;
			r.top    += deltaTop;
			r.right  += deltaRight;
			r.bottom += deltaBottom;
		}
	}


protected:

	BORDER_TYPE m_type;

	int m_iWidth;
	int m_iHeight;

	int m_iOtherWidth;
	int m_iOtherHeight;

	bool m_bUseAlphaChannel;
	int m_iFillAlpha;
	RGBQUAD m_rgbFillFromConstructor;

	RGBQUAD m_rgbqSpace;
	RGBQUAD m_rgbqBlack;
	RGBQUAD m_rgbqFill;
	RGBQUAD m_rgbqShadow[6];
};

#endif
