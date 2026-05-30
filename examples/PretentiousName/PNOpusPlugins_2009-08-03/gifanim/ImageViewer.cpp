#include "StdAfx.h"
#include "LeoHelpers.h"
#include "MemoryImage.h"
#include "GifConfig.h"
#include "GifDecoder.h"
#include "FlatImage.h"
#include "TiledImage.h"
#include "ImageViewer.h"
#include "ImagePainter.h"
#include "FramePainter.h"
#include "InfoBoxPainter.h"
#include "SelectBoxPainter.h"
#include "GifTlsData.h"
#include "resource.h"
#include "gifanim.h"

CImageViewer::CImageViewer(HWND hWnd, DWORD dwFlags, CGifConfig *pConfig)
: m_sl(pConfig->GetOpusPluginHelper())
{
	m_hWnd = hWnd;
	m_pConfig = pConfig;

	m_bIsPreview = (DVPCVF_Preview & dwFlags) ? true : false;

	m_bUpdateScrollbarsAndImageOffsetNoRecurse = false;
	m_bGenerateFlattenedImageNoRecurse = false;

	m_hThread = NULL;
	m_hThreadStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThreadGetDelayEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hFramePaintedEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	InitializeCriticalSection(&m_cs);

	m_mouseDragging = IVM_NONE;
	m_mouseSelecting = IVM_NONE;

	m_iMouseWheelDelta = 0;

	m_szName = NULL;

	m_pFramesFrames  = new CAbstractImageList();
	m_pRestoreFrames = NULL;

	m_pInfoBoxPainter = new CInfoBoxPainter();
	m_pSelectBoxPainter = new CSelectBoxPainter();

	reset();
}

CImageViewer::~CImageViewer(void)
{
	reset();

	delete m_pSelectBoxPainter;
	m_pSelectBoxPainter = NULL;

	delete m_pInfoBoxPainter;
	m_pInfoBoxPainter = NULL;

	delete m_pFramesFrames;
	m_pFramesFrames = NULL;

	delete m_pRestoreFrames;
	m_pRestoreFrames = NULL;

	if (NULL != m_hThreadStopEvent)
	{
		CloseHandle(m_hThreadStopEvent);
	}

	if (NULL != m_hThreadGetDelayEvent)
	{
		CloseHandle(m_hThreadGetDelayEvent);
	}

	if (NULL != m_hFramePaintedEvent)
	{
		CloseHandle(m_hFramePaintedEvent);
	}

	DeleteCriticalSection(&m_cs);
}

void CImageViewer::reset()
{
	stopThread();

	SendMessage(GetParent(m_hWnd), TBM_SETRANGE, TRUE, MAKELONG(0,0));

	m_framesFlat.Clear();
	m_framesTiled.Clear();
	m_pFramesFrames->Clear();
	m_pCurrentFrames = m_pFramesFrames;
	m_pLastDrawnImage = NULL;

	delete m_pRestoreFrames;
	m_pRestoreFrames = NULL;

	m_bFlattenFitToWindow = false;
	m_bFlattenHorizontal  = false;
	m_iFlattenFitColumns  = 0;
	m_bBackwards = false;
	m_bDisplayInformation = false;
	m_bIsAlphaHidden = false;

	delete [] m_szName;
	m_szName = NULL;

	m_pInfoBoxPainter->SetInformationString(NULL);
	m_pInfoBoxPainter->FreeCache();

	m_pSelectBoxPainter->FreeCache();

	m_offset.cx = 0;
	m_offset.cy = 0;

	m_displayedImageSize.cx = 0;
	m_displayedImageSize.cy = 0;

	m_originalImageSize.cx = 0;
	m_originalImageSize.cy = 0;
	m_iOriginalBitDepth = 0;

	m_iDitherOffset = 0;

	m_bCanDrag = false;

	m_mouseDragStart.cx = 0;
	m_mouseDragStart.cy = 0;

	clearSelectionAndEndCapture(m_hWnd);
	m_iMouseWheelDelta = 0;

	m_hAbortEvent = NULL;

	m_bFullScreen = false;
	m_bFrameImage = false;
	m_bEnableScrollbars = false;

	m_currentZoomFactor = 0;

	m_initialRotation = 0;
	m_currentRotation = 0;

	m_bGammaEnable = false;
	m_dGammaValue = 0.0;

	m_bShowAnimationControls = (m_bIsPreview ? m_pConfig->GetAnimationControlsPreview() : m_pConfig->GetAnimationControlsViewer());
}

void CImageViewer::startThread()
{
	stopThread();

	if (1 < m_pCurrentFrames->GetNumberOfFrames() && NULL != m_hThreadStopEvent)
	{
		unsigned int uiIgnored;

		m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, animationThreadStatic, this, 0, &uiIgnored));
	}
}

void CImageViewer::stopThread()
{
	if (NULL != m_hThread && NULL != m_hThreadStopEvent)
	{
		SetEvent(m_hThreadStopEvent);
		WaitForSingleObject(m_hThread, INFINITE);
		ResetEvent(m_hThreadStopEvent);
	}

	m_hThread = NULL;
}

// static
unsigned __stdcall CImageViewer::animationThreadStatic(void *pThis)
{
	LeoHelpers::SetThreadName("Image Viewer Animation");

	reinterpret_cast<CImageViewer *>(pThis)->animationThread();
	return 0;
}

void CImageViewer::animationThread()
{
	EnterCriticalSection(m_pConfig->GetCS()); // Atomic config read.

	DWORD dwMinWait = m_pConfig->GetMinimumFrameDelay();
	DWORD dwMaxWait = m_pConfig->GetMaximumFrameDelay();
	bool bOnlyIncreaseZeroDelay = m_pConfig->GetOnlyIncreaseZeroDelay();

	LeaveCriticalSection(m_pConfig->GetCS());

	DWORD dwWaitTime = 0;

	HANDLE threadWaitHandles[3] = { m_hThreadStopEvent, m_hThreadGetDelayEvent, m_hFramePaintedEvent };

	SetEvent(m_hThreadGetDelayEvent);

	bool bPost = false;
	DWORD dwWaitRes;

	while (true)
	{
		// We only wait on the first two events here.
		dwWaitRes = WaitForMultipleObjects(2, threadWaitHandles, FALSE, dwWaitTime);
		if (WAIT_OBJECT_0 == dwWaitRes)
		{
			break; // End the thread.
		}
		else if (bPost && WAIT_TIMEOUT == dwWaitRes)
		{
			// Ensure we don't request the next frame until the current one has been painted.
			// This prevents us from going into a tight loop and overwhelming the UI/paint thread.

			// Wait on any of the three events.
			dwWaitRes = WaitForMultipleObjects(3, threadWaitHandles, FALSE, INFINITE);

			if (WAIT_OBJECT_0 == dwWaitRes)
			{
				break;
			}
		}

		bPost = false;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);

			// m_hThreadGetDelayEvent must be checked again once inside the critical section.

			if (WAIT_TIMEOUT == WaitForSingleObject(m_hThreadGetDelayEvent, 0))
			{
				m_pCurrentFrames->IncrementCurrentIndex( m_bBackwards ? -1 : 1 );

				bPost = true;
			}
			else
			{
				ResetEvent(m_hThreadGetDelayEvent);
			}

			dwWaitTime = m_pCurrentFrames->GetCurrentFrame()->GetDelayTime(); // 100ths of a second.
		}

		dwWaitTime *= 10; // Milliseconds.
	
		if (bPost)
		{
			ResetEvent(m_hFramePaintedEvent);
			PostMessage(m_hWnd, WM_IMAGEVIEWER_REDRAWANIMATIONFRAME, 0, 0);
		}

		// A lot of gifs, or the programs that create them, seem to assume a minimum delay. :-(
		if (0 == dwWaitTime || ((!bOnlyIncreaseZeroDelay) && dwWaitTime < dwMinWait))
		{
			dwWaitTime = dwMinWait;
		}

		if (0 != dwMaxWait && dwWaitTime > dwMaxWait)
		{
			dwWaitTime = dwMaxWait;
		}
	}
}

// static
ATOM CImageViewer::RegisterWindowClass(HMODULE hDllModule)
{
	WNDCLASS wc;

	// Register window class
	wc.style			= CS_DBLCLKS;
	wc.lpfnWndProc		= CImageViewer::wndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hDllModule;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= getWindowClassName();

	return(RegisterClass(&wc));
}

// static
BOOL CImageViewer::UnregisterWindowClass(HMODULE hDllModule)
{
	return(UnregisterClass(getWindowClassName(), hDllModule));
}

// static
HWND CImageViewer::CreateViewerWindow(HWND hWndParent, LPRECT lpRc, DWORD dwFlags, CGifConfig *pConfig)
{
	RECT r;

	if (NULL == lpRc)
	{
		ZeroMemory(&r, sizeof(r));
		lpRc = &r;
	}

	IVC_CreateParams cp;
	cp.dwFlags = dwFlags;
	cp.pConfig = pConfig;

	HWND hWnd = CreateWindowEx(	0, //(dwFlags&DVPCVF_Border) ? 0 : WS_EX_CLIENTEDGE,
								getWindowClassName(),
								NULL,
								WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
								lpRc->left, lpRc->top, lpRc->right - lpRc->left + 1, lpRc->bottom - lpRc->top + 1,
								hWndParent,
								NULL,
								pConfig->GetDllModule(),
								&cp);

	return(hWnd);
}

// static
LRESULT WINAPI CImageViewer::wndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	#pragma warning(suppress:4312) // spurious warning due to stupid Win32 SDK header definition.
	CImageViewer *pThis = reinterpret_cast<CImageViewer *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (uMsg)
	{
	case WM_CREATE:
		{
			IVC_CreateParams *pCP = (reinterpret_cast<IVC_CreateParams *>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
			pThis = new CImageViewer(hWnd, pCP->dwFlags, pCP->pConfig);
			#pragma warning(suppress:4244) // spurious warning due to stupid Win32 SDK header definition.
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			pThis->onCreate(hWnd);
		}
		return 0;

	case WM_DESTROY:
		delete pThis;
		pThis = NULL;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
		return 0;

	default:
		break;
	}

	if (NULL != pThis)
	{
		// Reminder: When adding new messages to here, don't forget to add them to the ViewerToolbarProxy window as well, else
		// we'll never receive them from Opus.

		switch (uMsg)
		{
		case DVPLUGINMSG_GETCAPABILITIES:
			return    VPCAPABILITY_RESIZE_FIT
					| VPCAPABILITY_RESIZE_TILE
					| VPCAPABILITY_RESIZE_ANY
					| VPCAPABILITY_ROTATE_RIGHTANGLE
					| VPCAPABILITY_ADDCONTEXTMENU
					| VPCAPABILITY_SELECTALL
					| VPCAPABILITY_COPYALL
					| VPCAPABILITY_COPYSELECTION
					| VPCAPABILITY_CANSETWALLPAPER
					| VPCAPABILITY_SUPPLYBITMAP
					| VPCAPABILITY_PRINT
					| VPCAPABILITY_GAMMA
					| VPCAPABILITY_FILEINFO
					| VPCAPABILITY_CROPSELECTION // Always set CROP and RESTORE capabilities. Opus sends messages to test whether they are available at a particular time.
					| VPCAPABILITY_HIDEALPHA; // Always set the HIDEALPHA capability. Opus checks the result if DVPLUGINMSG_ISALPHAHIDDEN to see if alpha can be hidden for the currently loaded file.

		case DVPLUGINMSG_ISALPHAHIDDEN:
			return pThis->onDVPIsAlphaHidden(hWnd, reinterpret_cast<BOOL*>(lParam));

		case DVPLUGINMSG_HIDEALPHA:
			pThis->onDVPHideAlpha(hWnd, wParam ? true : false);
			return 0;

		case DVPLUGINMSG_CROPSELECTION:
			// Crop image to current selection, wParam = TRUE to test (return TRUE/FALSE if cropping available)
			return pThis->onDVPCropSelection(hWnd, wParam ? true : false);

		case DVPLUGINMSG_RESTORE:
			// Restore original image, wParam = TRUE to test (return TRUE/FALSE if restore available)
			return pThis->onDVPRestore(hWnd, wParam ? true : false);

		case DVPLUGINMSG_SETABORTEVENT:
			pThis->m_hAbortEvent = reinterpret_cast<HANDLE>(lParam);
			return 0;

		case DVPLUGINMSG_FULLSCREEN:
			pThis->m_bFullScreen = wParam ? true : false;
			return FALSE; // Allow the change.

		case DVPLUGINMSG_SETIMAGEFRAME:
			pThis->m_bFrameImage = wParam ? true : false;
			if (!pThis->m_framesFlat.IsEmpty())
			{
				pThis->generateFlattenedImage(hWnd, false, false);
			}
			pThis->updateScrollbarsAndImageOffset(hWnd, false, NULL, false);
			return 0;

		case DVPLUGINMSG_SHOWHIDESCROLLBARS:
			pThis->m_bEnableScrollbars = wParam ? true : false;
			pThis->updateScrollbarsAndImageOffset(hWnd, false, NULL, false);
			return 0;

		case DVPLUGINMSG_SETROTATION:
			pThis->onDVPSetRotation(hWnd, static_cast<int>(wParam));
			return TRUE;

		case DVPLUGINMSG_ROTATE:
			pThis->onDVPRotate(hWnd, static_cast<int>(wParam));
			return TRUE;

		case DVPLUGINMSG_GAMMACHANGE:
			pThis->onDVPGammaChange(hWnd);
			return TRUE;

		case DVPLUGINMSG_SETZOOM:
			pThis->m_currentZoomFactor = static_cast<int>(wParam);
			return TRUE;

		case DVPLUGINMSG_ZOOM:
			pThis->m_currentZoomFactor = static_cast<int>(wParam);
			pThis->onDVPZoom(hWnd);
			return TRUE; // Must return TRUE if zoom succeeded.

		case DVPLUGINMSG_GETZOOMFACTOR:
			return(pThis->m_currentZoomFactor);

		case DVPLUGINMSG_GETZOOMLIMITS:
			// Return value indicates max/min zoom limits (HIWORD(max), LOWORD(min))
			return MAKELONG(10,1000);

		case DVPLUGINMSG_GETPICSIZE:
			//logLine(_T("GetPicSize\n"));
			if (NULL != wParam)
			{
				*reinterpret_cast<LPINT>(wParam) = 32;
			}
			if (NULL == lParam || pThis->getDisplayedImageSize(reinterpret_cast<LPSIZE>(lParam)))
			{
				return TRUE;
			}
			return FALSE;

		case DVPLUGINMSG_GETORIGINALPICSIZE:
			//logLine(_T("GetOriginalPicSize\n"));
			if (NULL != wParam) // Is sometimes NULL.
			{
				*reinterpret_cast<LPINT>(wParam) = pThis->m_iOriginalBitDepth;
			}
			if (NULL != lParam)
			{
				*reinterpret_cast<LPSIZE>(lParam) = pThis->m_originalImageSize;
			}
			return TRUE;

		case DVPLUGINMSG_GETAUTOBGCOL:
			return(pThis->onDVPGetAutoBGCol(hWnd));

		case DVPLUGINMSG_REDRAW:
			pThis->onDVPRedraw(hWnd, wParam ? reinterpret_cast<COLORREF *>(&lParam) : NULL);
			return 0;

		case WM_IMAGEVIEWER_REDRAWANIMATIONFRAME:
			pThis->onRedrawFrame(hWnd);
			return 0;

		case WM_IMAGEVIEWER_PRECACHENEXTFRAME:
			pThis->precacheFrames(hWnd, true);
			return 0;

		case DVPLUGINMSG_GETIMAGEINFO:
			if (NULL != lParam)
			{
				pThis->onDVPGetImageInfo(hWnd, reinterpret_cast<LPVIEWERPLUGINFILEINFO>(lParam));
			}
			return TRUE;

		// Default mouse wheel handling
		case DVPLUGINMSG_MOUSEWHEEL:
			// VPCAPABILITY_WANTMOUSEWHEEL is not set but the lister preview pane, unlike the
			// viewer window, ignores it and always sends DVPLUGINMSG_MOUSEWHEEL messages.
			// We take advantage of this. (In order to be consistent with the internal viewer.)
			return pThis->onDVPMouseWheel(hWnd,wParam);

		case WM_SIZE:
			//logLine(_T("WM_SIZE: %d x %d\n"), static_cast<int>(LOWORD(lParam)), static_cast<int>(HIWORD(lParam)));
			pThis->updateScrollbarsAndImageOffset(hWnd, false, NULL, false);
			return 0;

		case WM_ERASEBKGND:
			return 0;

		case WM_PAINT:
			return(pThis->onPaint(hWnd));

		case DVPLUGINMSG_LOAD:
			return(pThis->onDVPLoad(hWnd, reinterpret_cast<LPTSTR>(lParam)));

		case DVPLUGINMSG_LOADSTREAM:
			return(pThis->onDVPLoadStream(hWnd, reinterpret_cast<LPSTREAM>(lParam), reinterpret_cast<LPTSTR>(wParam)));

		case DVPLUGINMSG_CLEAR:
			pThis->onDVPClear(hWnd);
			return 0;

		case DVPLUGINMSG_REINITIALIZE:
			pThis->onDVPReinitialize(hWnd);
			return 0;

		case WM_HSCROLL:
			pThis->onHScroll(hWnd, LOWORD(wParam));
			return 0;

		case WM_VSCROLL:
			pThis->onVScroll(hWnd, LOWORD(wParam));
			return 0;

		case DVPLUGINMSG_SELECTALL:
			pThis->onDVPSelectAll(hWnd);
			return 0;

		case DVPLUGINMSG_TESTSELECTION:
			return(pThis->onDVPTestSelection(hWnd) ? TRUE : FALSE);

		case DVPLUGINMSG_COPYSELECTION:
			pThis->onDVPCopySelection(hWnd);
			return 0;

		case DVPLUGINMSG_GETBITMAP:
			return(reinterpret_cast<LRESULT>(pThis->onDVPGetBitmap(hWnd)));

		case DVPLUGINMSG_SETDESKWALLPAPER:
			pThis->onDVPSetDesktopWallpaper(hWnd, reinterpret_cast<const TCHAR *>(lParam));
			return 0;

		case DVPLUGINMSG_SHOWFILEINFO:
			pThis->m_bDisplayInformation = (wParam ? true : false);
			InvalidateRect(pThis->m_hWnd, NULL, FALSE);
			return 0;

		case DVPLUGINMSG_ISFILEINFOSHOWN:
			return (pThis->m_bDisplayInformation ? TRUE : FALSE);

		case WM_LBUTTONDBLCLK:
			{
				// Pass double-click to parent window.
				//  In preview this will open the separate viewer while
				//   in the separate viewer it will toggle slideshow mode.
				NMHDR notHdr;
				notHdr.hwndFrom = hWnd;
				notHdr.idFrom = 0;
				notHdr.code = NM_DBLCLK;
				SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));
			}
			return 0;

		case WM_LBUTTONDOWN:
			pThis->onButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), IVM_LEFT);
			return 0;

		case WM_MBUTTONDOWN:
			pThis->onButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), IVM_MIDDLE);
			return 0;

		case WM_LBUTTONUP:
			pThis->onButtonUp(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), IVM_LEFT);
			return 0;

		case WM_MBUTTONUP:
			pThis->onButtonUp(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), IVM_MIDDLE);
			return 0;

		case WM_CAPTURECHANGED:
			pThis->onCaptureChanged(hWnd);
			return 0;

		case WM_MOUSEMOVE:
			if (wParam & MK_LBUTTON || wParam & MK_MBUTTON)
			{
				pThis->onButtonDownMouseMove(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			}
			break;

		case WM_SETFOCUS:
			// We don't want the focus; give it to Opus.
			SetFocus(GetParent(GetParent(hWnd)));
			return 0;

		case WM_SETCURSOR:
			pThis->onSetCursor(hWnd);
			return 0;

		case WM_CONTEXTMENU:
			if (pThis->m_mouseDragging != IVM_NONE || pThis->m_mouseSelecting != IVM_NONE)
			{
				return 0; // Disable context menu while dragging.
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case IVC_PLAY:
				pThis->onButtonPlay(hWnd, true);
				return 0;
			case IVC_PLAYBACKWARDS:
				pThis->onButtonPlay(hWnd, false);
				return 0;
			case IVC_PAUSE:
				pThis->onButtonPause(hWnd);
				return 0;
			case IVC_NEXTFRAME:
				pThis->onButtonFrame(hWnd, true);
				return 0;
			case IVC_PREVFRAME:
				pThis->onButtonFrame(hWnd, false);
				return 0;
			case IVC_FLATTENHORIZ:
				pThis->onButtonFlatten(hWnd, false, true);
				return 0;
			case IVC_FLATTENVERT:
				pThis->onButtonFlatten(hWnd, false, false);
				return 0;
			case IVC_FLATTENFIT:
				pThis->onButtonFlatten(hWnd, true, false);
				return 0;
			case IVC_DITHERIMAGEALL:
			case IVC_DITHERIMAGEONE:
			case IVC_DITHERIMAGETWO:
				pThis->onButtonDitherImage(hWnd, LOWORD(wParam));
				return 0;
			default:
				break;
			}
			break;

		case DVPLUGINMSG_APPCOMMAND:
			switch(GET_APPCOMMAND_LPARAM(lParam))
			{
			case APPCOMMAND_MEDIA_NEXTTRACK:
				pThis->onButtonFrame(hWnd, true);
				return TRUE;

			case APPCOMMAND_MEDIA_PREVIOUSTRACK:
				pThis->onButtonFrame(hWnd, false);
				return TRUE;

			case APPCOMMAND_MEDIA_PAUSE:
			case APPCOMMAND_MEDIA_STOP:
				pThis->onButtonPause(hWnd);
				return TRUE;

			case APPCOMMAND_MEDIA_PLAY_PAUSE:
				{
					bool bShift   = (GetKeyState(VK_SHIFT)&0x8000)   ? true : false; // (GET_KEYSTATE_LPARAM(lParam) & MK_CONTROL) doesn't seem to work
					bool bControl = (GetKeyState(VK_CONTROL)&0x8000) ? true : false; // (GET_KEYSTATE_LPARAM(lParam) & MK_SHIFT)   doesn't seem to work
					bool bBackwards = (bShift || bControl);

					if (pThis->m_hThread != NULL
					&&	((bBackwards && pThis->m_bBackwards) || (!bBackwards && !pThis->m_bBackwards)))
					{
						pThis->onButtonPause(hWnd);
						return TRUE;
					}
					else
					{

						pThis->onButtonPlay(hWnd, !bBackwards);
						return TRUE;
					}
				}

			case APPCOMMAND_MEDIA_PLAY:
				{
					bool bShift   = (GetKeyState(VK_SHIFT)&0x8000)   ? true : false; // (GET_KEYSTATE_LPARAM(lParam) & MK_CONTROL) doesn't seem to work
					bool bControl = (GetKeyState(VK_CONTROL)&0x8000) ? true : false; // (GET_KEYSTATE_LPARAM(lParam) & MK_SHIFT)   doesn't seem to work
					bool bBackwards = (bShift || bControl);

					pThis->onButtonPlay(hWnd, !bBackwards);
					return TRUE;
				}

			case APPCOMMAND_MEDIA_FAST_FORWARD:
				pThis->onButtonPlay(hWnd, true);
				return TRUE;

			case APPCOMMAND_MEDIA_REWIND:
				pThis->onButtonPlay(hWnd, false);
				return TRUE;

			default:
				break;
			}
			break;

		case WM_IMAGEVIEWER_SETFRAME:
			pThis->onSetFrame(hWnd, static_cast<int>(lParam));
			return 0;

		case DVPLUGINMSG_ADDCONTEXTMENU:
			return(reinterpret_cast<LRESULT>(pThis->onAddContextMenu(reinterpret_cast<DWORD *>(wParam))));

		case DVPLUGINMSG_PRINT:
			pThis->onDVPPrint(hWnd);
			return 0;

		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE:
	//	case WM_THEMECHANGED:
	//	case WM_POWERBROADCAST:
			pThis->onSystemSettingChange(hWnd);
			break;// Allow DefWindowProc to process them as well.

		default:
			break;
		}
	}

	return(DefWindowProc(hWnd,uMsg,wParam,lParam));
}

void CImageViewer::onCreate(HWND hWnd)
{
}

LRESULT CImageViewer::onDVPLoad(HWND hWnd, LPTSTR szFilename)
{
	NGifDecoder::CGifFile gifFile(szFilename, m_hAbortEvent);

	return(this->load(hWnd, &gifFile, szFilename, NULL));
}

LRESULT CImageViewer::onDVPLoadStream(HWND hWnd, LPSTREAM pStream, LPTSTR szFilename)
{
	NGifDecoder::CGifFile gifFile(pStream);

	return(this->load(hWnd, &gifFile, szFilename, NULL));
}

LRESULT CImageViewer::load(HWND hWnd, NGifDecoder::CGifFile *pGifFile, const TCHAR *szName, CAbstractImageList *pReplacementImageList)
{
	// If pReplacementImageList is non-NULL then it must either be deleted or stored for later deletion.


	// Check for invalid combination of arguments.
	if (pReplacementImageList != NULL && (pReplacementImageList->IsEmpty() || pGifFile != NULL))
	{
		delete pReplacementImageList;
		pReplacementImageList = NULL;

		return FALSE;
	}

	LRESULT lResult = FALSE;

	stopThread();

	bool bFlatten = false;
	bool bPlay = true;

	m_bBackwards = false;

	CGifTlsData *pTlsData = getGifViewerTlsData();

	if (NULL != pTlsData && NULL != pTlsData->GetHWnd())
	{
		if (GetParent(GetParent(hWnd)) != pTlsData->GetHWnd())
		{
			// The viewer window this data was for has been closed.

			pTlsData->SetHWnd(NULL);
		}
		else
		{
			// We have some saved state from the same thread and viewer window.

			if (pTlsData->IsFlatten())
			{
				bFlatten = true;
				bPlay = false;
				m_bFlattenHorizontal  = pTlsData->IsFlattenHorizontal();
				m_bFlattenFitToWindow = pTlsData->IsFlattenFitToWindow();
			}
			else if (pTlsData->IsPause())
			{
				bPlay = false;
			}
			else if (pTlsData->IsPlayBackwards())
			{
				m_bBackwards = true;
			}
		}
	}

	m_framesFlat.Clear();
	m_framesTiled.Clear();
	m_pLastDrawnImage = NULL;

	if (m_pRestoreFrames != pReplacementImageList)
	{
		delete m_pRestoreFrames;
	}

	m_pRestoreFrames = NULL;

	if (pReplacementImageList != NULL)
	{
		m_pRestoreFrames = m_pFramesFrames;
		m_pRestoreFrames->DeleteCachedPaintBitmaps(NULL, NULL);
		m_pFramesFrames = pReplacementImageList;
	}
	else
	{
		m_pFramesFrames->Clear();
	}

	m_pCurrentFrames = m_pFramesFrames;

	m_pInfoBoxPainter->FreeCache();
	m_pSelectBoxPainter->FreeCache();

	clearSelectionAndEndCapture(hWnd);
	m_iMouseWheelDelta = 0;

	CAbstractImage *pFirstFrame = NULL;
	int loadedImageRotation = 0;
	bool bNotAllFramesLoadedDueToMemory = false;

	if (pReplacementImageList != NULL)
	{
		pFirstFrame = m_pFramesFrames->GetFirstFrame();
		loadedImageRotation = m_currentRotation;
	}
	else
	{
		m_originalImageSize.cx = 0;
		m_originalImageSize.cy = 0;
		m_iOriginalBitDepth = 0;
		m_pInfoBoxPainter->SetInformationString(NULL);

		// Reset these on load. Opus will request they be turned on if required.
		// Plugins should always reset these flags since Opus will automatically request them if they should be on, but
		// won't turn them off if they should be off.
		m_bDisplayInformation = false;
		m_bIsAlphaHidden = false;

		m_currentRotation = m_initialRotation;

		delete [] m_szName;
		m_szName = NULL;

		if (NULL != szName)
		{
			m_szName = new TCHAR[ _tcslen(szName) + 1 ];
			_tcscpy_s(m_szName, _tcslen(szName) + 1, szName);
		}

		NMHDR notHdr;
		notHdr.hwndFrom = hWnd;
		notHdr.idFrom = 0;
		notHdr.code = DVPN_GETBGCOL;
		COLORREF crefViewerBackground = static_cast<COLORREF>(SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));
		RGBQUAD rgbViewerBackground;
		rgbViewerBackground.rgbRed	    = GetRValue(crefViewerBackground);
		rgbViewerBackground.rgbGreen	= GetGValue(crefViewerBackground);
		rgbViewerBackground.rgbBlue		= GetBValue(crefViewerBackground);
		rgbViewerBackground.rgbReserved = 0;

		getGammaFromOpus(hWnd);

		if (NGifDecoder::CGifImage::LoadGifImage(m_pFramesFrames, false, NULL, &bNotAllFramesLoadedDueToMemory, pGifFile, &rgbViewerBackground, &m_iOriginalBitDepth))
		{
			pFirstFrame = m_pFramesFrames->GetFirstFrame();
			int iNumFrames = m_pFramesFrames->GetNumberOfFrames();

			const TCHAR *szTypeName = pFirstFrame->GetImageTypeName(&m_sl);

			if (szTypeName == NULL)
			{
				szTypeName = m_sl.Get(STR_GIFANIM_IMAGE);
			}

			int loadedImageWidthForInfo;
			int loadedImageHeightForInfo;

			if (loadedImageRotation == 90 || loadedImageRotation == 270)
			{
				loadedImageWidthForInfo = pFirstFrame->GetHeight();
				loadedImageHeightForInfo = pFirstFrame->GetWidth();
			}
			else
			{
				loadedImageWidthForInfo  = pFirstFrame->GetWidth();
				loadedImageHeightForInfo = pFirstFrame->GetHeight();
			}

			m_originalImageSize.cx = loadedImageWidthForInfo;
			m_originalImageSize.cy = loadedImageHeightForInfo;

			TCHAR *szInformation = NULL;

			if (1 >= iNumFrames)
			{
				szInformation = LeoHelpers::StringAllocAndFormat(_T("%d x %d x %d %s"),
					loadedImageWidthForInfo,
					loadedImageHeightForInfo,
					m_iOriginalBitDepth,
					szTypeName);
			}
			else
			{
				int iTotalDelay = 0;

				for(CAbstractImageList::size_type frmIdx = 0; frmIdx < iNumFrames; ++frmIdx)
				{
					iTotalDelay += m_pFramesFrames->GetFrame(frmIdx)->GetDelayTime(); // 100ths of a second.
				}

				double dTotalSeconds = iTotalDelay / 100.0;

				szInformation = LeoHelpers::StringAllocAndFormat(
					m_sl.Get(bNotAllFramesLoadedDueToMemory ? STR_GIFANIM_INFOBOX_INCOMPLETE : STR_GIFANIM_INFOBOX_EXACT),
					loadedImageWidthForInfo,
					loadedImageHeightForInfo,
					m_iOriginalBitDepth,
					szTypeName,
					iNumFrames,
					dTotalSeconds);
			}

			if (szInformation != NULL)
			{
				m_pInfoBoxPainter->SetInformationString(szInformation);
				delete[] szInformation;
			}
		}
	}

	if (pFirstFrame == NULL)
	{
		bPlay = false;
	}
	else
	{
		if (m_bBackwards)
		{
			m_pCurrentFrames->IncrementCurrentIndex(-1);
		}

		int iNumFrames = m_pFramesFrames->GetNumberOfFrames();

		if (1 >= iNumFrames)
		{
			bFlatten = false;
			bPlay = false;
		}

		SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SHOWTOOLBAR, m_bShowAnimationControls && (1 < iNumFrames) ? TRUE : FALSE, 0);

		if (!bFlatten)
		{
			if (bPlay)
			{
				SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, m_bBackwards ? IVC_PLAYBACKWARDS : IVC_PLAY, 0);
			}
			else
			{
				SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_PAUSE, 0);
			}
		}
		else if (m_bFlattenFitToWindow)
		{
			SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_FLATTENFIT, 0);
		}
		else if (m_bFlattenHorizontal)
		{
			SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_FLATTENHORIZ, 0);
		}
		else
		{
			SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_FLATTENVERT, 0);
		}

		if (pReplacementImageList == NULL)
		{
			bool bRotated = m_pFramesFrames->RotateImages(m_initialRotation);
			assert(bRotated);
		}

		m_offset.cx = 0;
		m_offset.cy = 0;

//		RECT initialClientRect;
//
//		if (!GetClientRect(hWnd, &initialClientRect) || ::IsRectEmpty(&initialClientRect))
//		{
//			// Force Opus to resize our window (if possible) so we have valid sizes for the client rect, toolbar (if any), and so on.
//			// Without this we can't tell how much space is used by the non-image parts of the window.
//			m_displayedImageSize.cx = 1;
//			m_displayedImageSize.cy = 1;
//			sendSizeChange(hWnd, true);
//			
//			//logLine(_T("Requested resize\n"));
//			SIZE sizeMax;
//			getMaxClientSize(hWnd, &sizeMax);
//
//		}

		m_displayedImageSize.cx = 0;
		m_displayedImageSize.cy = 0;

		m_iDitherOffset = 0;

		if (bFlatten)
		{
			// generateFlattenedImage will call generateTiledImage automatically if it needs to.
			generateFlattenedImage(hWnd, false, true);
		}
		else if (ZOOM_TILED == m_currentZoomFactor)
		{
			generateTiledImage(hWnd);
		}
		else if (ZOOM_FITPAGE == m_currentZoomFactor)
		{
			// Ask to resize the viewer window to best fit the image size if fit-to-page is on.
			// The request may be ignored.

			bool bUseDisplayedSize = false;

			SIZE sizeImage;
			if (getActualImageSize(&sizeImage, false))
			{
				bUseDisplayedSize = setDisplayedImageSizeToMaxFitToPage(hWnd, &sizeImage);
			}

			sendSizeChange(hWnd, bUseDisplayedSize);
		}

		updateScrollbarsAndImageOffset(hWnd, true, NULL, false);

//		this->onDVPRedraw(hWnd, NULL);

		// Resize the viewer window after resizing the image if fit-to-page is off.
		// If we're in ZOOM_TILED mode then we don't resize the window at all.
		if ((!bFlatten) && ZOOM_FITPAGE != m_currentZoomFactor && ZOOM_TILED != m_currentZoomFactor)
		{
			sendSizeChange(hWnd, false);
		}

		/*
		if (pReplacementImageList != NULL)
		{
			DVPNMSTATUSTEXT notHdrStatusText = {0};
			notHdrStatusText.hdr.hwndFrom = hWnd;
			notHdrStatusText.hdr.idFrom   = 0;
			notHdrStatusText.hdr.code     = DVPN_STATUSTEXT;
			notHdrStatusText.fUnicode       = TRUE;
			notHdrStatusText.lpszStatusText = NULL;
			SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdrStatusText));
		}
		*/

		precacheFrames(hWnd, false);

		if (bNotAllFramesLoadedDueToMemory)
		{
			MessageBox(GetParent(GetParent(hWnd)),
				m_sl.Get(STR_GIFANIM_LOAD_INCOMPLETE),
				(m_szName != NULL) ? LeoHelpers::GetLastPathPart(m_szName) : m_sl.Get(STR_GIFANIM_GIF_IMAGE),
				MB_OK|MB_ICONWARNING);
		}

		lResult = TRUE;
	}

	SendMessage(GetParent(hWnd), TBM_SETRANGE, TRUE, MAKELONG(1, m_pFramesFrames->GetNumberOfFrames()));
	LONG ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
	PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);

	if (bPlay)
	{
		startThread();
	}

	return(lResult);
}

void CImageViewer::onDVPClear(HWND hWnd)
{
//	SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SHOWTOOLBAR, FALSE, 0);

	reset();
	InvalidateRect(hWnd, NULL, FALSE);
	UpdateWindow(hWnd);

	NMHDR notHdr;
	notHdr.hwndFrom = hWnd;
	notHdr.idFrom = 0;
	notHdr.code = DVPN_CLEARED;

	SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));
}

void CImageViewer::onSystemSettingChange(HWND hWnd)
{
	m_pSelectBoxPainter->FreeCache();
	m_pInfoBoxPainter->FreeCache();
	InvalidateRect(hWnd, NULL, FALSE);
}

void CImageViewer::onDVPReinitialize(HWND hWnd)
{
	m_bShowAnimationControls = (m_bIsPreview ? m_pConfig->GetAnimationControlsPreview() : m_pConfig->GetAnimationControlsViewer());

	SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SHOWTOOLBAR, m_bShowAnimationControls && (1 < m_pFramesFrames->GetNumberOfFrames()) ? TRUE : FALSE, 0);

	if (NULL != m_hThread)
	{
		stopThread();
		startThread();
	}
}

void CImageViewer::onDVPSetRotation(HWND hWnd, int iRotation)
{
	iRotation %= 360;

	if (0 > iRotation)
	{
		iRotation += 360;
	}

	if (270 <= iRotation)
	{
		iRotation = 270;
	}
	else if (180 <= iRotation)
	{
		iRotation = 180;
	}
	else if (90 <= iRotation)
	{
		iRotation = 90;
	}
	else
	{
		iRotation = 0;
	}

	m_initialRotation = iRotation;
}

void CImageViewer::onDVPRotate(HWND hWnd, int iRotation)
{
	RECT oldMouseSelectRect = m_mouseSelectRect;

	clearSelectionAndEndCapture(hWnd);
	m_iMouseWheelDelta = 0;

	iRotation %= 360;

	if (0 > iRotation)
	{
		iRotation += 360;
	}

	if (270 <= iRotation)
	{
		iRotation = 270;
	}
	else if (180 <= iRotation)
	{
		iRotation = 180;
	}
	else if (90 <= iRotation)
	{
		iRotation = 90;
	}
	else
	{
		iRotation = 0;
	}

	m_currentRotation = (m_currentRotation + iRotation) % 360;

	if (!m_pFramesFrames->IsEmpty())
	{
		const int iOldBitmapW = m_pFramesFrames->GetFirstFrame()->GetWidth();
		const int iOldBitmapH = m_pFramesFrames->GetFirstFrame()->GetHeight();

		if (0 != m_iDitherOffset)
		{
			if (( 90 == iRotation && 0 == (iOldBitmapH%2))
			||	(270 == iRotation && 0 == (iOldBitmapW%2))
			||	(180 == iRotation && (iOldBitmapW%2) != (iOldBitmapH%2)))
			{
				m_iDitherOffset = (1 == m_iDitherOffset) ? 2 : 1;
			}
		}

		bool bRotated = m_pFramesFrames->RotateImages(iRotation);
		assert(bRotated);

		if (m_pRestoreFrames != NULL && !m_pRestoreFrames->IsEmpty())
		{
			bRotated = m_pRestoreFrames->RotateImages(iRotation); // Keep the undo frames rotated in step, else the dimensions we report in the F1 info will be the wrong way around, and maybe other things will go wrong.
			assert(bRotated);
		}

		if (m_framesFlat.IsEmpty())
		{
			SIZE sizeImage;

			if (ZOOM_FITPAGE == m_currentZoomFactor
			&&	getActualImageSize(&sizeImage, false)
			&&	setDisplayedImageSizeToMaxFitToPage(hWnd, &sizeImage))
			{
				// no-op; m_displayedImageSize alread set by setDisplayedImageSizeToMaxFitToPage
			}
			else if (90 == iRotation || 270 == iRotation)
			{
				LONG tempCX = m_displayedImageSize.cx;
				m_displayedImageSize.cx = m_displayedImageSize.cy;
				m_displayedImageSize.cy = tempCX;
			}

			if (m_framesTiled.IsEmpty())
			{
				// Restore the selection, rotated appropriately.

				if (iRotation == 270)
				{
					m_mouseSelectRect.left   = oldMouseSelectRect.top;
					m_mouseSelectRect.top    = iOldBitmapW - oldMouseSelectRect.right;
					m_mouseSelectRect.right  = oldMouseSelectRect.bottom;
					m_mouseSelectRect.bottom = iOldBitmapW - oldMouseSelectRect.left;
				}
				else if (iRotation == 180)
				{
					m_mouseSelectRect.left   = iOldBitmapW - oldMouseSelectRect.right;
					m_mouseSelectRect.top    = iOldBitmapH - oldMouseSelectRect.bottom;
					m_mouseSelectRect.right  = iOldBitmapW - oldMouseSelectRect.left;
					m_mouseSelectRect.bottom = iOldBitmapH - oldMouseSelectRect.top;
				}
				else if (iRotation == 90)
				{
					m_mouseSelectRect.left   = iOldBitmapH - oldMouseSelectRect.bottom;
					m_mouseSelectRect.top    = oldMouseSelectRect.left;
					m_mouseSelectRect.right  = iOldBitmapH - oldMouseSelectRect.top;
					m_mouseSelectRect.bottom = oldMouseSelectRect.right;
				}

				sendSizeChange(hWnd, true);
			}
		}
		else
		{
			generateFlattenedImage(hWnd, false, true);
		}

		updateScrollbarsAndImageOffset(hWnd, false, NULL, false);
		precacheFrames(hWnd, true);
	}
}

void CImageViewer::getGammaFromOpus(HWND hWnd)
{
	DVPNMGAMMA notHdr;
	notHdr.hdr.hwndFrom = hWnd;
	notHdr.hdr.idFrom = 0;
	notHdr.hdr.code = DVPN_GETGAMMA;
	notHdr.fEnable = FALSE;
	notHdr.dbGamma = 0.0;

	SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));

	m_bGammaEnable = notHdr.fEnable ? true : false;
	m_dGammaValue = notHdr.dbGamma;

	// Generate gamma lookup table.
	if (m_bGammaEnable)
	{
		if (m_dGammaValue < 0.1)
		{
			m_dGammaValue = 0.1;
		}

		m_GammaTable[0]=0;

		for (int i=1; i < 255; i++)
		{
			DWORD dwGC = (DWORD)(255.0*pow(i*(1.0/255.0),1.0/m_dGammaValue)+0.4);
			if (dwGC>255) dwGC=255;
			m_GammaTable[i] = (BYTE)dwGC;
		}

		m_GammaTable[255]=255;
	}
}

void CImageViewer::onDVPGammaChange(HWND hWnd)
{
	getGammaFromOpus(hWnd);

	precacheFrames(hWnd, true);

	invalidateImage(hWnd);
}

void CImageViewer::onDVPZoom(HWND hWnd)
{
	if (ZOOM_TILED == m_currentZoomFactor)
	{
		generateTiledImage(hWnd);
	}
	else if (!m_framesTiled.IsEmpty())
	{
		clearSelectionAndEndCapture(hWnd);
		m_iMouseWheelDelta = 0;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);

			if (m_framesFlat.IsEmpty())
			{
				m_pCurrentFrames = m_pFramesFrames;
				m_pCurrentFrames->SetCurrentIndex( m_framesTiled.GetCurrentIndex() );
			}
			else
			{
				m_pCurrentFrames = &m_framesFlat;
			}

			m_pFramesFrames->DeleteCachedPaintBitmaps(NULL, NULL);
			m_framesFlat.DeleteCachedPaintBitmaps(NULL, NULL);
			m_framesTiled.Clear();
		}

		m_pLastDrawnImage = NULL;
	}

	SIZE sizeImage;
	if (ZOOM_FITPAGE == m_currentZoomFactor
	&&	getActualImageSize(&sizeImage, false)
	&&	setDisplayedImageSizeToMaxFitToPage(hWnd, &sizeImage))
	{
		sendSizeChange(hWnd, true);
	}

	updateScrollbarsAndImageOffset(hWnd, false, NULL, true);
	precacheFrames(hWnd, true);
}

void CImageViewer::precacheFrames(HWND hWnd, bool bOnlyIfPlaying)
{
	CAbstractImage *pCurrentImage;
	CAbstractImage *pNextImage;

	{
		LeoHelpers::CriticalSectionScoper css(&m_cs);

		pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
		pNextImage = (NULL == m_hThread ? NULL : m_pCurrentFrames->GetRelativeFrame(m_bBackwards ? -1 : 1));
	}

	if (pCurrentImage == NULL || (bOnlyIfPlaying && NULL == m_hThread))
	{
		m_pCurrentFrames->DeleteCachedPaintBitmaps(pCurrentImage, NULL);
	}
	else
	{
//		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		m_pCurrentFrames->DeleteCachedPaintBitmaps(pCurrentImage, pNextImage);

		HDC hDC = GetDC(hWnd);

		if (NULL != hDC)
		{
			SIZE displayedImageSize;

			if (getDisplayedImageSize(&displayedImageSize))
			{
				pCurrentImage->GeneratePaintCache(hDC, &displayedImageSize, m_iDitherOffset, m_bGammaEnable, &m_dGammaValue, m_GammaTable);

				if (pNextImage != NULL && pNextImage != pCurrentImage)
				{
					pNextImage->GeneratePaintCache(hDC, &displayedImageSize, m_iDitherOffset, m_bGammaEnable, &m_dGammaValue, m_GammaTable);
				}
			}

			ReleaseDC(hWnd, hDC);
		}

//		onSetCursor(hWnd);
	}
}

LRESULT CImageViewer::onPaint(HWND hWnd)
{
	PAINTSTRUCT ps;

	HDC hDC = BeginPaint(hWnd, &ps);

	if (NULL != hDC)
	{
		bool bDoubleBuffer = false;

		RECT clientRect;
		if (GetClientRect(hWnd, &clientRect))
		{
			assert(clientRect.left == 0 && clientRect.top == 0); // If this fails then the FramePainter call needs adjusting to use an offset.

			SIZE clientSize;
			clientSize.cx = clientRect.right  - clientRect.left;
			clientSize.cy = clientRect.bottom - clientRect.top;

			RECT selectedRect = m_mouseSelectRect;

			if (!imageToClient(&selectedRect))
			{
				SetRectEmpty(&selectedRect);
			}

			SIZE displayedImageSize;

			if (getDisplayedImageSize(&displayedImageSize) && !m_pCurrentFrames->IsEmpty())
			{
				RECT imageRectInClient;
				imageRectInClient.left   = -m_offset.cx;
				imageRectInClient.top    = -m_offset.cy;
				imageRectInClient.right  = imageRectInClient.left + displayedImageSize.cx;
				imageRectInClient.bottom = imageRectInClient.top  + displayedImageSize.cy;

				RECT imageRectInClientClipped;
				bool bPaintImage = (IntersectRect(&imageRectInClientClipped, &ps.rcPaint, &imageRectInClient) ? true : false);
				bool bPaintFrame = (EqualRect(&imageRectInClientClipped, &ps.rcPaint) ? false : true);

				CAbstractImage *pImageToDraw;
				{
					LeoHelpers::CriticalSectionScoper css(&m_cs);
					pImageToDraw = m_pCurrentFrames->GetCurrentFrame();
				}
				m_pLastDrawnImage = pImageToDraw;

				if (bPaintImage || bPaintFrame)
				{
					m_pInfoBoxPainter->Cache(hDC, ps, clientRect, m_bDisplayInformation);
				}

				if (bPaintImage)
				{
					m_pSelectBoxPainter->Cache(hDC, ps, selectedRect);

					CImagePainter imagePainter;
					imagePainter.Cache(hDC, ps, clientRect, imageRectInClient, pImageToDraw, m_pSelectBoxPainter, m_pInfoBoxPainter,
						m_iDitherOffset, m_bGammaEnable, &m_dGammaValue, m_GammaTable);
					imagePainter.PaintFromCache(hDC, ps);
				}

				if (bPaintFrame)
				{
					// Draw the background and frame first to make resizing look a bit nicer in Vista (black space overwritten sooner).
					CFramePainter framePainter;
					framePainter.Cache(hDC, ps, hWnd, clientSize, imageRectInClient, m_bFrameImage && pImageToDraw->AllowFrameAroundImage(), m_pConfig->GetOpusPluginHelper(), m_pInfoBoxPainter);
					framePainter.PaintFromCache(hDC, ps, 0, 0);
				}
			}
		}

		EndPaint(hWnd, &ps);
	}

	PostMessage(hWnd, WM_IMAGEVIEWER_PRECACHENEXTFRAME, 0, 0);

	SetEvent(m_hFramePaintedEvent);

	return 0;
}

void CImageViewer::onRedrawFrame(HWND hWnd)
{
	LONG ilTrackpos = 0;
	bool bInvalidate = 0;

	{
		LeoHelpers::CriticalSectionScoper css(&m_cs);

		ilTrackpos = 0;
		bInvalidate = (m_pLastDrawnImage != m_pCurrentFrames->GetCurrentFrame());

		if (bInvalidate)
		{
			ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
		}
	}

	if (bInvalidate)
	{
		invalidateImage(hWnd);
	}

	if (0 != ilTrackpos)
	{
		PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);
	}
}

void CImageViewer::onSetFrame(HWND hWnd, int iFrame) // iFrame starts from one not zero.
{
	if (iFrame <= m_pCurrentFrames->GetNumberOfFrames())
	{
		bool bInvalidate = false;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);

			SetEvent(m_hThreadGetDelayEvent);

			m_pCurrentFrames->SetCurrentIndex(iFrame - 1);

			bInvalidate = (m_pLastDrawnImage != m_pCurrentFrames->GetCurrentFrame());
		}

		if (bInvalidate)
		{
			invalidateImage(hWnd);
		}
	}
}

bool CImageViewer::setDisplayedImageSizeToMaxFitToPage(HWND hWnd, const SIZE *pMaxDisplayedImageSize)
{
	bool bResult = false;

	SIZE sizeImage = *pMaxDisplayedImageSize;
	SIZE sizeMaxClient;

	if (getMaxClientSize(hWnd, &sizeMaxClient))
	{
		adjustSizeForFrame(&sizeMaxClient);

		if (sizeImage.cx > sizeMaxClient.cx || sizeImage.cy > sizeMaxClient.cy)
		{
			if (LeoHelpers::MulDivRoundDown(sizeImage.cy, sizeMaxClient.cx, sizeImage.cx) <= sizeMaxClient.cy)
			{
				sizeImage.cy = LeoHelpers::MulDivRoundDown(sizeImage.cy, sizeMaxClient.cx, sizeImage.cx);
				sizeImage.cx = LeoHelpers::MulDivRoundDown(sizeImage.cx, sizeMaxClient.cx, sizeImage.cx);
			}
			else
			{
				sizeImage.cx = LeoHelpers::MulDivRoundDown(sizeImage.cx, sizeMaxClient.cy, sizeImage.cy);
				sizeImage.cy = LeoHelpers::MulDivRoundDown(sizeImage.cy, sizeMaxClient.cy, sizeImage.cy);
			}
		}

		if (sizeImage.cx > 0
		&&	sizeImage.cy > 0)
		{
			m_displayedImageSize = sizeImage;
			bResult = true;
		}
	}

	return bResult;
}

bool CImageViewer::calculateDisplayedImageSize(HWND hWnd, SIZE *pSize, bool bZooming)
{
	SIZE sizeTemp;

	if (NULL == pSize)
	{
		pSize = &sizeTemp;
	}

	if (m_bFlattenFitToWindow && !m_framesFlat.IsEmpty() && m_framesTiled.IsEmpty())
	{
		generateFlattenedImage(hWnd, true, bZooming); // Regenerate flattened image if number of columns has changed and we're not tiled.
	}

	if (!getActualImageSize(pSize, false))
	{
		return(false);
	}

	if (0 < pSize->cx && 0 < pSize->cy)
	{
		if (ZOOM_FITPAGE == m_currentZoomFactor || ZOOM_TILED == m_currentZoomFactor)
		{
			RECT windowRect;

			if (getClientAndScrollbarsRect(hWnd, &windowRect, false, false))
			{
				adjustRectForFrame(&windowRect);

				if (ZOOM_FITPAGE == m_currentZoomFactor)
				{
					if (pSize->cx > windowRect.right || pSize->cy > windowRect.bottom)
					{
						if (LeoHelpers::MulDivRoundDown(pSize->cy, windowRect.right, pSize->cx) <= windowRect.bottom)
						{
							pSize->cy = LeoHelpers::MulDivRoundDown(pSize->cy, windowRect.right, pSize->cx);
							pSize->cx = LeoHelpers::MulDivRoundDown(pSize->cx, windowRect.right, pSize->cx);
						}
						else
						{
							pSize->cx = LeoHelpers::MulDivRoundDown(pSize->cx, windowRect.bottom, pSize->cy);
							pSize->cy = LeoHelpers::MulDivRoundDown(pSize->cy, windowRect.bottom, pSize->cy);
						}
					}
				}
				else if (ZOOM_TILED == m_currentZoomFactor)
				{
					pSize->cx = windowRect.right;
					pSize->cy = windowRect.bottom;
					m_framesTiled.SetImageSize(pSize);
				}
			}
		}
		else if (0 != m_currentZoomFactor && 100 != m_currentZoomFactor)
		{
			pSize->cx = LeoHelpers::MulDivRoundDown(pSize->cx, m_currentZoomFactor, 100);
			pSize->cy = LeoHelpers::MulDivRoundDown(pSize->cy, m_currentZoomFactor, 100);
		}
	}

	m_displayedImageSize.cx = pSize->cx;
	m_displayedImageSize.cy = pSize->cy;

	return(true);
}

void CImageViewer::keepOffsetInRange(const SIZE *pImageSize, const RECT *pWindowRect)
{
	m_bCanDrag = (pImageSize->cx > pWindowRect->right || pImageSize->cy > pWindowRect->bottom);

	// Update the offsets to center the image (when smaller than the window) and
	// keep it within range (when larger than the window).
	if (pImageSize->cx <= pWindowRect->right)
	{
		m_offset.cx = -((pWindowRect->right - pImageSize->cx)/2);
	}
	else if (0 > m_offset.cx)
	{
		m_offset.cx = 0;
	}
	else if ((pImageSize->cx - pWindowRect->right) < m_offset.cx)
	{
		m_offset.cx = (pImageSize->cx - pWindowRect->right);
	}

	if (pImageSize->cy <= pWindowRect->bottom)
	{
		m_offset.cy = -((pWindowRect->bottom - pImageSize->cy)/2);
	}
	else if (0 > m_offset.cy)
	{
		m_offset.cy = 0;
	}
	else if ((pImageSize->cy - pWindowRect->bottom) < m_offset.cy)
	{
		m_offset.cy = (pImageSize->cy - pWindowRect->bottom);
	}
}

void CImageViewer::updateScrollbarsAndImageOffset(HWND hWnd, bool bResetOffset, const SIZE *pOffsetAdjust, bool bZooming)
{
	// Adding or removing scrollbars causes a resize, and thus a recursive call,
	// in the middle of this function. It totally screws things up if we don't ignore it.

	if (!m_bUpdateScrollbarsAndImageOffsetNoRecurse)
	{
		m_bUpdateScrollbarsAndImageOffsetNoRecurse = true;

		RECT windowSize;
		SIZE displayedImageSize;

		SIZE oldOffset;
		oldOffset.cx = m_offset.cx;
		oldOffset.cy = m_offset.cy;

		SIZE oldSize;
		oldSize.cx = m_displayedImageSize.cx;
		oldSize.cy = m_displayedImageSize.cy;

		// Reset the offset if required.

		if (m_pCurrentFrames->IsEmpty() || bResetOffset)
		{
			m_offset.cx = 0;
			m_offset.cy = 0;
		}

		if (m_pCurrentFrames->IsEmpty())
		{
			ShowScrollBar(hWnd, SB_BOTH, FALSE);
		}
		else if (calculateDisplayedImageSize(hWnd, &displayedImageSize, bZooming)
		&&		 getClientAndScrollbarsRect(hWnd, &windowSize, false, false))
		{
			// Update the scrollbars and update the windowSize RECT to take account of any scrollbars.
			// Work out which scrollbars we'll need and how it will change the window size.

			LONG style = GetWindowLong(hWnd, GWL_STYLE);
			bool bCurrentH = (style&WS_HSCROLL)!=0;
			bool bCurrentV = (style&WS_VSCROLL)!=0;

			bool bScrollBarH = false;
			bool bScrollBarV = false;
			bool bKeepUpdating = true;

			while (bKeepUpdating && m_bEnableScrollbars)
			{
				bKeepUpdating = false;

				if (displayedImageSize.cx > windowSize.right && (!bScrollBarH))
				{
					bScrollBarH = true;
					bKeepUpdating = true;

					getClientAndScrollbarsRect(hWnd, &windowSize, bScrollBarH, bScrollBarV);
				}

				if (displayedImageSize.cy > windowSize.bottom && m_bEnableScrollbars && (!bScrollBarV))
				{
					bScrollBarV = true;
					bKeepUpdating = true;

					getClientAndScrollbarsRect(hWnd, &windowSize, bScrollBarH, bScrollBarV);
				}
			}

			// Adjust the offset if requested.

			if (NULL != pOffsetAdjust)
			{
				m_offset.cx += pOffsetAdjust->cx;
				m_offset.cy += pOffsetAdjust->cy;
			}

			keepOffsetInRange(&displayedImageSize, &windowSize);

			// Update the scrollbars we're going to keep to the new values.
			// ...Actually, always update the scrollbars with SetScrollInfo. This works around a bug we found in Vista:
			// http://groups.google.com/group/microsoft.public.win32.programmer.ui/browse_thread/thread/22b27405b24e2b79/d0405c0ef55bd91c?lnk=st&q=vista+scrollbar+wm_timer&rnum=1
			// ...Actually, that also causes issues. If the scrollbars are hidden they get painted every time SetScrollInfo is
			// called... So now we call SetScrollInfo if the scrollbar is currently visible (whether it is going to stay visible or not)
			// or about to be made visible.

			SCROLLINFO si;

			if (bScrollBarH || bCurrentH)
			{
				ZeroMemory(&si, sizeof(si));
				si.cbSize = sizeof(si);

				si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
				si.nMin   = 0;
				si.nMax   = displayedImageSize.cx - 1;
				si.nPage  = windowSize.right;
				si.nPos   = m_offset.cx;
				SetScrollInfo(hWnd, SB_HORZ, &si, bScrollBarH ? TRUE : FALSE);
			}

			if (bScrollBarV || bCurrentV)
			{
				ZeroMemory(&si, sizeof(si));
				si.cbSize = sizeof(si);

				si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
				si.nMin   = 0;
				si.nMax   = displayedImageSize.cy - 1;
				si.nPage  = windowSize.bottom;
				si.nPos   = m_offset.cy;
				SetScrollInfo(hWnd, SB_VERT, &si, bScrollBarV ? TRUE : FALSE);
			}

			// Show the scrollbars we need to turn on.

			if (bScrollBarH && bScrollBarV && !bCurrentH && !bCurrentV)
			{
				ShowScrollBar(hWnd, SB_BOTH, TRUE);
				bCurrentH = true;
				bCurrentV = true;
			}

			if (bScrollBarH && !bCurrentH)
			{
				ShowScrollBar(hWnd, SB_HORZ, TRUE);
				bCurrentH = true;
			}
			
			if (bScrollBarV)
			{
				ShowScrollBar(hWnd, SB_VERT, TRUE);
				bCurrentV = true;
			}

			// Hide the scrollbars we don't want.

			if (!bScrollBarH && !bScrollBarV && bCurrentH && bCurrentV)
			{
				ShowScrollBar(hWnd, SB_BOTH, FALSE);
				bCurrentH = false;
				bCurrentV = false;
			}
			
			if (!bScrollBarH && bCurrentH)
			{
				ShowScrollBar(hWnd, SB_HORZ, FALSE);
				bCurrentH = false;
			}
			
			if (!bScrollBarV && bCurrentV)
			{
				ShowScrollBar(hWnd, SB_VERT, FALSE);
				bCurrentV = false;
			}
		}

		InvalidateRect(hWnd, NULL, FALSE);

		m_bUpdateScrollbarsAndImageOffsetNoRecurse = false;
	}
}

void CImageViewer::onHScroll(HWND hWnd, WORD wRequest)
{
	RECT windowSize;
	SIZE displayedImageSize;

	if (GetClientRect(hWnd, &windowSize)
	&&	getDisplayedImageSize(&displayedImageSize))
	{
		SCROLLINFO si;

		ZeroMemory(&si, sizeof(si));
		si.cbSize = sizeof(si);

		LONG oldOffset = m_offset.cx;

		switch(wRequest)
		{
		default:
		case SB_ENDSCROLL:
			break;
		case SB_RIGHT:
			m_offset.cx = (displayedImageSize.cx - windowSize.right);
			break;
		case SB_LEFT:
			m_offset.cx = 0;
			break;

		case SB_PAGERIGHT:
			m_offset.cx += windowSize.right;
			break;
		case SB_PAGELEFT:
			m_offset.cx -= windowSize.right;
			break;

		case SB_LINERIGHT:
			m_offset.cx += (windowSize.right/4);
			break;
		case SB_LINELEFT:
			m_offset.cx -= (windowSize.right/4);
			break;

		case SB_THUMBPOSITION:
			si.fMask = SIF_POS;
			if (GetScrollInfo(hWnd, SB_HORZ, &si))
			{
				m_offset.cx = si.nPos;
			}
			break;

		case SB_THUMBTRACK:
			si.fMask = SIF_TRACKPOS;
			if (GetScrollInfo(hWnd, SB_HORZ, &si))
			{
				m_offset.cx = si.nTrackPos;
			}
			break;
		}

		keepOffsetInRange(&displayedImageSize, &windowSize);

		if (oldOffset != m_offset.cx)
		{
			InvalidateRect(hWnd, NULL, FALSE);
		}

		si.fMask = SIF_POS;
		si.nPos = m_offset.cx;

		SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
	}
}

void CImageViewer::onVScroll(HWND hWnd, WORD wRequest)
{
	RECT windowSize;
	SIZE displayedImageSize;

	if (GetClientRect(hWnd, &windowSize)
	&&	getDisplayedImageSize(&displayedImageSize))
	{
		SCROLLINFO si;

		ZeroMemory(&si, sizeof(si));
		si.cbSize = sizeof(si);

		LONG oldOffset = m_offset.cy;

		switch(wRequest)
		{
		default:
		case SB_ENDSCROLL:
			break;
		case SB_BOTTOM:
			m_offset.cy = (displayedImageSize.cy - windowSize.bottom);
			break;
		case SB_TOP:
			m_offset.cy = 0;
			break;

		case SB_PAGEDOWN:
			m_offset.cy += windowSize.bottom;
			break;
		case SB_PAGEUP:
			m_offset.cy -= windowSize.bottom;
			break;

		case SB_LINEDOWN:
			m_offset.cy += (windowSize.bottom/4);
			break;
		case SB_LINEUP:
			m_offset.cy -= (windowSize.bottom/4);
			break;

		case SB_THUMBPOSITION:
			si.fMask = SIF_POS;
			if (GetScrollInfo(hWnd, SB_VERT, &si))
			{
				m_offset.cy = si.nPos;
			}
			break;

		case SB_THUMBTRACK:
			si.fMask = SIF_TRACKPOS;
			if (GetScrollInfo(hWnd, SB_VERT, &si))
			{
				m_offset.cy = si.nTrackPos;
			}
			break;
		}

		keepOffsetInRange(&displayedImageSize, &windowSize);

		if (oldOffset != m_offset.cy)
		{
			InvalidateRect(hWnd, NULL, FALSE);
		}

		si.fMask = SIF_POS;
		si.nPos = m_offset.cy;

		SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
	}
}

// VPCAPABILITY_WANTMOUSEWHEEL is not set but the lister preview pane, unlike the
// viewer window, ignores it and always sends DVPLUGINMSG_MOUSEWHEEL messages.
// We take advantage of this. (In order to be consistent with the internal viewer.)
LRESULT CImageViewer::onDVPMouseWheel(HWND hWnd, WPARAM wParam)
{
	if (m_mouseDragging != IVM_NONE || m_mouseSelecting != IVM_NONE)
	{
		// Ignore the mouse wheel if dragging, for consistency with the Opus viewer.
		return 0;
	}

	m_iMouseWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);

	if (m_iMouseWheelDelta >= WHEEL_DELTA || m_iMouseWheelDelta <= -WHEEL_DELTA)
	{
		RECT windowSize;
		SIZE displayedImageSize;

		if (GetClientRect(hWnd, &windowSize)
		&&	getDisplayedImageSize(&displayedImageSize)
		&&	0 < windowSize.right
		&&	0 < windowSize.bottom)
		{
			LONG oldOffset = m_offset.cy;

			int iLines = LeoHelpers::MulDivRoundDown(m_iMouseWheelDelta, 30, WHEEL_DELTA);

			SIZE scrollOffset;

			// This is the same calculation that the internal Opus viewer uses.
			scrollOffset.cx = 0;
			scrollOffset.cy = -LeoHelpers::MulDivRoundDown(iLines, (displayedImageSize.cy - 1) + windowSize.bottom - 1, windowSize.bottom);

			updateScrollbarsAndImageOffset(hWnd, false, &scrollOffset, false);
		}

		m_iMouseWheelDelta = 0;
	}

	return 0;
}

void CImageViewer::getSelectOrScrollFromOpus(HWND hWnd, IVMouse mouseButton, bool *pbScroll, bool *pbSelect)
{
	*pbScroll = false;
	*pbSelect = false;

	DVPNMBUTTONOPTS notHdr;
	notHdr.hdr.hwndFrom = hWnd;
	notHdr.hdr.idFrom = 0;
	notHdr.hdr.code = DVPN_BUTTONOPTS;
	SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));

	if (IVM_LEFT == mouseButton)
	{
		bool bShift = (GetKeyState(VK_SHIFT)&0x8000) ? true : false;

		*pbScroll = bShift ? notHdr.iLeft == BUTTONOPT_SELECT : notHdr.iLeft == BUTTONOPT_SCROLL;
		*pbSelect = bShift ? notHdr.iLeft != BUTTONOPT_SELECT : notHdr.iLeft == BUTTONOPT_SELECT;
	}
	else if (IVM_MIDDLE == mouseButton)
	{
		*pbScroll = notHdr.iMiddle == BUTTONOPT_SCROLL;
		*pbSelect = notHdr.iMiddle == BUTTONOPT_SELECT;
	}
}

void CImageViewer::onButtonDown(HWND hWnd, int x, int y, IVMouse mouseButton)
{
	BOOL bSwallowed = FALSE;

	if (IVM_LEFT == mouseButton || IVM_MIDDLE == mouseButton)
	{
		RECT windowRect;

		if (GetClientRect(hWnd, &windowRect))
		{
			DVPNMCLICK notHdr;
			notHdr.hdr.hwndFrom = hWnd;
			notHdr.hdr.idFrom = 0;
			notHdr.hdr.code = (IVM_LEFT == mouseButton) ? DVPN_CLICK : DVPN_MCLICK;
			notHdr.pt.x = x;
			notHdr.pt.y = y;

			// Note: For either the menu bar or the control bar to appear in full-screen mode, they have
			// to be enabled in the viewer menu (i.e. appear in non-full-screen mode).

			if ( y < 2 ) // Changed from <=2 to <2 for consistency with Opus viewer.
			{
				notHdr.fMenu = 1; // Reveal menu if they click on the first two lines of pixels.
			}
			else if ( y > (windowRect.bottom - 4) ) // Changed from >=2 to >4 for consistency with Opus viewer.
			{
				notHdr.fMenu = -1; // Reveal control bar if they click on the last two lines of pixels.
			}
			else
			{
				notHdr.fMenu = 0;
			}

			bSwallowed = static_cast<BOOL>(SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));
		}
	}

	if ((!bSwallowed)
	&&	(IVM_LEFT == mouseButton || IVM_MIDDLE == mouseButton)
	&&	((IVM_NONE == m_mouseDragging) && (IVM_NONE == m_mouseSelecting)))
	{
		bool bScroll = false;
		bool bSelect = false;
		getSelectOrScrollFromOpus(hWnd, mouseButton, &bScroll, &bSelect);

		if (bScroll && m_bCanDrag && isCursorWithinImageArea(hWnd))
		{
			m_mouseDragStart.cx = x;
			m_mouseDragStart.cy = y;

			m_mouseDragging  = mouseButton;
			SetCapture(hWnd);
			onSetCursor(hWnd);
		}
		else if (bSelect)
		{
			m_mouseSelectStart.cx    = x;
			m_mouseSelectStart.cy    = y;

			m_mouseSelectRect.left   = x;
			m_mouseSelectRect.right  = x;
			m_mouseSelectRect.top    = y;
			m_mouseSelectRect.bottom = y;

			if (clientToImage(&m_mouseSelectRect))
			{
				m_mouseSelecting = mouseButton;
				SetCapture(hWnd);
				onSetCursor(hWnd);

				invalidateImage(hWnd);
			}
		}
	}
}

void CImageViewer::onButtonUp(HWND hWnd, int x, int y, IVMouse mouseButton)
{
	if (mouseButton == m_mouseDragging || mouseButton == m_mouseSelecting)
	{
		if (NULL != hWnd && hWnd == GetCapture())
		{
			ReleaseCapture();
		}

		onCaptureChanged(hWnd); // Call it whether or not we call ReleaseCapture, just in case.
	}
}

void CImageViewer::clearSelectionAndEndCapture(HWND hWnd)
{
	m_mouseSelectStart.cx    = 0;
	m_mouseSelectStart.cy    = 0;

	m_mouseSelectRect.left   = 0;
	m_mouseSelectRect.right  = 0;
	m_mouseSelectRect.top    = 0;
	m_mouseSelectRect.bottom = 0;

	m_mouseDragging  = IVM_NONE;
	m_mouseSelecting = IVM_NONE;

	if (NULL != hWnd && hWnd == GetCapture())
	{
		ReleaseCapture();
	}
}

void CImageViewer::onCaptureChanged(HWND hWnd)
{
	// May be called multiple times when the mouse button is released
	// or when something else takes the mouse capture.
	m_mouseDragStart.cx = 0;
	m_mouseDragStart.cy = 0;

	m_mouseSelectStart.cx = 0;
	m_mouseSelectStart.cy = 0;

	m_mouseDragging  = IVM_NONE;
	m_mouseSelecting = IVM_NONE;
}

void CImageViewer::onButtonDownMouseMove(HWND hWnd, int x, int y)
{
	RECT windowSize;
	SIZE displayedImageSize;

	if ((IVM_NONE != m_mouseDragging || IVM_NONE != m_mouseSelecting)
	&&	GetClientRect(hWnd, &windowSize)
	&&	getDisplayedImageSize(&displayedImageSize)
	&&	0 < windowSize.right
	&&	0 < windowSize.bottom)
	{
		if (IVM_NONE != m_mouseDragging)
		{
			SIZE dragOffset;

//			dragOffset.cx = LeoHelpers::MulDivRoundDown(m_mouseDragStart.cx - x, displayedImageSize.cx, windowSize.right);
//			dragOffset.cy = LeoHelpers::MulDivRoundDown(m_mouseDragStart.cy - y, displayedImageSize.cy, windowSize.bottom);

			// This is the same calculation that the internal Opus viewer uses.
			dragOffset.cx = LeoHelpers::MulDivRoundDown(m_mouseDragStart.cx - x, (displayedImageSize.cx - 1) + windowSize.right  - 1, windowSize.right);
			dragOffset.cy = LeoHelpers::MulDivRoundDown(m_mouseDragStart.cy - y, (displayedImageSize.cy - 1) + windowSize.bottom - 1, windowSize.bottom);

			updateScrollbarsAndImageOffset(hWnd, false, &dragOffset, false);

			m_mouseDragStart.cx = x;
			m_mouseDragStart.cy = y;
		}
		else if (IVM_NONE != m_mouseSelecting)
		{
			RECT mouseSelectRectClient;
			mouseSelectRectClient.left   = m_mouseSelectStart.cx;
			mouseSelectRectClient.top    = m_mouseSelectStart.cy;
			mouseSelectRectClient.right  = x;
			mouseSelectRectClient.bottom = y;

			normaliseRect(&m_mouseSelectRect, &mouseSelectRectClient);

			// Bump the bottom/right coordinates so that:
			// - If the rect goes left/up from the start point, the start point is included in the rect.
			// - If the rect goes right/down from the start point, the pointer location is included.
			// This means that the pixel clicked to start the selection is always included in the selection
			// and the cross-hair cursor always lines-up with the edges of the select-rect.
			// Note that this is *not* done as soon as the mouse pointer is pushed -- only when the mouse
			// moves afterwards -- so that it remains possible to click and release to clear the selection.
			++m_mouseSelectRect.right;
			++m_mouseSelectRect.bottom;

			clientToImage(&m_mouseSelectRect);
			
			invalidateImage(hWnd);
		}
	}
}

void CImageViewer::onDVPSelectAll(HWND hWnd)
{
	SIZE imageSize;

	if (getActualImageSize(&imageSize, false))
	{
		clearSelectionAndEndCapture(hWnd);

		SetRect(&m_mouseSelectRect, 0, 0, imageSize.cx, imageSize.cy);

		invalidateImage(hWnd);
	}
}

bool CImageViewer::onDVPTestSelection(HWND hWnd)
{
	return(IVM_NONE == m_mouseSelecting
		&& m_mouseSelectRect.left != m_mouseSelectRect.right
		&& m_mouseSelectRect.top  != m_mouseSelectRect.bottom);
}

void CImageViewer::onDVPCopySelection(HWND hWnd)
{
	if (!m_pCurrentFrames->IsEmpty())
	{
		CAbstractImage *pCurrentImage;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);
			pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
		}

		pCurrentImage->CopyToClipboard(hWnd, &m_mouseSelectRect, m_iDitherOffset);
	}
}

HBITMAP CImageViewer::onDVPGetBitmap(HWND hWnd)
{
	HBITMAP hbmResult = NULL;

	if (!m_pCurrentFrames->IsEmpty())
	{
		CAbstractImage *pCurrentImage;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);
			pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
		}

		HDC hDC = GetDC(hWnd);

		if (NULL != hDC)
		{
			hbmResult = pCurrentImage->CreateDIBSection(hDC, &m_mouseSelectRect, m_iDitherOffset);

			ReleaseDC(hWnd, hDC);
		}
	}

	return(hbmResult);
}

void CImageViewer::onDVPSetDesktopWallpaper(HWND hWnd, const TCHAR *szMode)
{
	if (!m_pCurrentFrames->IsEmpty())
	{
		CAbstractImage *pCurrentImage;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);
			pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
		}

		DWORD dwWallpaperStyle = WPSTYLE_CENTER;

		if (szMode != NULL)
		{
			if (0 == _tcsicmp(szMode, _T("tile")))
			{
				dwWallpaperStyle = WPSTYLE_TILE;
			}
			else if (0 == _tcsicmp(szMode, _T("stretch")))
			{
				dwWallpaperStyle = WPSTYLE_STRETCH;
			}
		}

		pCurrentImage->SetDesktopWallpaper(hWnd, NULL, m_iDitherOffset, dwWallpaperStyle);
	}
}

void CImageViewer::onSetCursor(HWND hWnd)
{
	bool bWantHandOpen   = false;
	bool bWantHandClosed = false;
	bool bWantCrosshair  = false;

	if (IVM_NONE != m_mouseDragging)
	{
		bWantHandClosed = true;
	}
	else if (IVM_NONE != m_mouseSelecting)
	{
		bWantCrosshair = true;
	}
	else
	{
		POINT pos;
		RECT clientRect;

		if (GetCursorPos(&pos) && GetClientRect(hWnd, &clientRect))
		{
			ScreenToClient(hWnd, &pos);

			if (isWithinImageArea(hWnd, pos.x, pos.y))
			{
				DVPNMSETCURSOR notHdr;
				notHdr.hdr.hwndFrom = hWnd;
				notHdr.hdr.idFrom = 0;
				notHdr.hdr.code = DVPN_SETCURSOR;
				notHdr.fCanScroll = m_bCanDrag ? TRUE : FALSE;
				notHdr.pt.x = pos.x;
				notHdr.pt.y = pos.y;
				notHdr.iCursor = VPCURSOR_NONE;

				// Note: For either the menu bar or the control bar to appear in full-screen mode, they have
				// to be enabled in the viewer menu (i.e. appear in non-full-screen mode).

				if ( pos.y < 2 ) // Changed from <=2 to <2 for consistency with Opus viewer.
				{
					notHdr.fMenu = 1; // Reveal menu if they click on the first two lines of pixels.
				}
				else if ( pos.y > (clientRect.bottom - 4) ) // Changed from >=2 to >4 for consistency with Opus viewer.
				{
					notHdr.fMenu = -1; // Reveal control bar if they click on the last two lines of pixels.
				}
				else
				{
					notHdr.fMenu = 0;
				}

				SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));

				if (VPCURSOR_DRAG == notHdr.iCursor)
				{
					bWantHandOpen = true;
				}
				else if (VPCURSOR_SELECT == notHdr.iCursor)
				{
					bWantCrosshair = true;
				}
			}
		}
	}

	HCURSOR hcur = NULL;

	if (bWantHandOpen || bWantHandClosed || bWantCrosshair)
	{
		DVPNMGETCURSORS notHdr;
		notHdr.hdr.hwndFrom = hWnd;
		notHdr.hdr.idFrom = 0;
		notHdr.hdr.code = DVPN_GETCURSORS;
		notHdr.hCurHandOpen = NULL;
		notHdr.hCurHandClosed = NULL;
		notHdr.hCurCrosshair = NULL;

		SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr));

		if (bWantHandOpen)
		{
			hcur = notHdr.hCurHandOpen;
		}
		else if (bWantHandClosed)
		{
			hcur = notHdr.hCurHandClosed;
		}
		else if (bWantCrosshair)
		{
			hcur = notHdr.hCurCrosshair;
		}
	}

	if (hcur == NULL)
	{
		hcur = ::LoadCursor(NULL, IDC_ARROW);
	}

	::SetCursor( hcur );
}

COLORREF CImageViewer::onDVPGetAutoBGCol(HWND hWnd)
{
	return(m_pCurrentFrames->IsEmpty() ? 0 : m_pCurrentFrames->GetFirstFrame()->GetAutomaticBackgroundColor());
}

LRESULT CImageViewer::onDVPIsAlphaHidden(HWND hWnd, BOOL *pbIsHidden)
{
	if (!m_pFramesFrames->GetHasUsedTransparency())
	{
		return FALSE; // no alpha channel
	}

	*pbIsHidden = (m_bIsAlphaHidden ? TRUE : FALSE);
	return TRUE; // alpha channel
}

void CImageViewer::onDVPHideAlpha(HWND hWnd, bool bHide)
{
	if (m_pCurrentFrames->GetHasUsedTransparency())
	{
		if (bHide)
		{
			m_pFramesFrames->HideAlphaChannel();
			m_framesFlat.HideAlphaChannel();
			m_framesTiled.HideAlphaChannel();
			if (m_pRestoreFrames != NULL)
			{
				m_pRestoreFrames->HideAlphaChannel();
			}
		}
		else
		{
			NMHDR notHdr;
			notHdr.hwndFrom = hWnd;
			notHdr.idFrom = 0;
			notHdr.code = DVPN_GETBGCOL;
			COLORREF crefViewerBackground = static_cast<COLORREF>(SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));

			m_pFramesFrames->UpdateViewerBackgroundColor(crefViewerBackground);
			m_framesFlat.UpdateViewerBackgroundColor(crefViewerBackground);
			m_framesTiled.UpdateViewerBackgroundColor(crefViewerBackground);
			if (m_pRestoreFrames != NULL)
			{
				m_pRestoreFrames->UpdateViewerBackgroundColor(crefViewerBackground);
			}
		}

		m_bIsAlphaHidden = bHide;

		invalidateImage(hWnd);
	}
}

LRESULT CImageViewer::onDVPCropSelection(HWND hWnd, bool bTest)
{
	if (!onDVPTestSelection(hWnd) || m_pCurrentFrames->IsEmpty())
	{
		return FALSE;
	}

	if (bTest)
	{
		return TRUE;
	}

	CAbstractImage *pCurrentImage;

	{
		LeoHelpers::CriticalSectionScoper css(&m_cs);
		pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
	}

	if (pCurrentImage != NULL && !m_pFramesFrames->IsEmpty())
	{
		// We use the type name of the first m_pFramesFrames image because only the images in m_pFramesFrames
		// are guaranteed to have type names.
		CAbstractImage *pNewImage = new CMemoryImage(pCurrentImage, &m_mouseSelectRect, m_pFramesFrames->GetFirstFrame()->GetImageTypeName(&m_sl));

		if (!pNewImage->InitOK())
		{
			delete pNewImage;
		}
		else
		{
			CAbstractImageList *pImageList = new CAbstractImageList();

			pImageList->AddFrame(pNewImage, true);
			pNewImage = NULL;

			return load(hWnd, NULL, NULL, pImageList);
		}
	}

	return FALSE;
}

LRESULT CImageViewer::onDVPRestore(HWND hWnd, bool bTest)
{
	if (m_pRestoreFrames == NULL || m_pRestoreFrames->IsEmpty())
	{
		return FALSE;
	}

	if (bTest)
	{
		return TRUE;
	}

	return load(hWnd, NULL, NULL, m_pRestoreFrames);
}

void CImageViewer::onDVPRedraw(HWND hWnd, const COLORREF *pcrNewBackground)
{
	NMHDR notHdr;
	notHdr.hwndFrom = hWnd;
	notHdr.idFrom = 0;
	notHdr.code = DVPN_GETBGCOL;
	COLORREF crefViewerBackground = static_cast<COLORREF>(SendMessage(GetParent(hWnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));

	pcrNewBackground = &crefViewerBackground; // Ignore the one that was passed in.

	if (pcrNewBackground != NULL && !m_bIsAlphaHidden)
	{
		m_pFramesFrames->UpdateViewerBackgroundColor(*pcrNewBackground);
		m_framesFlat.UpdateViewerBackgroundColor(*pcrNewBackground);
		m_framesTiled.UpdateViewerBackgroundColor(*pcrNewBackground);
		if (m_pRestoreFrames != NULL)
		{
			m_pRestoreFrames->UpdateViewerBackgroundColor(*pcrNewBackground);
		}
	}

	InvalidateRect(hWnd, NULL, FALSE);
}

void CImageViewer::onDVPGetImageInfo(HWND hWnd, VIEWERPLUGINFILEINFO *pInfo)
{
	pInfo->dwFlags = DVPFIF_CanReturnThumbnail | DVPFIF_CanReturnViewer;
	pInfo->wMajorType = DVPMajorType_Image;
	pInfo->wMinorType = 0;

	pInfo->szImageSize = m_originalImageSize;
	pInfo->iNumBits = m_iOriginalBitDepth;
	pInfo->iColorSpace = DVPColorSpace_RGB;

	// We use the type name of the first m_pFramesFrames image because only the images in m_pFramesFrames
	// are guaranteed to have type names.
	LeoHelpers::WriteFileInfoInfoLine(pInfo, m_pFramesFrames->GetFirstFrame()->GetImageTypeName(&m_sl));
}

void CImageViewer::onButtonPlay(HWND hWnd, bool bForwards)
{
	CGifTlsData *pTlsData = getGifViewerTlsData();
	if (NULL != pTlsData)
	{
		pTlsData->SetHWnd(GetParent(GetParent(hWnd)));

		if (bForwards)
		{
			pTlsData->SetPlayForwards();
		}
		else
		{
			pTlsData->SetPlayBackwards();
		}
	}

	stopThread();

	if (!m_pFramesFrames->IsEmpty())
	{
		SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, bForwards ? IVC_PLAY : IVC_PLAYBACKWARDS, 0);

		if (!m_framesFlat.IsEmpty())
		{
			clearSelectionAndEndCapture(hWnd);
			m_iMouseWheelDelta = 0;

			m_framesFlat.Clear();
			m_pCurrentFrames = m_pFramesFrames;
			m_pLastDrawnImage = NULL;

			m_pCurrentFrames->SetCurrentIndex(0);

			if (!bForwards)
			{
				m_pCurrentFrames->IncrementCurrentIndex(-1);
			}

			SendMessage(GetParent(hWnd), TBM_SETRANGE, TRUE, MAKELONG(1, m_pFramesFrames->GetNumberOfFrames()));
			LONG ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
			PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);

			if (ZOOM_TILED == m_currentZoomFactor)
			{
				generateTiledImage(hWnd);
			}

			updateScrollbarsAndImageOffset(hWnd, true, NULL, false);
			sendSizeChange(hWnd, false);
		}

		m_bBackwards = (!bForwards);

		precacheFrames(hWnd, false);

		startThread();
	}
}

void CImageViewer::onButtonPause(HWND hWnd)
{
	CGifTlsData *pTlsData = getGifViewerTlsData();
	if (NULL != pTlsData)
	{
		pTlsData->SetHWnd(GetParent(GetParent(hWnd)));
		pTlsData->SetPause();
	}

	stopThread();

	if (!m_pFramesFrames->IsEmpty())
	{
		SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_PAUSE, 0);

		if (!m_framesFlat.IsEmpty())
		{
			clearSelectionAndEndCapture(hWnd);
			m_iMouseWheelDelta = 0;

			m_framesFlat.Clear();
			m_pCurrentFrames = m_pFramesFrames;
			m_pLastDrawnImage = NULL;

			m_pCurrentFrames->SetCurrentIndex(0);

			SendMessage(GetParent(hWnd), TBM_SETRANGE, TRUE, MAKELONG(1, m_pFramesFrames->GetNumberOfFrames()));
			LONG ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
			PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);

			if (ZOOM_TILED == m_currentZoomFactor)
			{
				generateTiledImage(hWnd);
			}

			updateScrollbarsAndImageOffset(hWnd, true, NULL, false);
			sendSizeChange(hWnd, false);
		}
	}
}

void CImageViewer::onButtonFrame(HWND hWnd, bool bForwards)
{
	CGifTlsData *pTlsData = getGifViewerTlsData();
	if (NULL != pTlsData)
	{
		pTlsData->SetHWnd(GetParent(GetParent(hWnd)));
		pTlsData->SetPause();
	}

	stopThread();

	if (!m_pFramesFrames->IsEmpty())
	{
		if (!m_framesFlat.IsEmpty())
		{
			clearSelectionAndEndCapture(hWnd);
			m_iMouseWheelDelta = 0;

			m_framesFlat.Clear();
			m_pCurrentFrames = m_pFramesFrames;
			m_pLastDrawnImage = NULL;

			m_pCurrentFrames->SetCurrentIndex(0);

			if (ZOOM_TILED == m_currentZoomFactor)
			{
				generateTiledImage(hWnd);
			}

			updateScrollbarsAndImageOffset(hWnd, true, NULL, false);
			sendSizeChange(hWnd, false);
		}
		else
		{
			m_pCurrentFrames->IncrementCurrentIndex(bForwards ? 1 : -1);
			invalidateImage(hWnd);
		}

		SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, IVC_PAUSE, 0);

		SendMessage(GetParent(hWnd), TBM_SETRANGE, TRUE, MAKELONG(1, m_pFramesFrames->GetNumberOfFrames()));
		LONG ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
		PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);
	}
}

void CImageViewer::onButtonFlatten(HWND hWnd, bool bFitToWindow, bool bHorizontal)
{
	CGifTlsData *pTlsData = getGifViewerTlsData();
	if (NULL != pTlsData)
	{
		pTlsData->SetHWnd(GetParent(GetParent(hWnd)));

		if (bFitToWindow)
		{
			pTlsData->SetFlattenFitToWindow();
		}
		else if (bHorizontal)
		{
			pTlsData->SetFlattenHorizontal();
		}
		else
		{
			pTlsData->SetFlattenVertical();
		}
	}

	if (!m_pFramesFrames->IsEmpty())
	{
		m_bFlattenFitToWindow = bFitToWindow;
		m_bFlattenHorizontal  = bHorizontal;

		generateFlattenedImage(hWnd, false, true);

		updateScrollbarsAndImageOffset(hWnd, true, NULL, false);

		SendMessage(GetParent(hWnd), WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON, bFitToWindow ? IVC_FLATTENFIT : (bHorizontal ? IVC_FLATTENHORIZ : IVC_FLATTENVERT), 0);

		SendMessage(GetParent(hWnd), TBM_SETRANGE, TRUE, MAKELONG(1, m_pFramesFrames->GetNumberOfFrames()));
		LONG ilTrackpos = m_pCurrentFrames->GetCurrentIndex() + 1;
		PostMessage(GetParent(hWnd), TBM_SETPOS, TRUE, ilTrackpos);
	}
}

DVPCONTEXTMENUITEM *CImageViewer::onAddContextMenu(DWORD *pdwNumItems)
{
	m_contextMenuItems[0].lpszLabel = const_cast<wchar_t *>(m_sl.Get(STR_GIFANIM_MENU_DITHERED_IMAGES));
	m_contextMenuItems[0].dwFlags = DVPCMF_BEGINSUBMENU;
	m_contextMenuItems[0].uID = 0;

	m_contextMenuItems[1].lpszLabel = const_cast<wchar_t *>(m_sl.Get(STR_GIFANIM_MENU_SHOW_ALL_PIXELS));
	m_contextMenuItems[1].dwFlags = (0 == m_iDitherOffset ? DVPCMF_CHECKED : 0) | DVPCMF_RADIOCHECK;
	m_contextMenuItems[1].uID = IVC_DITHERIMAGEALL;

	m_contextMenuItems[2].lpszLabel = const_cast<wchar_t *>(m_sl.Get(STR_GIFANIM_MENU_SHOW_EVEN_PIXELS));
	m_contextMenuItems[2].dwFlags = (1 == m_iDitherOffset ? DVPCMF_CHECKED : 0) | DVPCMF_RADIOCHECK;
	m_contextMenuItems[2].uID = IVC_DITHERIMAGEONE;

	m_contextMenuItems[3].lpszLabel = const_cast<wchar_t *>(m_sl.Get(STR_GIFANIM_MENU_SHOW_ODD_PIXELS));
	m_contextMenuItems[3].dwFlags = (2 == m_iDitherOffset ? DVPCMF_CHECKED : 0) | DVPCMF_RADIOCHECK | DVPCMF_ENDSUBMENU;
	m_contextMenuItems[3].uID = IVC_DITHERIMAGETWO;

	*pdwNumItems = 4;

	return(m_contextMenuItems);
}

void CImageViewer::onButtonDitherImage(HWND hWnd, int iCmd)
{
	switch(iCmd)
	{
	default:
	case IVC_DITHERIMAGEALL:
		m_iDitherOffset = 0;
		break;
	case IVC_DITHERIMAGEONE:
		m_iDitherOffset = 1;
		break;
	case IVC_DITHERIMAGETWO:
		m_iDitherOffset = 2;
		break;
	}

	precacheFrames(hWnd, true);

	invalidateImage(hWnd);
}

void CImageViewer::onDVPPrint(HWND hWnd)
{
	bool bResult = false;

	if (!m_pCurrentFrames->IsEmpty())
	{
		CAbstractImage *pCurrentImage;

		{
			LeoHelpers::CriticalSectionScoper css(&m_cs);
			pCurrentImage = m_pCurrentFrames->GetCurrentFrame();
		}

		PRINTDLG pd;
		ZeroMemory(&pd, sizeof(pd));

		pd.lStructSize = sizeof(pd);
		pd.hwndOwner = hWnd;
		pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
		pd.nCopies     = 1;
		pd.nFromPage   = 1;
		pd.nToPage     = 1;
		pd.nMinPage    = 1;
		pd.nMaxPage    = 1;

		if (!onDVPTestSelection(hWnd))
		{
			pd.Flags |= PD_NOSELECTION;
		}

		if (PrintDlg(&pd))
		{
			if (NULL != pd.hDevMode)  { GlobalFree(pd.hDevMode);  }
			if (NULL != pd.hDevNames) { GlobalFree(pd.hDevNames); }

			if (NULL != pd.hDC)
			{
				DOCINFO docInfo;
				docInfo.cbSize			= sizeof(docInfo);
				docInfo.lpszDocName		= (NULL != m_szName) ? LeoHelpers::GetLastPathPart(m_szName) : m_sl.Get(STR_GIFANIM_GIF_IMAGE);
				docInfo.lpszOutput		= NULL;
				docInfo.lpszDatatype	= NULL;
				docInfo.fwType			= 0;

				if (0 < StartDoc(pd.hDC, &docInfo))
				{
					if (0 < StartPage(pd.hDC))
					{
						if (pCurrentImage->Print(pd.hDC, hWnd, (pd.Flags&PD_SELECTION) ? &m_mouseSelectRect : NULL, m_iDitherOffset, &m_sl))
						{
							bResult = true;
						}

						EndPage(pd.hDC);
					}

					if (!bResult)
					{
						AbortDoc(pd.hDC);
					}
					else
					{
						EndDoc(pd.hDC);
					}
				}

				DeleteDC(pd.hDC);
			}
		}
	}
}

void CImageViewer::generateFlattenedImage(HWND hWnd, bool bOnlyIfColsChanged, bool bResizeWindow)
{
	if (!m_bGenerateFlattenedImageNoRecurse)
	{
		//logLine(_T("GenFlat: Start\n"));

		// If we resize the window and are already tile-flattened then an unwanted recursive call may result.
		m_bGenerateFlattenedImageNoRecurse = true;

		stopThread();

		if (!m_pFramesFrames->IsEmpty())
		{
			// m_pFramesFrames->DeleteCachedPaintBitmaps(); Don't want this anymore as the flattened image may use them.

			if (bResizeWindow && !m_bIsPreview && !m_bFullScreen && ZOOM_TILED != m_currentZoomFactor)
			{
				// Work out the ideal window size.

				SIZE sizeDesired;
				sizeDesired.cx = 0;
				sizeDesired.cy = 0;

				//logLine(_T("GenFlat: Want to resize...\n"));

				if (m_bFlattenFitToWindow)
				{
					//logLine(_T("GenFlat: FlattenFitToWindow\n"));

					SIZE sizeMaxClient;

					if (getMaxClientSize(hWnd, &sizeMaxClient))
					{
						SIZE imageSize = { m_pFramesFrames->GetFirstFrame()->GetWidth(), m_pFramesFrames->GetFirstFrame()->GetHeight() };

						//logLine(_T("GenFlat: Frame size %ld x %ld\n"), imageSize.cx, imageSize.cy);

						bool bResultantScrollbarH = false;
						bool bResultantScrollbarV = false;

						int iColumns = getFlattenFitNumberOfColumns(m_pFramesFrames->GetNumberOfFrames(), &imageSize, &sizeMaxClient, m_currentZoomFactor, m_bFrameImage, m_bEnableScrollbars, &bResultantScrollbarH, &bResultantScrollbarV);

						int iRows = (m_pFramesFrames->GetNumberOfFrames() + (iColumns - 1)) / iColumns;

						//logLine(_T("GenFlat: Want %d columns and %d rows\n"), iColumns, iRows);

						sizeDesired.cx = imageSize.cx * iColumns;
						sizeDesired.cy = imageSize.cy * iRows;

						//logLine(_T("GenFlat: Raw size %ld x %ld\n"), sizeDesired.cx, sizeDesired.cy);

						SIZE sizeBorders;

						if (m_pFramesFrames->GetFirstFrame()->WantBorders()
						&&	m_pFramesFrames->GetFirstFrame()->GetTotalBorderSize(iColumns, iRows, &sizeBorders))
						{
							sizeDesired.cx += sizeBorders.cx;
							sizeDesired.cy += sizeBorders.cy;
						}

						//logLine(_T("GenFlat: Bordered size %ld x %ld\n"), sizeDesired.cx, sizeDesired.cy);

						if (ZOOM_FITPAGE != m_currentZoomFactor && ZOOM_TILED != m_currentZoomFactor
						&&	0 != m_currentZoomFactor && 100 != m_currentZoomFactor)
						{
							sizeDesired.cx = LeoHelpers::MulDivRoundDown(sizeDesired.cx, m_currentZoomFactor, 100);
							sizeDesired.cy = LeoHelpers::MulDivRoundDown(sizeDesired.cy, m_currentZoomFactor, 100);
						}

						//logLine(_T("GenFlat: Zoomed size %ld x %ld\n"), sizeDesired.cx, sizeDesired.cy);

						// Should not be needed. Opus does this if the image is too large.
						//if (bResultantScrollbarH)
						//{
						//	sizeDesired.cy += GetSystemMetrics(SM_CYHSCROLL);
						//}
						//
						//if (bResultantScrollbarV)
						//{
						//	sizeDesired.cx += GetSystemMetrics(SM_CXVSCROLL);
						//}
					}
				}
				else
				{
					//logLine(_T("GenFlat: NOT FlattenFitToWindow\n"));

					int iColumns = (m_bFlattenHorizontal ? m_pFramesFrames->GetNumberOfFrames() : 1);
					int iRows    = (m_bFlattenHorizontal ? 1 : m_pFramesFrames->GetNumberOfFrames());

					sizeDesired.cx = m_pFramesFrames->GetFirstFrame()->GetWidth() * iColumns;
					sizeDesired.cy = m_pFramesFrames->GetFirstFrame()->GetHeight() * iRows;

					SIZE sizeBorders;

					if (m_pFramesFrames->GetFirstFrame()->WantBorders()
					&&	m_pFramesFrames->GetFirstFrame()->GetTotalBorderSize(iColumns, iRows, &sizeBorders))
					{
						sizeDesired.cx += sizeBorders.cx;
						sizeDesired.cy += sizeBorders.cy;
					}

					if (ZOOM_FITPAGE != m_currentZoomFactor && ZOOM_TILED != m_currentZoomFactor
					&&	0 != m_currentZoomFactor && 100 != m_currentZoomFactor)
					{
						sizeDesired.cx = LeoHelpers::MulDivRoundDown(sizeDesired.cx, m_currentZoomFactor, 100);
						sizeDesired.cy = LeoHelpers::MulDivRoundDown(sizeDesired.cy, m_currentZoomFactor, 100);
					}
				}

				if (0 < sizeDesired.cx && 0 < sizeDesired.cy)
				{
					//logLine(_T("GenFlat: Requesting %ld x %ld\n"), sizeDesired.cx, sizeDesired.cy);

					if (ZOOM_FITPAGE != m_currentZoomFactor
					||	!setDisplayedImageSizeToMaxFitToPage(hWnd, &sizeDesired))
					{
						m_displayedImageSize.cx = sizeDesired.cx;
						m_displayedImageSize.cy = sizeDesired.cy;
					}

					sendSizeChange(hWnd, true);

					m_displayedImageSize.cx = 0;
					m_displayedImageSize.cy = 0;
				}

				//logLine(_T("GenFlat: Resize stuff finished.\n"));
			}
			else
			{
				//logLine(_T("GenFlat: Not resizing\n"));
			}

			// Work out the number of columns based on the current window size.
			// Note that our SizeChange message is ignored in situations where the viewer window size is fixed.

			int iFlattenFitColumns = 0; // in case of failure, specify unlimited columns.

			if (m_bFlattenFitToWindow)
			{
				RECT clientRect;

				if (getClientAndScrollbarsRect(hWnd, &clientRect, false, false))
				{
					//logLine(_T("GenFlat(2): ClientRect: %ld, %ld, %ld, %ld\n"), clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);

					SIZE imageSize = { m_pFramesFrames->GetFirstFrame()->GetWidth(), m_pFramesFrames->GetFirstFrame()->GetHeight() };
					SIZE clientSize = { clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };

					//logLine(_T("GenFlat(2): ClientSize: %ld, %ld\n"), clientSize.cx, clientSize.cy);
					//logLine(_T("GenFlat(2): Frame size %ld x %ld\n"), imageSize.cx, imageSize.cy);

					bool bResultantScrollbarH = false;
					bool bResultantScrollbarV = false;

					iFlattenFitColumns = getFlattenFitNumberOfColumns(m_pFramesFrames->GetNumberOfFrames(), &imageSize, &clientSize, m_currentZoomFactor, false, m_bEnableScrollbars, &bResultantScrollbarH, &bResultantScrollbarV);

					//logLine(_T("GenFlat(2): Want %d columns\n"), iFlattenFitColumns);
				}
			}
			else if (m_bFlattenHorizontal)
			{
				//logLine(_T("GenFlat(2): NOT FlattenFitToWindow\n"));

				iFlattenFitColumns = m_pFramesFrames->GetNumberOfFrames();
			}
			else
			{
				//logLine(_T("GenFlat(2): NOT FlattenFitToWindow\n"));
				iFlattenFitColumns = 1;
			}

			// Generate the new flattened image if it should be generated.

			if ((!bOnlyIfColsChanged) || m_iFlattenFitColumns != iFlattenFitColumns || m_framesFlat.IsEmpty()) //m_pCurrentFrames != &m_framesFlat)
			{
				//logLine(_T("GenFlat(3): Going ahead\n"));

				clearSelectionAndEndCapture(hWnd);
				m_iMouseWheelDelta = 0;

				m_iFlattenFitColumns = iFlattenFitColumns;

				m_pFramesFrames->DeleteCachedPaintBitmaps(NULL, NULL);
				m_framesFlat.Clear();
				m_framesTiled.Clear();

				m_pCurrentFrames = m_pFramesFrames;	// Required for the DVPN_GETBGCOL call, plus in case of error below.
				m_pCurrentFrames->SetCurrentIndex(0);
				m_pLastDrawnImage = NULL;

				CFlatImage *pFlatImage = new CFlatImage(m_pFramesFrames, m_iFlattenFitColumns, m_pFramesFrames->GetFirstFrame()->GetScaleMode(), m_bFrameImage, m_pConfig->GetOpusPluginHelper());

				if (!pFlatImage->InitOK())
				{
					delete pFlatImage;
					MessageBeep(MB_ICONHAND);
				}
				else
				{
					m_framesFlat.AddFrame(pFlatImage, true);

					m_pCurrentFrames = &m_framesFlat;
					m_pCurrentFrames->SetCurrentIndex(0);
					m_pLastDrawnImage = NULL;
				}

				if (ZOOM_TILED == m_currentZoomFactor)
				{
					generateTiledImage(hWnd);
				}
			}
			else
			{
				//logLine(_T("GenFlat(3): Not doing anything\n"));
			}
		}

		//logLine(_T("GenFlat: End\n"));

		m_bGenerateFlattenedImageNoRecurse = false;
	}
}

// Size should be for window with scrollbars turned off.
int CImageViewer::getFlattenFitNumberOfColumns(int iNumFrames, const SIZE *pImageSize, const SIZE *pWindowSize, int iZoomFactor, bool bFrameImage, bool bEnableScrollbars, bool *pbResultantScrollbarH, bool *pbResultantScrollbarV)
{
	int iResult = 1;

	*pbResultantScrollbarH = false;
	*pbResultantScrollbarV = false;

	if (0 != iNumFrames && NULL != pWindowSize && 0 < pWindowSize->cx && 0 < pWindowSize->cy)
	{
		SIZE effectiveWindowSize;
		effectiveWindowSize.cx = pWindowSize->cx;
		effectiveWindowSize.cy = pWindowSize->cy;

		if (ZOOM_FITPAGE == iZoomFactor)
		{
			adjustSizeForFrame(&effectiveWindowSize);

			// Find out the number of columns which gives us the largest images.
			// In the event of a tie, pick the one which also results in the least left-over empty space.

			int iBestNumCols = 1;
			int iBestWastedSpaces = 0;
			LONG ilBestResultantFrameWidth = 0;

			bool bBorders = m_pFramesFrames->GetFirstFrame()->WantBorders();

			for (int iNumCols = 1; iNumCols <= iNumFrames; ++iNumCols)
			{
				int iNumRows = (iNumFrames + (iNumCols - 1)) / iNumCols;
				int iWastedSpaces = (iNumCols - (iNumFrames % iNumCols)) % iNumCols;

				SIZE sizeFlat = *pImageSize;
				sizeFlat.cx *= iNumCols;
				sizeFlat.cy *= iNumRows;

				SIZE sizeBorders;

				if (bBorders
				&&	m_pFramesFrames->GetFirstFrame()->GetTotalBorderSize(iNumCols, iNumRows, &sizeBorders))
				{
					sizeFlat.cx += sizeBorders.cx;
					sizeFlat.cy += sizeBorders.cy;
				}

				LONG ilResultantFrameWidth = pImageSize->cx;

				if (sizeFlat.cx > effectiveWindowSize.cx || sizeFlat.cy > effectiveWindowSize.cy)
				{
					if (LeoHelpers::MulDivRoundDown(sizeFlat.cy, effectiveWindowSize.cx, sizeFlat.cx) <= effectiveWindowSize.cy)
					{
						ilResultantFrameWidth = LeoHelpers::MulDivRoundDown(ilResultantFrameWidth, effectiveWindowSize.cx, sizeFlat.cx);
					}
					else
					{
						ilResultantFrameWidth = LeoHelpers::MulDivRoundDown(ilResultantFrameWidth, effectiveWindowSize.cy, sizeFlat.cy);
					}
				}

				if ( ilBestResultantFrameWidth <  ilResultantFrameWidth
				||	(ilBestResultantFrameWidth == ilResultantFrameWidth && iBestWastedSpaces >= iWastedSpaces))
				{
					ilBestResultantFrameWidth = ilResultantFrameWidth;
					iBestWastedSpaces = iWastedSpaces;
					iBestNumCols = iNumCols;
				}
			}

			iResult = iBestNumCols;
		}
		else
		{
			bool bBorders = m_pFramesFrames->GetFirstFrame()->WantBorders();

			for (int i = 0; i < 2; i++)
			{
				if (1 == i)
				{
					if (!bEnableScrollbars)
					{
						break;
					}

					// We did this once and ended up requring a vertical scrollbar.
					// Remove the size that will take up and do it a second time.
					effectiveWindowSize.cx -= GetSystemMetrics(SM_CXVSCROLL);
					*pbResultantScrollbarV = true;
				}

				int iNormalisedZoomFactor = 100;

				if (0 != iZoomFactor && 100 != iZoomFactor && ZOOM_TILED != iZoomFactor)
				{
					iNormalisedZoomFactor = iZoomFactor;
				}

				iResult = m_pFramesFrames->GetFirstFrame()->CalcNumFramesFitHoriz(effectiveWindowSize.cx, iNormalisedZoomFactor, bBorders);

				if (0 >= iResult)
				{
					iResult = 1;
				}
				else if (iResult > iNumFrames)
				{
					iResult = iNumFrames;
				}

				int iNumRows = (iNumFrames + (iResult - 1)) / iResult;

				int iResultantWidth  = pImageSize->cx * iResult;
				int iResultantHeight = pImageSize->cy * iNumRows;

				SIZE sizeBorders;

				if (bBorders
				&&	m_pFramesFrames->GetFirstFrame()->GetTotalBorderSize(iResult, iNumRows, &sizeBorders))
				{
					iResultantWidth  += sizeBorders.cx;
					iResultantHeight += sizeBorders.cy;
				}

				if (iNormalisedZoomFactor != 100)
				{
					iResultantWidth  = LeoHelpers::MulDivRoundDown(iResultantWidth,  iNormalisedZoomFactor, 100);
					iResultantHeight = LeoHelpers::MulDivRoundDown(iResultantHeight, iNormalisedZoomFactor, 100);
				}

				if (bEnableScrollbars && iResultantWidth > effectiveWindowSize.cx && !*pbResultantScrollbarH)
				{
					effectiveWindowSize.cy -= GetSystemMetrics(SM_CYHSCROLL);
					*pbResultantScrollbarH = true;
				}

				if (iResultantHeight <= effectiveWindowSize.cy)
				{
					int iBestNumCols = iResult;
					int iBestWastedSpaces = (iResult - (iNumFrames % iResult)) % iResult;
					int iWastedSpaces;

					// We haven't filled up the window so check if there are other layouts which also fit but waste fewer spaces.
					for (int iNumCols = iResult-1; iNumCols > 0 && iBestWastedSpaces != 0; --iNumCols)
					{
						iNumRows = (iNumFrames + (iNumCols - 1)) / iNumCols;

						iResultantHeight = pImageSize->cy * iNumRows;

						if (bBorders
						&&	m_pFramesFrames->GetFirstFrame()->GetTotalBorderSize(iNumCols, iNumRows, &sizeBorders))
						{
							iResultantHeight += sizeBorders.cy;
						}

						if (iNormalisedZoomFactor != 100)
						{
							iResultantHeight = LeoHelpers::MulDivRoundDown(iResultantHeight, iNormalisedZoomFactor, 100);
						}

						if (iResultantHeight > effectiveWindowSize.cy)
						{
							break; // Fewer columns will result in scrollbars so stop searching.
						}

						iWastedSpaces = (iNumCols - (iNumFrames % iNumCols)) % iNumCols;

						if (iBestWastedSpaces >= iWastedSpaces)
						{
							iBestWastedSpaces = iWastedSpaces;
							iBestNumCols = iNumCols;
						}
					}

					iResult = iBestNumCols;

					// Break the for-loop since we didn't trigger a vertical scrollbar.
					break;
				}
			}
		}
	}

	return(iResult);
}

void CImageViewer::generateTiledImage(HWND hWnd)
{
	clearSelectionAndEndCapture(hWnd);
	m_iMouseWheelDelta = 0;

	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_pFramesFrames->DeleteCachedPaintBitmaps(NULL, NULL);
	m_framesFlat.DeleteCachedPaintBitmaps(NULL, NULL);
	m_framesTiled.Clear();

	if (m_framesFlat.IsEmpty())
	{
		const CAbstractImageList::size_type numFrames = m_pFramesFrames->GetNumberOfFrames();

		for(CAbstractImageList::size_type i = 0; i < numFrames; ++i)
		{
			m_framesTiled.AddFrame(new CTiledImage( m_pFramesFrames->GetFrame( i ) ), true);
		}

		m_framesTiled.SetCurrentIndex( m_pFramesFrames->GetCurrentIndex() );
	}
	else
	{
		const CAbstractImageList::size_type numFrames = m_framesFlat.GetNumberOfFrames();

		for(CAbstractImageList::size_type i = 0; i < numFrames; ++i)
		{
			m_framesTiled.AddFrame(new CTiledImage( m_framesFlat.GetFrame( i ) ), true);
		}

		m_framesTiled.SetCurrentIndex( m_framesFlat.GetCurrentIndex() );
	}

	m_pCurrentFrames = &m_framesTiled;
	m_pLastDrawnImage = NULL;
	InvalidateRect(hWnd, NULL, FALSE);
}
