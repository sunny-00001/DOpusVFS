#pragma once

class CInfoBoxPainter
{
private:
	HDC m_hMemDC;
	HBITMAP m_hbm;
	HGDIOBJ m_hbmOld;
	RECT m_infoRectInClient;
	std::basic_string< TCHAR > m_strInfo;
public:
	CInfoBoxPainter();
	~CInfoBoxPainter();
	void SetInformationString(const TCHAR *szInformation);
	void Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &clientRect, bool bWanted);
	void PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const;
	void FreeCache();
	RECT GetRectInClient() const;
};
