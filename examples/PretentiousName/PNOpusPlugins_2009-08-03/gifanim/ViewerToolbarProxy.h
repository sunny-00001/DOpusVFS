#pragma once

class CViewerToolbarProxy
{
protected:
	HWND m_hWndViewer;
	HWND m_hWndToolbar;
	HWND m_hWndTrackbar;
	HIMAGELIST m_hImageList;
	bool m_bFrameImage;
	LeoHelpers::OpusStringLoader m_sl;

	enum { ID_TOOLBAR = 1,
		   ID_TRACKBAR };

public:
	CViewerToolbarProxy(CGifConfig *pConfig);
	~CViewerToolbarProxy();

	static ATOM RegisterWindowClass(HMODULE hDllModule);
	static BOOL UnregisterWindowClass(HMODULE hDllModule);

	static HWND CreateViewerToolbarProxyWindow(HWND hWndParent,LPRECT lpRc,DWORD dwFlags,CGifConfig *pConfig);

protected:
	inline static const TCHAR *getWindowClassName() { return(_T("dopusviewerplugin.gifanim.proxy")); }
	static LRESULT WINAPI wndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

	void onCreate(HWND hWnd, CImageViewer::IVC_CreateParams *pCP);
	void onSize(HWND hWnd);
	BOOL onGetPicSize(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT onNotify(HWND hWnd, WPARAM wParam, LPARAM lParam);
	LRESULT onPaint(HWND hWnd);
	void onSetPressedToolbarButton(HWND hWnd, int iButton);

	inline void enableTrackbarIffPaused(HWND hWnd);

	void createAnimationControls(HWND hWnd, HINSTANCE hInstance);
	void sizeAnimationControls(HWND hWnd);
	inline bool getAnimationControlsHeight(LONG *pResult, bool bMustBeVisible);

	static LRESULT WINAPI toolbarSubProcW(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
	static BOOL CViewerToolbarProxy::onToolbarLmb(HWND hWnd, int x, int y);
};
