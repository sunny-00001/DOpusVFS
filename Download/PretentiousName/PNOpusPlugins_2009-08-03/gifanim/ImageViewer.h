#pragma once
#include "AbstractImage.h"

class CSelectBoxPainter;
class CInfoBoxPainter;

class CImageViewer
{
public:
	typedef struct _IVC_CreateParams
	{
		DWORD		dwFlags;
		CGifConfig *pConfig;
	} IVC_CreateParams;

	enum { IVC_PLAYBACKWARDS = 0,
		   IVC_PAUSE,
		   IVC_PLAY,
		   IVC_PREVFRAME,
		   IVC_NEXTFRAME,
		   IVC_FLATTENHORIZ,
		   IVC_FLATTENVERT,
		   IVC_FLATTENFIT,
		   IVC_DITHERIMAGEALL,
		   IVC_DITHERIMAGEONE,
		   IVC_DITHERIMAGETWO	};

	enum { WM_IMAGEVIEWER_REDRAWANIMATIONFRAME = WM_APP,
		   WM_IMAGEVIEWER_SHOWTOOLBAR,
		   WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON,
		   WM_IMAGEVIEWER_SETFRAME,
		   WM_IMAGEVIEWER_PRECACHENEXTFRAME };

	typedef enum
	{
		IVM_NONE = 0,
        IVM_LEFT,
		IVM_MIDDLE,
		IVM_RIGHT
	} IVMouse;

protected:
	HWND m_hWnd;
	CGifConfig *m_pConfig;
	LeoHelpers::OpusStringLoader m_sl;

	HANDLE				m_hThread;
	HANDLE				m_hThreadStopEvent;
	HANDLE				m_hThreadGetDelayEvent;
	HANDLE				m_hFramePaintedEvent;
	CRITICAL_SECTION	m_cs;

	TCHAR *m_szName; // Name of image we're showing. Sometimes, but not always, a file path.

	CSelectBoxPainter *m_pSelectBoxPainter;
	CInfoBoxPainter *m_pInfoBoxPainter;

	HANDLE m_hAbortEvent;

	CAbstractImageList *m_pFramesFrames;
	CAbstractImageList m_framesFlat;
	CAbstractImageList m_framesTiled;
	CAbstractImageList *m_pRestoreFrames;
	CAbstractImageList *m_pCurrentFrames;
	CAbstractImage *m_pLastDrawnImage;

	bool m_bFlattenHorizontal;
	bool m_bFlattenFitToWindow;
	int  m_iFlattenFitColumns;

	bool m_bBackwards;

	bool m_bDisplayInformation;
	bool m_bIsAlphaHidden;

	bool m_bIsPreview;
	bool m_bFullScreen;
	bool m_bFrameImage;
	bool m_bEnableScrollbars;
	bool m_bShowAnimationControls;

	bool m_bUpdateScrollbarsAndImageOffsetNoRecurse;
	bool m_bGenerateFlattenedImageNoRecurse;

	SIZE	m_offset; // Offset: top-left image to top-left window.
	SIZE	m_displayedImageSize;

	SIZE	m_originalImageSize;
	int		m_iOriginalBitDepth;

	int		m_iDitherOffset;

	IVMouse	m_mouseDragging;
	SIZE	m_mouseDragStart;
	bool	m_bCanDrag;

	IVMouse	m_mouseSelecting;
	SIZE	m_mouseSelectStart; // client coordinates
	RECT	m_mouseSelectRect; // image coordinates

	int		m_currentZoomFactor;

	int		m_iMouseWheelDelta;

	int		m_initialRotation;
	int		m_currentRotation;

	bool	m_bGammaEnable;
	double	m_dGammaValue;
	BYTE	m_GammaTable[256];

	DVPCONTEXTMENUITEM m_contextMenuItems[4];

public:
	CImageViewer(HWND hWnd, DWORD dwFlags, CGifConfig *pConfig);
	~CImageViewer(void);

	static ATOM RegisterWindowClass(HMODULE hDllModule);
	static BOOL UnregisterWindowClass(HMODULE hDllModule);

	static HWND CreateViewerWindow(HWND hWndParent, LPRECT lpRc, DWORD dwFlags, CGifConfig *pConfig);

protected:
	void reset();

	void startThread();
	void stopThread();
	static unsigned __stdcall animationThreadStatic(void *pThis);
	void animationThread();

	inline static const TCHAR *getWindowClassName() { return(_T("dopusviewerplugin.liv.viewer")); }
	static LRESULT WINAPI wndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

	void onCreate(HWND hWnd);
	LRESULT onDVPLoad(HWND hWnd, LPTSTR szFilename);
	LRESULT onDVPLoadStream(HWND hWnd, LPSTREAM pStream, LPTSTR szFilename);
	LRESULT load(HWND hWnd, NGifDecoder::CGifFile *pGifFile, const TCHAR *szName, CAbstractImageList *pReplacementImageList);
	void onDVPClear(HWND hWnd);
	void onSystemSettingChange(HWND hWnd);
	void onDVPReinitialize(HWND hWnd);
	void precacheFrames(HWND hWnd, bool bOnlyIfPlaying);
	LRESULT onPaint(HWND hWnd);
	void onRedrawFrame(HWND hWnd);
	void onSetFrame(HWND hWnd, int iFrame); // iFrame starts from one not zero.

	void keepOffsetInRange(const SIZE *pImageSize, const RECT *pWindowRect);

	void onVScroll(HWND hWnd, WORD wRequest);
	void onHScroll(HWND hWnd, WORD wRequest);
	void getSelectOrScrollFromOpus(HWND hWnd, IVMouse mouseButton, bool *pbScroll, bool *pbSelect);
	void onButtonDown(HWND hWnd, int x, int y, IVMouse mouseButton);
	void onButtonUp(HWND hWnd, int x, int y, IVMouse mouseButton);
	void clearSelectionAndEndCapture(HWND hWnd);
	void onCaptureChanged(HWND hWnd);
	void onButtonDownMouseMove(HWND hWnd, int x, int y);
	void onDVPSelectAll(HWND hWnd);
	bool onDVPTestSelection(HWND hWnd);
	void onDVPCopySelection(HWND hWnd);
	HBITMAP onDVPGetBitmap(HWND hWnd);
	void onDVPSetDesktopWallpaper(HWND hWnd, const TCHAR *szMode);
	void onSetCursor(HWND hWnd);
	COLORREF onDVPGetAutoBGCol(HWND hWnd);
	LRESULT onDVPIsAlphaHidden(HWND hWnd, BOOL *pbIsHidden);
	void onDVPHideAlpha(HWND hWnd, bool bHide);
	LRESULT onDVPCropSelection(HWND hWnd, bool bTest);
	LRESULT onDVPRestore(HWND hWnd, bool bTest);
	void onDVPRedraw(HWND hWnd, const COLORREF *pcrNewBackground);
	void onDVPGetImageInfo(HWND hWnd, VIEWERPLUGINFILEINFO *pInfo);
	void onDVPSetRotation(HWND hWnd, int iRotation);
	void onDVPRotate(HWND hWnd, int iRotation);
	LRESULT onDVPMouseWheel(HWND hWnd, WPARAM wParam);
	void getGammaFromOpus(HWND hWnd);
	void onDVPGammaChange(HWND hWnd);
	void onDVPZoom(HWND hWnd);

	void onButtonPlay(HWND hWnd, bool bForwards);
	void onButtonPause(HWND hWnd);
	void onButtonFrame(HWND hWnd, bool bForwards);
	void onButtonFlatten(HWND hWnd, bool bFitToWindow, bool bHorizontal);

	DVPCONTEXTMENUITEM *onAddContextMenu(DWORD *pdwNumItems);
	void onButtonDitherImage(HWND hWnd, int iCmd);

	void onDVPPrint(HWND hWnd);

	void generateFlattenedImage(HWND hWnd, bool bOnlyIfColsChanged, bool bResizeWindow);
	void generateTiledImage(HWND hWnd);

	// Size should be for window with scrollbars turned off.
	int getFlattenFitNumberOfColumns(int iNumFrames, const SIZE *pImageSize, const SIZE *pWindowSize, int iZoomFactor, bool bFrameImage, bool bEnableScrollbars, bool *pbResultantScrollbarH, bool *pbResultantScrollbarV);

	void updateScrollbarsAndImageOffset(HWND hWnd, bool bResetOffset, const SIZE *pOffsetAdjust, bool bZooming);

	inline bool getClientAndScrollbarsRect(HWND hWnd, RECT *pWindowRect, bool bHScroll, bool bVScroll)
	{
		if (!GetClientRect(hWnd, pWindowRect) || ::IsRectEmpty(pWindowRect))
		{
			//logLine(_T("getClientAndScrollbarsRect failed\n"));
			return(false);
		}

		int scrollx = GetSystemMetrics(SM_CXVSCROLL);
		int scrolly = GetSystemMetrics(SM_CYHSCROLL);

		if (WS_HSCROLL & GetWindowLong(hWnd,GWL_STYLE))
		{
			if (!bHScroll)
			{
				pWindowRect->bottom += scrolly;
			}
		}
		else if (bHScroll)
		{
			pWindowRect->bottom -= scrolly;
		}

		if (WS_VSCROLL & GetWindowLong(hWnd,GWL_STYLE))
		{
			if (!bVScroll)
			{
				pWindowRect->right += scrollx;
			}
		}
		else if (bVScroll)
		{
			pWindowRect->right -= scrollx;
		}

		return(true);
	}

	// Size is for window with scrollbars turned off.
	inline bool getMaxClientSize(HWND hWnd, SIZE *pSizeMaxClient)
	{
		DVPNMCALCULATERECT notHdr = {0};
		notHdr.hdr.hwndFrom = hWnd;
		notHdr.hdr.idFrom   = 0;
		notHdr.hdr.code     = DVPN_CALCULATERECT;
		notHdr.operation    = VPCALCRECT_MAX_VIEWER_AUTOSIZE;

		if (SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)))
		{
			pSizeMaxClient->cx = notHdr.rc.right - notHdr.rc.left;
			pSizeMaxClient->cy = notHdr.rc.bottom - notHdr.rc.top;

			if (0 < pSizeMaxClient->cx
			&&	0 < pSizeMaxClient->cy)
			{
				return true;
			}
		}

		return false;
	}

	inline bool getActualImageSize(SIZE *pSize, bool bAsIfNotRotated)
	{
		if (m_pCurrentFrames->IsEmpty())
		{
			return(false);
		}

		pSize->cx = m_pCurrentFrames->GetFirstFrame()->GetWidth();
		pSize->cy = m_pCurrentFrames->GetFirstFrame()->GetHeight();

		if (bAsIfNotRotated && (90 == m_currentRotation || 270 == m_currentRotation))
		{
			LONG tempX = pSize->cx;
			pSize->cx = pSize->cy;
			pSize->cy = tempX;
		}

		return(pSize->cx > 0 && pSize->cy > 0);
	}

	inline bool getDisplayedImageSize(SIZE *pSize)
	{
		if (m_pCurrentFrames->IsEmpty() || 0 == m_displayedImageSize.cx || 0 == m_displayedImageSize.cy)
		{
			return(false);
		}

		pSize->cx = m_displayedImageSize.cx;
		pSize->cy = m_displayedImageSize.cy;

		return(true);
	}

	inline bool clientToImage(RECT *pRect)
	{
		SIZE sizeActualImage;
		SIZE sizeDisplayedImage;

		if (getActualImageSize(&sizeActualImage, false)
		&&	getDisplayedImageSize(&sizeDisplayedImage))
		{
			pRect->left   = LeoHelpers::MulDivRoundDown(sizeActualImage.cx, pRect->left   + m_offset.cx, sizeDisplayedImage.cx);
			pRect->right  = LeoHelpers::MulDivRoundDown(sizeActualImage.cx, pRect->right  + m_offset.cx, sizeDisplayedImage.cx);
			pRect->top    = LeoHelpers::MulDivRoundDown(sizeActualImage.cy, pRect->top    + m_offset.cy, sizeDisplayedImage.cy);
			pRect->bottom = LeoHelpers::MulDivRoundDown(sizeActualImage.cy, pRect->bottom + m_offset.cy, sizeDisplayedImage.cy);

			if (0 > pRect->left  ) { pRect->left   = 0; }
			if (0 > pRect->right ) { pRect->right  = 0; }
			if (0 > pRect->top   ) { pRect->top    = 0; }
			if (0 > pRect->bottom) { pRect->bottom = 0; }

			if (sizeActualImage.cx < pRect->left  ) { pRect->left   = sizeActualImage.cx; }
			if (sizeActualImage.cx < pRect->right ) { pRect->right  = sizeActualImage.cx; }
			if (sizeActualImage.cy < pRect->top   ) { pRect->top    = sizeActualImage.cy; }
			if (sizeActualImage.cy < pRect->bottom) { pRect->bottom = sizeActualImage.cy; }

			return(true);
		}
		else
		{
			return(false);
		}
	}

	inline bool imageToClient(RECT *pRect)
	{
		SIZE sizeActualImage;
		SIZE sizeDisplayedImage;

		if (getActualImageSize(&sizeActualImage, false)
		&&	getDisplayedImageSize(&sizeDisplayedImage))
		{
			pRect->left   = LeoHelpers::MulDivRoundDown(sizeDisplayedImage.cx, pRect->left,   sizeActualImage.cx) - m_offset.cx;
			pRect->right  = LeoHelpers::MulDivRoundDown(sizeDisplayedImage.cx, pRect->right,  sizeActualImage.cx) - m_offset.cx;
			pRect->top    = LeoHelpers::MulDivRoundDown(sizeDisplayedImage.cy, pRect->top,    sizeActualImage.cy) - m_offset.cy;
			pRect->bottom = LeoHelpers::MulDivRoundDown(sizeDisplayedImage.cy, pRect->bottom, sizeActualImage.cy) - m_offset.cy;

			return(true);
		}
		else
		{
			return(false);
		}
	}

	inline void invalidateImage(HWND hWnd)
	{
		RECT imageRect;
		if (getVisibleDisplayedImageAreaInClient(hWnd, &imageRect))
		{
			InvalidateRect(hWnd, &imageRect, FALSE);
		}
	}

	inline bool getVisibleDisplayedImageAreaInClient(HWND hWnd, RECT *pRect)
	{
		RECT clientRect;
		SIZE displayedImageSize;
		RECT displayedImageRect;

		if (GetClientRect(hWnd, &clientRect)
		&&	getDisplayedImageSize(&displayedImageSize))
		{
			displayedImageRect.left   = -m_offset.cx;
			displayedImageRect.top    = -m_offset.cy;
			displayedImageRect.right  = displayedImageSize.cx - m_offset.cx;
			displayedImageRect.bottom = displayedImageSize.cy - m_offset.cy;

			if (IntersectRect(pRect, &clientRect, &displayedImageRect))
			{
				return true;
			}
		}

		SetRectEmpty(pRect);
		return false;
	}

	inline bool isWithinImageArea(HWND hWnd, int x, int y)
	{
		RECT windowRect;
		SIZE displayedImageSize;

		return(	GetClientRect(hWnd, &windowRect)
			&&	getDisplayedImageSize(&displayedImageSize)
			&&	x < windowRect.right
			&&	y < windowRect.bottom
			&&	x >=  (-m_offset.cx)
			&&	x <  ((-m_offset.cx) + displayedImageSize.cx)
			&&	y >=  (-m_offset.cy)
			&&	y <  ((-m_offset.cy) + displayedImageSize.cy));
	}

	inline bool isCursorWithinImageArea(HWND hWnd)
	{
		POINT pos;
//		DWORD dwPos = GetMessagePos();
//		POINTS posTemp = MAKEPOINTS(dwPos);
//		pos.x = posTemp.x;
//		pos.y = posTemp.y;

		if (!GetCursorPos(&pos))
		{
			return(false);
		}

		ScreenToClient(hWnd, &pos);

		return(isWithinImageArea(hWnd, pos.x, pos.y));
	}

	inline void sendSizeChange(HWND hWnd, bool bAlwaysDisplayedSize)
	{
		if (ZOOM_TILED != m_currentZoomFactor)
		{
			DVPNMSIZECHANGE notHdr;
			notHdr.hdr.hwndFrom = hWnd;
			notHdr.hdr.idFrom = 0;
			notHdr.hdr.code = DVPN_SIZECHANGE;

			if (((bAlwaysDisplayedSize || ZOOM_FITPAGE != m_currentZoomFactor) && getDisplayedImageSize(&notHdr.szSize))
			||	getActualImageSize(&notHdr.szSize, false))
			{
				LONG sx = notHdr.szSize.cx;
				LONG sy = notHdr.szSize.cy;

				SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));

			//	RECT tempRect;
			//	if (getClientAndScrollbarsRect(hWnd, &tempRect, false, false))
			//	{
			//		wchar_t *szMsg = LeoHelpers::StringAllocAndFormat(L"Requested %ld x %ld; Got %ld x %ld", sx, sy, tempRect.right - tempRect.left, tempRect.bottom - tempRect.top);
			//		OutputDebugString(szMsg);
			//		delete[] szMsg;
			//	}
			}
		}
	}

	inline bool adjustRectForFrame(RECT *pRect)
	{
		const LONG lFrameAdjust = (OPUSVIEWER_IMAGE_FRAME_SIZE * 2);

		if (m_bFrameImage
		&&	pRect != NULL
		&&	pRect->right  > lFrameAdjust
		&&	pRect->bottom > lFrameAdjust)
		{
			pRect->right  -= lFrameAdjust;
			pRect->bottom -= lFrameAdjust;
			return true;
		}
		return false;
	}

	inline bool adjustSizeForFrame(SIZE *pSize)
	{
		const LONG lFrameAdjust = (OPUSVIEWER_IMAGE_FRAME_SIZE * 2);

		if (m_bFrameImage
		&&	pSize != NULL
		&&	pSize->cx > lFrameAdjust
		&&	pSize->cy > lFrameAdjust)
		{
			pSize->cx -= lFrameAdjust;
			pSize->cy -= lFrameAdjust;
			return true;
		}
		return false;
	}

	inline void normaliseRect(RECT *pRectOut, const RECT *pRectIn)
	{
		if (NULL == pRectIn)
		{
			::SetRectEmpty(pRectOut);
		}
		else
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
	}

	bool setDisplayedImageSizeToMaxFitToPage(HWND hWnd, const SIZE *pMaxDisplayedImageSize);
	bool calculateDisplayedImageSize(HWND hWnd, SIZE *pSize, bool bZooming);
};
