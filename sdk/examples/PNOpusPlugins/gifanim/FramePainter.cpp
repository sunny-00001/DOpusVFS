#include "StdAfx.h"
#include "LeoHelpers.h"
#include "FramePainter.h"
#include "InfoBoxPainter.h"

CFramePainter::CFramePainter()
: m_hMemDC(NULL)
, m_hbm(NULL)
, m_pPixelData(NULL)
, m_hbmOld(NULL)
{
	m_backgroundSize.cx = 0;
	m_backgroundSize.cy = 0;
	SetRectEmpty(&m_imageRectInBackground);
}

CFramePainter::~CFramePainter(void)
{
	FreeCache();
}

void CFramePainter::Cache(HDC hDC, const PAINTSTRUCT &ps, HWND hWnd, const SIZE &backgroundSize, const RECT &imageRectInBackground, bool bFrame, DOpusPluginHelperUtil *pPluginHelper, const CInfoBoxPainter *pInfoBoxPainter)
{
	FreeCache();

	NMHDR notHdr;
	notHdr.hwndFrom = hWnd;
	notHdr.idFrom = 0;
	notHdr.code = DVPN_GETBGCOL;

	COLORREF bgCol = static_cast<COLORREF>(SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));

	m_hMemDC = CreateCompatibleDC(hDC);

	if (NULL != m_hMemDC)
	{
		BITMAPINFO bitmapInfo;
		LeoHelpers::InitRGBQBitmapHeader(&bitmapInfo, backgroundSize.cx, backgroundSize.cy, true);

		m_hbm = CreateDIBSection(hDC, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_pPixelData), NULL, 0);

		if (m_hbm != NULL)
		{
			m_hbmOld = SelectObject(m_hMemDC, m_hbm);

			HBRUSH hbrushBackground = CreateSolidBrush(bgCol);

			if (NULL != hbrushBackground)
			{
				RECT dibRect;
				::SetRect(&dibRect, 0, 0, backgroundSize.cx, backgroundSize.cy);
				FillRect(m_hMemDC, &dibRect, hbrushBackground);

				if (bFrame)
				{
					GdiFlush();

					pPluginHelper->DrawPictureFrameInDIB(&bitmapInfo, m_pPixelData, &imageRectInBackground, 0, 0);
				}

				if (NULL != m_hMemDC && NULL != m_hbm && NULL != pInfoBoxPainter)
				{
					pInfoBoxPainter->PaintFromCache(m_hMemDC, ps, 0, 0);
				}

				DeleteObject(hbrushBackground);
			}
		}

		m_backgroundSize = backgroundSize;
		m_imageRectInBackground = imageRectInBackground;
	}
}

void CFramePainter::FreeCache()
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

	m_pPixelData = NULL;

	m_backgroundSize.cx = 0;
	m_backgroundSize.cy = 0;
	SetRectEmpty(&m_imageRectInBackground);
}

void CFramePainter::PaintFromCache(HDC hDC, const PAINTSTRUCT &ps, int offsetX, int offsetY) const
{
	if (NULL != m_hMemDC && NULL != m_hbm)
	{
		bool bOriginalRegionIsNull = true;

		HRGN hrgnClipOld = ::CreateRectRgn(0,0,1,1);

		if (1 == ::GetClipRgn(hDC, hrgnClipOld))
		{
			bOriginalRegionIsNull = false;
		}

		ExcludeClipRect(hDC,
			offsetX + m_imageRectInBackground.left, 
			offsetY + m_imageRectInBackground.top,
			offsetX + m_imageRectInBackground.right,
			offsetY + m_imageRectInBackground.bottom);

		BitBlt(hDC, offsetX, offsetY, m_backgroundSize.cx, m_backgroundSize.cy, m_hMemDC, 0, 0, SRCCOPY);

		SelectClipRgn(hDC, bOriginalRegionIsNull ? NULL : hrgnClipOld);

		DeleteObject(hrgnClipOld);
		hrgnClipOld = NULL;
	}
}
/*
void CFramePainter::CopyRGBQPixelRectFromCache(RGBQUAD *pDest, const SIZE *pDestSize, const SIZE *pDestOffset, bool bTopDown, const RECT *pWantedRect)
{
	if (NULL != m_hMemDC && NULL != m_hbm)
	{
		const int w = m_backgroundSize.cx;
		const int h = m_backgroundSize.cy;

		RECT rt1;
		SIZE st1;
		SIZE st2;

		if (pWantedRect == NULL)
		{
			pWantedRect = &rt1;
			rt1.left   = 0;
			rt1.top    = 0;
			rt1.right  = w;
			rt1.bottom = h;
		}

		if (pDestSize == NULL)
		{
			pDestSize = &st1;
			st1.cx = pWantedRect->right - pWantedRect->left;
			st1.cy = pWantedRect->bottom - pWantedRect->top;
		}

		if (pDestOffset == NULL)
		{
			pDestOffset = &st2;
			st2.cx = 0;
			st2.cy = 0;
		}

		const int iDestLineOffset = pDestSize->cx - (pWantedRect->right - pWantedRect->left);

		if (bTopDown)
		{
			pDest += (pDestOffset->cx + pDestSize->cx * pDestOffset->cy);
		}
		else
		{
			pDest += (pDestOffset->cx + pDestSize->cx * (pDestSize->cy - (pDestOffset->cy + (pWantedRect->bottom - pWantedRect->top))));
		}

		if (bTopDown)
		{
			const int yStart = pWantedRect->top;
			const int yEnd   = pWantedRect->bottom;

			const int xStart = pWantedRect->left;
			const int xEnd   = pWantedRect->right;

			const int iSourceLineOffset = w - (xEnd - xStart);

			const RGBQUAD *pSource = m_pPixelData + xStart + yStart * w;

			for (int y = yStart; y < yEnd; ++y)
			{
				memcpy(pDest, pSource, sizeof(RGBQUAD) * (xEnd - xStart));

				pDest   += ((xEnd - xStart) + iDestLineOffset);
				pSource += ((xEnd - xStart) + iSourceLineOffset);
			}
		}
		else
		{
			const int yStart = pWantedRect->bottom;
			const int yEnd   = pWantedRect->top;

			const int xStart = pWantedRect->left;
			const int xEnd   = pWantedRect->right;

			const int iSourceLineOffset = -(w + (xEnd - xStart));

			const RGBQUAD *pSource = m_pPixelData + xStart + (yStart - 1) * w;

			for (int y = yStart - 1; y >= yEnd; --y)
			{
				memcpy(pDest, pSource, sizeof(RGBQUAD) * (xEnd - xStart));

				pDest   += ((xEnd - xStart) + iDestLineOffset);
				pSource += ((xEnd - xStart) + iSourceLineOffset);
			}
		}
	}
}
*/
