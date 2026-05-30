#pragma once

#define TEXTTHUMB_REG_PATH _T("Software\\GPSoftware\\Directory Opus\\Config\\User\\Viewers\\TextThumb")

class CTextThumbGenerator;

namespace NTextThumbConfigDlg
{
	struct ConfigDlgData
	{
	public:
		ConfigDlgData(HWND hWndNotifyIn, DWORD dwNotifyDataIn, int iSeedIn, CTextThumbGenerator *pGeneratorIn, CTextThumbConfig *pGlobalConfigIn)
		: hWndNotify(hWndNotifyIn)
		, dwNotifyData(dwNotifyDataIn)
		, iSeed(iSeedIn)
		, pGenerator(pGeneratorIn)
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
		HWND										hWndNotify;
		DWORD										dwNotifyData;
		int											iSeed;
		CTextThumbGenerator *						pGenerator;
		CTextThumbConfig *							pGlobalConfig;
		CTextThumbConfig							dialogConfig;
		std::map< UINT, std::basic_string<TCHAR> >	mapCodePages;
		HFONT										hFont;
	};

	INT_PTR CALLBACK configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL configInitDialog(ConfigDlgData *pcd, HWND hwndDlg);

	CTextThumbConfig::CTypeConfig getCurrentTypeConfig(ConfigDlgData *pcd, HWND hwndDlg, HWND *phwndListBox, bool *pbIsDefault, LRESULT *pSelIdx);
};
