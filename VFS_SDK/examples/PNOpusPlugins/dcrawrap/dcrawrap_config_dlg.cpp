#include "StdAfx.h"
#include "resource.h"
#include "../common/plugindialogs.h"
#include "../common/messages.hpp"
#include "LeoHelpers.h"
#include "dcraw_settings.h"
#include "dcrawrap_config.h"
#include "dcrawrap_config_statedata.h"
#include "dcraw_interface.h"
#include "dcrawrap.h"
#include "dcrawrap_config_dlg.h"

// TODO: Visually indicate the default Add button when the file extension field is active, like the Docs and TextThumbs plugin config dialogs do.

DCRawConfigDlg::DropTarget::DropTarget()
: m_bDragEnabled(true)
, m_hwnd(NULL)
{
}

DCRawConfigDlg::DropTarget::~DropTarget()
{
	Revoke();
}

void DCRawConfigDlg::DropTarget::SetDragEnabled(bool bEnabled)
{
	m_bDragEnabled = bEnabled;
}

void DCRawConfigDlg::DropTarget::AddDropWindow(HWND hWnd)
{
	m_vecDropWindows.push_back( hWnd );
}

void DCRawConfigDlg::DropTarget::Register(HWND hwnd)
{
	Revoke();

	if (NULL != hwnd)
	{
		m_hwnd = hwnd;
		RegisterDragDrop(hwnd, this);
	}
}

void DCRawConfigDlg::DropTarget::Revoke()
{
	if (NULL != m_hwnd)
	{
		RevokeDragDrop(m_hwnd);
		m_hwnd = NULL;
	}
}

std::vector< std::wstring > &DCRawConfigDlg::DropTarget::GetFilenames()
{
	return m_vecFilenames;
}

HWND DCRawConfigDlg::DropTarget::GetDropWindowFromScreenPoint(POINT &ptScreen)
{
	HWND hWndDrop = NULL;

	if (m_bDragEnabled)
	{
		RECT rect;

		for (std::vector< HWND >::iterator pHwnd = m_vecDropWindows.begin(); pHwnd != m_vecDropWindows.end(); ++pHwnd)
		{
			DWORD dwStyle = GetWindowLong(*pHwnd, GWL_STYLE);

			if ( (dwStyle & WS_VISIBLE)
			&&	!(dwStyle & WS_DISABLED)
			&&	GetWindowRect(*pHwnd, &rect)
			&&	PtInRect(&rect, ptScreen))
			{
				bool bAllParentsSuitable = true;

				HWND hWnd = *pHwnd;

				while(dwStyle & WS_CHILD)
				{
					hWnd = GetParent(hWnd);
					dwStyle = GetWindowLong(hWnd, GWL_STYLE);

					if (!(dwStyle & WS_VISIBLE) || (dwStyle & WS_DISABLED))
					{
						bAllParentsSuitable = false;
						break;
					}
				}

				if (bAllParentsSuitable)
				{
					hWndDrop = *pHwnd;
					break;
				}
			}
		}
	}

	return(hWndDrop);
}

// IUnknown
HRESULT STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject)
{
	if (IID_IUnknown == riid)
	{
		*ppvObject = static_cast<IUnknown *>(this);
		return S_OK;
	}
	else if (IID_IDropTarget == riid)
	{
		*ppvObject = static_cast<IDropTarget *>(this);
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}

ULONG STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::AddRef(void)
{
	return 1;
}

ULONG STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::Release(void)
{
	return 1;
}

// IDropTarget
HRESULT STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	POINT pt2 = { pt.x, pt.y };
	*pdwEffect = (NULL != GetDropWindowFromScreenPoint(pt2) ? DROPEFFECT_COPY : DROPEFFECT_NONE);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	POINT pt2 = { pt.x, pt.y };
	*pdwEffect = (NULL != GetDropWindowFromScreenPoint(pt2) ? DROPEFFECT_COPY : DROPEFFECT_NONE);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::DragLeave(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DCRawConfigDlg::DropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	POINT pt2 = { pt.x, pt.y };
	HWND hWndDrop = GetDropWindowFromScreenPoint(pt2);
	if (NULL != hWndDrop)
	{
		FORMATETC format;
		format.cfFormat = CF_HDROP;
		format.ptd = NULL;
		format.dwAspect = DVASPECT_CONTENT;
		format.lindex = -1;
		format.tymed = TYMED_HGLOBAL;

		STGMEDIUM medium;
		ZeroMemory(&medium, sizeof(medium));

		if (SUCCEEDED(pDataObj->GetData(&format, &medium)))
		{
			if (medium.tymed == TYMED_HGLOBAL)
			{
				HDROP hDrop = reinterpret_cast<HDROP>(GlobalLock(medium.hGlobal));
				if (NULL != hDrop)
				{
					UINT uicDroppedFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

					for (UINT uiFileIdx = 0; uiFileIdx < uicDroppedFiles; uiFileIdx++)
					{
						UINT uiccBuffer = DragQueryFile(hDrop, uiFileIdx, NULL, 0);
						if (0 < uiccBuffer)
						{
							uiccBuffer++; // Increment for null terminator.
							wchar_t *szBuffer = new(std::nothrow) wchar_t[uiccBuffer + 1]; // Add space for a second null because I don't trust the API.

							if (szBuffer != NULL)
							{
								szBuffer[uiccBuffer] = L'\0'; // Make sure that second null is there.

								if (0 < DragQueryFile(hDrop, uiFileIdx, szBuffer, uiccBuffer))
								{
									m_vecFilenames.push_back( szBuffer );
								}

								delete[] szBuffer;
							}
						}
					}

					GlobalUnlock(medium.hGlobal);
				}
			}

			ReleaseStgMedium(&medium);
		}

		if (!m_vecFilenames.empty())
		{
			// Note: This is a non-standard use of the WM_DROPFILES message that m_hwnd must understand.
			PostMessage(m_hwnd, WM_DROPFILES, 0, reinterpret_cast<LPARAM>(hWndDrop));
		}
	}

	return S_OK;
}

INT_PTR CALLBACK DCRawConfigDlg::configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL bResult = FALSE;
	bool bDeletePCD = false;
	bool bLoadSettings = false;
	bool bLoadDefaultSettings = false;
	bool bSaveSettings = false;
	bool bEnableDisable = false;

	DCRawConfigDlg_Data *pcd = reinterpret_cast<DCRawConfigDlg_Data *>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));

    switch (message)
    {
	case WM_INITDIALOG:
		configInitDialog(bResult, bDeletePCD, bLoadSettings, bLoadDefaultSettings, bSaveSettings, bEnableDisable, pcd, hwndDlg, message, wParam, lParam);
		break;
	case WM_NOTIFY:
		configNotify(    bResult, bDeletePCD, bLoadSettings, bLoadDefaultSettings, bSaveSettings, bEnableDisable, pcd, hwndDlg, message, wParam, lParam);
		break;
	case WM_VSCROLL:
		configVScroll(   bResult, bDeletePCD, bLoadSettings, bLoadDefaultSettings, bSaveSettings, bEnableDisable, pcd, hwndDlg, message, wParam, lParam);
		break;
	case WM_COMMAND:
		configCommand(   bResult, bDeletePCD, bLoadSettings, bLoadDefaultSettings, bSaveSettings, bEnableDisable, pcd, hwndDlg, message, wParam, lParam);
		break;
	case WM_DROPFILES:
		configDropFiles( bResult, bDeletePCD, bLoadSettings, bLoadDefaultSettings, bSaveSettings, bEnableDisable, pcd, hwndDlg, message, wParam, lParam);
		break;
	//case WM_SYSCOLORCHANGE:
	//	bResult = FALSE; // Pass on to standard handling.
	//	break;
	case WM_SETTINGCHANGE:
		// One reason for doing this is so the numeric edit controls pick up new locale decimal settings.
		SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
		SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
		SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);
		bEnableDisable = true; // Re-evaluating the config may mean we need to enable/disable some things.
		bResult = FALSE; // Pass on to standard handling.
		break;
	default:
		break;
	}

	if (NULL != pcd && (bLoadSettings || bLoadDefaultSettings))
	{
		configLoadSettings(pcd, bLoadDefaultSettings, message == WM_INITDIALOG);
		bEnableDisable = true;
	}

	if (NULL != pcd && bSaveSettings)
	{
		configSaveSettings(pcd);

		if (!bDeletePCD)
		{
			// Reflect exactly what was saved (in particular, numeric edit control parsing/formating)
			SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
			SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
			SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);
			bEnableDisable = true; // Re-evaluating the config may mean we need to enable/disable some things.
		}
	}

	if (NULL != pcd && bEnableDisable) // Must be after settings load
	{
		configEnableDisable(pcd);
	}

	if (bDeletePCD && NULL != pcd)
	{
		SetWindowLongPtr(pcd->hWndMainDlg,        GWLP_USERDATA, NULL);
		SetWindowLongPtr(pcd->hWndFormats,        GWLP_USERDATA, NULL);
		SetWindowLongPtr(pcd->hWndDCRawThumbs,    GWLP_USERDATA, NULL);
		SetWindowLongPtr(pcd->hWndDCRawViewers,   GWLP_USERDATA, NULL);
		SetWindowLongPtr(pcd->hWndDCRawConverter, GWLP_USERDATA, NULL);
		delete pcd;
	}

	return(bResult);
}

INT_PTR CALLBACK DCRawConfigDlg::childProxyDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_INITDIALOG:
		{
			DCRawConfigDlg_Data *pcd = reinterpret_cast<DCRawConfigDlg_Data *>(lParam);
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pcd));
		}
		break;
	default:
		return (DCRawConfigDlg::configDlgProc(hwndDlg, message, wParam, lParam));
		break;
	}

	return(FALSE);
}

LRESULT CALLBACK DCRawConfigDlg::editFloatFilterSubProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Ignore all but numbers and decimal
	if (uMsg == WM_CHAR)
	{
		if (!iswdigit(static_cast<wint_t>(wParam)) && wParam!=L'\b')
		{
			wchar_t szDecSep[8]; // 8 is more than the documented max of 4 including the null.
			if (0 == ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, szDecSep, _countof(szDecSep)))
			{
				szDecSep[0] = L'.';
				szDecSep[1] = L'\0';
			}

			if (szDecSep[1]==L'\0' && wParam==szDecSep[0])
			{
				// Locale decimal separator is one character and that's what they typed.
				wchar_t szBuf[256];
				GetWindowText(hWnd,szBuf,_countof(szBuf));
				if (wcschr(szBuf,szDecSep[0]))
				{
					// It's already got a decimal separator so reject the extra one.
					MessageBeep(-1);
					return 0;
				}
			}
			else if (szDecSep[1]!=L'\0' && NULL!=wcschr(szDecSep,static_cast<wchar_t>(wParam)))
			{
				// Locale decimal separator is multi-character and they typed one of them.
				// I don't think any real locale actually has a multi-character decimal separator
				// so I haven't bothered coding decent input validation for it. Just accept
				// all of those characters and if the user types them in an invalid combination
				// then the number will be rejected/truncated during parsing, which is fine, TBH.
			}
			else
			{
				MessageBeep(-1);
				return 0;
			}
		}
	}

	return CallWindowProc(reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd,GWLP_USERDATA)),hWnd,uMsg,wParam,lParam);
}

void DCRawConfigDlg::configInitDialog(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *&pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hWndTab = GetDlgItem(hwndDlg, IDC_RAW_TAB);
	if (NULL != hWndTab)
	{
		pcd = new DCRawConfigDlg_Data( reinterpret_cast<DCRawConfigDlg_CreationData *>(lParam) );
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pcd));

		pcd->hWndMainDlg        = hwndDlg;
		pcd->hWndFormats        = pcd->config.GetOpusPluginHelper()->CreateLangDlg(pcd->config.GetInstance(), MAKEINTRESOURCE(IDD_RAW_FORMATS),    hwndDlg, DCRawConfigDlg::childProxyDlgProc, reinterpret_cast<LPARAM>(pcd));
		pcd->hWndDCRawThumbs    = pcd->config.GetOpusPluginHelper()->CreateLangDlg(pcd->config.GetInstance(), MAKEINTRESOURCE(IDD_RAW_DCRAW),      hwndDlg, DCRawConfigDlg::childProxyDlgProc, reinterpret_cast<LPARAM>(pcd));
		pcd->hWndDCRawViewers   = pcd->config.GetOpusPluginHelper()->CreateLangDlg(pcd->config.GetInstance(), MAKEINTRESOURCE(IDD_RAW_DCRAW),      hwndDlg, DCRawConfigDlg::childProxyDlgProc, reinterpret_cast<LPARAM>(pcd));
		pcd->hWndDCRawConverter = pcd->config.GetOpusPluginHelper()->CreateLangDlg(pcd->config.GetInstance(), MAKEINTRESOURCE(IDD_RAW_DCRAW),      hwndDlg, DCRawConfigDlg::childProxyDlgProc, reinterpret_cast<LPARAM>(pcd));

		// This would be great if the stupid tab control didn't block drag & drop to anything on top of it. Using IDropTarget instead.
		//HWND hWndListCamsThumbs    = GetDlgItem(pcd->hWndDCRawThumbs,    IDC_RAW_LIST_CAMERAS);
		//HWND hWndListCamsViewers   = GetDlgItem(pcd->hWndDCRawViewers,   IDC_RAW_LIST_CAMERAS);
		//HWND hWndListCamsConverter = GetDlgItem(pcd->hWndDCRawConverter, IDC_RAW_LIST_CAMERAS);
		//if (NULL != hWndListCamsThumbs)    { DragAcceptFiles(hWndListCamsThumbs,    TRUE); }
		//if (NULL != hWndListCamsViewers)   { DragAcceptFiles(hWndListCamsViewers,   TRUE); }
		//if (NULL != hWndListCamsConverter) { DragAcceptFiles(hWndListCamsConverter, TRUE); }

		TCITEM tabItem;
		ZeroMemory(&tabItem, sizeof(tabItem));
		tabItem.mask = TCIF_PARAM|TCIF_TEXT;

		tabItem.pszText = const_cast< wchar_t *>( pcd->config.GetString(STR_DCRAW_TAB_FILEFORMATS) ); // "File Formats"
		tabItem.lParam = reinterpret_cast<LPARAM>(pcd->hWndFormats);
		SendMessage(hWndTab, TCM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&tabItem));

		tabItem.pszText = const_cast< wchar_t *>( pcd->config.GetString(STR_DCRAW_TAB_RAWTHUMBNAILS) ); // "Raw Thumbnails"
		tabItem.lParam = reinterpret_cast<LPARAM>(pcd->hWndDCRawThumbs);
		SendMessage(hWndTab, TCM_INSERTITEM, 1, reinterpret_cast<LPARAM>(&tabItem));

		tabItem.pszText = const_cast< wchar_t *>( pcd->config.GetString(STR_DCRAW_TAB_RAWVIEWERS) ); // "Raw Viewers"
		tabItem.lParam = reinterpret_cast<LPARAM>(pcd->hWndDCRawViewers);
		SendMessage(hWndTab, TCM_INSERTITEM, 2, reinterpret_cast<LPARAM>(&tabItem));

		tabItem.pszText = const_cast< wchar_t *>( pcd->config.GetString(STR_DCRAW_TAB_RAWIMAGECONVERTER) ); // "Raw Image Converter"
		tabItem.lParam = reinterpret_cast<LPARAM>(pcd->hWndDCRawConverter);
		SendMessage(hWndTab, TCM_INSERTITEM, 3, reinterpret_cast<LPARAM>(&tabItem));

		RECT rectTabDisplay;
		GetWindowRect(hWndTab, &rectTabDisplay);
		LeoHelpers::ScreenToClientRect(hwndDlg, &rectTabDisplay);
		TabCtrl_AdjustRect(hWndTab, FALSE, &rectTabDisplay);

		SetWindowPos(pcd->hWndFormats,        hWndTab, rectTabDisplay.left, rectTabDisplay.top, rectTabDisplay.right - rectTabDisplay.left, rectTabDisplay.bottom - rectTabDisplay.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER);
		SetWindowPos(pcd->hWndDCRawThumbs,    hWndTab, rectTabDisplay.left, rectTabDisplay.top, rectTabDisplay.right - rectTabDisplay.left, rectTabDisplay.bottom - rectTabDisplay.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER);
		SetWindowPos(pcd->hWndDCRawViewers,   hWndTab, rectTabDisplay.left, rectTabDisplay.top, rectTabDisplay.right - rectTabDisplay.left, rectTabDisplay.bottom - rectTabDisplay.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER);
		SetWindowPos(pcd->hWndDCRawConverter, hWndTab, rectTabDisplay.left, rectTabDisplay.top, rectTabDisplay.right - rectTabDisplay.left, rectTabDisplay.bottom - rectTabDisplay.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER);

		LeoHelpers::ThemeHelper themeHelper;
		// The tab control doesn't resize and isn't too large for the stupid theme texture
		// so we can enable the texture and then forget about both it and the Theme Helper.
		themeHelper.EnableThemeDialogTexture(pcd->hWndFormats,        ETDT_ENABLETAB);
		themeHelper.EnableThemeDialogTexture(pcd->hWndDCRawThumbs,    ETDT_ENABLETAB);
		themeHelper.EnableThemeDialogTexture(pcd->hWndDCRawViewers,   ETDT_ENABLETAB);
		themeHelper.EnableThemeDialogTexture(pcd->hWndDCRawConverter, ETDT_ENABLETAB);

		// Integer edit controls should not be in this list. Just use the control's "number" style instead.
		HWND hwndsFloatEditDlgs[] = { pcd->hWndDCRawThumbs,
									  pcd->hWndDCRawViewers,
									  pcd->hWndDCRawConverter,
									  NULL };
		int iFloatEditCtrlIds[] = { IDC_RAW_EDIT_USERMUL_1,
									IDC_RAW_EDIT_USERMUL_2,
									IDC_RAW_EDIT_USERMUL_3,
									IDC_RAW_EDIT_USERMUL_4,
									IDC_RAW_EDIT_BRIGHTNESS,
									IDC_RAW_EDIT_GAMMA_POWER,
									IDC_RAW_EDIT_GAMMA_TOESLOPE,
									IDC_RAW_EDIT_CHROMA_RED,
									IDC_RAW_EDIT_CHROMA_BLUE,
									0 };

		for (HWND *phwnd = hwndsFloatEditDlgs; *phwnd != NULL; phwnd++)
		{
			for (int *piEditCtrlId = iFloatEditCtrlIds; *piEditCtrlId != 0; piEditCtrlId++)
			{
				HWND hWndEdit = GetDlgItem(*phwnd, *piEditCtrlId);
				if (NULL != hWndEdit)
				{
					SetWindowLongPtr(hWndEdit, GWLP_USERDATA, SetWindowLongPtr(hWndEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editFloatFilterSubProc)));
				}
			}
		}

		TabCtrl_SetCurSel(hWndTab, 0);
		ShowWindow(pcd->hWndFormats, SW_SHOW);

		HWND hWndCameraListThumbs    = GetDlgItem(pcd->hWndDCRawThumbs,    IDC_RAW_LIST_CAMERAS);
		HWND hWndCameraListViewers   = GetDlgItem(pcd->hWndDCRawViewers,   IDC_RAW_LIST_CAMERAS);
		HWND hWndCameraListConverter = GetDlgItem(pcd->hWndDCRawConverter, IDC_RAW_LIST_CAMERAS);
		if (NULL != hWndCameraListThumbs
		&&	NULL != hWndCameraListViewers
		&&	NULL != hWndCameraListConverter)
		{
			pcd->dropTarget.AddDropWindow(hWndCameraListThumbs);
			pcd->dropTarget.AddDropWindow(hWndCameraListViewers);
			pcd->dropTarget.AddDropWindow(hWndCameraListConverter);
		}
		pcd->dropTarget.Register(pcd->hWndMainDlg);

		addSupportedCameras(GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_SUPCAMS));

		SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_SETTARGETDEVICE, NULL, 0);
		SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_AUTOURLDETECT, TRUE, 0);
		SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_SETEVENTMASK, 0, SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_GETEVENTMASK, 0, 0) | ENM_LINK);

		HRSRC hResCredits = FindResource(pcd->config.GetInstance(), MAKEINTRESOURCE(IDR_RTF_CREDITS), L"RTF");
		HGLOBAL hGlobCredits = LoadResource(pcd->config.GetInstance(), hResCredits);
		const char *szCreditsA = reinterpret_cast<const char *>(LockResource(hGlobCredits));

	//	SetDlgItemTextA(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS, szCreditsA);

		SETTEXTEX ste={0};
		ste.flags = ST_DEFAULT;
		ste.codepage = CP_ACP; // Should not be relevant with RTF input.
		SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_SETTEXTEX, reinterpret_cast<WPARAM>(&ste), reinterpret_cast<LPARAM>(szCreditsA));

		std::wstring strDCRawVersion;
		{
			std::string strDCRawVersionA;
			if (!DCRawResult::GetDCRawVersion(&strDCRawVersionA)
			||	!LeoHelpers::LeetMBtoWC(&strDCRawVersion, strDCRawVersionA.c_str()))
			{
				assert(false);
				strDCRawVersion.clear();
			}
		}

		const wchar_t *aszCreditFR[] =
		{
			L"$source$",   pcd->config.GetString(STR_DCRAW_CREDITS_SOURCE),
			L"$plugin$",   pcd->config.GetString(STR_DCRAW_CREDITS_PLUGIN),
			L"$dcrawver$", strDCRawVersion.c_str(),
			L"$rawconv$",  pcd->config.GetString(STR_DCRAW_CREDITS_CONVERSION),
			L"$jpeg$",     pcd->config.GetString(STR_DCRAW_CREDITS_JPEG)
		};

		for(size_t cfrIdx = 0; cfrIdx < _countof(aszCreditFR); cfrIdx += 2)
		{
			FINDTEXT ft={0};
			ft.chrg.cpMin = 0;
			ft.chrg.cpMax = -1;
			ft.lpstrText = aszCreditFR[cfrIdx];

			LRESULT dcrpos = SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_FINDTEXT, FR_DOWN | FR_MATCHCASE | FR_WHOLEWORD, reinterpret_cast<LPARAM>( &ft ));

			assert(dcrpos != -1);

			if (dcrpos != -1)
			{
				SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_SETSEL, dcrpos, dcrpos + wcslen(ft.lpstrText));
				SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>( aszCreditFR[cfrIdx+1] ));
				SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_SETSEL, 0, 0);
			}
		}

		LeoHelpers::CenterWindow(pcd->hWndMainDlg, GetParent(pcd->hWndMainDlg));
		LeoHelpers::EnableDlgItem(pcd->hWndMainDlg, IDC_RAW_BUTTON_APPLY, TRUE); //FALSE); <-- Leave it enabled all the time. Too much hassle otherwise.

		bLoadSettings = true;
		bResult = TRUE;
	}
}

void DCRawConfigDlg::configNotify(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (NULL != pcd)
	{
		NMHDR *pHdr = reinterpret_cast<NMHDR *>(lParam);
		switch(pHdr->code)
		{
		default:
			break;
		case(TCN_SELCHANGE):
			{
				TCITEM tabItem;
				ZeroMemory(&tabItem, sizeof(tabItem));
				tabItem.mask = TCIF_PARAM;
				if (TabCtrl_GetItem(pHdr->hwndFrom, TabCtrl_GetCurSel(pHdr->hwndFrom), &tabItem))
				{
					ShowWindow(pcd->hWndFormats,        tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndFormats)        ? SW_SHOW : SW_HIDE);
					ShowWindow(pcd->hWndDCRawThumbs,    tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawThumbs)    ? SW_SHOW : SW_HIDE);
					ShowWindow(pcd->hWndDCRawViewers,   tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawViewers)   ? SW_SHOW : SW_HIDE);
					ShowWindow(pcd->hWndDCRawConverter, tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawConverter) ? SW_SHOW : SW_HIDE);

					HWND hWndFocus = GetFocus();

					if (IsChild(pcd->hWndFormats,        hWndFocus)
					||	IsChild(pcd->hWndDCRawThumbs,    hWndFocus)
					||	IsChild(pcd->hWndDCRawViewers,   hWndFocus)
					||	IsChild(pcd->hWndDCRawConverter, hWndFocus))
					{
						SetFocus(reinterpret_cast<HWND>(tabItem.lParam));
					}

					if      (tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndFormats))        { pcd->strActivePage = L"Formats";      } // Do not translate string
					else if (tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawThumbs))    { pcd->strActivePage = L"RawThumbs";    } // Do not translate string
					else if (tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawViewers))   { pcd->strActivePage = L"RawViewers";   } // Do not translate string
					else if (tabItem.lParam == reinterpret_cast<LPARAM>(pcd->hWndDCRawConverter)) { pcd->strActivePage = L"RawConverter"; } // Do not translate string
				}
			}
			break;
		case(EN_LINK):
			{
				if (wParam == IDC_RAW_RICHEDIT_CREDITS)
				{
					ENLINK *pEnLink = reinterpret_cast<ENLINK *>(lParam);
					if (pEnLink->msg == WM_LBUTTONDOWN)
					{
						wchar_t szBuffer[1024]; // Warning: Fixed size buffer. We assume none of the URLs in our credits text are longer.

						TEXTRANGE tr;
						tr.chrg = pEnLink->chrg;
						tr.lpstrText = szBuffer;
						SendMessage(GetDlgItem(pcd->hWndFormats, IDC_RAW_RICHEDIT_CREDITS), EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));

						ShellExecute(NULL, L"open", szBuffer, NULL, NULL, SW_SHOW);
					}
				}
			}
			break;
		}
	}
}

void DCRawConfigDlg::configVScroll(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (NULL != pcd)
	{
		if (!pcd->bProcessingWMScroll)
		{
			pcd->bProcessingWMScroll = true;

			HWND hWndEdit = NULL;
			bool bDivide = true;

			if      (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_1     ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_1     ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_2     ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_2     ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_3     ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_3     ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_4     ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_4     ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_BRIGHTNESS    ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_BRIGHTNESS    ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_GAMMA_POWER   ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_GAMMA_POWER   ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_GAMMA_TOESLOPE))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_GAMMA_TOESLOPE); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_CHROMA_RED    ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_CHROMA_RED    ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_CHROMA_BLUE   ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_CHROMA_BLUE   ); }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_NOISETHRESHOLD))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_NOISETHRESHOLD); bDivide = false; }
			else if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_MEDIAN        ))) { hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_MEDIAN        ); bDivide = false; }

			if (lParam == reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_SPIN_MEDIAN)))
			{
				// "pass" or "passes"
				SetDlgItemText(hwndDlg, IDC_RAW_STATIC_MEDIAN_PASSES,
					pcd->config.GetString((HIWORD(wParam)==1) ? STR_DCRAW_PASS_SINGULAR : STR_DCRAW_PASSES_PLURAL));
			}

			wchar_t szDecSep[8]; // 8 is more than the documented max of 4 including the null.
			if (0 == ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, szDecSep, _countof(szDecSep)))
			{
				szDecSep[0] = L'.';
				szDecSep[1] = L'\0';
			}

			if (NULL != hWndEdit)
			{
				wchar_t szEdit[64];
				if (bDivide)
				{
					int wholePart = HIWORD(wParam) / 1000;
					int fracPart  = HIWORD(wParam) % 1000;
					if (fracPart < 0) { fracPart = -fracPart; }
					LeoHelpers::StringFormat(szEdit, _countof(szEdit), L"%d%s%03d", wholePart, szDecSep, fracPart);
				}
				else
				{
					DWORD dwPos = HIWORD(wParam);
					LeoHelpers::StringFormat(szEdit, _countof(szEdit), L"%d", dwPos);
				}
				SetWindowText(hWndEdit, szEdit);
				bResult = TRUE;
			}

			pcd->bProcessingWMScroll = false;
		}
	}
}

void DCRawConfigDlg::configCommand(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (EN_CHANGE == HIWORD(wParam) && NULL != pcd)
	{
		if (!pcd->bProcessingWMScroll)
		{
			HWND hWndEdit = NULL;
			HWND hWndSpin = NULL;
			bool bMultiply = true;

			switch(LOWORD(wParam))
			{
			case(IDC_RAW_EDIT_USERMUL_1):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_1);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_1);
				break;
			case(IDC_RAW_EDIT_USERMUL_2):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_2);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_2);
				break;
			case(IDC_RAW_EDIT_USERMUL_3):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_3);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_3);
				break;
			case(IDC_RAW_EDIT_USERMUL_4):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_USERMUL_4);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_USERMUL_4);
				break;
			case(IDC_RAW_EDIT_BRIGHTNESS):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_BRIGHTNESS);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_BRIGHTNESS);
				break;
			case(IDC_RAW_EDIT_GAMMA_POWER):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_GAMMA_POWER);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_GAMMA_POWER);
				break;
			case(IDC_RAW_EDIT_GAMMA_TOESLOPE):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_GAMMA_TOESLOPE);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_GAMMA_TOESLOPE);
				break;
			case(IDC_RAW_EDIT_CHROMA_RED):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_CHROMA_RED);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_CHROMA_RED);
				break;
			case(IDC_RAW_EDIT_CHROMA_BLUE):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_CHROMA_BLUE);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_CHROMA_BLUE);
				break;
			case(IDC_RAW_EDIT_NOISETHRESHOLD):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_NOISETHRESHOLD);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_NOISETHRESHOLD);
				bMultiply = false;
				break;
			case(IDC_RAW_EDIT_MEDIAN):
				hWndEdit = GetDlgItem(hwndDlg, IDC_RAW_EDIT_MEDIAN);
				hWndSpin = GetDlgItem(hwndDlg, IDC_RAW_SPIN_MEDIAN);
				bMultiply = false;
				break;
			case IDC_RAW_EDIT_RAW_EXTENSION:
				bEnableDisable = true;
				bResult = TRUE;
				break;
			default:
				break;
			}

			if (NULL != hWndEdit && NULL != hWndSpin)
			{
				wchar_t szEdit[64];
				GetWindowText(hWndEdit, szEdit, _countof(szEdit));
				szEdit[_countof(szEdit) - 1] = L'\0';

				int iPos = 0;

				if (bMultiply)
				{
					// Convert string into number out of 1000, avoiding floating-point rounding issues by not using floating-point numbers.
					// (e.g. "atof("1.001") * 1000.0" gives the result 1000 not 1001.)

					wchar_t szDecSep[8]; // 8 is more than the documented max of 4 including the null.
					if (0 == ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, szDecSep, _countof(szDecSep)))
					{
						szDecSep[0] = L'.';
						szDecSep[1] = L'\0';
					}

					wchar_t *szDec = wcsstr(szEdit, szDecSep);
					if (NULL != szDec)
					{
						*szDec = L'\0';
					}
					iPos = _wtoi(szEdit) * 1000;
					if (NULL != szDec)
					{
						wchar_t szFracPart[4];
						szFracPart[0]=L'0';
						szFracPart[1]=L'0';
						szFracPart[2]=L'0';
						szFracPart[3]=L'\0';

						szDec += wcslen(szDecSep);
						for(size_t i = 0; i < _countof(szFracPart) && iswdigit(szDec[i]); ++i)
						{
							szFracPart[i] = szDec[i];
						}

						iPos += _wtoi(szFracPart);
					}
				}
				else
				{
					iPos = _wtoi(szEdit);
				}

				SendMessage(hWndSpin, UDM_SETPOS, 0, MAKELONG(iPos, 0));
				bResult = TRUE;
			}
		}
	}

	if (!bResult && NULL != pcd)
	{
		const bool bIsBnClicked = (BN_CLICKED == HIWORD(wParam)); // Meaningless if it isn't a button, but harmless to calculate once in advance.

		switch (LOWORD(wParam))
		{
		case IDC_RAW_BUTTON_OUT_ICC:
			if (bIsBnClicked)
			{
				FilenameToEdit(pcd, hwndDlg, IDC_RAW_EDIT_OUT_ICC, pcd->config.GetString(STR_DCRAW_FILEREQ_ICC_OUT)); // "Select output ICC profile"
			}
			break;
		case IDC_RAW_BUTTON_CAM_ICC:
			if (bIsBnClicked)
			{
				FilenameToEdit(pcd, hwndDlg, IDC_RAW_EDIT_CAM_ICC, pcd->config.GetString(STR_DCRAW_FILEREQ_ICC_CAM)); // "Select camera ICC profile"
			}
			break;
		case IDC_RAW_BUTTON_BADPIXELS:
			if (bIsBnClicked)
			{
				FilenameToEdit(pcd, hwndDlg, IDC_RAW_EDIT_BADPIXELS, pcd->config.GetString(STR_DCRAW_FILEREQ_BADPIXELS)); // "Select bad pixels file"
			}
			break;
		case IDC_RAW_BUTTON_BADPIXELSHELP:
			if (bIsBnClicked)
			{
				MessageBox(hwndDlg,
					pcd->config.GetString(STR_DCRAW_HELP_BADPIXELS),
					pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
					MB_OK | MB_ICONINFORMATION);
			}
			break;
#ifdef _DEBUG
		case IDC_RAW_BUTTON_DEBUGCMD:
			if (bIsBnClicked)
			{
				debugCmdLine(pcd, hwndDlg);
			}
			break;
#endif
		case IDC_RAW_BUTTON_COPY:
			if (bIsBnClicked)
			{
				RawToClipboard(pcd, hwndDlg);
				bEnableDisable = true;
			}
			break;
		case IDC_RAW_BUTTON_PASTE:
			if (bIsBnClicked)
			{
				RawFromClipboard(pcd, hwndDlg);
				bEnableDisable = true;
			}
			break;
		case IDC_RAW_LIST_CAMERAS:
			if (LBN_SELCANCEL == HIWORD(wParam) || LBN_SELCHANGE == HIWORD(wParam))
			{
				StoreRawDialogData(pcd, DCR_RawSettings::DP_THUMBS);
				StoreRawDialogData(pcd, DCR_RawSettings::DP_VIEWERS);
				StoreRawDialogData(pcd, DCR_RawSettings::DP_CONVERTER);

				int idxNewSel = static_cast<int>( (LBN_SELCANCEL == HIWORD(wParam) ? 0 : SendMessage(reinterpret_cast<HWND>(lParam), LB_GETCURSEL, 0, 0)) );

				pcd->strSelectedProfileSafeName = reinterpret_cast<const wchar_t *>(SendMessage(reinterpret_cast<HWND>(lParam), LB_GETITEMDATA, idxNewSel, 0));
				SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
				SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
				SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);

				bEnableDisable = true;
			}
			break;
		case IDC_RAW_BUTTON_ADDCAMERA:
			if (bIsBnClicked)
			{

				// "Select one or more sample raw images (you can also drop them on the profile list)"
				FilenamesToVector(pcd, hwndDlg,  &pcd->dropTarget.GetFilenames(), false, pcd->config.GetString(STR_DCRAW_FILEREQ_ADDRAWPROFILE));

				// Note: This is a non-standard use of the WM_DROPFILES message.
				PostMessage(pcd->hWndMainDlg, WM_DROPFILES, 0, reinterpret_cast<LPARAM>(GetDlgItem(hwndDlg, IDC_RAW_LIST_CAMERAS)));
			}
			break;
		case IDC_RAW_BUTTON_REMOVECAMERA:
			if (bIsBnClicked && !pcd->strSelectedProfileSafeName.empty()) // Can't remove the default (empty-name) profile.
			{
				HWND hWndList = GetDlgItem(pcd->hWndDCRawThumbs, IDC_RAW_LIST_CAMERAS);

				if (NULL != hWndList)
				{
					int idxCurSel = static_cast<int>( SendMessage(hWndList, LB_GETCURSEL, 0, 0) );
					int iTotalItems = static_cast<int>( SendMessage(hWndList, LB_GETCOUNT, 0, 0) );

					pcd->config.RemoveProfileNoLock(pcd->strSelectedProfileSafeName);

					if ((idxCurSel + 1) == iTotalItems)
					{
						idxCurSel--;
					}
					else
					{
						idxCurSel++;
					}

					pcd->strSelectedProfileSafeName = reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, idxCurSel, 0));
					SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
					SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
					SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);

					bEnableDisable = true;
				}
			}
			break;
		case IDC_RAW_CHECK_DCRAW:
		case IDC_RAW_CHECK_PNM:
		case IDC_RAW_CHECK_BADPIXELS:
		case IDC_RAW_CHECK_NOISETHRESHOLD:
		case IDC_RAW_CHECK_MEDIAN:
			if (bIsBnClicked)
			{
				bEnableDisable = true;
				bResult = TRUE;
			}
			break;
		case IDC_RAW_COMBO_DISPLAYEDIMAGE:
		case IDC_RAW_COMBO_OUT_ICC:
		case IDC_RAW_COMBO_CAM_ICC:
		case IDC_RAW_COMBO_WHITEBALANCE:
		case IDC_RAW_COMBO_GAMMA:
			if (HIWORD(wParam) == CBN_SELENDOK) //CBN_EDITCHANGE)
			{
				bEnableDisable = true;
				bResult = TRUE;
			}
			break;
		case IDC_RAW_LIST_RAW_EXTENSIONS:
			switch(HIWORD(wParam))
			{
			case LBN_SELCANCEL:
			case LBN_SELCHANGE:
				{
					HWND hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS);
					if (NULL != hwndListBox)
					{
						bEnableDisable = true;
						bResult = TRUE;

						LRESULT selCount = SendMessage(hwndListBox, LB_GETSELCOUNT, 0, 0);
						if (0 < selCount)
						{
							int *pSelItems = new int[ selCount ];

							if (selCount == SendMessage(hwndListBox, LB_GETSELITEMS, selCount, reinterpret_cast<LPARAM>(pSelItems)))
							{
								std::wstring strLBItem;

								if (LeoHelpers::GetListBoxItemText(hwndListBox, pSelItems[0], &strLBItem) && (!strLBItem.empty()))
								{
									SetDlgItemText(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION, strLBItem.c_str());
								}
							}

							delete [] pSelItems;
						}
					}
				}
				break;
			default:
				break;
			}
			break;
		case IDC_RAW_BUTTON_REMOVE_RAW_EXTENSION:
			if (bIsBnClicked)
			{
				bEnableDisable = true;

				HWND hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS);
				if (NULL != hwndListBox)
				{
					LeoHelpers::EnableDlgItem(pcd->hWndFormats, LOWORD(wParam), FALSE);

					LRESULT selCount = SendMessage(hwndListBox, LB_GETSELCOUNT, 0, 0);
					if (0 < selCount)
					{
						int *pSelItems = new int[ selCount ];

						if (selCount == SendMessage(hwndListBox, LB_GETSELITEMS, selCount, reinterpret_cast<LPARAM>(pSelItems)))
						{
							int iDeleteCount = 0;

							for(LRESULT i = 0; i < selCount; i++)
							{
								if (0 < SendMessage(hwndListBox, LB_DELETESTRING, pSelItems[i] - iDeleteCount, 0))
								{
									iDeleteCount++;
								}
								else
								{
									break;
								}
							}
						}

						delete [] pSelItems;
					}
				}
			}
			break;
		case IDC_RAW_BUTTON_ADD_RAW_EXTENSION:
			if (bIsBnClicked)
			{
				LONG_PTR lStyleDisabled = WS_DISABLED;
				LONG_PTR lButtonStyle;

				if ((!LeoHelpers::GetDlgItemLongPtr(pcd->hWndFormats, IDC_RAW_BUTTON_ADD_RAW_EXTENSION, GWL_STYLE, &lButtonStyle))
				||	(lButtonStyle & lStyleDisabled))
				{
					MessageBeep(-1);
				}
				else
				{
					bEnableDisable = true;

					std::wstring strText;
					if (LeoHelpers::GetDlgItemText(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION, &strText) && !strText.empty())
					{
						DCRawConfig::ProcessExtension(&strText, true);

						HWND hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS);
						if (NULL != hwndListBox && !strText.empty())
						{
							SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strText.c_str()));
							SetDlgItemText(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION, L"");
						}
					}
				}
			}
			break;
		case IDC_RAW_BUTTON_DEFAULTS:
			if (bIsBnClicked)
			{
				// "This will reset all Raw plugin configuration pages. All customized camera profiles will be removed."
				if (IDOK == MessageBox(pcd->hWndMainDlg,
								pcd->config.GetString(STR_DCRAW_DEFAULTS_RESET),
								pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
								MB_OKCANCEL|MB_ICONEXCLAMATION))
				{
					bLoadDefaultSettings = true;
					bResult = TRUE;
				}
			}
			break;
		case IDC_RAW_BUTTON_APPLY:
			if (bIsBnClicked)
			{
				//LeoHelpers::EnableDlgItem(pcd->hWndMainDlg, IDC_RAW_BUTTON_APPLY, FALSE);
				bSaveSettings = true;
				bResult = TRUE;
			}
			break;
		case IDOK:
			if (bIsBnClicked)
			{
				HWND hwndFocus = GetFocus();

				if (NULL != hwndFocus && NULL != pcd)
				{
					if (hwndFocus == GetDlgItem(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION))
					{
						SendMessage(pcd->hWndFormats, WM_COMMAND, IDC_RAW_BUTTON_ADD_RAW_EXTENSION, reinterpret_cast<LPARAM>(GetDlgItem(pcd->hWndFormats, IDC_RAW_BUTTON_ADD_RAW_EXTENSION)));
						bResult = TRUE;
						break;
					}
				}

				bSaveSettings = true;
			}
			// *** FALL THROUGH FROM IDOK TO IDCANCEL *** //
		case IDCANCEL:
			if (bIsBnClicked)
			{
				bDeletePCD = true;
				PostQuitMessage(0);
				bResult = TRUE;
			}
			break;
		default:
			break;
		}
	}
}

void DCRawConfigDlg::configDropFiles(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (NULL != pcd && 0 == wParam
	&&	(lParam == reinterpret_cast<LPARAM>(GetDlgItem(pcd->hWndDCRawThumbs,    IDC_RAW_LIST_CAMERAS))
	||	 lParam == reinterpret_cast<LPARAM>(GetDlgItem(pcd->hWndDCRawViewers,   IDC_RAW_LIST_CAMERAS))
	||	 lParam == reinterpret_cast<LPARAM>(GetDlgItem(pcd->hWndDCRawConverter, IDC_RAW_LIST_CAMERAS))))
	{
		// Note: This non-standard WM_DROPFILES is sent from our DropTarget class, not from the OS. i.e. We are re-using the window message.

		StoreRawDialogData(pcd, DCR_RawSettings::DP_THUMBS);
		StoreRawDialogData(pcd, DCR_RawSettings::DP_VIEWERS);
		StoreRawDialogData(pcd, DCR_RawSettings::DP_CONVERTER);

		std::vector< std::wstring > &vecFilenames = pcd->dropTarget.GetFilenames();

		int nTotalFiles = static_cast<int>( vecFilenames.size() );
		int nRecognisedFiles = 0;

		for (std::vector< std::wstring >::iterator viterPath = vecFilenames.begin(); viterPath != vecFilenames.end(); ++viterPath)
		{
			LeoHelpers::FileAndStream fas(viterPath->c_str(), static_cast<HANDLE>(NULL), FALSE);
			
			std::wstring strMake;
			std::wstring strModel;

			DCRawResult *pRawResult = CallDCRaw(fas, true, DCRawResult::DCRO_IDENTIFY, NULL);

			if (pRawResult
			&&	!pRawResult->GetMake().empty()
			&&	!pRawResult->GetModel().empty()
			&&	LeoHelpers::LeetMBtoWC(&strMake, pRawResult->GetMake().c_str())
			&&	LeoHelpers::LeetMBtoWC(&strModel, pRawResult->GetModel().c_str()))
			{
				std::wstring strDisplayName = DCRawConfig::DCR_ProfileMap::GenerateDisplayName(strMake.c_str(), strModel.c_str());

				if (!strDisplayName.empty())
				{
					std::wstring strSafeName = LeoHelpers::GenerateSafeName(strDisplayName.c_str());

					nRecognisedFiles++;

					HWND hWndList = GetDlgItem(pcd->hWndDCRawThumbs, IDC_RAW_LIST_CAMERAS);
					if (NULL != hWndList)
					{
						const wchar_t *szCurrentProfileSafeName = reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, SendMessage(hWndList, LB_GETCURSEL, 0, 0), 0));

						DCRawConfig::DCR_ProfileMap &mapProfs = pcd->config.GetProfileMapNoLock();

						DCRawConfig::DCR_ProfileMap::iterator pCurrentRawProfile = mapProfs.find(szCurrentProfileSafeName);

						DCRawConfig::DCR_ProfileMap::iterator pNewRawProfile = mapProfs.find(strSafeName.c_str());

						if (pNewRawProfile == mapProfs.end())
						{
							// Create the new profile with a copy of the current profile's settings.

							pNewRawProfile = mapProfs.insert(std::pair< std::wstring, DCRawConfig::DCR_RawProfile >(strSafeName, pCurrentRawProfile->second)).first;
							pNewRawProfile->second.strDisplayName = strDisplayName;
							pNewRawProfile->second.strSafeName = strSafeName;
						}

						pcd->strSelectedProfileSafeName = strSafeName;
						SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
						SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
						SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);

						bEnableDisable = true;
					}
				}
			}

			delete pRawResult;
			pRawResult = 0;
		}

		vecFilenames.clear();

		if (nRecognisedFiles != nTotalFiles)
		{
			const wchar_t *szMessage;

			if (nTotalFiles == 1)
			{
				szMessage = pcd->config.GetString(STR_DCRAW_ADDRAWPROFILE_ONLYUNREC); // "The file was not recognised as a raw camera image."
			}
			else if (nRecognisedFiles == 0)
			{
				szMessage = pcd->config.GetString(STR_DCRAW_ADDRAWPROFILE_ALLUNREC); // "None of the files were recognised as raw camera images."
			}
			else
			{
				szMessage = pcd->config.GetString(STR_DCRAW_ADDRAWPROFILE_SOMEUNREC); // "Not all files were recognised as raw camera images."
			}

			MessageBox(pcd->hWndMainDlg, szMessage, pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION), MB_OK|MB_ICONWARNING);
		}
	}
}

void DCRawConfigDlg::configLoadSettings(DCRawConfigDlg_Data *pcd, bool bLoadDefaultSettings, bool bInitDialog)
{
	if (pcd == NULL)
	{
		return;
	}

	if (bLoadDefaultSettings)
	{
		pcd->config.LoadDefaults();
	}

	CheckDlgButton(pcd->hWndFormats, IDC_RAW_CHECK_DCRAW, pcd->config.IsRawEnabled() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(pcd->hWndFormats, IDC_RAW_CHECK_PNM,   pcd->config.IsPnmEnabled() ? BST_CHECKED : BST_UNCHECKED);

	std::vector< std::wstring > vecStrings;
	HWND hwndListBox;

	if (NULL != (hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS)))
	{
		for (LRESULT lr = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0); 0 < lr; lr = SendMessage(hwndListBox, LB_DELETESTRING, 0, 0))
		{
		}

		pcd->config.GetExtensions(&vecStrings, false, true, false, false);
		std::sort( vecStrings.begin(), vecStrings.end() );
		for (std::vector< std::wstring >::iterator viter = vecStrings.begin(); viter != vecStrings.end(); ++viter)
		{
			DCRawConfig::ProcessExtension(&(*viter),true);
			SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(viter->c_str()));
		}
	}

	if (bInitDialog)
	{
		// Load State Data
		{
			DCRawStateData state(&pcd->config);
			if (!state.Load())
			{
				pcd->strActivePage.clear();
				pcd->strSelectedProfileSafeName.clear();
			}
			else
			{
				pcd->strActivePage              = state.GetCfgDlgPage();
				pcd->strSelectedProfileSafeName = state.GetCfgDlgRawProfile();
			}
		}

		HWND hWndTab = GetDlgItem(pcd->hWndMainDlg, IDC_RAW_TAB);
		if (NULL != hWndTab)
		{
			int activeTab = 0;

			// Do not localise the tab names. They are part of the configuration, not the actual tab labels.
			if      (0 == wcscmp(pcd->strActivePage.c_str(), L"Formats"     )) { activeTab = 0; }
			else if (0 == wcscmp(pcd->strActivePage.c_str(), L"RawThumbs"   )) { activeTab = 1; }
			else if (0 == wcscmp(pcd->strActivePage.c_str(), L"RawViewers"  )) { activeTab = 2; }
			else if (0 == wcscmp(pcd->strActivePage.c_str(), L"RawConverter")) { activeTab = 3; }

			TabCtrl_SetCurSel(hWndTab, activeTab);

			ShowWindow(pcd->hWndFormats,        (activeTab == 0) ? SW_SHOW : SW_HIDE);
			ShowWindow(pcd->hWndDCRawThumbs,    (activeTab == 1) ? SW_SHOW : SW_HIDE);
			ShowWindow(pcd->hWndDCRawViewers,   (activeTab == 2) ? SW_SHOW : SW_HIDE);
			ShowWindow(pcd->hWndDCRawConverter, (activeTab == 3) ? SW_SHOW : SW_HIDE);
		}
	}

	SetupRawDialog(pcd, DCR_RawSettings::DP_THUMBS);
	SetupRawDialog(pcd, DCR_RawSettings::DP_VIEWERS);
	SetupRawDialog(pcd, DCR_RawSettings::DP_CONVERTER);
}

void DCRawConfigDlg::configSaveSettings(DCRawConfigDlg_Data *pcd)
{
	if (pcd == NULL)
	{
		return;
	}

	pcd->config.SetRawEnabled( BST_CHECKED == IsDlgButtonChecked(pcd->hWndFormats, IDC_RAW_CHECK_DCRAW) );
	pcd->config.SetPnmEnabled( BST_CHECKED == IsDlgButtonChecked(pcd->hWndFormats, IDC_RAW_CHECK_PNM  ) );

	StoreRawDialogData(pcd, DCR_RawSettings::DP_THUMBS);
	StoreRawDialogData(pcd, DCR_RawSettings::DP_VIEWERS);
	StoreRawDialogData(pcd, DCR_RawSettings::DP_CONVERTER);

	std::vector< std::wstring > vecStrings;
	HWND hwndListBox;

	if (NULL != (hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS)))
	{
		LRESULT itemCount = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0);
		if (0 <= itemCount)
		{
			vecStrings.clear();
			for (LRESULT i = 0; i < itemCount; i++)
			{
				std::wstring strLBItem;

				if (LeoHelpers::GetListBoxItemText(hwndListBox, i, &strLBItem) && (!strLBItem.empty()))
				{
					if (strLBItem[0]==L'.')
					{
						if (strLBItem.length() > 1)
						{
							vecStrings.push_back( strLBItem.c_str() + 1 );
						}
					}
					else
					{
						vecStrings.push_back( strLBItem );
					}
				}
			}

			pcd->config.SetRawExtensions(vecStrings);
		}
	}

	// Save State Data
	{
		DCRawStateData state(&pcd->config);

		if (!state.SetCfgDlgPage(pcd->strActivePage)
		||	!state.SetCfgDlgRawProfile(pcd->strSelectedProfileSafeName)
		||	!state.Save())
		{
			assert(false);
		}
	}

	std::vector< std::wstring > vecOldExtensions;
	std::vector< std::wstring > vecNewExtensions;
	pcd->pGlobalConfig->GetExtensions(&vecOldExtensions, true, false, true, false);
	pcd->config.GetExtensions(        &vecNewExtensions, true, false, true, false);

	std::set< std::wstring > setAllExtensions;
	for (std::vector< std::wstring >::const_iterator viter = vecOldExtensions.begin(); viter != vecOldExtensions.end(); ++viter)
	{
		setAllExtensions.insert(*viter);
	}
	for (std::vector< std::wstring >::const_iterator viter = vecNewExtensions.begin(); viter != vecNewExtensions.end(); ++viter)
	{
		setAllExtensions.insert(*viter);
	}

	*pcd->pGlobalConfig = pcd->config;

	if (!pcd->config.Save()) // Doesn't matter which one we save as they're identical. Saving our one means we don't need to guard against any external modifications being saved by mistake (not that anything else modifies the config at the moment).
	{
		MessageBox(pcd->hWndMainDlg,
			pcd->config.GetString(STR_ACTIVEX_MESSAGE_BOX_CONFIG_SAVE_ERROR),
			pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
			MB_OK|MB_ICONEXCLAMATION);
	}	

	HWND  hWndNotify	= (NULL == pcd) ? NULL : pcd->hWndNotify;
	DWORD dwNotifyData	= (NULL == pcd) ? 0    : pcd->dwNotifyData;

	if (NULL != hWndNotify)
	{
		PostMessage(pcd->hWndNotify, DVPLUGINMSG_REINITIALIZE, 0, pcd->dwNotifyData);

		//if (bThumbnailSettingsDiffer)
		
		if (setAllExtensions.size() > 0)
		{
			// Old way: PostMessage(pcd->hWndNotify, DVPLUGINMSG_THUMBSCHANGED, DVPTCF_REDRAW | DVPTCF_FLUSHCACHE, 0);

			// Any extension that we did or do handle could need its thumbnails regenerating, so make a list of them all.
			bool bFirst = true;
			std::wstring strExtPattern = L"*.(";

			for (std::set< std::wstring >::const_iterator siter = setAllExtensions.begin(); siter != setAllExtensions.end(); ++siter)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					strExtPattern += L"|";
				}

				strExtPattern += *siter;
			}

			strExtPattern += L")";

			THUMBCACHECONTROLDATA ccd = {0};
			ccd.cbSize             = sizeof(ccd);
			ccd.iOperation         = TCCOP_EMPTY;
			ccd.pszPath            = NULL;
			ccd.pszFileNamePattern = const_cast<wchar_t *>(strExtPattern.c_str());
			ccd.hEvent             = NULL;
			ccd.dwFlags            = TCCF_REGENERATE | TCCF_NUKELEGACYTHUMBS;
			ccd.dwlCacheSize       = 0;

			pcd->config.GetOpusPluginHelper()->ThumbnailCacheControl(&ccd);
		}
	}
}

void DCRawConfigDlg::configEnableDisable(DCRawConfigDlg_Data *pcd)
{
	if (pcd == NULL)
	{
		return;
	}

	BOOL bRawEnabled = (BST_CHECKED == IsDlgButtonChecked(pcd->hWndFormats, IDC_RAW_CHECK_DCRAW) ? TRUE : FALSE);

	LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS, bRawEnabled);
	LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION,  bRawEnabled);
	LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_STATIC_SUPCAMS,      bRawEnabled);
	LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_LIST_SUPCAMS,        bRawEnabled);

	// Remove Raw Extension button
	HWND hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS);
	if (NULL != hwndListBox)
	{
		LRESULT selCount = SendMessage(hwndListBox, LB_GETSELCOUNT, 0, 0);
		if (0 >= selCount)
		{
			LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_BUTTON_REMOVE_RAW_EXTENSION, FALSE);
		}
		else
		{
			LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_BUTTON_REMOVE_RAW_EXTENSION, bRawEnabled);
		}
	}

	// Add Raw Extension button
	std::wstring strText;
	if (LeoHelpers::GetDlgItemText(pcd->hWndFormats, IDC_RAW_EDIT_RAW_EXTENSION, &strText))
	{
		HWND hwndListBox = GetDlgItem(pcd->hWndFormats, IDC_RAW_LIST_RAW_EXTENSIONS);
		if (NULL != hwndListBox)
		{
			DCRawConfig::ProcessExtension(&strText, true);

			BOOL bNotFound = TRUE;
			if (strText.empty()
			||	0 <= SendMessage(hwndListBox, LB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(strText.c_str())))
			{
				bNotFound = FALSE;
			}
			LeoHelpers::EnableDlgItem(pcd->hWndFormats, IDC_RAW_BUTTON_ADD_RAW_EXTENSION, bRawEnabled && bNotFound);
		}
	}

	// DCRaw dialogs
	EnableDisableRawDialog(pcd, bRawEnabled, DCR_RawSettings::DP_THUMBS);
	EnableDisableRawDialog(pcd, bRawEnabled, DCR_RawSettings::DP_VIEWERS);
	EnableDisableRawDialog(pcd, bRawEnabled, DCR_RawSettings::DP_CONVERTER);
}

void DCRawConfigDlg::SetupRawDialog(DCRawConfigDlg_Data *pcd, DCR_RawSettings::Purpose purp)
{
	if (pcd == NULL)
	{
		return;
	}

	HWND hWndDCRaw = NULL;
	
	switch(purp)
	{
	case DCR_RawSettings::DP_THUMBS:	hWndDCRaw = pcd->hWndDCRawThumbs;		break;
	case DCR_RawSettings::DP_VIEWERS:	hWndDCRaw = pcd->hWndDCRawViewers;		break;
	case DCR_RawSettings::DP_CONVERTER:	hWndDCRaw = pcd->hWndDCRawConverter;	break;
	default: assert(false); return;
	}

	HWND hWndList = GetDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS);

	bool bLoadedOne = false;
	bool bAddedOne = false;

	std::wstring strDefaultDisplayName = pcd->config.GetString(STR_DCRAW_DEFAULT_PROFILE_NAME); // "< Default >"

	if (NULL != hWndList)
	{
		SendMessage(hWndList, LB_RESETCONTENT, 0, 0);

		DCRawConfig::DCR_ProfileMap &mapProfiles = pcd->config.GetProfileMapNoLock();

		for (DCRawConfig::DCR_ProfileMap::const_iterator miter = mapProfiles.begin(); miter != mapProfiles.end(); ++miter)
		{
			const wchar_t *szDisplayName = (miter->second.strSafeName.empty() ? strDefaultDisplayName.c_str() : miter->second.strDisplayName.c_str());

			LRESULT idx = SendMessage(hWndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szDisplayName));

			if (idx >= 0)
			{
				SendMessage(hWndList, LB_SETITEMDATA, idx, reinterpret_cast<LPARAM>(miter->second.strSafeName.c_str()));
				bAddedOne = true;

				if (0 == wcscmp(miter->second.strSafeName.c_str(), pcd->strSelectedProfileSafeName.c_str()))
				{
					SendMessage(hWndList, LB_SETCURSEL, idx, 0);
					SetupRawProfileInDialog(pcd, miter->second, purp);
					bLoadedOne = true;
				}
			}
		}

		if (bAddedOne && !bLoadedOne)
		{
			SendMessage(hWndList, LB_SETCURSEL, 0, 0);
			SetupRawProfileInDialog(pcd, mapProfiles.begin()->second, purp);

			// Whatever should have been selected isn't there, so make a note of what actually has been selected.
			pcd->strSelectedProfileSafeName = reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, 0, 0));
		}
	}

	if (!bAddedOne)
	{
		assert(false); // Not even the default was added.

		pcd->strSelectedProfileSafeName.clear();
	}
}

void DCRawConfigDlg::SetupRawProfileInDialog(DCRawConfigDlg_Data *pcd, const DCRawConfig::DCR_RawProfile &rawProfile, DCR_RawSettings::Purpose purp)
{
	HWND hWndDCRaw = NULL;
	const DCR_RawSettings *prs = NULL;
	const wchar_t *szHeadingDefault = NULL;
	const wchar_t *szHeadingCamera = NULL;

	switch(purp)
	{
	case DCR_RawSettings::DP_THUMBS:
		hWndDCRaw = pcd->hWndDCRawThumbs;
		prs = &rawProfile.rsThumbs;
		szHeadingDefault = pcd->config.GetString(STR_DCRAW_HEADING_THUMBNAIL_DEFAULTS); //"Thumbnail defaults:"
		szHeadingCamera = pcd->config.GetString(STR_DCRAW_HEADING_THUMBNAIL_SETTINGS); // "Thumbnail settings for %s:"
		break;

	case DCR_RawSettings::DP_VIEWERS:
		hWndDCRaw = pcd->hWndDCRawViewers;
		prs = &rawProfile.rsViewers;
		szHeadingDefault = pcd->config.GetString(STR_DCRAW_HEADING_VIEWER_DEFAULTS); // "Viewer defaults:"
		szHeadingCamera = pcd->config.GetString(STR_DCRAW_HEADING_VIEWER_SETTINGS); // "Viewer settings for %s:"
		break;

	case DCR_RawSettings::DP_CONVERTER:
		hWndDCRaw = pcd->hWndDCRawConverter;
		prs = &rawProfile.rsConverter;
		szHeadingDefault = pcd->config.GetString(STR_DCRAW_HEADING_CONVERTER_DEFAULTS); // "Converter defaults:"
		szHeadingCamera = pcd->config.GetString(STR_DCRAW_HEADING_CONVERTER_SETTINGS); // "Converter settings for %s:"
		break;

	default:
		assert(false);
		return;
	}

	const DCR_RawSettings &rs = *prs;

	// The main point of this heading is to display the full camera name when it is wider than our fixed-width list box. :(

	if (rawProfile.strSafeName.empty())
	{
		SetDlgItemText(hWndDCRaw, IDC_RAW_STATIC_CAMERA_NAME_HEADING, szHeadingDefault);
	}
	else
	{
		wchar_t *szMsg = LeoHelpers::StringAllocAndFormat(szHeadingCamera, rawProfile.strDisplayName.c_str());

		SetDlgItemText(hWndDCRaw, IDC_RAW_STATIC_CAMERA_NAME_HEADING, szMsg ? szMsg : L"");

		delete[] szMsg;
	}

	DWORD dwDISelect = (rs.bTryPreview ? CF_DI_PREVIEW : 0) | (rs.bTryFull ? CF_DI_FULL : 0);
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, true,  0,                        dwDISelect, pcd->config.GetString(STR_DCRAW_COMBO_IMAGEMODE_DISABLED)); // "Disabled"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, false, CF_DI_PREVIEW,            dwDISelect, pcd->config.GetString(STR_DCRAW_COMBO_IMAGEMODE_PREVIEW));  // "Fast preview if available")
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, false, CF_DI_PREVIEW|CF_DI_FULL, dwDISelect, pcd->config.GetString(STR_DCRAW_COMBO_IMAGEMODE_BOTH));     // "Fast preview if available, else full decode")
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, false, CF_DI_FULL,               dwDISelect, pcd->config.GetString(STR_DCRAW_COMBO_IMAGEMODE_DECODE));   // "Full decode")

#ifdef _DEBUG
	LeoHelpers::ShowDlgItem(hWndDCRaw, IDC_RAW_BUTTON_DEBUGCMD, TRUE);
#else
	LeoHelpers::ShowDlgItem(hWndDCRaw, IDC_RAW_BUTTON_DEBUGCMD, FALSE);
#endif

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, true,   -1, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_AUTOMATIC)); // "Automatic"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,   0, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_NONE));      // "None"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,  90, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_90));        // "90° CW"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false, 180, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_180));       // "180°"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false, 270, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_270));       // "270° CW"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,   1, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_FLIP_H));    // "Flip Horizontal"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,   2, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_FLIP_V));    // "Flip Vertical"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,   4, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_FLIP_T));    // "Transpose"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION, false,   7, rs.iRotation, pcd->config.GetString(STR_DCRAW_COMBO_ROTATION_FLIP_HVT));  // "Horizontal + Vertical + Transpose"

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_CORRECTGEOMETRY, rs.bCorrectGeometry);

	DWORD dwInterpCombo = rs.iInterpQuality;
	if      (rs.bDocModeNoCol)  { dwInterpCombo = -2; }
	else if (rs.bDocModeRaw)    { dwInterpCombo = -3; }
	else if (rs.bHalfSizeColor) { dwInterpCombo = -4; }

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, true,  -1, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_AUTOMATIC)); // "Automatic"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false,  0, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_BILINEAR));  // "Bilinear (high speed, low quality)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false,  1, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_VNG));       // "VNG: Variable Number of Gradients"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false,  2, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_PPG));       // "PPG: Patterned Pixel Grouping"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false,  3, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_AHD));       // "AHD: Adaptive Homogeneity-Directed"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false, -4, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_QUARTER));   // "Quarter size (very fast)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false, -2, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_GRAY1));     // "Grayscale w/o interpolation"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, false, -3, dwInterpCombo, pcd->config.GetString(STR_DCRAW_COMBO_INTERP_GRAY2));     // "Grayscale w/o int. or color scaling"

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_RGBASFOUR, rs.bRGGB);

	DWORD dwWhiteBalanceCombo = 0;
	// Don't combine the flags as that can result in invalid combinations (e.g. Cam+Auto+Set) which we don't have a drop-down item for.
	if      (rs.bCamWhite && rs.bAutoWhite) { dwWhiteBalanceCombo = CF_WB_CamAuto; }
	else if (rs.bCamWhite && rs.bSetWhite ) { dwWhiteBalanceCombo = CF_WB_CamSet;  }
	else if (rs.bCamWhite                 ) { dwWhiteBalanceCombo = CF_WB_Cam;     }
	else if (                rs.bAutoWhite) { dwWhiteBalanceCombo = CF_WB_Auto;    }
	else if (                rs.bSetWhite ) { dwWhiteBalanceCombo = CF_WB_Set;     }
	else                                    { dwWhiteBalanceCombo = CF_WB_None;    }

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, true,  CF_WB_CamAuto, dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_CAM_ELSE_AVE));  // "Camera specified, else average calc."
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, false, CF_WB_CamSet,  dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_CAM_ELSE_USER)); // "Camera specified, else user specified"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, false, CF_WB_Cam,     dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_CAM_ELSE_NONE)); // "Camera specified, else none"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, false, CF_WB_Auto,    dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_AVE));           // "Average calculation"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, false, CF_WB_Set,     dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_USER));          // "User specified (red, green, blue, green)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, false, CF_WB_None,    dwWhiteBalanceCombo, pcd->config.GetString(STR_DCRAW_COMBO_WHITEBAL_NONE));          // "None"

	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_1, 10000, 1, rs.iUserMul1); // Min/Max chosen somewhat arbitrarily.
	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_2, 10000, 1, rs.iUserMul2); // Min/Max chosen somewhat arbitrarily.
	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_3, 10000, 1, rs.iUserMul3); // Min/Max chosen somewhat arbitrarily.
	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_4, 10000, 1, rs.iUserMul4); // Min/Max chosen somewhat arbitrarily.

	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_BRIGHTNESS, 10000, 1, rs.iBrightness); // Min/Max chosen somewhat arbitrarily.

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_FIXEDWHITELEVEL, rs.bFixedWhite);

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, true,  0, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_CLIP));   // "Clip all to solid white (default)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 1, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_UNCLIP)); // "Leave unclipped in shades of pink"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 2, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_BLEND));  // "Blend clipped and unclipped"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 3, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_3));  // "Reconstruct (H3 - favor whites)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 4, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_4));  // "Reconstruct (H4)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 5, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_5));  // "Reconstruct (H5)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 6, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_6));  // "Reconstruct (H6)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 7, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_7));  // "Reconstruct (H7)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 8, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_8));  // "Reconstruct (H8)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, false, 9, rs.iHighlightMode, pcd->config.GetString(STR_DCRAW_COMBO_HIGHLIGHTS_REC_9));  // "Reconstruct (H9 - favor colors)"

	DWORD dwGammaMode = CF_G_CUSTOM;
	if (rs.iGammaPower    == DCR_RawSettings::GAMMA_BT709_POWER
	&&	rs.iGammaToeSlope == DCR_RawSettings::GAMMA_BT709_TOESLOPE)
	{
		dwGammaMode = CF_G_BT709;
	}
	else
	if (rs.iGammaPower    == DCR_RawSettings::GAMMA_SRGB_POWER
	&&	rs.iGammaToeSlope == DCR_RawSettings::GAMMA_SRGB_TOESLOPE)
	{
		dwGammaMode = CF_G_SRGB;
	}

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_GAMMA, true,  CF_G_BT709,  dwGammaMode, pcd->config.GetString(STR_DCRAW_COMBO_GAMMA_BT709));  // "BT.709 (default)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_GAMMA, false, CF_G_SRGB,   dwGammaMode, pcd->config.GetString(STR_DCRAW_COMBO_GAMMA_SRGB));   // "sRGB"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_GAMMA, false, CF_G_CUSTOM, dwGammaMode, pcd->config.GetString(STR_DCRAW_COMBO_GAMMA_CUSTOM)); // "Custom"

	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_POWER,     10000, 1, rs.iGammaPower);    // Min/Max chosen somewhat arbitrarily.
	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_TOESLOPE, 100000, 1, rs.iGammaToeSlope); // Min/Max chosen somewhat arbitrarily.

	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_RED,  10000, 1, rs.iChromaRed);  // Min/Max chosen somewhat arbitrarily.
	LeoHelpers::SetupSpinItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_BLUE, 10000, 1, rs.iChromaBlue); // Min/Max chosen somewhat arbitrarily.

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, true,   0, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_RAW));   // "Raw color (unique to each camera)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false,  1, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_SRGB));  // "sRGB D65 (default)"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false,  2, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_ADOBE)); // "Adobe RGB (1998) D65"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false,  3, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_WIDE));  // "Wide Gamut RGB D65"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false,  4, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_KODAK)); // "Kodak ProPhoto RGB D65"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false,  5, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_XYZ));   // "XYZ"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, false, -1, rs.iOutIccType, pcd->config.GetString(STR_DCRAW_COMBO_OUTICC_FILE));  // "Load ICC profile from file..."

	LeoHelpers::SetupEditItem(hWndDCRaw, IDC_RAW_EDIT_OUT_ICC, rs.strOutIcc.c_str());

	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC, true,   0, rs.iCamIccType, pcd->config.GetString(STR_DCRAW_COMBO_CAMICC_NONE));  // "None"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC, false,  1, rs.iCamIccType, pcd->config.GetString(STR_DCRAW_COMBO_CAMICC_EMBED)); // "Embedded profile in raw files"
	LeoHelpers::SetupComboItem(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC, false, -1, rs.iCamIccType, pcd->config.GetString(STR_DCRAW_COMBO_CAMICC_FILE));  // "Load ICC profile from file..."

	LeoHelpers::SetupEditItem(hWndDCRaw, IDC_RAW_EDIT_CAM_ICC, rs.strCamIcc.c_str());

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_BADPIXELS, rs.bBadPixels);
	LeoHelpers::SetupEditItem(hWndDCRaw, IDC_RAW_EDIT_BADPIXELS, rs.strBadPixelsPath.c_str());

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_NOISETHRESHOLD,           rs.bNoiseFilter);
	LeoHelpers::SetupSpinItem(    hWndDCRaw, IDC_RAW_SPIN_NOISETHRESHOLD,  10000, 1, rs.iNoiseThreshold); // Min/Max chosen somewhat arbitrarily.

	LeoHelpers::SetupCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_MEDIAN,       rs.bMedianFilter);
	LeoHelpers::SetupSpinItem(    hWndDCRaw, IDC_RAW_SPIN_MEDIAN, 10, 1, rs.iMedianPasses); // Min/Max chosen somewhat arbitrarily.
}

void DCRawConfigDlg::StoreRawDialogData(DCRawConfigDlg_Data *pcd, DCR_RawSettings::Purpose purp)
{
	HWND hWndDCRaw = NULL;
	
	switch(purp)
	{
	case DCR_RawSettings::DP_THUMBS:	hWndDCRaw = pcd->hWndDCRawThumbs;		break;
	case DCR_RawSettings::DP_VIEWERS:	hWndDCRaw = pcd->hWndDCRawViewers;		break;
	case DCR_RawSettings::DP_CONVERTER:	hWndDCRaw = pcd->hWndDCRawConverter;	break;
	default: assert(false); return;
	}

	HWND hWndList = GetDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS);

	if (NULL != hWndList)
	{
		const wchar_t *szProfileSafeName = pcd->strSelectedProfileSafeName.c_str(); // reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, SendMessage(hWndList, LB_GETCURSEL, 0, 0), 0));

		DCRawConfig::DCR_ProfileMap::iterator miter = pcd->config.GetProfileMapNoLock().find(szProfileSafeName);

		if (miter != pcd->config.GetProfileMapNoLock().end())
		{
			DCR_RawSettings *prs = NULL;

			switch(purp)
			{
			case DCR_RawSettings::DP_THUMBS:	prs = &(miter->second.rsThumbs);	break;
			case DCR_RawSettings::DP_VIEWERS:	prs = &(miter->second.rsViewers);	break;
			case DCR_RawSettings::DP_CONVERTER:	prs = &(miter->second.rsConverter);	break;
			default: assert(false); return;
			}

			DCR_RawSettings &rs = *prs;

			rs = DCR_RawSettings(purp); // Overwrite with defaults.

			DWORD dwDISelect = CF_DI_PREVIEW;
			LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, &dwDISelect);
			rs.bTryPreview = ((dwDISelect & CF_DI_PREVIEW) ? true : false);
			rs.bTryFull    = ((dwDISelect & CF_DI_FULL)    ? true : false);

			LeoHelpers::GetComboItemIntCast(hWndDCRaw, IDC_RAW_COMBO_ROTATION, &rs.iRotation);
			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_CORRECTGEOMETRY, &rs.bCorrectGeometry);

			DWORD dwInterpCombo = -1;
			LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY, &dwInterpCombo);
			rs.iInterpQuality = -1;
			rs.bDocModeNoCol = false;
			rs.bDocModeRaw = false;
			rs.bHalfSizeColor = false;
			if      (dwInterpCombo == -2) { rs.bDocModeNoCol = true; }
			else if (dwInterpCombo == -3) { rs.bDocModeRaw = true; }
			else if (dwInterpCombo == -4) { rs.bHalfSizeColor = true; }
			else                          { rs.iInterpQuality = dwInterpCombo; }

			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_RGBASFOUR, &rs.bRGGB);

			DWORD dwWhiteBalanceCombo = 0;
			LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, &dwWhiteBalanceCombo);
			rs.bCamWhite  = (dwWhiteBalanceCombo & CF_WB_Cam ) ? true : false;
			rs.bAutoWhite = (dwWhiteBalanceCombo & CF_WB_Auto) ? true : false;
			rs.bSetWhite  = (dwWhiteBalanceCombo & CF_WB_Set ) ? true : false;

			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_1, &rs.iUserMul1);
			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_2, &rs.iUserMul2);
			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_3, &rs.iUserMul3);
			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_4, &rs.iUserMul4);

			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_BRIGHTNESS, &rs.iBrightness);

			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_FIXEDWHITELEVEL, &rs.bFixedWhite);

			LeoHelpers::GetComboItemIntCast(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE, &rs.iHighlightMode);

			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_POWER,    &rs.iGammaPower);
			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_TOESLOPE, &rs.iGammaToeSlope);

			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_RED,  &rs.iChromaRed);
			LeoHelpers::GetSpinItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_BLUE, &rs.iChromaBlue);

			LeoHelpers::GetComboItemIntCast(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, &rs.iOutIccType);
			LeoHelpers::GetEditItem(        hWndDCRaw, IDC_RAW_EDIT_OUT_ICC,  &rs.strOutIcc);

			LeoHelpers::GetComboItemIntCast(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC, &rs.iCamIccType);
			LeoHelpers::GetEditItem(        hWndDCRaw, IDC_RAW_EDIT_CAM_ICC,  &rs.strCamIcc);

			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_BADPIXELS, &rs.bBadPixels);
			LeoHelpers::GetEditItem(    hWndDCRaw, IDC_RAW_EDIT_BADPIXELS,  &rs.strBadPixelsPath);

			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_NOISETHRESHOLD, &rs.bNoiseFilter);
			LeoHelpers::GetSpinItem(    hWndDCRaw, IDC_RAW_SPIN_NOISETHRESHOLD,  &rs.iNoiseThreshold);

			LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_MEDIAN, &rs.bMedianFilter);
			LeoHelpers::GetSpinItem(    hWndDCRaw, IDC_RAW_SPIN_MEDIAN,  &rs.iMedianPasses);
		}
	}
}

void DCRawConfigDlg::EnableDisableRawDialog(DCRawConfigDlg_Data *pcd, BOOL bRawEnabled, DCR_RawSettings::Purpose purp)
{
	HWND hWndDCRaw = NULL;

	switch(purp)
	{
	case DCR_RawSettings::DP_THUMBS:	hWndDCRaw = pcd->hWndDCRawThumbs;		break;
	case DCR_RawSettings::DP_VIEWERS:	hWndDCRaw = pcd->hWndDCRawViewers;		break;
	case DCR_RawSettings::DP_CONVERTER:	hWndDCRaw = pcd->hWndDCRawConverter;	break;
	default: assert(false); return;
	}

	DWORD dwTemp;
	bool bTemp;

	dwTemp = CF_DI_PREVIEW;
	LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE, &dwTemp);
	BOOL bTryPreview = bRawEnabled && ((dwTemp & CF_DI_PREVIEW) ? true : false);
	BOOL bTryFull    = bRawEnabled && ((dwTemp & CF_DI_FULL)    ? true : false);

	dwTemp = CF_WB_CamAuto;
	LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE, &dwTemp);
	BOOL bSetWhite = (dwTemp & CF_WB_Set) ? TRUE : FALSE;

	dwTemp = 1; LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC, &dwTemp); BOOL bOutIccFile = (dwTemp == -1 ? TRUE : FALSE);
	dwTemp = 0; LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC, &dwTemp); BOOL bCamIccFile = (dwTemp == -1 ? TRUE : FALSE);

	bTemp = false; LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_BADPIXELS,      &bTemp); BOOL bBadPixels      = (bTemp ? TRUE : FALSE);
	bTemp = false; LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_NOISETHRESHOLD, &bTemp); BOOL bNoiseThreshold = (bTemp ? TRUE : FALSE);
	bTemp = false; LeoHelpers::GetCheckBoxItem(hWndDCRaw, IDC_RAW_CHECK_MEDIAN,         &bTemp); BOOL bMedian         = (bTemp ? TRUE : FALSE);

	BOOL bCustomGamma = FALSE;
	dwTemp = CF_G_BT709;
	LeoHelpers::GetComboItem(hWndDCRaw, IDC_RAW_COMBO_GAMMA, &dwTemp);
	switch(dwTemp)
	{
	default:
	case CF_G_BT709:
		LeoHelpers::SetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_POWER,    DCR_RawSettings::GAMMA_BT709_POWER);
		LeoHelpers::SetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_TOESLOPE, DCR_RawSettings::GAMMA_BT709_TOESLOPE);
		break;
	case CF_G_SRGB:
		LeoHelpers::SetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_POWER,    DCR_RawSettings::GAMMA_SRGB_POWER);
		LeoHelpers::SetSpinItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_TOESLOPE, DCR_RawSettings::GAMMA_SRGB_TOESLOPE);
		break;
	case CF_G_CUSTOM:
		bCustomGamma = TRUE;
		break;
	}

	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CAMERAS,			bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS,				bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_ADDCAMERA,			bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_REMOVECAMERA,		(bRawEnabled && !pcd->strSelectedProfileSafeName.empty() ? TRUE : FALSE));
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CAMERA_NAME_HEADING,bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_COPY,				bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_PASTE,				bRawEnabled);
#ifdef _DEBUG
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_DEBUGCMD,			bRawEnabled);
#else
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_DEBUGCMD,			FALSE);
#endif
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_DISPLAYEDIMAGE,		bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_DISPLAYEDIMAGE,		bRawEnabled);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_ROTATION,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_ROTATION,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_CORRECTGEOMETRY,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_INTERPQUALITY,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_INTERPQUALITY,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_RGBASFOUR,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_WHITEBALANCE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_WHITEBALANCE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_USERMUL_1,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_1,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_USERMUL_2,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_2,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_USERMUL_3,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_3,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_USERMUL_4,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_USERMUL_4,			bTryFull && bSetWhite);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_BRIGHTNESS,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_BRIGHTNESS,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_BRIGHTNESS,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_FIXEDWHITELEVEL,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_HIGHLIGHTMODE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_HIGHLIGHTMODE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_GAMMA,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_GAMMA,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_GAMMA_POWER,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_GAMMA_POWER,			bTryFull && bCustomGamma);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_POWER,			bTryFull && bCustomGamma);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_GAMMA_TOESLOPE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_GAMMA_TOESLOPE,		bTryFull && bCustomGamma);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_GAMMA_TOESLOPE,		bTryFull && bCustomGamma);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CHROMA,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_CHROMA_RED,           bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_RED,           bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CHROMA_RED,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_CHROMA_BLUE,          bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_CHROMA_BLUE,          bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CHROMA_BLUE,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_OUT_ICC,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_OUT_ICC,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_OUT_ICC,				bTryFull && bOutIccFile);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_OUT_ICC,			bTryFull && bOutIccFile);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_CAM_ICC,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_COMBO_CAM_ICC,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_CAM_ICC,				bTryFull && bCamIccFile);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_CAM_ICC,			bTryFull && bCamIccFile);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_BADPIXELS,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_BADPIXELS,			bTryFull && bBadPixels);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_BADPIXELS,			bTryFull && bBadPixels);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_BUTTON_BADPIXELSHELP,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_FILTERS,			bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_NOISETHRESHOLD,		bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_NOISETHRESHOLD,		bTryFull && bNoiseThreshold);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_NOISETHRESHOLD,		bTryFull && bNoiseThreshold);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_CHECK_MEDIAN,				bTryFull);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_EDIT_MEDIAN,				bTryFull && bMedian);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_SPIN_MEDIAN,				bTryFull && bMedian);
	LeoHelpers::EnableDlgItem(hWndDCRaw, IDC_RAW_STATIC_MEDIAN_PASSES,		bTryFull && bMedian);
}

void DCRawConfigDlg::FilenamesToVector(DCRawConfigDlg_Data *pcd, HWND hwndDlg, std::vector< std::wstring > *pvecFiles, bool bClearVec, const wchar_t *szTitle)
{
	if (bClearVec)
	{
		pvecFiles->clear();
	}

	size_t bufferSize = 50*1024;
	wchar_t *szBuffer = new wchar_t[bufferSize];
	szBuffer[0] = L'\0';

	OPENFILENAME ofn={0};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = pcd->hWndMainDlg;
	ofn.hInstance = pcd->config.GetInstance();
	ofn.lpstrFile = szBuffer;
	ofn.nMaxFile = static_cast<DWORD>(bufferSize);
	ofn.lpstrTitle = szTitle;
	ofn.Flags = OFN_EXTENSIONDIFFERENT|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_LONGNAMES|OFN_ALLOWMULTISELECT|OFN_EXPLORER;

	if (GetOpenFileName(&ofn) && ofn.nFileOffset > 0)
	{
		// GetOpenFileName with OFN_ALLOWMULTISELECT works in a dumb way:
		// If one file was selected then szBuffer will be the path to that.
		// If multiple files are selected then szBuffer is a multi-sz where the first sting is the parent
		// folder and the rest of the strings are files.
		// We can tell whether one or more were returned by checking for a null before the first filename
		// which is always indicated by ofn.nFileOffset

		if (szBuffer[ofn.nFileOffset-1]!=L'\0')
		{
			pvecFiles->push_back(szBuffer);
		}
		else
		{
			std::wstring strPath;

			for (const wchar_t *sp = szBuffer + wcslen(szBuffer) + 1; *sp != L'\0'; sp += wcslen(sp) + 1)
			{
				strPath = szBuffer;
				LeoHelpers::AppendPathString(&strPath, sp);

				pvecFiles->push_back(strPath);
			}
		}
	}

	delete[] szBuffer;
}

void DCRawConfigDlg::FilenameToEdit(DCRawConfigDlg_Data *pcd, HWND hwndDlg, int iDlgItem, const wchar_t *szTitle)
{
	wchar_t szBuffer[_MAX_PATH];
	std::wstring strCurrent;
	LeoHelpers::GetDlgItemText(hwndDlg, iDlgItem, &strCurrent);
	LeoHelpers::StringCopy(szBuffer, strCurrent.c_str(), _countof(szBuffer));

	OPENFILENAME ofn={0};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = pcd->hWndMainDlg;
	ofn.hInstance = pcd->config.GetInstance();
	ofn.lpstrFile = szBuffer;
	ofn.nMaxFile = _countof(szBuffer);
	ofn.lpstrTitle = szTitle;
	ofn.Flags = OFN_EXTENSIONDIFFERENT|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_LONGNAMES;

	if (GetOpenFileName(&ofn))
	{
		SetDlgItemText(hwndDlg, iDlgItem, ofn.lpstrFile);
	}
}

static const wchar_t *pszSupCams[] =
{
	L"Adobe Digital Negative (DNG)",
	L"Apple QuickTake 100",
	L"Apple QuickTake 150",
	L"Apple QuickTake 200",
	L"AVT F-080C",
	L"AVT F-145C",
	L"AVT F-201C",
	L"AVT F-510C",
	L"AVT F-810C",
	L"Canon PowerShot 600",
	L"Canon PowerShot A5",
	L"Canon PowerShot A5 Zoom",
	L"Canon PowerShot A50",
	L"Canon PowerShot A460 (CHDK hack)",
	L"Canon PowerShot A530 (CHDK hack)",
	L"Canon PowerShot A610 (CHDK hack)",
	L"Canon PowerShot A620 (CHDK hack)",
	L"Canon PowerShot A630 (CHDK hack)",
	L"Canon PowerShot A640 (CHDK hack)",
	L"Canon PowerShot A650 (CHDK hack)",
	L"Canon PowerShot A710 IS (CHDK hack)",
	L"Canon PowerShot A720 IS (CHDK hack)",
	L"Canon PowerShot Pro70",
	L"Canon PowerShot Pro90 IS",
	L"Canon PowerShot G1",
	L"Canon PowerShot G2",
	L"Canon PowerShot G3",
	L"Canon PowerShot G5",
	L"Canon PowerShot G6",
	L"Canon PowerShot G7 (CHDK hack)",
	L"Canon PowerShot G9",
	L"Canon PowerShot G10",
	L"Canon PowerShot S2 IS (CHDK hack)",
	L"Canon PowerShot S3 IS (CHDK hack)",
	L"Canon PowerShot S5 IS (CHDK hack)",
	L"Canon PowerShot SD300 (CHDK hack)",
	L"Canon PowerShot S30",
	L"Canon PowerShot S40",
	L"Canon PowerShot S45",
	L"Canon PowerShot S50",
	L"Canon PowerShot S60",
	L"Canon PowerShot S70",
	L"Canon PowerShot Pro1",
	L"Canon EOS D30",
	L"Canon EOS D60",
	L"Canon EOS 5D",
	L"Canon EOS 5D Mark II",
	L"Canon EOS 10D",
	L"Canon EOS 20D",
	L"Canon EOS 30D",
	L"Canon EOS 40D",
	L"Canon EOS 50D",
	L"Canon EOS 300D / Digital Rebel / Kiss Digital",
	L"Canon EOS 350D / Digital Rebel XT / Kiss Digital N",
	L"Canon EOS 400D / Digital Rebel XTi / Kiss Digital X",
	L"Canon EOS 450D / Digital Rebel XSi / Kiss Digital X2",
	L"Canon EOS 1000D / Digital Rebel XS / Kiss Digital F",
	L"Canon EOS D2000C",
	L"Canon EOS-1D",
	L"Canon EOS-1DS",
	L"Canon EOS-1D Mark II",
	L"Canon EOS-1D Mark III",
	L"Canon EOS-1D Mark II N",
	L"Canon EOS-1Ds Mark II",
	L"Canon EOS-1Ds Mark III",
	L"Casio QV-2000UX",
	L"Casio QV-3000EX",
	L"Casio QV-3500EX",
	L"Casio QV-4000",
	L"Casio QV-5700",
	L"Casio QV-R41",
	L"Casio QV-R51",
	L"Casio QV-R61",
	L"Casio EX-S100",
	L"Casio EX-Z4",
	L"Casio EX-Z50",
	L"Casio EX-Z55",
	L"Casio Exlim Pro 505",
	L"Casio Exlim Pro 600",
	L"Casio Exlim Pro 700",
	L"Contax N Digital",
	L"Creative PC-CAM 600",
	L"Epson R-D1",
	L"Foculus 531C",
	L"Fuji FinePix E550",
	L"Fuji FinePix E900",
	L"Fuji FinePix F700",
	L"Fuji FinePix F710",
	L"Fuji FinePix F800",
	L"Fuji FinePix F810",
	L"Fuji FinePix S2Pro",
	L"Fuji FinePix S3Pro",
	L"Fuji FinePix S5Pro",
	L"Fuji FinePix S20Pro",
	L"Fuji FinePix S100FS",
	L"Fuji FinePix S5000",
	L"Fuji FinePix S5100/S5500",
	L"Fuji FinePix S5200/S5600",
	L"Fuji FinePix S6000fd",
	L"Fuji FinePix S7000",
	L"Fuji FinePix S9000/S9500",
	L"Fuji FinePix S9100/S9600",
	L"Fuji IS-1",
	L"Hasselblad CFV",
	L"Hasselblad H3D",
	L"Hasselblad V96C",
	L"Imacon Ixpress 16-megapixel",
	L"Imacon Ixpress 22-megapixel",
	L"Imacon Ixpress 39-megapixel",
	L"ISG 2020x1520",
	L"Kodak DC20 (see Oliver Hartman's page)",
	L"Kodak DC25 (see Jun-ichiro Itoh's page)",
	L"Kodak DC40",
	L"Kodak DC50",
	L"Kodak DC120 (also try kdc2tiff)",
	L"Kodak DCS200",
	L"Kodak DCS315C",
	L"Kodak DCS330C",
	L"Kodak DCS420",
	L"Kodak DCS460",
	L"Kodak DCS460A",
	L"Kodak DCS520C",
	L"Kodak DCS560C",
	L"Kodak DCS620C",
	L"Kodak DCS620X",
	L"Kodak DCS660C",
	L"Kodak DCS660M",
	L"Kodak DCS720X",
	L"Kodak DCS760C",
	L"Kodak DCS760M",
	L"Kodak EOSDCS1",
	L"Kodak EOSDCS3B",
	L"Kodak NC2000F",
	L"Kodak ProBack",
	L"Kodak PB645C",
	L"Kodak PB645H",
	L"Kodak PB645M",
	L"Kodak DCS Pro 14n",
	L"Kodak DCS Pro 14nx",
	L"Kodak DCS Pro SLR/c",
	L"Kodak DCS Pro SLR/n",
	L"Kodak C330",
	L"Kodak C603",
	L"Kodak P850",
	L"Kodak P880",
	L"Kodak KAI-0340",
	L"Konica KD-400Z",
	L"Konica KD-510Z",
	L"Leaf AFi 7",
	L"Leaf Aptus 17",
	L"Leaf Aptus 22",
	L"Leaf Aptus 54S",
	L"Leaf Aptus 65",
	L"Leaf Aptus 75",
	L"Leaf Aptus 75S",
	L"Leaf Cantare",
	L"Leaf CatchLight",
	L"Leaf CMost",
	L"Leaf DCB2",
	L"Leaf Valeo 6",
	L"Leaf Valeo 11",
	L"Leaf Valeo 17",
	L"Leaf Valeo 22",
	L"Leaf Volare",
	L"Leica Digilux 2",
	L"Leica Digilux 3",
	L"Leica D-LUX2",
	L"Leica D-LUX3",
	L"Leica D-LUX4",
	L"Leica V-LUX1",
	L"Logitech Fotoman Pixtura",
	L"Mamiya ZD",
	L"Micron 2010",
	L"Minolta RD175",
	L"Minolta DiMAGE 5",
	L"Minolta DiMAGE 7",
	L"Minolta DiMAGE 7i",
	L"Minolta DiMAGE 7Hi",
	L"Minolta DiMAGE A1",
	L"Minolta DiMAGE A2",
	L"Minolta DiMAGE A200",
	L"Minolta DiMAGE G400",
	L"Minolta DiMAGE G500",
	L"Minolta DiMAGE G530",
	L"Minolta DiMAGE G600",
	L"Minolta DiMAGE Z2",
	L"Minolta Alpha/Dynax/Maxxum 5D",
	L"Minolta Alpha/Dynax/Maxxum 7D",
	L"Nikon D1",
	L"Nikon D1H",
	L"Nikon D1X",
	L"Nikon D2H",
	L"Nikon D2Hs",
	L"Nikon D2X",
	L"Nikon D2Xs",
	L"Nikon D3",
	L"Nikon D3X",
	L"Nikon D40",
	L"Nikon D40X",
	L"Nikon D50",
	L"Nikon D60",
	L"Nikon D70",
	L"Nikon D70s",
	L"Nikon D80",
	L"Nikon D90",
	L"Nikon D100",
	L"Nikon D200",
	L"Nikon D300",
	L"Nikon D700",
	L"Nikon E700 (\"DIAG RAW\" hack)",
	L"Nikon E800 (\"DIAG RAW\" hack)",
	L"Nikon E880 (\"DIAG RAW\" hack)",
	L"Nikon E900 (\"DIAG RAW\" hack)",
	L"Nikon E950 (\"DIAG RAW\" hack)",
	L"Nikon E990 (\"DIAG RAW\" hack)",
	L"Nikon E995 (\"DIAG RAW\" hack)",
	L"Nikon E2100 (\"DIAG RAW\" hack)",
	L"Nikon E2500 (\"DIAG RAW\" hack)",
	L"Nikon E3200 (\"DIAG RAW\" hack)",
	L"Nikon E3700 (\"DIAG RAW\" hack)",
	L"Nikon E4300 (\"DIAG RAW\" hack)",
	L"Nikon E4500 (\"DIAG RAW\" hack)",
	L"Nikon E5000",
	L"Nikon E5400",
	L"Nikon E5700",
	L"Nikon E8400",
	L"Nikon E8700",
	L"Nikon E8800",
	L"Nikon Coolpix P6000",
	L"Nikon Coolpix S6 (\"DIAG RAW\" hack)",
	L"Nokia N95",
	L"Olympus C3030Z",
	L"Olympus C5050Z",
	L"Olympus C5060WZ",
	L"Olympus C7070WZ",
	L"Olympus C70Z,C7000Z",
	L"Olympus C740UZ",
	L"Olympus C770UZ",
	L"Olympus C8080WZ",
	L"Olympus E-1",
	L"Olympus E-3",
	L"Olympus E-10",
	L"Olympus E-20",
	L"Olympus E-300",
	L"Olympus E-330",
	L"Olympus E-400",
	L"Olympus E-410",
	L"Olympus E-420",
	L"Olympus E-500",
	L"Olympus E-510",
	L"Olympus E-520",
	L"Olympus SP310",
	L"Olympus SP320",
	L"Olympus SP350",
	L"Olympus SP500UZ",
	L"Olympus SP510UZ",
	L"Olympus SP550UZ",
	L"Olympus SP560UZ",
	L"Olympus SP570UZ",
	L"Panasonic DMC-FZ8",
	L"Panasonic DMC-FZ18",
	L"Panasonic DMC-FZ28",
	L"Panasonic DMC-FZ30",
	L"Panasonic DMC-FZ50",
	L"Panasonic DMC-FX150",
	L"Panasonic DMC-G1",
	L"Panasonic DMC-L1",
	L"Panasonic DMC-L10",
	L"Panasonic DMC-LC1",
	L"Panasonic DMC-LX1",
	L"Panasonic DMC-LX2",
	L"Panasonic DMC-LX3",
	L"Pentax *ist D",
	L"Pentax *ist DL",
	L"Pentax *ist DL2",
	L"Pentax *ist DS",
	L"Pentax *ist DS2",
	L"Pentax K10D",
	L"Pentax K20D",
	L"Pentax K100D",
	L"Pentax K100D Super",
	L"Pentax K200D",
	L"Pentax K2000/K-m",
	L"Pentax Optio S",
	L"Pentax Optio S4",
	L"Pentax Optio 33WR",
	L"Pentax Optio 750Z",
	L"Phase One LightPhase",
	L"Phase One H 10",
	L"Phase One H 20",
	L"Phase One H 25",
	L"Phase One P 20",
	L"Phase One P 25",
	L"Phase One P 30",
	L"Phase One P 45",
	L"Pixelink A782",
	L"Polaroid x530",
	L"Rollei d530flex",
	L"RoverShot 3320af",
	L"Samsung GX-1S",
	L"Samsung GX-10",
	L"Samsung S85 (hacked)",
	L"Sarnoff 4096x5440",
	L"Sigma SD9",
	L"Sigma SD10",
	L"Sigma SD14",
	L"Sinar 3072x2048",
	L"Sinar 4080x4080",
	L"Sinar 4080x5440",
	L"Sinar STI format",
	L"SMaL Ultra-Pocket 3",
	L"SMaL Ultra-Pocket 4",
	L"SMaL Ultra-Pocket 5",
	L"Sony DSC-F828",
	L"Sony DSC-R1",
	L"Sony DSC-V3",
	L"Sony DSLR-A100",
	L"Sony DSLR-A200",
	L"Sony DSLR-A300",
	L"Sony DSLR-A350",
	L"Sony DSLR-A700",
	L"Sony DSLR-A900",
	L"Sony XCD-SX910CR",
	L"STV680 VGA",
};

void DCRawConfigDlg::addSupportedCameras(HWND hWndList)
{
	LVCOLUMN col={0};
	col.mask = LVCF_TEXT;
	col.fmt = LVCFMT_LEFT;
	col.pszText = L"Camera Name"; // Do not localise as it is not actually shown.


	ListView_InsertColumn(hWndList, 0, &col);

	LVITEM item={0};
	item.mask = LVIF_TEXT;

	for (int i = 0; i < _countof(pszSupCams); ++i)
	{
		item.iItem = i;
		item.pszText = const_cast<wchar_t *>( pszSupCams[i] );
		ListView_InsertItem(hWndList, &item);
	}

	ListView_SetColumnWidth(hWndList, 0, LVSCW_AUTOSIZE);
}

void DCRawConfigDlg::RawToClipboard(DCRawConfigDlg_Data *pcd, HWND hwndDlg)
{
	DCR_RawSettings::Purpose purp = DCR_RawSettings::DP_UNKNOWN;

	if      (hwndDlg == pcd->hWndDCRawThumbs)		{ purp = DCR_RawSettings::DP_THUMBS;	}
	else if (hwndDlg == pcd->hWndDCRawViewers)		{ purp = DCR_RawSettings::DP_VIEWERS;	}
	else if (hwndDlg == pcd->hWndDCRawConverter)	{ purp = DCR_RawSettings::DP_CONVERTER;	}
	else											{ assert(false); return; }

	StoreRawDialogData(pcd, purp);
	SetupRawDialog(pcd, purp); // Reflect exactly what will be copied to the clipboard so the user sees if any invalid values were modified, rather than wondering why different data is coming out of the clipboard.

	HWND hWndDCRaw = hwndDlg;

	HWND hWndList = GetDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS);

	if (NULL != hWndList)
	{
		const wchar_t *szProfileSafeName = pcd->strSelectedProfileSafeName.c_str(); // reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, SendMessage(hWndList, LB_GETCURSEL, 0, 0), 0));

		DCRawConfig::DCR_ProfileMap::iterator miter = pcd->config.GetProfileMapNoLock().find(szProfileSafeName);

		if (miter != pcd->config.GetProfileMapNoLock().end())
		{
			DCR_RawSettings *prs = NULL;

			switch(purp)
			{
			case DCR_RawSettings::DP_THUMBS:	prs = &(miter->second.rsThumbs);	break;
			case DCR_RawSettings::DP_VIEWERS:	prs = &(miter->second.rsViewers);	break;
			case DCR_RawSettings::DP_CONVERTER:	prs = &(miter->second.rsConverter);	break;
			default: assert(false); return;
			}

			std::wstring strClip;

			if (!pcd->config.RawSettingsToClipboardString(&strClip, *prs)
			||	!LeoHelpers::SetClipboard(pcd->hWndMainDlg, strClip.c_str()))
			{
				// "Failed to set clipboard."
				MessageBox(hwndDlg,
					pcd->config.GetString(STR_DCRAW_MESSAGE_BOX_CLIPBOARD_SET_ERROR),
					pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
					MB_OK | MB_ICONEXCLAMATION);
			}
		}
	}
}

void DCRawConfigDlg::RawFromClipboard(DCRawConfigDlg_Data *pcd, HWND hwndDlg)
{
	DCR_RawSettings::Purpose purp = DCR_RawSettings::DP_UNKNOWN;

	if      (hwndDlg == pcd->hWndDCRawThumbs)		{ purp = DCR_RawSettings::DP_THUMBS;	}
	else if (hwndDlg == pcd->hWndDCRawViewers)		{ purp = DCR_RawSettings::DP_VIEWERS;	}
	else if (hwndDlg == pcd->hWndDCRawConverter)	{ purp = DCR_RawSettings::DP_CONVERTER;	}
	else											{ assert(false); return; }

	StoreRawDialogData(pcd, purp);

	HWND hWndDCRaw = hwndDlg;

	HWND hWndList = GetDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS);

	if (NULL != hWndList)
	{
		const wchar_t *szProfileSafeName = pcd->strSelectedProfileSafeName.c_str(); // reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, SendMessage(hWndList, LB_GETCURSEL, 0, 0), 0));

		DCRawConfig::DCR_ProfileMap::iterator miter = pcd->config.GetProfileMapNoLock().find(szProfileSafeName);

		if (miter != pcd->config.GetProfileMapNoLock().end())
		{
			DCR_RawSettings *prs = NULL;

			switch(purp)
			{
			case DCR_RawSettings::DP_THUMBS:	prs = &(miter->second.rsThumbs);	break;
			case DCR_RawSettings::DP_VIEWERS:	prs = &(miter->second.rsViewers);	break;
			case DCR_RawSettings::DP_CONVERTER:	prs = &(miter->second.rsConverter);	break;
			default: assert(false); return;
			}

			std::wstring strClip;

			bool bOpenFailed = false;

			if (!LeoHelpers::GetClipboard(pcd->hWndMainDlg, &strClip, true, &bOpenFailed)
			||	!pcd->config.RawSettingsFromClipboardString(prs, strClip, purp))
			{
				if (bOpenFailed)
				{
					// "Failed to open clipboard."
					MessageBox(hwndDlg,
						pcd->config.GetString(STR_DCRAW_MESSAGE_BOX_CLIPBOARD_OPEN_ERROR),
						pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
						MB_OK | MB_ICONEXCLAMATION);
				}
				else
				{
					// "Clipboard does not contain raw settings."
					MessageBox(hwndDlg,
						pcd->config.GetString(STR_DCRAW_MESSAGE_BOX_CLIPBOARD_CONTENT_ERROR),
						pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
						MB_OK | MB_ICONEXCLAMATION);
				}
			}

			SetupRawDialog(pcd, purp); // If there was a failure then the settings should be unchanged, but reflect what's there anyway just in case. Better for the user to see something is wrong ASAP than not tell them.
		}
	}
}

#ifdef _DEBUG
void DCRawConfigDlg::debugCmdLine(DCRawConfigDlg_Data *pcd, HWND hwndDlg)
{
	DCR_RawSettings::Purpose purp = DCR_RawSettings::DP_UNKNOWN;

	if      (hwndDlg == pcd->hWndDCRawThumbs)		{ purp = DCR_RawSettings::DP_THUMBS;	}
	else if (hwndDlg == pcd->hWndDCRawViewers)		{ purp = DCR_RawSettings::DP_VIEWERS;	}
	else if (hwndDlg == pcd->hWndDCRawConverter)	{ purp = DCR_RawSettings::DP_CONVERTER;	}
	else											{ assert(false); return; }

	StoreRawDialogData(pcd, purp);

	HWND hWndDCRaw = hwndDlg;

	HWND hWndList = GetDlgItem(hWndDCRaw, IDC_RAW_LIST_CAMERAS);

	if (NULL != hWndList)
	{
		const wchar_t *szProfileSafeName = pcd->strSelectedProfileSafeName.c_str(); // reinterpret_cast<const wchar_t *>(SendMessage(hWndList, LB_GETITEMDATA, SendMessage(hWndList, LB_GETCURSEL, 0, 0), 0));

		DCRawConfig::DCR_ProfileMap::iterator miter = pcd->config.GetProfileMapNoLock().find(szProfileSafeName);

		if (miter != pcd->config.GetProfileMapNoLock().end())
		{
			DCR_RawSettings *prs = NULL;

			switch(purp)
			{
			case DCR_RawSettings::DP_THUMBS:	prs = &(miter->second.rsThumbs);	break;
			case DCR_RawSettings::DP_VIEWERS:	prs = &(miter->second.rsViewers);	break;
			case DCR_RawSettings::DP_CONVERTER:	prs = &(miter->second.rsConverter);	break;
			default: assert(false); return;
			}

			DCR_RawSettings &rs = *prs;

			std::vector< std::string > vecCmdLine;

			DCRawResult *pRawResult =
				DCRawResult::DoFile(NULL, NULL, NULL, DCRawResult::DCRO_DEBUGCMDLINE,
									L"X:\\input_dir\\input_file", "input_file", L"X:\\input_dir",
									&rs, &vecCmdLine);

			delete pRawResult; // Should always be NULL with DCRO_DEBUGCMDLINE but doesn't hurt to delete it just in case.

			bool bSuccess = false;

			if (!vecCmdLine.empty())
			{
				std::string strCmdLine;

				for (std::vector< std::string >::const_iterator viter = vecCmdLine.begin(); viter != vecCmdLine.end(); ++viter)
				{
					if (!strCmdLine.empty())
					{
						strCmdLine+= " ";
					}

					strCmdLine += *viter;
				}

				std::wstring wstrCmdLine;
				if (LeoHelpers::LeetMBtoWC(&wstrCmdLine, strCmdLine.c_str()))
				{
					MessageBox(hwndDlg, wstrCmdLine.c_str(), pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION), MB_OK | MB_ICONINFORMATION);

					bSuccess = true;
				}
			}

			if (!bSuccess)
			{
				// "Command-line generation failed."
				MessageBox(hwndDlg,
					pcd->config.GetString(STR_DCRAW_MESSAGE_BOX_CMDLINE_GENERATION_ERROR),
					pcd->config.GetString(STR_DCRAW_PLUGIN_DESCRIPTION),
					MB_OK | MB_ICONEXCLAMATION);
			}
		}
	}
}
#endif
