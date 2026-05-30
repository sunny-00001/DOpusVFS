#include "StdAfx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "GifConfig.h"
#include "GifConfigDlg.h"
#include "GifDecoder.h"
#include "gifanim.h"

INT_PTR CALLBACK NGifConfigDlg::configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL bResult = FALSE;
	bool bQuit = false;
	bool bNcDestroy = false;
	bool bUpdateEnabledControls = false;
	bool bLoadSettings = false;
	bool bLoadDefaultSettings = false;
	bool bSaveSettings = false;
	bool bVerifyEditBoxes = false;

	#pragma warning(suppress:4312) // spurious warning due to stupid Win32 SDK header definition.
	ConfigDlgData *pcd = reinterpret_cast<ConfigDlgData *>(GetWindowLongPtr(hwndDlg, DWLP_USER));

    switch (message)
    {
	case WM_INITDIALOG:
		{
			pcd = reinterpret_cast<ConfigDlgData *>(lParam);
			#pragma warning(suppress:4244) // spurious warning due to stupid Win32 SDK header definition.
			SetWindowLongPtr(hwndDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pcd));

			bResult = configInitDialog(pcd, hwndDlg);

			bLoadSettings = true;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CHECK_MINDELAY:
		case IDC_CHECK_MAXDELAY:
			if (BN_CLICKED == HIWORD(wParam))
			{
				bUpdateEnabledControls = true;
				bResult = TRUE;
			}
			break;
		case IDC_BUTTON_DEFAULTS:
			if (BN_CLICKED == HIWORD(wParam))
			{
				bLoadDefaultSettings = true;
				bResult = TRUE;
			}
			break;
		case IDC_EDIT_MINDELAY:
		case IDC_EDIT_MAXDELAY:
			if (EN_CHANGE == HIWORD(wParam))
			{
				bVerifyEditBoxes = true;
				bResult = TRUE;
			}
			break;
		case IDC_BUTTON_APPLY:
			if (BN_CLICKED == HIWORD(wParam))
			{
				bSaveSettings = true;
				bResult = TRUE;
			}
			break;
		case IDOK:
			if (BN_CLICKED == HIWORD(wParam))
			{
				bSaveSettings = true;
				bQuit = true;
				bResult = TRUE;
			}
			break;
		case IDCANCEL:
			if (BN_CLICKED == HIWORD(wParam))
			{
				bQuit = true;
				bResult = TRUE;
			}
			break;
		default:
			break;
		}
		break;
	case WM_NCDESTROY:
		bNcDestroy = true;
		break;
    default:
		break;
	}

	if (bVerifyEditBoxes)
	{
		BOOL bSuccess;
		UINT uiVal;

		uiVal = GetDlgItemInt(hwndDlg, IDC_EDIT_MINDELAY, &bSuccess, FALSE);

		if (bSuccess && UD_MAXVAL < uiVal)
		{
			SetDlgItemInt(hwndDlg, IDC_EDIT_MINDELAY, UD_MAXVAL, FALSE);
		}

		uiVal = GetDlgItemInt(hwndDlg, IDC_EDIT_MAXDELAY, &bSuccess, FALSE);

		if (bSuccess && UD_MAXVAL < uiVal)
		{
			SetDlgItemInt(hwndDlg, IDC_EDIT_MAXDELAY, UD_MAXVAL, FALSE);
		}
	}

	if (pcd != NULL && (bLoadSettings || bLoadDefaultSettings))
	{
		if (bLoadDefaultSettings)
		{
			pcd->dialogConfig.LoadDefaults();
		}

		DWORD dwMinDelay = pcd->dialogConfig.GetMinimumFrameDelay();
		DWORD dwMaxDelay = pcd->dialogConfig.GetMaximumFrameDelay();

		CheckDlgButton(hwndDlg, IDC_CHECK_ANIMCTRL_VIEWER,  pcd->dialogConfig.GetAnimationControlsViewer()  ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_ANIMCTRL_PREVIEW, pcd->dialogConfig.GetAnimationControlsPreview() ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_SPROCKETS,        pcd->dialogConfig.GetThumbnailSprockets()       ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_MINDELAY,         0 != dwMinDelay                                 ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_MAXDELAY,         0 != dwMaxDelay                                 ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_INCREASEZEROONLY, pcd->dialogConfig.GetOnlyIncreaseZeroDelay()    ? BST_CHECKED : BST_UNCHECKED);

		SetDlgItemInt(hwndDlg, IDC_EDIT_MINDELAY, dwMinDelay, FALSE);
		SetDlgItemInt(hwndDlg, IDC_EDIT_MAXDELAY, dwMaxDelay, FALSE);

		bUpdateEnabledControls = true;
	}

	if (bUpdateEnabledControls)
	{
		HWND hwndCheckSprockets = GetDlgItem(hwndDlg, IDC_CHECK_SPROCKETS);
		HWND hwndEditMinDelay   = GetDlgItem(hwndDlg, IDC_EDIT_MINDELAY);
		HWND hwndSpinMinDelay   = GetDlgItem(hwndDlg, IDC_SPIN_MINDELAY);
		HWND hwndEditMaxDelay   = GetDlgItem(hwndDlg, IDC_EDIT_MAXDELAY);
		HWND hwndSpinMaxDelay   = GetDlgItem(hwndDlg, IDC_SPIN_MAXDELAY);
		HWND hwndCheckOnlyZero  = GetDlgItem(hwndDlg, IDC_CHECK_INCREASEZEROONLY);

		if (NULL != hwndCheckSprockets && NULL != hwndEditMinDelay && NULL != hwndSpinMinDelay && NULL != hwndEditMaxDelay && NULL != hwndSpinMaxDelay && NULL != hwndCheckOnlyZero)
		{
			LeoHelpers::EnableDlgControl(hwndEditMinDelay,  BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MINDELAY));
			LeoHelpers::EnableDlgControl(hwndSpinMinDelay,  BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MINDELAY));

			LeoHelpers::EnableDlgControl(hwndEditMaxDelay,  BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MAXDELAY));
			LeoHelpers::EnableDlgControl(hwndSpinMaxDelay,  BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MAXDELAY));

			LeoHelpers::EnableDlgControl(hwndCheckOnlyZero, BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MINDELAY));
		}
	}

	if (NULL != pcd && bSaveSettings)
	{
		BOOL bSuccess;
		UINT uiVal;

		DWORD dwMinDelay = 0;
		DWORD dwMaxDelay = 0;

		uiVal = GetDlgItemInt(hwndDlg, IDC_EDIT_MINDELAY, &bSuccess, FALSE);

		if (bSuccess && BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MINDELAY))
		{
			dwMinDelay = uiVal;
		}

		uiVal = GetDlgItemInt(hwndDlg, IDC_EDIT_MAXDELAY, &bSuccess, FALSE);

		if (bSuccess && BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_MAXDELAY))
		{
			dwMaxDelay = uiVal;
		}

		pcd->dialogConfig.SetAnimationControlsViewer( (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_ANIMCTRL_VIEWER )) ? true : false);
		pcd->dialogConfig.SetAnimationControlsPreview((BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_ANIMCTRL_PREVIEW)) ? true : false);
		pcd->dialogConfig.SetThumbnailSprockets(      (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_SPROCKETS       )) ? true : false);
		pcd->dialogConfig.SetMinimumFrameDelay(dwMinDelay);
		pcd->dialogConfig.SetMaximumFrameDelay(dwMaxDelay);
		pcd->dialogConfig.SetOnlyIncreaseZeroDelay(   (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_INCREASEZEROONLY)) ? true : false);

		bool bThumbnailSettingsDiffer = pcd->pGlobalConfig->DoThumbnailSettingsDiffer(pcd->dialogConfig);

		*pcd->pGlobalConfig = pcd->dialogConfig;

		if (!pcd->dialogConfig.Save()) // Doesn't matter which one we save as they're identical. Saving our one means we don't need to guard against any external modifications being saved by mistake (not that anything else modifies the config at the moment).
		{
			LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());
			MessageBox(hwndDlg, sl.Get(STR_ACTIVEX_MESSAGE_BOX_CONFIG_SAVE_ERROR), sl.Get(STR_GIFANIM_PLUGIN_DESCRIPTION), MB_OK|MB_ICONERROR);
		}

		if (NULL != pcd->hWndNotify)
		{
			PostMessage(pcd->hWndNotify, DVPLUGINMSG_REINITIALIZE, 0, pcd->dwNotifyData);

			if (bThumbnailSettingsDiffer)
			{
				// Old way: PostMessage(pcd->hWndNotify, DVPLUGINMSG_THUMBSCHANGED, DVPTCF_REDRAW | DVPTCF_FLUSHCACHE, 0);

				// This will only update *.gif extensions, so the odd mislabeled GIF file will be left with the old thumbnail/settings.
				// Opus could track the source/plugin GUID respondible for thumbnails but it doesn't seem worth adding 16 bytes
				// per thumb just for this rare situation.
				THUMBCACHECONTROLDATA ccd = {0};
				ccd.cbSize             = sizeof(ccd);
				ccd.iOperation         = TCCOP_EMPTY;
				ccd.pszPath            = NULL;
				ccd.pszFileNamePattern = L"*.gif";
				ccd.hEvent             = NULL;
				ccd.dwFlags            = TCCF_REGENERATE | TCCF_NUKELEGACYTHUMBS;
				ccd.dwlCacheSize       = 0;

				pcd->pGlobalConfig->GetOpusPluginHelper()->ThumbnailCacheControl(&ccd);
			}
		}
	}

	if (bQuit && NULL != pcd)
	{
		PostQuitMessage(0);
	}

	if (bNcDestroy && NULL != pcd)
	{
		delete pcd;
		SetWindowLongPtr(hwndDlg, DWLP_USER, NULL);
	}

	return(bResult);
}

BOOL NGifConfigDlg::configInitDialog(ConfigDlgData *pcd, HWND hwndDlg)
{
	BOOL bResult = TRUE;

	LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());
	::SetWindowText(hwndDlg, sl.Get(STR_GIFANIM_CONFIG_TITLE));

	HFONT hFont = pcd->hFont;

	if (hFont == NULL)
	{
		hFont = reinterpret_cast< HFONT >(::SendMessage(hwndDlg, WM_GETFONT, 0, 0));
	}
	else
	{
		::SendMessage(hwndDlg, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	}

	LeoHelpers::DialogLayoutHelper layout(hwndDlg);
	LeoHelpers::WindowMaker maker(pcd->pGlobalConfig->GetDllModule(), hwndDlg, hFont, &layout, &sl);

	RECT rcCheckboxAnimCtrlPreview;
	RECT rcCheckboxAnimCtrlViewer;
	RECT rcCheckboxSprockets;
	RECT rcCheckboxMinDelay;
	RECT rcCheckboxMaxDelay;
	RECT rcCheckboxIncreaseZeroOnly;
	RECT rcEditMinDelay;
	RECT rcEditMaxDelay;
	RECT rcGroupBoxAnimatedGifs;
	RECT rcDefaults;
	RECT rcOK;
	RECT rcCancel;
	RECT rcApply;

	HWND hwndCheckboxAnimCtrlPreview;
	HWND hwndCheckboxAnimCtrlViewer;
	HWND hwndCheckboxSprockets;
	HWND hwndCheckboxMinDelay;
	HWND hwndEditMinDelay;
	HWND hwndSpinMinDelay;
	HWND hwndCheckboxMaxDelay;
	HWND hwndEditMaxDelay;
	HWND hwndSpinMaxDelay;
	HWND hwndCheckboxIncreaseZeroOnly;
	HWND hwndGroupBoxAnimatedGifs;
	HWND hwndDefaults;
	HWND hwndOK;
	HWND hwndCancel;
	HWND hwndApply;

	// The groupbox must be at the bottom of the z-order, else the controls inside of it will not repaint
	// properly (when not using Aero).

	if (maker.CreateCheck( &hwndCheckboxAnimCtrlPreview,  &rcCheckboxAnimCtrlPreview,  IDC_CHECK_ANIMCTRL_PREVIEW, STR_GIFANIM_CONFIG_ANIMCTRL_PREVIEW, false, false, false, true, true)
	&&	maker.CreateCheck( &hwndCheckboxAnimCtrlViewer,   &rcCheckboxAnimCtrlViewer,   IDC_CHECK_ANIMCTRL_VIEWER,  STR_GIFANIM_CONFIG_ANIMCTRL_VIEWER,  false, false, false, true, true)
	&&	maker.CreateCheck( &hwndCheckboxSprockets,        &rcCheckboxSprockets,        IDC_CHECK_SPROCKETS,        STR_GIFANIM_CONFIG_SPROCKETS,        false, false, false, true, true)
	&&	maker.CreateCheck( &hwndCheckboxMaxDelay,         &rcCheckboxMaxDelay,         IDC_CHECK_MAXDELAY,         STR_GIFANIM_CONFIG_MAXDELAY,         false, false, false, true, true)
	&&	maker.CreateEdit(  &hwndEditMaxDelay,             &rcEditMaxDelay,             IDC_EDIT_MAXDELAY,                                               false, layout.GetButtonWidthXPixels(), true, true, true, false)
	&&	maker.CreateSpin(  &hwndSpinMaxDelay,                                          IDC_SPIN_MAXDELAY,                                               true, true, true, true)
	&&	maker.CreateCheck( &hwndCheckboxMinDelay,         &rcCheckboxMinDelay,         IDC_CHECK_MINDELAY,         STR_GIFANIM_CONFIG_MINDELAY,         false, false, false, true, true)
	&&	maker.CreateEdit(  &hwndEditMinDelay,             &rcEditMinDelay,             IDC_EDIT_MINDELAY,                                               false, layout.GetButtonWidthXPixels(), true, true, true, false)
	&&	maker.CreateSpin(  &hwndSpinMinDelay,                                          IDC_SPIN_MINDELAY,                                               true, true, true, true)
	&&	maker.CreateCheck( &hwndCheckboxIncreaseZeroOnly, &rcCheckboxIncreaseZeroOnly, IDC_CHECK_INCREASEZEROONLY, STR_GIFANIM_CONFIG_INCREASEZEROONLY, false, false, false, true, true)
	&&	maker.CreateGroup( &hwndGroupBoxAnimatedGifs,                                  IDC_GROUPBOX_ANIMATEDGIFS,  STR_GIFANIM_CONFIG_ANIMATED_GIFS)
	&&	maker.CreateButton(&hwndDefaults,                 &rcDefaults,                 IDC_BUTTON_DEFAULTS,        STR_ACTIVEX_CONFIG_BUTTON_DEFAULTS,  false, false)
	&&	maker.CreateButton(&hwndOK,                       &rcOK,                       IDOK,                       STR_ACTIVEX_CONFIG_BUTTON_OK,        false, true)
	&&	maker.CreateButton(&hwndCancel,                   &rcCancel,                   IDCANCEL,                   STR_ACTIVEX_CONFIG_BUTTON_CANCEL,    false, false)
	&&	maker.CreateButton(&hwndApply,                    &rcApply,                    IDC_BUTTON_APPLY,           STR_ACTIVEX_CONFIG_BUTTON_APPLY,     false, false))
	{
		LONG lEditHeight     = (rcEditMinDelay.bottom - rcEditMinDelay.top);
		LONG lCheckboxHeight = (rcCheckboxMinDelay.bottom - rcCheckboxMinDelay.top);
		LONG lGroupBottomAdjust = 0;
		if (lEditHeight > lCheckboxHeight)
		{
			lGroupBottomAdjust = (lEditHeight - lCheckboxHeight) / 2;
			lCheckboxHeight = lEditHeight;
		}
		else
		{
			lEditHeight = lCheckboxHeight;
		}

		rcCheckboxAnimCtrlPreview.bottom  = rcCheckboxAnimCtrlPreview.top  + lCheckboxHeight;
		rcCheckboxAnimCtrlViewer.bottom   = rcCheckboxAnimCtrlViewer.top   + lCheckboxHeight;
		rcCheckboxSprockets.bottom        = rcCheckboxSprockets.top        + lCheckboxHeight;
		rcCheckboxMinDelay.bottom         = rcCheckboxMinDelay.top         + lCheckboxHeight;
		rcCheckboxMaxDelay.bottom         = rcCheckboxMaxDelay.top         + lCheckboxHeight;
		rcCheckboxIncreaseZeroOnly.bottom = rcCheckboxIncreaseZeroOnly.top + lCheckboxHeight;
		rcEditMinDelay.bottom             = rcEditMinDelay.top             + lEditHeight;
		rcEditMaxDelay.bottom             = rcEditMaxDelay.top             + lEditHeight;

		if ((rcCheckboxMinDelay.right - rcCheckboxMinDelay.left) > (rcCheckboxMaxDelay.right - rcCheckboxMaxDelay.left))
		{
			rcCheckboxMaxDelay.right = rcCheckboxMaxDelay.left + (rcCheckboxMinDelay.right - rcCheckboxMinDelay.left);
		}
		else
		{
			rcCheckboxMinDelay.right = rcCheckboxMinDelay.left + (rcCheckboxMaxDelay.right - rcCheckboxMaxDelay.left);
		}

		rcEditMinDelay.right = rcEditMinDelay.left + (rcOK.right - rcOK.left);
		rcEditMaxDelay.right = rcEditMaxDelay.left + (rcOK.right - rcOK.left);

		OffsetRect(&rcCheckboxAnimCtrlPreview, 2 * layout.GetMarginPixels(), layout.GetMarginPixels() + layout.GetGroupBoxFirstControlYPixels());
		LeoHelpers::BelowRect(&rcCheckboxAnimCtrlViewer,   &rcCheckboxAnimCtrlPreview, layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxSprockets,        &rcCheckboxAnimCtrlViewer,  layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxMaxDelay,         &rcCheckboxSprockets,       layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxMinDelay,         &rcCheckboxMaxDelay,        layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxIncreaseZeroOnly, &rcCheckboxMinDelay,        layout.GetRelatedControlGapYPixels());

		LeoHelpers::RightRect(&rcEditMinDelay,             &rcCheckboxMinDelay,        layout.GetRelatedControlGapXPixels());
		LeoHelpers::RightRect(&rcEditMaxDelay,             &rcCheckboxMaxDelay,        layout.GetRelatedControlGapXPixels());

		rcGroupBoxAnimatedGifs = rcCheckboxAnimCtrlPreview;
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcCheckboxAnimCtrlViewer);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcCheckboxSprockets);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcCheckboxMinDelay);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcCheckboxMaxDelay);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcCheckboxIncreaseZeroOnly);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcEditMinDelay);
		LeoHelpers::UnionRect(&rcGroupBoxAnimatedGifs, &rcEditMaxDelay);

		rcGroupBoxAnimatedGifs.top    -= layout.GetGroupBoxFirstControlYPixels();
		rcGroupBoxAnimatedGifs.left   -= layout.GetMarginPixels();
		rcGroupBoxAnimatedGifs.right  += layout.GetMarginPixels();
		rcGroupBoxAnimatedGifs.bottom += (layout.GetMarginPixels() - lGroupBottomAdjust);

		LONG lMinMainButtonWidth = 6 * layout.GetButtonGapXPixels() 
			+ (rcDefaults.right - rcDefaults.left)
			+ (rcOK.right - rcOK.left)
			+ (rcCancel.right - rcCancel.left)
			+ (rcApply.right - rcApply.left);

		if ((rcGroupBoxAnimatedGifs.right - rcGroupBoxAnimatedGifs.left) < lMinMainButtonWidth)
		{
			rcGroupBoxAnimatedGifs.right = rcGroupBoxAnimatedGifs.left + lMinMainButtonWidth;
		}

		LeoHelpers::BelowRect(     &rcDefaults, &rcGroupBoxAnimatedGifs, layout.GetMarginPixels());
		LeoHelpers::BelowRightRect(&rcApply,    &rcGroupBoxAnimatedGifs, layout.GetMarginPixels());
		LeoHelpers::LeftRect(      &rcCancel,   &rcApply,                layout.GetButtonGapXPixels());
		LeoHelpers::LeftRect(      &rcOK,       &rcCancel,               layout.GetButtonGapXPixels());

		RECT rcEverything = rcGroupBoxAnimatedGifs;
		LeoHelpers::UnionRect(&rcEverything, &rcDefaults);
		LeoHelpers::UnionRect(&rcEverything, &rcApply);
		rcEverything.bottom += layout.GetMarginPixels();
		rcEverything.right += layout.GetMarginPixels();

		RECT rcWindow;
		RECT rcClient;
		GetWindowRect(hwndDlg, &rcWindow);
		GetClientRect(hwndDlg, &rcClient);
		rcWindow.right  -= (rcClient.right  - rcEverything.right);
		rcWindow.bottom -= (rcClient.bottom - rcEverything.bottom);
		LeoHelpers::MoveWindowRect(hwndDlg, &rcWindow, FALSE);

		LeoHelpers::MoveWindowRect(hwndCheckboxAnimCtrlPreview,  &rcCheckboxAnimCtrlPreview,  FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxAnimCtrlViewer,   &rcCheckboxAnimCtrlViewer,   FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxSprockets,        &rcCheckboxSprockets,        FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxMinDelay,         &rcCheckboxMinDelay,         FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxMaxDelay,         &rcCheckboxMaxDelay,         FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxIncreaseZeroOnly, &rcCheckboxIncreaseZeroOnly, FALSE);
		LeoHelpers::MoveWindowRect(hwndEditMinDelay,             &rcEditMinDelay,             FALSE);
		LeoHelpers::MoveWindowRect(hwndEditMaxDelay,             &rcEditMaxDelay,             FALSE);
		LeoHelpers::MoveWindowRect(hwndGroupBoxAnimatedGifs,     &rcGroupBoxAnimatedGifs,     FALSE);
		LeoHelpers::MoveWindowRect(hwndDefaults,                 &rcDefaults,                 FALSE);
		LeoHelpers::MoveWindowRect(hwndOK,                       &rcOK,                       FALSE);
		LeoHelpers::MoveWindowRect(hwndCancel,                   &rcCancel,                   FALSE);
		LeoHelpers::MoveWindowRect(hwndApply,                    &rcApply,                    FALSE);

		// UpDown Buddies & Ranges.
		::SendMessage(hwndSpinMinDelay, UDM_SETBUDDY, reinterpret_cast<WPARAM>(hwndEditMinDelay), 0);
		::SendMessage(hwndSpinMaxDelay, UDM_SETBUDDY, reinterpret_cast<WPARAM>(hwndEditMaxDelay), 0);
		::SendMessage(hwndSpinMinDelay, UDM_SETRANGE, 0, MAKELONG(UD_MAXVAL, 0));
		::SendMessage(hwndSpinMaxDelay, UDM_SETRANGE, 0, MAKELONG(UD_MAXVAL, 0));
	}

	// Window setup.

	LeoHelpers::CenterWindow(hwndDlg, GetParent(hwndDlg));

	if (hwndCheckboxAnimCtrlPreview != NULL)
	{
		// Set focus. (Do not use SetFocus in dialogs. Use WM_NEXTDLGCTL instead.)
		SendMessage(hwndDlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(hwndCheckboxAnimCtrlPreview), TRUE);

		bResult = FALSE; // Do *not* allow keyboard focus to be set to whatever is in wParam. We set focus to something more suitable.
	}
	else
	{
		bResult = TRUE; // Allow keyboard focus to be set to whatever is in wParam.
	}

	return bResult;
}
