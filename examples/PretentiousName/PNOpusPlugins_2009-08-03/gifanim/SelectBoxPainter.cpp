#include "StdAfx.h"
#include "SelectBoxPainter.h"

CSelectBoxPainter::CSelectBoxPainter(void)
: m_hMemDC(NULL)
, m_hbm(NULL)
, m_hbmOld(NULL)
{
	SetRectEmpty(&m_selectRectInClient);
}

CSelectBoxPainter::~CSelectBoxPainter(void)
{
	FreeCache();
}

void CSelectBoxPainter::Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &selectRectInClient)
{
	bool bIsRectEmpty = IsRectEmpty(&selectRectInClient) ? true : false;

	COLORREF crHighlight = GetSysColor(COLOR_HIGHLIGHT);

	// Cached bitmap is always the same 1x1 image, regardless of destination size.

	if (NULL == m_hMemDC
	||	bIsRectEmpty)
	{
		FreeCache();

		if (!bIsRectEmpty)
		{
			m_hMemDC = CreateCompatibleDC(hDC);

			if (NULL != m_hMemDC)
			{
				m_hbm = CreateCompatibleBitmap(hDC, 1, 1);

				if (m_hbm != NULL)
				{
					m_hbmOld = SelectObject(m_hMemDC, m_hbm);
				}
			}
		}
	}

	m_selectRectInClient = selectRectInClient;
}

void CSelectBoxPainter::FreeCache()
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

	SetRectEmpty(&m_selectRectInClient);
}

void CSelectBoxPainter::PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const
{
	if (NULL != m_hMemDC)
	{
		BLENDFUNCTION blendFunc;
		blendFunc.BlendOp = AC_SRC_OVER;
		blendFunc.BlendFlags = 0;
		blendFunc.SourceConstantAlpha = 127;
		blendFunc.AlphaFormat = 0;

		RECT frameRect = m_selectRectInClient;

		if (!IsRectEmpty(&frameRect))
		{
			OffsetRect(&frameRect, offsetX, offsetY);

			RECT paintRect = ps.rcPaint;
			OffsetRect(&paintRect, offsetX, offsetY);

			RECT fillRect = frameRect;
			if (fillRect.left < fillRect.right) { ++(fillRect.left);   }
			if (fillRect.left < fillRect.right) { --(fillRect.right);  }
			if (fillRect.top < fillRect.bottom) { ++(fillRect.top);    }
			if (fillRect.top < fillRect.bottom) { --(fillRect.bottom); }

			RECT fillRectClipped;
			IntersectRect(&fillRectClipped, &fillRect, &paintRect);

			if (!IsRectEmpty(&fillRectClipped))
			{
				SetPixel(m_hMemDC, 0, 0, GetSysColor(COLOR_HIGHLIGHT));

				AlphaBlend(hDC, fillRectClipped.left, fillRectClipped.top, fillRectClipped.right-fillRectClipped.left, fillRectClipped.bottom-fillRectClipped.top, m_hMemDC, 0, 0, 1, 1, blendFunc);
			}

			FrameRect(hDC, &frameRect, GetSysColorBrush(COLOR_HIGHLIGHT));
		}
	}
}

RECT CSelectBoxPainter::GetRectInClient() const
{
	return m_selectRectInClient;
}
