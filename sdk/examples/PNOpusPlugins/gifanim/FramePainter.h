#pragma once

class CInfoBoxPainter;

class CFramePainter
{
private:
	HDC m_hMemDC;
	RGBQUAD *m_pPixelData;
	HBITMAP m_hbm;
	HGDIOBJ m_hbmOld;
	SIZE m_backgroundSize;
	RECT m_imageRectInBackground;
public:
	CFramePainter();
	~CFramePainter(void);
	void Cache(HDC hDC, const PAINTSTRUCT &ps, HWND hWnd, const SIZE &backgroundSize, const RECT &imageRectInBackground, bool bFrame, DOpusPluginHelperUtil *pPluginHelper, const CInfoBoxPainter *pInfoBoxPainter);
	void PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const;
//	void CopyRGBQPixelRectFromCache(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect);
	void FreeCache();
};
