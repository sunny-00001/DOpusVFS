#include "StdAfx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "GifConfig.h"
#include "GifDecoder.h"
#include "ImageViewer.h"
#include "ViewerToolbarProxy.h"
#include "gifanim.h"

// 2 for the top divider (which doesn't count in the client rect)
// 1 for a single line at the bottom (instead of the ugly heap of lines the control tries to draw)
#define VIEWER_TOOLBAR_HEIGHT_ADJUSTMENT (3)

// static
ATOM CViewerToolbarProxy::RegisterWindowClass(HMODULE hDllModule)
{
	WNDCLASS wc;

	// Register window class
	wc.style			= CS_DBLCLKS;
	wc.lpfnWndProc		= CViewerToolbarProxy::wndProc;
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
BOOL CViewerToolbarProxy::UnregisterWindowClass(HMODULE hDllModule)
{
	return(UnregisterClass(getWindowClassName(), hDllModule));
}

// static
HWND CViewerToolbarProxy::CreateViewerToolbarProxyWindow(HWND hWndParent, LPRECT lpRc, DWORD dwFlags, CGifConfig *pConfig)
{
	RECT r;

	if (NULL == lpRc)
	{
		ZeroMemory(&r, sizeof(r));
		lpRc = &r;
	}

	CImageViewer::IVC_CreateParams cp;
	cp.dwFlags = dwFlags;
	cp.pConfig = pConfig;

	HWND hWnd = CreateWindowEx(	0, //(dwFlags&DVPCVF_Border) ? 0 : WS_EX_CLIENTEDGE,
								getWindowClassName(),
								NULL,
								WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								lpRc->left, lpRc->top, lpRc->right - lpRc->left + 1, lpRc->bottom - lpRc->top + 1,
								hWndParent,
								NULL,
								pConfig->GetDllModule(),
								&cp);

	return(hWnd);
}

// static
LRESULT WINAPI CViewerToolbarProxy::wndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	#pragma warning(suppress:4312) // spurious warning due to stupid Win32 SDK header definition.
	CViewerToolbarProxy *pThis = reinterpret_cast<CViewerToolbarProxy *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (uMsg)
	{
	case WM_CREATE:
		{
			CImageViewer::IVC_CreateParams *pCP = (reinterpret_cast<CImageViewer::IVC_CreateParams *>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
			pThis = new CViewerToolbarProxy(pCP->pConfig);
			#pragma warning(suppress:4244) // spurious warning due to stupid Win32 SDK header definition.
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			pThis->onCreate(hWnd, pCP);
		}
		return 0;

	case WM_DESTROY:
		if (pThis != NULL && pThis->m_hWndToolbar != NULL)
		{
			#pragma warning(suppress:4244) // spurious warning due to stupid Win32 SDK header definition.
			::SetWindowLongPtr(pThis->m_hWndToolbar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( ::RemoveProp(pThis->m_hWndToolbar, LeoHelpers::s_SubclassAtomPropHolder.GetAtomCast()) ));
		}
		return 0;

	case WM_NCDESTROY:
		delete pThis;
		pThis = NULL;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
		return 0;

	case DVPLUGINMSG_LOADA:
	case DVPLUGINMSG_LOADW:
	case DVPLUGINMSG_LOADSTREAMA:
	case DVPLUGINMSG_LOADSTREAMW:
	case DVPLUGINMSG_GETIMAGEINFOA:
	case DVPLUGINMSG_GETIMAGEINFOW:
	case DVPLUGINMSG_GETCAPABILITIES:
	case DVPLUGINMSG_SETROTATION:
	case DVPLUGINMSG_ROTATE:
	case DVPLUGINMSG_SETZOOM:
	case DVPLUGINMSG_ZOOM:
	case DVPLUGINMSG_GETZOOMFACTOR:
	case DVPLUGINMSG_SELECTALL:
	case DVPLUGINMSG_TESTSELECTION:
	case DVPLUGINMSG_COPYSELECTION:
	case DVPLUGINMSG_PRINT:
	case DVPLUGINMSG_PROPERTIES:
	case DVPLUGINMSG_REDRAW:
	case DVPLUGINMSG_GETAUTOBGCOL:
	case DVPLUGINMSG_MOUSEWHEEL:
	case DVPLUGINMSG_ADDCONTEXTMENUA:
	case DVPLUGINMSG_ADDCONTEXTMENUW:
	case DVPLUGINMSG_SETABORTEVENT:
	case DVPLUGINMSG_GETORIGINALPICSIZE:
	case DVPLUGINMSG_NOTIFY_LOADPROGRESS:
	case DVPLUGINMSG_ISDLGMESSAGE:
	case DVPLUGINMSG_TRANSLATEACCEL:
	case DVPLUGINMSG_REINITIALIZE:
	case DVPLUGINMSG_SHOWHIDESCROLLBARS:
	case DVPLUGINMSG_INLOADLOOP:
	case DVPLUGINMSG_SETDESKWALLPAPER:
	case DVPLUGINMSG_GETZOOMLIMITS:
	case DVPLUGINMSG_GETBITMAP:
	case DVPLUGINMSG_GAMMACHANGE:
	case DVPLUGINMSG_SHOWFILEINFO:
	case DVPLUGINMSG_ISFILEINFOSHOWN:
	case DVPLUGINMSG_ISALPHAHIDDEN:
	case DVPLUGINMSG_HIDEALPHA:
	case DVPLUGINMSG_CROPSELECTION:
	case DVPLUGINMSG_RESTORE:
	case DVPLUGINMSG_APPCOMMAND:
	case DVPLUGINMSG_FULLSCREEN:
		if (NULL != pThis)
		{
			return(SendMessage(pThis->m_hWndViewer, uMsg, wParam, lParam));
		}
		break;

	case DVPLUGINMSG_CLEAR:
		if (NULL != pThis)
		{
			pThis->m_bFrameImage = false;
			return(SendMessage(pThis->m_hWndViewer, uMsg, wParam, lParam));
		}
		break;

	case DVPLUGINMSG_SETIMAGEFRAME:
		if (NULL != pThis)
		{
			pThis->m_bFrameImage = wParam ? true : false;
			return(SendMessage(pThis->m_hWndViewer, uMsg, wParam, lParam));
		}
		return 0;

	case DVPLUGINMSG_GETPICSIZE:
		if (NULL != pThis)
		{
			return(pThis->onGetPicSize(hWnd, uMsg, wParam, lParam));
		}
		return(FALSE);

	case WM_NOTIFY:
		if (NULL != pThis)
		{
			return(pThis->onNotify(hWnd, wParam, lParam));
		}
		break;

	case DVPLUGINMSG_RESIZE:
		//logLine(_T("PROXY DVPLUGINMSG_RESIZE (fallthrough to WM_SIZE)\n"));
	case WM_SIZE:
		//logLine(_T("PROXY WM_SIZE: %d x %d\n"), static_cast<int>(LOWORD(lParam)), static_cast<int>(HIWORD(lParam)));
		if (NULL != pThis)
		{
			pThis->onSize(hWnd);
		}
		return 0;

	case WM_ERASEBKGND:
		return 0;

	case WM_PAINT:
		if (NULL != pThis)
		{
			pThis->onPaint(hWnd);
		}
		return 0;

	case CImageViewer::WM_IMAGEVIEWER_SHOWTOOLBAR:
		if (NULL != pThis && NULL != pThis->m_hWndToolbar)
		{
			ShowWindow(pThis->m_hWndToolbar, wParam ? SW_SHOW : SW_HIDE);
			pThis->onSize(hWnd);
		}
		return 0;

	case WM_COMMAND:
		if (NULL != pThis && NULL != pThis->m_hWndViewer)
		{
			switch(LOWORD(wParam))
			{
			case CImageViewer::IVC_PLAYBACKWARDS:
			case CImageViewer::IVC_PAUSE:
			case CImageViewer::IVC_PLAY:
			case CImageViewer::IVC_PREVFRAME:
			case CImageViewer::IVC_NEXTFRAME:
			case CImageViewer::IVC_FLATTENHORIZ:
			case CImageViewer::IVC_FLATTENVERT:
			case CImageViewer::IVC_FLATTENFIT:
			case CImageViewer::IVC_DITHERIMAGEALL:
			case CImageViewer::IVC_DITHERIMAGEONE:
			case CImageViewer::IVC_DITHERIMAGETWO:
				SendMessage(pThis->m_hWndViewer, WM_COMMAND, wParam, lParam);
				break;
			default:
				break;
			}
		}
		return 0;

	case CImageViewer::WM_IMAGEVIEWER_SETPRESSEDTOOLBARBUTTON:
		if (NULL != pThis)
		{
			pThis->onSetPressedToolbarButton(hWnd, static_cast<int>(wParam));
		}
		return 0;

	case TBM_SETRANGE:
		if (NULL != pThis && NULL != pThis->m_hWndTrackbar && IsWindow(pThis->m_hWndTrackbar))
		{
			if (HIWORD(lParam) != LOWORD(lParam))
			{
				SendMessage(pThis->m_hWndTrackbar, TBM_SETRANGE, TRUE, lParam);
			}

			SendMessage(pThis->m_hWndTrackbar, TBM_SETPOS, TRUE, 1);
			pThis->enableTrackbarIffPaused(hWnd);
		}
		return 0;

	case TBM_SETPOS:
		if (NULL != pThis && NULL != pThis->m_hWndTrackbar && IsWindow(pThis->m_hWndTrackbar))
		{
			SendMessage(pThis->m_hWndTrackbar, TBM_SETPOS, wParam, lParam);
		}
		return 0;

	case WM_HSCROLL:
		if (NULL != pThis && reinterpret_cast<HWND>(lParam) == pThis->m_hWndTrackbar)
		{
			SendMessage(pThis->m_hWndViewer, CImageViewer::WM_IMAGEVIEWER_SETFRAME, 0,
							SendMessage(pThis->m_hWndTrackbar, TBM_GETPOS, 0, 0));
			return 0;
		}
		break;
	case WM_SETTINGCHANGE:
	case WM_SYSCOLORCHANGE:
//	case WM_THEMECHANGED:
	case WM_POWERBROADCAST:
		if (NULL != pThis)
		{
			if (NULL != pThis->m_hWndTrackbar && IsWindow(pThis->m_hWndTrackbar))
			{
				SendMessage(pThis->m_hWndTrackbar, uMsg, wParam, lParam);
			}
			if (NULL != pThis->m_hWndToolbar)
			{
				SendMessage(pThis->m_hWndToolbar, uMsg, wParam, lParam);
			}
			if (NULL != pThis->m_hWndViewer)
			{
				SendMessage(pThis->m_hWndViewer, uMsg, wParam, lParam);
			}
		}
		break;// Allow DefWindowProc to process them as well.

	default:
		break;
	}

	return(DefWindowProc(hWnd,uMsg,wParam,lParam));
}

CViewerToolbarProxy::CViewerToolbarProxy(CGifConfig *pConfig)
: m_hWndViewer(NULL)
, m_hWndToolbar(NULL)
, m_hWndTrackbar(NULL)
, m_hImageList(NULL)
, m_bFrameImage(false)
, m_sl(pConfig->GetOpusPluginHelper())
{
}

CViewerToolbarProxy::~CViewerToolbarProxy()
{
	if (NULL != m_hImageList)
	{
		ImageList_Destroy(m_hImageList);
	}
}

void CViewerToolbarProxy::onCreate(HWND hWnd, CImageViewer::IVC_CreateParams *pCP)
{
	RECT windowRect;
	GetWindowRect(hWnd, &windowRect);

	createAnimationControls(hWnd, pCP->pConfig->GetDllModule());

	m_hWndViewer = CImageViewer::CreateViewerWindow(hWnd, &windowRect, pCP->dwFlags, pCP->pConfig);
}

void CViewerToolbarProxy::onSize(HWND hWnd)
{
	sizeAnimationControls(hWnd);

	RECT wrc;

	if (NULL != m_hWndViewer && GetClientRect(hWnd, &wrc) && !::IsRectEmpty(&wrc))
	{
		LONG animationControlsHeight;

		if (getAnimationControlsHeight(&animationControlsHeight, true))
		{
			wrc.bottom -= animationControlsHeight;
		}

		MoveWindow(m_hWndViewer, wrc.left, wrc.top, wrc.right-wrc.left, wrc.bottom-wrc.top, TRUE);
	}
}

LRESULT CViewerToolbarProxy::onPaint(HWND hWnd)
{
	PAINTSTRUCT ps;

	HDC hDC = BeginPaint(hWnd, &ps);

	if (NULL != hDC)
	{
		EndPaint(hWnd, &ps);
	}

	return 0;
}

LRESULT WINAPI CViewerToolbarProxy::toolbarSubProcW(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_LBUTTONDOWN:
			if (onToolbarLmb(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
			{
				return 0; // Swallowed.
			}
			break;
		default:
			break;
	}

	// Default handling
	#pragma warning(suppress:4312) // spurious warning due to stupid Win32 SDK header definition.
	return ::CallWindowProc(reinterpret_cast<WNDPROC>( ::GetProp(hWnd, LeoHelpers::s_SubclassAtomPropHolder.GetAtomCast()) ), hWnd, uMsg, wParam, lParam);
}

// static
BOOL CViewerToolbarProxy::onToolbarLmb(HWND hWnd, int x, int y)
{
	BOOL bSwallowed = FALSE;

	HWND hWndOpus = GetParent(GetParent(hWnd));

	SetFocus(hWndOpus);

	// Allow the bottom of our toolbar to trigger Opus's viewer toolbar in full-screen mode.
	// Don't allow other things to happen, though, such as toggling fullscreen mode or closing the viewer.
	// Note: For either the menu bar or the control bar to appear in full-screen mode, they have
	// to be enabled in the viewer menu (i.e. appear in non-full-screen mode).

	RECT parentWindowRect;
	RECT windowRect;
	RECT clientRect;

	if (GetWindowRect(GetParent(hWnd), &parentWindowRect)
	&&	GetWindowRect(hWnd, &windowRect)
	&&	GetClientRect(hWnd, &clientRect)
	&&	y >= (clientRect.bottom - (1 + VIEWER_TOOLBAR_HEIGHT_ADJUSTMENT)))
	{
		DVPNMCLICK notHdr;
		notHdr.hdr.hwndFrom = GetParent(hWnd);
		notHdr.hdr.idFrom = 0;
		notHdr.hdr.code = DVPN_CLICKQUERY;
		notHdr.pt.x = x + (windowRect.left - parentWindowRect.left);
		notHdr.pt.y = y + (windowRect.top  - parentWindowRect.top);
		notHdr.fMenu = -1; // Reveal control bar if they click on the last two lines of pixels.

		int iAction = static_cast<int>(SendMessage(hWndOpus, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));

		if (iAction == BUTTONOPT_SHOWTOOLBAR || iAction == BUTTONOPT_SHOWMENUBAR || iAction == BUTTONOPT_HIDEMENUANDTOOLBAR)
		{
			notHdr.hdr.code = DVPN_CLICK;
			bSwallowed = static_cast<BOOL>(SendMessage(hWndOpus, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&notHdr)));
		}
	}

	return bSwallowed;
}

void CViewerToolbarProxy::createAnimationControls(HWND hWnd, HINSTANCE hInstance)
{
	int iIconX = 16;
	int iIconY = 15;

	HBITMAP hBmToolbar = reinterpret_cast<HBITMAP>(LoadImage(hInstance, MAKEINTRESOURCE(IDB_ANIMCTRLS), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));

	m_hImageList = ImageList_Create(iIconX, iIconY, ILC_COLOR | ILC_MASK, 8, 0);

	m_hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
									WS_CHILD|CCS_NOMOVEY|TBSTYLE_TOOLTIPS|TBSTYLE_FLAT|TBSTYLE_CUSTOMERASE|WS_CLIPCHILDREN,
									0,0,0,0, hWnd,
									reinterpret_cast<HMENU>(CViewerToolbarProxy::ID_TOOLBAR), hInstance, NULL);

	assert(m_hWndToolbar != NULL);
	assert(::IsWindowUnicode(m_hWndToolbar));

	if (m_hWndToolbar != NULL)
	{
		#pragma warning(suppress:4312) // spurious warning due to stupid Win32 SDK header definition.
		#pragma warning(suppress:4244) // spurious warning due to stupid Win32 SDK header definition.
		::SetProp(m_hWndToolbar, LeoHelpers::s_SubclassAtomPropHolder.GetAtomCast(), reinterpret_cast<WNDPROC>(::SetWindowLongPtr(m_hWndToolbar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(toolbarSubProcW))));
	}

	if (NULL != m_hImageList && NULL != m_hWndToolbar && NULL != hBmToolbar
	&&	(-1) != ImageList_AddMasked(m_hImageList, hBmToolbar, RGB(255,255,255)))
	{
		SendMessage(m_hWndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
		SendMessage(m_hWndToolbar, TB_SETBITMAPSIZE, 0, MAKELONG(iIconX, iIconY));
		SendMessage(m_hWndToolbar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(m_hImageList));
		SendMessage(m_hWndToolbar, TB_SETMAXTEXTROWS, 0, 0);

		TBBUTTON tbb[10];
		ZeroMemory(tbb, sizeof(tbb));

		TBBUTTON *pb = tbb;
	 
		pb->iBitmap   = CImageViewer::IVC_PLAYBACKWARDS;
		pb->idCommand = CImageViewer::IVC_PLAYBACKWARDS;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_PLAY_BACKWARDS));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_PREVFRAME;
		pb->idCommand = CImageViewer::IVC_PREVFRAME;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_PREVIOUS_FRAME));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_PAUSE;
		pb->idCommand = CImageViewer::IVC_PAUSE;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_PAUSE));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_NEXTFRAME;
		pb->idCommand = CImageViewer::IVC_NEXTFRAME;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_NEXT_FRAME));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_PLAY;
		pb->idCommand = CImageViewer::IVC_PLAY;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_PLAY));
		pb++;
		pb->iBitmap   = 0;
		pb->idCommand = 0;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = BTNS_SEP;
		pb->iString   = 0;
		pb++;
		pb->iBitmap   = CImageViewer::IVC_FLATTENHORIZ;
		pb->idCommand = CImageViewer::IVC_FLATTENHORIZ;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_FLATTEN_HORIZONTALLY));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_FLATTENVERT;
		pb->idCommand = CImageViewer::IVC_FLATTENVERT;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_FLATTEN_VERTICALLY));
		pb++;
		pb->iBitmap   = CImageViewer::IVC_FLATTENFIT;
		pb->idCommand = CImageViewer::IVC_FLATTENFIT;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = TBSTYLE_BUTTON;
		pb->iString   = reinterpret_cast<INT_PTR>(m_sl.Get(STR_GIFANIM_TOOLBAR_FLATTEN_TO_TILES));
		pb++;
		pb->iBitmap   = 0;
		pb->idCommand = 0;
		pb->fsState   = TBSTATE_ENABLED;
		pb->fsStyle   = BTNS_SEP;
		pb->iString   = 0;
		pb++;

		SendMessage(m_hWndToolbar, TB_ADDBUTTONS, sizeof(tbb)/sizeof(tbb[0]), reinterpret_cast<LPARAM>(&tbb));
		SendMessage(m_hWndToolbar, TB_AUTOSIZE, 0, 0);

		// Track bar -- create as child of main window (so it gets notification messages) and then set its parent
		// to the toolbar so it is positioned properly.

		m_hWndTrackbar = CreateWindowEx(0, TRACKBAR_CLASS, NULL,
										WS_DISABLED|WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_AUTOTICKS|TBS_TOOLTIPS, 0,0,0,0,
										hWnd, reinterpret_cast<HMENU>(CViewerToolbarProxy::ID_TRACKBAR), hInstance, NULL);
		SetParent(m_hWndTrackbar, m_hWndToolbar);

		SendMessage(m_hWndTrackbar, TBM_SETTIPSIDE, TBTS_BOTTOM, 0);
	}

	if (NULL != hBmToolbar)
	{
		DeleteObject(hBmToolbar);
	}
}

void CViewerToolbarProxy::sizeAnimationControls(HWND hWnd)
{
	if (NULL != m_hWndToolbar)
	{
		RECT r;
		LONG animationControlsHeight;

		if (GetClientRect(hWnd, &r) && getAnimationControlsHeight(&animationControlsHeight, true))
		{
			r.top = (r.bottom - animationControlsHeight);

			MoveWindow(m_hWndToolbar, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);

			RECT tb;

			if (NULL != m_hWndTrackbar && SendMessage(m_hWndToolbar, TB_GETITEMRECT, 9, reinterpret_cast<LPARAM>(&tb)))
			{
				//--tb.top;
				//++tb.bottom;
				tb.left = tb.right;
				tb.right = r.right;

				if ((tb.right - tb.left) < (2*(tb.bottom - tb.top)))
				{
					tb.right = tb.left + (2*(tb.bottom - tb.top));
				}

				MoveWindow(m_hWndTrackbar, tb.left, tb.top, tb.right - tb.left, tb.bottom - tb.top, TRUE);
			}
		}
	}
}

inline bool CViewerToolbarProxy::getAnimationControlsHeight(LONG *pResult, bool bMustBeVisible)
{
	RECT buttonRectFirst;

	if (NULL == m_hWndToolbar
	||	(bMustBeVisible && (!(WS_VISIBLE & GetWindowLong(m_hWndToolbar,GWL_STYLE))))
	||	(!SendMessage(m_hWndToolbar, TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&buttonRectFirst))))
	{
		return false;
	}
	else
	{
		*pResult = buttonRectFirst.bottom + VIEWER_TOOLBAR_HEIGHT_ADJUSTMENT;

		return true;
	}
}

BOOL CViewerToolbarProxy::onGetPicSize(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!SendMessage(m_hWndViewer, uMsg, wParam, lParam))
	{
		return(FALSE);
	}

	LONG animationControlsHeight;

	if (NULL != lParam && getAnimationControlsHeight(&animationControlsHeight, true))
	{
		reinterpret_cast<LPSIZE>(lParam)->cy += animationControlsHeight;
	}

	return(TRUE);
}

LRESULT CViewerToolbarProxy::onNotify(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	NMHDR *pNotHdr = reinterpret_cast<NMHDR *>(lParam);

	if (m_hWndViewer == pNotHdr->hwndFrom)
	{
		// Forward notification messages from the viewer window on to Opus

		pNotHdr->hwndFrom = hWnd; // Pretend the message comes from us.

		bool bRestoreSizeChangeSize = false;
		SIZE sizeOrig;

		LONG animationControlsHeight;

		if (DVPN_SIZECHANGE == pNotHdr->code && getAnimationControlsHeight(&animationControlsHeight, true))
		{
			// Intercept DVPN_SIZECHANGE messages and include the height
			// of the animation controls if they are visible.
			DVPNMSIZECHANGE *pNotSizeHdr = reinterpret_cast<DVPNMSIZECHANGE *>(pNotHdr);

			bRestoreSizeChangeSize = true;
			sizeOrig = pNotSizeHdr->szSize;

			RECT buttonRectFirst;
			RECT buttonRectLast;

			pNotSizeHdr->szSize.cy += animationControlsHeight;

			if (SendMessage(m_hWndToolbar, TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&buttonRectFirst))
			&&	SendMessage(m_hWndToolbar, TB_GETITEMRECT, 4, reinterpret_cast<LPARAM>(&buttonRectLast)))
			{
				if (m_bFrameImage)
				{
					pNotSizeHdr->szSize.cx += (OPUSVIEWER_IMAGE_FRAME_SIZE * 2);
				}

				if (pNotSizeHdr->szSize.cx < (buttonRectLast.right - buttonRectFirst.left))
				{
					pNotSizeHdr->szSize.cx = (buttonRectLast.right - buttonRectFirst.left);
				}

				if (m_bFrameImage)
				{
					pNotSizeHdr->szSize.cx -= (OPUSVIEWER_IMAGE_FRAME_SIZE * 2);
				}
			}

			//logLine(_T("PROXY Requesting %ld x %ld\n"), pNotSizeHdr->szSize.cx, pNotSizeHdr->szSize.cy);
		}

		LRESULT lRes = SendMessage(GetParent(hWnd), WM_NOTIFY, wParam, lParam);

		// Restore the original data in case the sender wants to re-use the structure.

		if (bRestoreSizeChangeSize)
		{
			reinterpret_cast<DVPNMSIZECHANGE *>(pNotHdr)->szSize = sizeOrig;
		}
		else if (DVPN_CALCULATERECT == pNotHdr->code && getAnimationControlsHeight(&animationControlsHeight, true))
		{
			// Intercept DVPN_CALCULATERECT results and add/subtract the height of the animation controls if they are visible.

			DVPNMCALCULATERECT *pNotAdjustHdr = reinterpret_cast<DVPNMCALCULATERECT *>(pNotHdr);

			switch(pNotAdjustHdr->operation)
			{
			default:
				assert(false);
				lRes = FALSE;
				break;
			case VPCALCRECT_VIEWER_TO_WINDOW:
				pNotAdjustHdr->rc.bottom += animationControlsHeight;
				break;
			case VPCALCRECT_WINDOW_TO_VIEWER:
			case VPCALCRECT_MAX_VIEWER_AUTOSIZE:
				pNotAdjustHdr->rc.bottom -= animationControlsHeight;
				break;
			}

			if (lRes && pNotAdjustHdr->rc.top >= pNotAdjustHdr->rc.bottom)
			{
				lRes = FALSE; // It was a success until we subtracted too much more. Now it's a failure.
			}
		}

		pNotHdr->hwndFrom = m_hWndViewer;

		return(lRes);
	}
	else if (m_hWndToolbar == pNotHdr->hwndFrom)
	{
		if (NM_CUSTOMDRAW == pNotHdr->code)
		{
			NMTBCUSTOMDRAW *pCustomDrawToolbar = reinterpret_cast<NMTBCUSTOMDRAW *>(lParam);

			if (CDDS_PREERASE == pCustomDrawToolbar->nmcd.dwDrawStage)
			{
				// Change the toolbar's background colour to COLOR_BTNFACE.

				RECT rectClientToolbar;
				if (GetClientRect(pNotHdr->hwndFrom, &rectClientToolbar))
				{
					FillRect(pCustomDrawToolbar->nmcd.hdc, &rectClientToolbar, reinterpret_cast<HBRUSH>(GetSysColorBrush(COLOR_BTNFACE)));
					return(CDRF_SKIPDEFAULT);
				}
			}

			return(CDRF_DODEFAULT);
		}
	}

	return(0);
}

void CViewerToolbarProxy::onSetPressedToolbarButton(HWND hWnd, int iButton)
{
	if (NULL != m_hWndToolbar)
	{
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_PLAYBACKWARDS, MAKELONG(CImageViewer::IVC_PLAYBACKWARDS == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_PAUSE,         MAKELONG(CImageViewer::IVC_PAUSE         == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_PLAY,          MAKELONG(CImageViewer::IVC_PLAY          == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_PREVFRAME,     MAKELONG(CImageViewer::IVC_PREVFRAME     == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_NEXTFRAME,     MAKELONG(CImageViewer::IVC_NEXTFRAME     == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_FLATTENHORIZ,  MAKELONG(CImageViewer::IVC_FLATTENHORIZ  == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_FLATTENVERT,   MAKELONG(CImageViewer::IVC_FLATTENVERT   == iButton ? TRUE : FALSE, 0));
		SendMessage(m_hWndToolbar, TB_PRESSBUTTON, CImageViewer::IVC_FLATTENFIT,    MAKELONG(CImageViewer::IVC_FLATTENFIT    == iButton ? TRUE : FALSE, 0));
	}

	enableTrackbarIffPaused(hWnd);
}

inline void CViewerToolbarProxy::enableTrackbarIffPaused(HWND hWnd)
{
	// The enabled trackbar is distracting when moving around so we disable it during playback.
	// It still moves around but is less eye catching, at least using XP's default visual style.

	if (NULL != m_hWndToolbar && NULL != m_hWndTrackbar)
	{
		BOOL bPaused = static_cast<BOOL>(SendMessage(m_hWndToolbar, TB_ISBUTTONPRESSED, CImageViewer::IVC_PAUSE, 0));

		if (m_hWndTrackbar == GetFocus() && (!bPaused))
		{
			SetFocus(GetParent(hWnd));
		}

		EnableWindow(m_hWndTrackbar, bPaused);
	}
}
