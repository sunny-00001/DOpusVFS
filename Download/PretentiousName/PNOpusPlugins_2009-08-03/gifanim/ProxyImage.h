#pragma once
#include "AbstractImage.h"

// This class allows for different cached bitmaps based on the same underlying pixels.
// It was made to support tiled mode, where image selection may mean we need different
// representations of the same image.

class CProxyImage : public CAbstractImage
{
public:

	CProxyImage(CAbstractImage *pImage);
	virtual ~CProxyImage();

private:

	CProxyImage(const CProxyImage &rhs); // disallow
	CProxyImage &operator=(const CProxyImage &rhs); // disallow

public:
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

	virtual void GetRGBQPixel(RGBQUAD *pPixel, int x, int y);

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest);

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void DeleteRGBQPixels(RGBQUAD *pPixels);

	virtual void PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize);

protected:

	CAbstractImage *m_pImage;
};
