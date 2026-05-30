#pragma once

namespace NGifConfigDlg
{
	struct ConfigDlgData
	{
	public:
		ConfigDlgData(HWND hWndNotifyIn, DWORD dwNotifyDataIn, CGifConfig *pGlobalConfigIn)
		: hWndNotify(hWndNotifyIn)
		, dwNotifyData(dwNotifyDataIn)
		, pGlobalConfig(pGlobalConfigIn)
		, dialogConfig(*pGlobalConfigIn)
		, hFont(LeoHelpers::GetMessageFont())
		{
		}

		~ConfigDlgData()
		{
			// Created in the constructor, so freed in the destructor. Other stuff gets cleaned-up by the dialog proc.

			if (hFont != NULL)
			{
				::DeleteObject(hFont);
				hFont = NULL;
			}
		}

	public:
		HWND		hWndNotify;
		DWORD		dwNotifyData;
		CGifConfig *pGlobalConfig;
		CGifConfig	dialogConfig;
		HFONT       hFont;
	};

	INT_PTR CALLBACK configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL configInitDialog(ConfigDlgData *pcd, HWND hwndDlg);
};
