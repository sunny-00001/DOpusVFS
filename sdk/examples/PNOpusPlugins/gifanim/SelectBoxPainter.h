#pragma once

class CSelectBoxPainter
{
private:
	HDC m_hMemDC;
	HBITMAP m_hbm;
	HGDIOBJ m_hbmOld;
	RECT m_selectRectInClient;
public:
	CSelectBoxPainter(void);
	~CSelectBoxPainter(void);
	void Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &selectRectInClient);
	void PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const;
	void FreeCache();
	RECT GetRectInClient() const;
};
