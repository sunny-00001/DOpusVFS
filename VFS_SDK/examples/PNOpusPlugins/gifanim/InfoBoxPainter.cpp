#include "StdAfx.h"
#include "InfoBoxPainter.h"

CInfoBoxPainter::CInfoBoxPainter()
: m_hMemDC(NULL)
, m_hbm(NULL)
, m_hbmOld(NULL)
{
	SetRectEmpty(&m_infoRectInClient);
}

CInfoBoxPainter::~CInfoBoxPainter()
{
	FreeCache();
}

void CInfoBoxPainter::SetInformationString(const TCHAR *szInformation)
{
	if (szInformation == NULL)
	{
		szInformation = _T("");
	}

	if (0 != m_strInfo.compare(szInformation))
	{
		FreeCache();
		m_strInfo = szInformation;
	}
}

void CInfoBoxPainter::Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &clientRect, bool bWanted)
{
	const int infoBoxOffset = 20;

	if (!bWanted || m_strInfo.empty())
	{
		// If we're not going to paint anything then it's important that we set m_infoRectInClient to empty so other things know we won't paint.
		FreeCache();
	}
	else if (NULL == m_hMemDC)
	{
		// We'll need to paint and don't have a cached bitmap, so create the cached bitmap.

		m_hMemDC = CreateCompatibleDC(hDC);

		if (NULL != m_hMemDC)
		{
			RECT rectInfoText;
			SetRectEmpty(&rectInfoText);

			NONCLIENTMETRICS ncm;
			ZeroMemory(&ncm, sizeof(ncm));
			ncm.cbSize = sizeof(ncm);

			HFONT hfontNew = NULL;
			HGDIOBJ hfontOld = NULL;

			if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)
			||	NULL == (hfontNew = CreateFontIndirect(&ncm.lfMessageFont)))
			{
				hfontOld = SelectObject(m_hMemDC, GetStockObject(DEFAULT_GUI_FONT));
			}
			else
			{
				hfontOld = SelectObject(m_hMemDC, hfontNew);
			}

			DrawText(m_hMemDC, m_strInfo.c_str(), -1, &rectInfoText, DT_TOP|DT_LEFT|DT_NOCLIP|DT_NOPREFIX|DT_CALCRECT);

			rectInfoText.left += 4;
			rectInfoText.right += 4;
			rectInfoText.top += 2;
			rectInfoText.bottom += 2;

			RECT rectInfoFull;
			rectInfoFull.left = 0;
			rectInfoFull.right = rectInfoText.right + 4;
			rectInfoFull.top = 0;
			rectInfoFull.bottom = rectInfoText.bottom + 3;

			m_infoRectInClient.right  = clientRect.right  - infoBoxOffset;
			m_infoRectInClient.bottom = clientRect.bottom - infoBoxOffset;
			m_infoRectInClient.left   = m_infoRectInClient.right  - rectInfoFull.right;
			m_infoRectInClient.top    = m_infoRectInClient.bottom - rectInfoFull.bottom;

			m_hbm = CreateCompatibleBitmap(hDC, rectInfoFull.right, rectInfoFull.bottom);

			if (m_hbm != NULL)
			{
				m_hbmOld = SelectObject(m_hMemDC, m_hbm);

				FillRect(m_hMemDC, &rectInfoFull, GetSysColorBrush(COLOR_INFOBK));
				FrameRect(m_hMemDC, &rectInfoFull, GetSysColorBrush(COLOR_WINDOWFRAME));
				
				SetTextColor(m_hMemDC, COLOR_INFOTEXT);
				SetBkMode(m_hMemDC, TRANSPARENT);
				DrawText(m_hMemDC, m_strInfo.c_str(), -1, &rectInfoText, DT_TOP|DT_LEFT|DT_NOCLIP|DT_NOPREFIX);
			}

			if (NULL != hfontOld)
			{
				SelectObject(m_hMemDC, hfontOld);
				hfontOld = NULL;
			}

			if (NULL != hfontNew)
			{
				DeleteObject(hfontNew);
				hfontNew = NULL;
			}
		}
	}
	else
	{
		// We've already cached the required bitmap in a previous call. We just need to update m_infoRectInClient for the new clientRect.

		int w = m_infoRectInClient.right  - m_infoRectInClient.left;
		int h = m_infoRectInClient.bottom - m_infoRectInClient.top;

		m_infoRectInClient.right  = clientRect.right  - infoBoxOffset;
		m_infoRectInClient.bottom = clientRect.bottom - infoBoxOffset;
		m_infoRectInClient.left   = m_infoRectInClient.right  - w;
		m_infoRectInClient.top    = m_infoRectInClient.bottom - h;
	}
}

void CInfoBoxPainter::FreeCache()
{
	if (NULL != m_hMemDC)
	{
		if (NULL != m_hbmOld)
		{
			SelectObject(m_hMemDC, m_hbmOld);
			m_hbmOld = NULL;
		}

		if (NULL != m_hbm)
		{
			DeleteObject(m_hbm);
			m_hbm = NULL;
		}

		DeleteDC(m_hMemDC);
		m_hMemDC = NULL;
	}

	SetRectEmpty(&m_infoRectInClient);
}

void CInfoBoxPainter::PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const
{
	if (NULL != m_hMemDC)
	{
		BLENDFUNCTION blendFunc;
		blendFunc.BlendOp = AC_SRC_OVER;
		blendFunc.BlendFlags = 0;
		blendFunc.SourceConstantAlpha = 200;
		blendFunc.AlphaFormat = 0;

		int infoWidth  = m_infoRectInClient.right  - m_infoRectInClient.left;
		int infoHeight = m_infoRectInClient.bottom - m_infoRectInClient.top;

		if (!AlphaBlend(hDC, m_infoRectInClient.left + offsetX, m_infoRectInClient.top + offsetY, infoWidth, infoHeight, m_hMemDC, 0, 0, infoWidth, infoHeight, blendFunc))
		{
			BitBlt(hDC, m_infoRectInClient.left + offsetX, m_infoRectInClient.top + offsetY, infoWidth, infoHeight, m_hMemDC, 0, 0, SRCCOPY);
		}
	}
}

RECT CInfoBoxPainter::GetRectInClient() const
{
	return m_infoRectInClient;
}
