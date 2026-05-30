#pragma once
#include "AbstractImage.h"

class CEmptyImage;

class CFlatImage : public CAbstractImage
{
public:

	CFlatImage(CAbstractImageList *pFrames, int iNumberOfColumns, int iScaleMode, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper);
	virtual ~CFlatImage();

private:

	CFlatImage(const CFlatImage &rhs); // disallow
	CFlatImage &operator=(const CFlatImage &rhs); // disallow

public:

	void SetNumberOfColumns(CAbstractImageList::size_type numColumns, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper);

	virtual void DeleteCachedPaintBitmap();
	virtual void GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

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
	virtual bool AllowFrameAroundImage() const { return !m_bBorders; }

	virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest);

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void DeleteRGBQPixels(RGBQUAD *pPixels);

	virtual void PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize);

protected:

	void deleteBorders();

	inline bool translateRects1(const RECT &frameRect, const SIZE *pDestOffset, const RECT *pWantedRect, //const RECT *pHighlightRect,
								SIZE *pFrameDestOffset, RECT *pWantedFrameRelFrameRect) //, RECT *pHighlightFrameRelFrameRect, RECT **ppHighlightFrameRelFrameRect)
	{
		RECT wantedFrameRect;
	//	RECT highlightFrameRect;

		if (!::IntersectRect(&wantedFrameRect, pWantedRect, &frameRect))
		{
			return false;
		}

		pFrameDestOffset->cx = pDestOffset->cx + (wantedFrameRect.left - pWantedRect->left);
		pFrameDestOffset->cy = pDestOffset->cy + (wantedFrameRect.top  - pWantedRect->top);

		pWantedFrameRelFrameRect->left   = wantedFrameRect.left   - frameRect.left;
		pWantedFrameRelFrameRect->top    = wantedFrameRect.top    - frameRect.top;
		pWantedFrameRelFrameRect->right  = wantedFrameRect.right  - frameRect.left;
		pWantedFrameRelFrameRect->bottom = wantedFrameRect.bottom - frameRect.top;

	//	if (::IntersectRect(&highlightFrameRect, pHighlightRect, &frameRect))
	//	{
	//		pHighlightFrameRelFrameRect->left   = highlightFrameRect.left   - frameRect.left;
	//		pHighlightFrameRelFrameRect->top    = highlightFrameRect.top    - frameRect.top;
	//		pHighlightFrameRelFrameRect->right  = highlightFrameRect.right  - frameRect.left;
	//		pHighlightFrameRelFrameRect->bottom = highlightFrameRect.bottom - frameRect.top;
	//
	//		*ppHighlightFrameRelFrameRect = pHighlightFrameRelFrameRect;
	//	}
	//	else
	//	{
	//		*ppHighlightFrameRelFrameRect = NULL;
	//	}

		return true;
	}

	inline bool translateRects2(const RECT &frameRectScaled, const RECT &frameRectFull, const RECT &windowRect, //const RECT &selectRect, const bool &bSelectImage,
								SIZE *pFrameWindowOffset, SIZE *pFrameSize) //, RECT *pSelectFrameRelFrameRect, RECT **ppSelectFrameRelFrameRect)
	{
		RECT wantedFrameRect;
	//	RECT selectFrameRect;

		if (!::IntersectRect(&wantedFrameRect, &windowRect, &frameRectScaled))
		{
			return false;
		}

		pFrameWindowOffset->cx = windowRect.left - frameRectScaled.left;
		pFrameWindowOffset->cy = windowRect.top  - frameRectScaled.top;

		pFrameSize->cx = frameRectScaled.right  - frameRectScaled.left;
		pFrameSize->cy = frameRectScaled.bottom - frameRectScaled.top;

	//	if (bSelectImage && ::IntersectRect(&selectFrameRect, &selectRect, &frameRectFull))
	//	{
	//		pSelectFrameRelFrameRect->left   = selectFrameRect.left   - frameRectFull.left;
	//		pSelectFrameRelFrameRect->top    = selectFrameRect.top    - frameRectFull.top;
	//		pSelectFrameRelFrameRect->right  = selectFrameRect.right  - frameRectFull.left;
	//		pSelectFrameRelFrameRect->bottom = selectFrameRect.bottom - frameRectFull.top;
	//
	//		*ppSelectFrameRelFrameRect = pSelectFrameRelFrameRect;
	//	}
	//	else
	//	{
	//		*ppSelectFrameRelFrameRect = NULL;
	//	}

		return true;
	}

protected:

	bool m_bInitOK;

	int m_iWidth;
	int m_iHeight;

	bool m_bBorders;
	bool m_bOwnImageList;

	CAbstractImage *m_pBorderType2;

	CAbstractImage *m_pTopBorder;
	CAbstractImage *m_pLeftBorder;
	CAbstractImage *m_pRightBorder;
	CAbstractImage *m_pBottomBorder;

	CAbstractImage *m_pOutsideTopBorder;
	CAbstractImage *m_pOutsideLeftBorder;
	CAbstractImage *m_pOutsideRightBorder;
	CAbstractImage *m_pOutsideBottomBorder;

	CAbstractImage *m_pOutsideTopLeftBorder;
	CAbstractImage *m_pOutsideTopRightBorder;
	CAbstractImage *m_pOutsideBottomLeftBorder;
	CAbstractImage *m_pOutsideBottomRightBorder;

	CEmptyImage *m_pEmptyImage;

	CAbstractImageList::size_type m_numColumns;
	CAbstractImageList::size_type m_numRows;

	CAbstractImageList *m_pFrames;
	CAbstractImageList *m_pSourceFrames;
};
