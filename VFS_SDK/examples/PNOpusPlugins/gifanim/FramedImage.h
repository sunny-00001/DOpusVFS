#pragma once

#if 0

#include "abstractimage.h"

class CFramedImage : public CAbstractImage
{
public:

	CFramedImage(CAbstractImage *pCentre, CAbstractImage *pTop, CAbstractImage *pLeft, CAbstractImage *pRight, CAbstractImage *pBottom, bool bTopSpans);
	virtual ~CFramedImage();

private:

	CFramedImage(const CFramedImage &rhs); // disallow
	CFramedImage &operator=(const CFramedImage &rhs); // disallow

public:

	virtual void DeleteCachedPaintBitmap();
	virtual void GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

	virtual COLORREF GetCurrentBackgroundColor() const;
	virtual COLORREF GetAutomaticBackgroundColor() const;
	virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground);
	virtual void HideAlphaChannel();
	virtual bool RotateImage(int rotationAmount);

	virtual bool InitOK() const   { return m_bInitOK; }
	virtual int GetWidth() const  { return (m_bInitOK ? m_width : 0); }
	virtual int GetHeight() const { return (m_bInitOK ? m_height : 0); }
	virtual bool GetHasUsedTransparency() const;
	virtual int GetDelayTime() const;
	virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const;

	virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest);

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void DeleteRGBQPixels(RGBQUAD *pPixels);

	virtual void PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize);

protected:

	bool m_bInitOK;

	bool m_bTopSpans;

	CAbstractImage *m_pCentre;
	CAbstractImage *m_pTop;
	CAbstractImage *m_pLeft;
	CAbstractImage *m_pRight;
	CAbstractImage *m_pBottom;

	RECT m_rectCentre;
	RECT m_rectTop;
	RECT m_rectLeft;
	RECT m_rectRight;
	RECT m_rectBottom;

	int m_width;
	int m_height;
};

#endif
