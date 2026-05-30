#pragma once
#include "AbstractImage.h"

class CImageInImage : public CAbstractImage
{
public:

	CImageInImage(CAbstractImage *pOuter, CAbstractImage *pInner, const RECT &rectInnerInOuter);
	virtual ~CImageInImage(void);

	static void ScaleFrames(const RECT &rectOuter, const RECT &rectInner, const SIZE *pSizeOuterScaled, RECT &rectOuterScaled, RECT &rectInnerScaled);

private:

	CImageInImage(const CImageInImage &rhs); // disallow
	CImageInImage &operator=(const CImageInImage &rhs); // disallow

	virtual void DeleteCachedPaintBitmap();
	virtual void GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

	virtual COLORREF GetCurrentBackgroundColor() const;
	virtual COLORREF GetAutomaticBackgroundColor() const;
	virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground);
	virtual void HideAlphaChannel();
	virtual bool RotateImage(int rotationAmount);

	virtual bool InitOK() const   { return m_bInitOK; }
	virtual int GetWidth() const  { return (m_bInitOK ? m_pOuter->GetWidth() : 0); }
	virtual int GetHeight() const { return (m_bInitOK ? m_pOuter->GetHeight() : 0); }
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

	CAbstractImage *m_pOuter;
	CAbstractImage *m_pInner;

	RECT m_rectInner;
};
