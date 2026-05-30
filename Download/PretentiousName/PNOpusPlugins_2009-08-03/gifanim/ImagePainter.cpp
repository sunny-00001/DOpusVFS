#include "StdAfx.h"
#include "LeoHelpers.h"
#include "ImagePainter.h"
#include "SelectBoxPainter.h"
#include "InfoBoxPainter.h"
#include "AbstractImage.h"

CImagePainter::CImagePainter(void)
: m_hMemDC(NULL)
, m_hbm(NULL)
, m_hbmOld(NULL)
, m_pImage(NULL)
, m_blitWidth(0)
, m_blitHeight(0)
, m_blitOffsetX(0)
, m_blitOffsetY(0)
, m_displayedImageWidth(0)
, m_displayedImageHeight(0)
, m_displayedImageOffsetX(0)
, m_displayedImageOffsetY(0)
, m_iDitherOffset(0)
, m_bGammaEnable(false)
, m_dGammaValue(0.0)
, m_GammaTable(NULL)
{
	SetRectEmpty(&m_clientRect);
	SetRectEmpty(&m_imageRectInClient);
}

CImagePainter::~CImagePainter(void)
{
	FreeCache();
}

void CImagePainter::Cache(HDC hDC, const PAINTSTRUCT &ps, const RECT &clientRect, const RECT &imageRectInClient, CAbstractImage *pImage, const CSelectBoxPainter *pSelectBoxPainter, const CInfoBoxPainter *pInfoBoxPainter, int iDitherOffset, bool bGammaEnable, const double *pdGammaValue, const BYTE *GammaTable)
{
	FreeCache();

	RECT imageRectInClientClipped;

	if (IntersectRect(&imageRectInClientClipped, &clientRect, &imageRectInClient))
	{
		m_pImage = pImage;

		m_iDitherOffset = iDitherOffset;

		if (bGammaEnable)
		{
			m_bGammaEnable = true;
			m_dGammaValue = *pdGammaValue;
			m_GammaTable = GammaTable;
		}

		m_clientRect = clientRect;
		m_imageRectInClient = imageRectInClient;

		m_blitWidth  = imageRectInClientClipped.right  - imageRectInClientClipped.left;
		m_blitHeight = imageRectInClientClipped.bottom - imageRectInClientClipped.top;

		m_blitOffsetX = clientRect.left - imageRectInClientClipped.left;
		m_blitOffsetY = clientRect.top  - imageRectInClientClipped.top;

		m_displayedImageWidth  = imageRectInClient.right  - imageRectInClient.left;
		m_displayedImageHeight = imageRectInClient.bottom - imageRectInClient.top;

		m_displayedImageOffsetX = clientRect.left - imageRectInClient.left;
		m_displayedImageOffsetY = clientRect.top  - imageRectInClient.top;

		RECT selectRectInClient = pSelectBoxPainter->GetRectInClient();
		RECT infoRectInClient = pInfoBoxPainter->GetRectInClient();

		RECT selectRectInImageInClientClipped;
		RECT infoRectInImageInClientClipped;

		BOOL bSelect = IntersectRect(&selectRectInImageInClientClipped, &imageRectInClientClipped, &selectRectInClient);
		BOOL bInfo   = IntersectRect(&infoRectInImageInClientClipped,   &imageRectInClientClipped, &infoRectInClient);

		if (bSelect || bInfo)
		{
			m_hMemDC = CreateCompatibleDC(hDC);

			if (NULL != m_hMemDC)
			{
				m_hbm = CreateCompatibleBitmap(hDC, m_blitWidth, m_blitHeight);

				if (m_hbm != NULL)
				{
					m_hbmOld = SelectObject(m_hMemDC, m_hbm);

					SIZE offset;
					offset.cx = m_displayedImageOffsetX - m_blitOffsetX;
					offset.cy = m_displayedImageOffsetY - m_blitOffsetY;

					SIZE virtualWindowSize;
					virtualWindowSize.cx = m_blitWidth;
					virtualWindowSize.cy = m_blitHeight;

					SIZE displayedImageSize;
					displayedImageSize.cx = m_displayedImageWidth;
					displayedImageSize.cy = m_displayedImageHeight;

					m_pImage->Paint(m_hMemDC, &offset, &virtualWindowSize, &displayedImageSize, m_iDitherOffset, m_bGammaEnable, &m_dGammaValue, m_GammaTable);

					if (bSelect)
					{
						pSelectBoxPainter->PaintFromCache(m_hMemDC, ps, m_blitOffsetX, m_blitOffsetY);
					}

					if (bInfo)
					{
						pInfoBoxPainter->PaintFromCache(m_hMemDC, ps, m_blitOffsetX, m_blitOffsetY);
					}
				}
			}
		}
	}
}

void CImagePainter::FreeCache()
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

	SetRectEmpty(&m_clientRect);
	SetRectEmpty(&m_imageRectInClient);

	m_blitWidth   = 0;
	m_blitHeight  = 0;
	m_blitOffsetX = 0;
	m_blitOffsetY = 0;
	m_displayedImageWidth = 0;
	m_displayedImageHeight = 0;
	m_displayedImageOffsetX = 0;
	m_displayedImageOffsetY = 0;

	m_iDitherOffset = 0;
	m_bGammaEnable = false;
	m_dGammaValue = 0.0;
	m_GammaTable = NULL;

	m_pImage = NULL;
}

void CImagePainter::PaintFromCache(HDC hDC, const PAINTSTRUCT &ps) const
{
	if (NULL != m_hMemDC)
	{
		BitBlt(hDC, -m_blitOffsetX, -m_blitOffsetY, m_blitWidth, m_blitHeight, m_hMemDC, 0, 0, SRCCOPY);
	}
	else if (NULL != m_pImage)
	{
		SIZE offset;
		offset.cx = m_displayedImageOffsetX;
		offset.cy = m_displayedImageOffsetY;

		SIZE clientSize;
		clientSize.cx = m_clientRect.right  - m_clientRect.left;
		clientSize.cy = m_clientRect.bottom - m_clientRect.top;

		SIZE displayedImageSize;
		displayedImageSize.cx = m_displayedImageWidth;
		displayedImageSize.cy = m_displayedImageHeight;

		m_pImage->Paint(hDC, &offset, &clientSize, &displayedImageSize, m_iDitherOffset, m_bGammaEnable, &m_dGammaValue, m_GammaTable);
	}
}
