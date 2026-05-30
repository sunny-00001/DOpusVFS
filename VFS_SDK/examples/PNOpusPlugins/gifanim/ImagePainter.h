#pragma once

class CAbstractImage;
class CSelectBoxPainter;
class CInfoBoxPainter;

class CImagePainter
{
private:
	HDC m_hMemDC;
	HBITMAP m_hbm;
	HGDIOBJ m_hbmOld;
	RECT m_clientRect;
	RECT m_imageRectInClient;
	CAbstractImage *m_pImage;
	int m_blitWidth;
	int m_blitHeight;
	int m_blitOffsetX;
	int m_blitOffsetY;
	int m_displayedImageWidth;
	int m_displayedImageHeight;
	int m_displayedImageOffsetX;
	int m_displayedImageOffsetY;
	int m_iDitherOffset;
	bool m_bGammaEnable;
	double m_dGammaValue;
	const BYTE *m_GammaTable;
public:
	CImagePainter(void);
	~CImagePainter(void);
	void Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &clientRect, const RECT &imageRectInClient, CAbstractImage *pImage, const CSelectBoxPainter *pSelectBoxPainter, const CInfoBoxPainter *pInfoBoxPainter, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable);
	void PaintFromCache(HDC hDC, const PAINTSTRUCT &ps) const; // The pImage, pInfoPainter and GammaTable given to Cache must still be valid when Paint is called.
	void FreeCache();
};
