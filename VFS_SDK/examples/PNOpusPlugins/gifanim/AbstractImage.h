#pragma once

class CAbstractImage;
class DOpusPluginHelperUtil;

class CAbstractImageList
{
public:

//	typedef std::vector< std::pair< CAbstractImage *, bool > >::size_type size_type;
	typedef LONG size_type;

public:

	CAbstractImageList();
	virtual ~CAbstractImageList();

private:

	CAbstractImageList(const CAbstractImageList &rhs); // disallow
	CAbstractImageList &operator=(const CAbstractImageList &rhs); // disallow

public:

	void Clear();
	bool IsEmpty() const;
	size_type GetNumberOfFrames() const;

	size_type GetCurrentIndex() const;
	void SetCurrentIndex(size_type frameIndex);
	void IncrementCurrentIndex(int iDelta);

	CAbstractImage *GetFirstFrame() const;
	CAbstractImage *GetCurrentFrame() const;
	CAbstractImage *GetFrame(size_type frameIndex) const;
	CAbstractImage *GetRelativeFrame(int iDelta) const;

	void AddFrame(CAbstractImage *pFrame, bool bTakeOwnership);

	bool InitOK() const;
	bool GetHasUsedTransparency() const;
	void UpdateViewerBackgroundColor(COLORREF crNewBackground);
	void HideAlphaChannel();
	bool RotateImages(int rotationAmount);
	void DeleteCachedPaintBitmaps(CAbstractImage *pFrameNoDelete1, CAbstractImage *pFrameNoDelete2);
//	void GeneratePaintCaches(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	bool SetImageSize(const SIZE *pSize);

protected:

	size_type m_currentFrameIndex;
	bool m_bInitOK;
	bool m_bHasUsedTransparency;
	std::vector< std::pair< CAbstractImage *, bool > > m_frames;
};

class CAbstractImage
{
public:

	CAbstractImage(int iScaleMode);
	virtual ~CAbstractImage();

private:

	CAbstractImage(const CAbstractImage &rhs); // disallow
	CAbstractImage &operator=(const CAbstractImage &rhs); // disallow

public:

	virtual void DeleteCachedPaintBitmap();
	virtual void GeneratePaintCache(HDC hDC, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	virtual void Paint(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);

public:
	bool CopyToClipboard(HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset);
	bool SetDesktopWallpaper(HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset, DWORD dwWallpaperStyle);
	HBITMAP CreateDIBSection(HDC hDC, const RECT *pSelectRectNotNormalised, int iDitherOffset);
	bool Print(HDC hDCPrint, HWND hWnd, const RECT *pSelectRectNotNormalised, int iDitherOffset, LeoHelpers::OpusStringLoader *pSL);

	HBITMAP CreateThumbnailDIBSection(HDC hDC, const SIZE *pSize, bool bSprockets, bool *pbHasUsedTransparency, bool *pbWantFrame, bool *pbRegenOnResize);

	int GetScaleMode() const;
	virtual COLORREF GetCurrentBackgroundColor() const = 0;
	virtual COLORREF GetAutomaticBackgroundColor() const = 0;
	virtual void UpdateViewerBackgroundColor(COLORREF crNewBackground) = 0;
	virtual void HideAlphaChannel() = 0;
	virtual bool RotateImage(int rotationAmount) = 0;

	// Stretch bitmap and preserve alpha channel information in the RGBQUAD reserved fields.
	static void StaticCopyRGBQPixelsWithSprockets(RGBQUAD *pDestRGBQPixels, const SIZE *pDestSize, const RGBQUAD *pSourceRGBQPixels, const SIZE *pSourceSize, int inSprocketStripWidth, int inSprocketSquare, int inBorderHeight, COLORREF bgCol, BYTE fillAlpha);

	virtual bool InitOK() const = 0;
	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;
	virtual bool GetHasUsedTransparency() const = 0;
	virtual int GetDelayTime() const = 0;
	virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const = 0;
	virtual bool AllowFrameAroundImage() const { return true; }

	// Most image types will not support borders and will return false for border calls.
	virtual bool WantBorders() const { return false; }
	virtual bool NeedNewBorders(CAbstractImageList::size_type oldNumCols, CAbstractImageList::size_type oldNumRows, CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows) const { return false; }
	virtual bool GetTotalBorderSize(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, SIZE *pSize) const { return false; }
	virtual CAbstractImageList::size_type CalcNumFramesFitHoriz(LONG lSpaceWidth, int iNormalisedZoomFactor, bool bBorders) const { return (lSpaceWidth * 100) / (GetWidth() * iNormalisedZoomFactor); }
	// If CreateBorders returns true then all of the image pointers will have been set to either NULL or to images which must be deleted by the caller.
	// If CreateBorders returns false then all of the image pointers will have been set to NULL. (The callee must delete any images which were made but then not returned due to later error.)
	// The caller of CreateBorders should always delete the returned images (including before another call to CreateBorders).
//	virtual bool CreateBorders(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, bool bUseAlphaChannel,
//		CAbstractImage **ppTop, CAbstractImage **ppLeft, CAbstractImage **ppRight, CAbstractImage **ppBottom, bool *pbTopSpans,
//		CAbstractImage **ppOutTop, CAbstractImage **ppOutLeft, CAbstractImage **ppOutRight, CAbstractImage **ppOutBottom,
//		CAbstractImage **ppOutTopLeft, CAbstractImage **ppOutTopRight, CAbstractImage **ppOutBottomLeft, CAbstractImage **ppOutBottomRight) const { return false; }
	virtual bool CreateBorders(bool bUseAlphaChannel, CAbstractImage **ppBorderImage, RECT *pInnerRect, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper) const { return false; }

	// Not all image types support SetSize. Returns false if the call failed or was ignored.
	virtual bool SetSize(const SIZE *pSize);

	virtual void CopyRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, int iDitherOffset, const RGBQUAD *pRGBForTransparent, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable) = 0;

	virtual void GetRGBQPixel(RGBQUAD *pPixel, int x, int y);

	virtual void CopyAllRGBQPixels(RGBQUAD *pDest) = 0;

	virtual RGBQUAD *GetRGBQPixels(bool *pbDelete, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable) = 0;
	virtual void DeleteRGBQPixels(RGBQUAD *pPixels) = 0;

	virtual RGBQUAD *GetRGBQStretchPreserveAlpha(int iNewWidth, int iNewHeight);
	virtual void DeleteRGBQStretchedPixels(RGBQUAD *pPixels);

	virtual void PaintUnmodified(HDC hDC, const SIZE *pOffset, const SIZE *pWindowSize, const SIZE *pImageSize);

	bool FixAspectRatio(SIZE *pSize);

	static void StaticFillRGBQPixelRect(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect, const RGBQUAD *prgbFill);
	static void StaticCopyRGBQPixelRectTopDownToTopDown(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, const RGBQUAD *pSource, const SIZE *pSourceSize);

protected:

	// pPixels and pDest may point to the same buffer.
	static bool StaticBlurPreserveAlpha( RGBQUAD *pDest, const RGBQUAD *pSource, int w, int h, const RGBQUAD rgbqBackground);

	inline bool normaliseSelectRect(RECT *pRectOut, const RECT *pRectIn)
	{
		if (NULL != pRectIn)
		{
			if (pRectIn->left < pRectIn->right)
			{
				pRectOut->left  = pRectIn->left;
				pRectOut->right = pRectIn->right;
			}
			else
			{
				pRectOut->right = pRectIn->left;
				pRectOut->left  = pRectIn->right;
			}

			if (pRectIn->top < pRectIn->bottom)
			{
				pRectOut->top    = pRectIn->top;
				pRectOut->bottom = pRectIn->bottom;
			}
			else
			{
				pRectOut->bottom = pRectIn->top;
				pRectOut->top    = pRectIn->bottom;
			}
		}

		if (NULL == pRectIn || IsRectEmpty(pRectOut))
		{
			SetRect(pRectOut, 0, 0, GetWidth(), GetHeight());
			return false;
		}

		return true;
	}

private:

	typedef struct _sprocketdata
	{
		int oldWidth;
		int oldHeight;
		int newWidth;
		int newHeight;
		int sprocketStripWidth;
		int sprocketSquare;
		int sprocketGap;
		int sprocketOffsetLeft;
		int sprocketOffsetRight;
		int borderHeight;
		int imageAndOffsetWidth;
		int imageAndOffsetHeight;
		int offsetLeft;
		int offsetRight;
		int offsetTop;
		int offsetBottom;
		RGBQUAD rgbBackground;
		RGBQUAD rgbSprocket;
		RGBQUAD rgbSprocketGap;
		RGBQUAD rgbSprocketShadow1;
		int iSprocketY;
		int x;
		RGBQUAD *pNew;
		const RGBQUAD *pOld;
	} SprocketData;

	static inline void drawSprocketStripLeft(SprocketData &sd, bool bGap)
	{
		if (0 <= sd.iSprocketY)
		{
			for (sd.x = 0; sd.x < sd.sprocketOffsetLeft;  sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocket); }
			for (sd.x = 0; sd.x < sd.sprocketSquare-1;    sd.x++) { *(sd.pNew++) = *((0 == sd.x || 0 == sd.iSprocketY) ? &sd.rgbSprocketShadow1 : &sd.rgbSprocketGap); }
			for (sd.x = 0; sd.x < sd.sprocketOffsetRight; sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocket); }
		}
		else
		{
			for (sd.x = 0; sd.x < sd.sprocketStripWidth-1; sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocket); }
		}

		if (0 != sd.sprocketStripWidth)			                   { *(sd.pNew++) = *(bGap ? &sd.rgbSprocketShadow1 : &sd.rgbSprocket); }
	}

	static inline void drawSprocketStripRight(SprocketData &sd, bool bGap)
	{
		if (0 != sd.sprocketStripWidth)			                   { *(sd.pNew++) = *(bGap ? &sd.rgbSprocketShadow1 : &sd.rgbSprocket); }

		if (0 <= sd.iSprocketY)
		{
			for (sd.x = 0; sd.x < sd.sprocketOffsetLeft;  sd.x++)  { *(sd.pNew++) = *(&sd.rgbSprocket); }
			for (sd.x = 0; sd.x < sd.sprocketSquare-1;    sd.x++)  { *(sd.pNew++) = *((0 == sd.x || 0 == sd.iSprocketY) ? &sd.rgbSprocketShadow1 : &sd.rgbSprocketGap); }
			for (sd.x = 0; sd.x < sd.sprocketOffsetRight; sd.x++)  { *(sd.pNew++) = *(&sd.rgbSprocket); }
		}
		else
		{
			for (sd.x = 0; sd.x < sd.sprocketStripWidth-1; sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocket); }
		}
	}

	static inline void drawSprocketBorderGap(SprocketData &sd)
	{
		drawSprocketStripLeft(sd, true);
		for (sd.x = 0; sd.x < sd.imageAndOffsetWidth; sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocketShadow1); }
		drawSprocketStripRight(sd, true);

		if (sd.sprocketSquare == ++(sd.iSprocketY)) { sd.iSprocketY = -(sd.sprocketGap); }
	}

	static inline void drawSprocketBorderLine(SprocketData &sd)
	{
		drawSprocketStripLeft(sd, false);
		for (sd.x = 0; sd.x < sd.imageAndOffsetWidth; sd.x++) { *(sd.pNew++) = *(&sd.rgbSprocket); }
		drawSprocketStripRight(sd, false);

		if (sd.sprocketSquare == ++(sd.iSprocketY)) { sd.iSprocketY = -(sd.sprocketGap); }
	}

	static inline void drawSprocketBlankImageLine(SprocketData &sd)
	{
		drawSprocketStripLeft(sd, true);
		for (sd.x = 0; sd.x < sd.imageAndOffsetWidth; sd.x++) { *(sd.pNew++) = *(&sd.rgbBackground); }
		drawSprocketStripRight(sd, true);

		if (sd.sprocketSquare == ++(sd.iSprocketY)) { sd.iSprocketY = -(sd.sprocketGap); }
	}

	static inline void drawSprocketImageLine(SprocketData &sd)
	{
		drawSprocketStripLeft(sd, true);
		for (sd.x = 0; sd.x < sd.offsetLeft;  sd.x++) { *(sd.pNew++) = *(&sd.rgbBackground); }
		for (sd.x = 0; sd.x < sd.oldWidth;    sd.x++) { *(sd.pNew++) = *(sd.pOld++); }
		for (sd.x = 0; sd.x < sd.offsetRight; sd.x++) { *(sd.pNew++) = *(&sd.rgbBackground); }
		drawSprocketStripRight(sd, true);

		if (sd.sprocketSquare == ++(sd.iSprocketY)) { sd.iSprocketY = -(sd.sprocketGap); }
	}

protected:

	int			m_iScaleMode;

	HBITMAP		m_hbmCachedImage;
	SIZE		m_sizeCachedImage;
	bool		m_bCachedImageIsNotReallyScaled;
	int			m_iCachedImageDitherOffset;
	bool		m_bCachedImageGammaEnable;
	double		m_dCachedImageGammaValue;
};
