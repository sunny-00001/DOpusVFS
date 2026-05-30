#include "StdAfx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "TextThumbConfig.h"
#include "TextFile.h"
#include "TextThumbConfigDlg.h"
#include "TextThumbGenerator.h"
#include "CodePageEnumerator.h"

static const char *sampleStrings[] = 
{
	"Gapless music playback:\n"
	"=======================\n"
	"The MP3 format has inherent flaws which, unless worked around, mean it cannot move seamlessly from one track to the next without adding silence or a click. Rather than address these problems the majority of MP3 players, both hardware and software, make things worse by inserting additional silence of their own.\n"
	"\n"
	"If you tend to listen to albums from start to finish, and albums where someone has spent time crafting a complete hour of music rather than a compilation of disconnected songs, or live albums, or mix albums, then maybe it will annoy you that between each track, where one piece of music should flow into the next, the whole experience is interrupted by silence or a click. If the album would have sounded better that way then the artist would've made it like that in the first place.\n"
	"\n"
	"If you only listen to randomly selected, individual tracks, or albums where the tracks are always independent, then you're not going to care. That's cool. I'm not trying to push my preferences on anyone else. If MP3, iPods and all that stuff does it for you then I'm really happy for you. Just don't push your preferences on me and tell me none of this should matter to me, please. Every time I hear a pause or a click between a track it irritates me a little bit and over time those irritations, combined with the fact that most of the industry is completely ignoring the problem, has annoyed me enough to make this page and warn other people who feel similarly.\n"
	"\n"
	"If you don't care about the issue of gapless album playback then you won't find anything interesting or useful on this page."
,
	"========================================================================\n"
	"  DYNAMIC LINK LIBRARY : textthumb Project Overview\n"
	"========================================================================\n"
	"\n"
	"textthumb.vcproj\n"
	"	This is the main project file for VC++ projects generated using an Application Wizard. \n"
	"	It contains information about the version of Visual C++ that generated the file, and \n"
	"	information about the platforms, configurations, and project features selected with the\n"
	"	Application Wizard.\n"
	"\n"
	"textthumb.cpp\n"
	"	This is the main DLL source file.\n"
	"\n"
	"	When created, this DLL does not export any symbols. As a result, it  \n"
	"	will not produce a .lib file when it is built. If you wish this project \n"
	"	to be a project dependency of some other project, you will either need to \n"
	"	add code to export some symbols from the DLL so that an export library \n"
	"	will be produced, or you can set the Ignore Input Library property to Yes \n"
	"	on the General propert page of the Linker folder in the project's Property \n"
	"	Pages dialog box.\n"
	"\n"
	"/////////////////////////////////////////////////////////////////////////////\n"
	"Other standard files:\n"
	"\n"
	"StdAfx.h, StdAfx.cpp\n"
	"	These files are used to build a precompiled header (PCH) file\n"
	"	named textthumb.pch and a precompiled types file named StdAfx.obj.\n"
	"\n"
	"/////////////////////////////////////////////////////////////////////////////\n"
,
	"------ Rebuild All started: Project: textthumb, Configuration: Release Win32 ------\n"
	"\n"
	"Deleting intermediate files and output files for project 'textthumb', configuration 'Release|Win32'.\n"
	"Compiling...\n"
	"stdafx.cpp\n"
	"Compiling...\n"
	"TextThumbGenerator.cpp\n"
	"TextThumbConfig.cpp\n"
	"textthumb.cpp\n"
	"TextFile.cpp\n"
	"LeoHelpers.cpp\n"
	"Generating Code...\n"
	"Compiling resources...\n"
	"Linking...\n"
	"   Creating library Release/textthumb.lib and object Release/textthumb.exp\n"
	"\n"
	"Build log was saved at \"file://h:\\Code\\textthumb\\Release\\BuildLog.htm\"\n"
	"textthumb - 0 error(s), 0 warning(s)\n"
	"\n"
	"\n"
	"---------------------- Done ----------------------\n"
	"\n"
	"    Rebuild All: 1 succeeded, 0 failed, 0 skipped\n"
,
	"namespace LeoHelpers\n"
	"{\n"
	"	// Returns false on failure. Use GetLastError().\n"
	"	// hKeyParent -- hKey of parent under which szKeyPath should be created. Usually a hive like HKEY_CURRENT_USER\n"
	"	// szKeyPath -- Sub key (can be a path) under hKeyHiveOrParent from which szValueName is read. Can be \"\" or NULL.\n"
	"	// szValueName -- Name of value to query. Can be "" or NULL to read the unnamed default value.\n"
	"	// pdwType -- Pointer to DWORD to receive type of data. NULL if unwanted.\n"
	"	// plpData -- POINTER TO POINTER to receive data buffer. NULL if data unwanted.\n"
	"	// pcbData -- Pointer to DWORD to receive size of data buffer. NULL if data unwanted.\n"
	"	// You do NOT allocate a buffer, this function does it for you, like the frigging API should.\n"
	"	// If the function succeeds and plpData was not NULL you must delete[] *plpData when finished with it.\n"
	"	// Like RegQueryValueEx, you may supply NULL plpData and non-NULL pcbData to get the required\n"
	"	// buffer size (except for HKEY_PERFORMANCE_DATA), but again the function always allocates a buffer for you.\n"
	"	// Although untested, reading HKEY_PERFORMANCE_DATA should work.\n"
	"bool LeetRegQueryValue(HKEY hKeyParent, const TCHAR *szKeyPath, const TCHAR *szValueName,\n"
	"						DWORD *pdwType, void **plpData, DWORD *pcbData);\n"
	"\n"
	"// Returns false on failure. Use GetLastError().\n"
	"// Uses LeetRegQueryValue() to get you a DWORD without having to worry about buffers, types and so on.\n"
	"bool LeetRegQueryDWORDValue(HKEY hKeyParent, const TCHAR *szKeyPath, const TCHAR *szValueName, DWORD *pdwRes);\n"
	"\n"
	"// Returns false on failure. Use GetLastError().\n"
	"// Uses LeetRegQueryValue() to get you a TCHAR string without having to worry about buffers, types and so on.\n"
	"bool LeetRegQueryStringValue(HKEY hKeyParent, const TCHAR *szKeyPath, const TCHAR *szValueName, std::basic_string<TCHAR> *pString);\n"
	"\n"
	"// Returns false on failure. Use GetLastError().\n"
	"// Uses LeetRegQueryValue() to get you a vector of TCHAR strings without having to worry about buffers, types and so on.\n"
	"bool LeetRegQueryMultiStringValue(HKEY hKeyParent, const TCHAR *szKeyPath, const TCHAR *szValueName, std::vector< std::basic_string<TCHAR> > *pVecStrings);\n"
};

static const char *dosSampleString =
	"       €\n"
	"      ‹≤‹\n"
	"      ±±≤\n"
	"ﬂ∞∞∞∞∞∞±≤≤≤≤≤≤ﬂ\n"
	"   ﬂ∞∞±±±±≤ﬂ\n"
	"     ≤≤≤≤≤\n"
	"    ≤≤ﬂ ﬂ€≤      ‹\n"
	"   €ﬂ     ﬂ€  €€€€€€€\n"
	"             €€≤≤∞ ≤€€\n"
	"            €€≤≤∞   ≤€€\n"
	"             €€≤≤∞ ≤€€\n"
	"              €€€€€€€\n"
	"                 ﬂ\n";

INT_PTR CALLBACK NTextThumbConfigDlg::configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL bResult = FALSE;
	bool bQuit = false;
	bool bNcDestroy = false;
	bool bUpdateAddButtons = false;
	bool bUpdatePreview = false;
	bool bUpdateFont = false;
	bool bUpdateCodePage = false;
	bool bLoadSettings = false;
	bool bLoadDefaultSettings = false;
	bool bSaveSettings = false;
	bool bUpdateDefButton = false;
	bool bFlagsToGui = false;
	bool bGuiToFlags = false;

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

			pcd->iSeed = pcd->iSeed % (sizeof(sampleStrings)/sizeof(sampleStrings[0]));

			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY,            FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_CONFIGEXT,    FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_CONFIGEXT, FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_EXTENSION,    FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_EXTENSION, FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_HEADER,       FALSE);
			LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_HEADER,    FALSE);

			CCodePageEnumerator::Enumerate(pcd->mapCodePages, CP_INSTALLED);

			bLoadSettings = true;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_COLOR:
		case IDC_BUTTON_TEXTCOLOR:
			if (NULL != pcd && BN_CLICKED == HIWORD(wParam))
			{
				COLORREF custColors[16] =
				{
					RGB(210,210,210),
					RGB(210,210,195),
					RGB(195,195,210),
					RGB(195,210,195),
					RGB(210,195,195),
					RGB(195,210,210),
					RGB(210,195,210),
					RGB(255,255,200),
					RGB(110,110,110),
					RGB(110,110, 95),
					RGB(195, 95,110),
					RGB(195,110, 95),
					RGB(110, 95, 95),
					RGB(195,110,110),
					RGB(110, 95,110),
					RGB(155,155,100),
				};

				CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

				CHOOSECOLOR cc;
				ZeroMemory(&cc, sizeof(cc));
				cc.lStructSize = sizeof(cc);
				cc.hwndOwner = hwndDlg;
				cc.hInstance = NULL;
				if (LOWORD(wParam) == IDC_BUTTON_COLOR)
				{
					cc.rgbResult = typeConfig.crBackground;
				}
				else
				{
					cc.rgbResult = typeConfig.crText;
				}
				cc.lpCustColors = custColors;
				cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;

				if (ChooseColor(&cc))
				{
					if (LOWORD(wParam) == IDC_BUTTON_COLOR)
					{
						typeConfig.crBackground = cc.rgbResult;
					}
					else
					{
						typeConfig.crText = cc.rgbResult;
					}

					pcd->dialogConfig.SetTypeConfig(typeConfig);

					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdatePreview = true;
				}

				bResult = TRUE;
			}
			break;
		case IDC_BUTTON_FONT:
			if (NULL != pcd && BN_CLICKED == HIWORD(wParam))
			{
				CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

				DOPUSCHOOSEFONT opusChooseFont;
				opusChooseFont.cbSize = sizeof(opusChooseFont);
				opusChooseFont.lpFont = &typeConfig.logFont;
				opusChooseFont.dwFlags = 0;
				opusChooseFont.lpszTitle = NULL;

				if (pcd->pGlobalConfig->GetOpusPluginHelper()->DOpusChooseFont(hwndDlg, &opusChooseFont))
				{
					pcd->dialogConfig.SetTypeConfig(typeConfig);

					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdatePreview = true;
					bUpdateFont = true;
				}

				bResult = TRUE;
			}
			break;
		case IDC_CHECK_WRAP:
		case IDC_CHECK_REMOVEBLANKLINES:
		case IDC_RADIO_ICONPREFS:
		case IDC_RADIO_ICONON:
		case IDC_RADIO_ICONOFF:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
				bGuiToFlags = true;
				bUpdatePreview = true;
				bResult = TRUE;
			}
			break;
		case IDC_CHECK_DESCRIPTION:
		case IDC_CHECK_FOLDERTHUMBNAILS:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
				bGuiToFlags = true;
				bResult = TRUE;
			}
			break;
		case IDC_COMBO_CODEPAGE:
			if (NULL != pcd
			&&	HIWORD(wParam) == CBN_SELCHANGE)
			{
				DWORD dwCodePage;
				if (LeoHelpers::GetComboItem(hwndDlg, IDC_COMBO_CODEPAGE, &dwCodePage))
				{
					CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);
					typeConfig.dwCodePage = dwCodePage;
					pcd->dialogConfig.SetTypeConfig(typeConfig);

					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdatePreview = true;
				}
				bResult = TRUE;
			}
			break;
		case IDC_BUTTON_DEFAULTS:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
				bLoadDefaultSettings = true;
				bResult = TRUE;
			}
			break;
		case IDC_LIST_CONFIGURED_EXTENSIONS:
			{
				switch(HIWORD(wParam))
				{
				case LBN_SELCANCEL:
				case LBN_SELCHANGE:
					if (NULL != pcd)
					{
						bUpdatePreview = true;
						bUpdateFont = true;
						bUpdateCodePage = true;
						bUpdateAddButtons = true;
						bFlagsToGui = true;

						bool bIsDefault = true;
						CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, &bIsDefault, NULL);

						LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_CONFIGEXT, bIsDefault ? FALSE : TRUE);
						SetDlgItemText(hwndDlg, IDC_EDIT_CONFIG_EXT, typeConfig.strExt.c_str());
					}
					break;
				default:
					break;
				}
			}
			break;
		case IDC_LIST_EXCLUDED_EXTENSIONS:
		case IDC_LIST_EXCLUDED_HEADERS:
			{
				switch(HIWORD(wParam))
				{
				case LBN_SELCANCEL:
				case LBN_SELCHANGE:
					{
						HWND hwndListBox = reinterpret_cast<HWND>(lParam);
						if (NULL != hwndListBox)
						{
							bUpdateAddButtons = true;

							int nIDDelete = (LOWORD(wParam)==IDC_LIST_EXCLUDED_EXTENSIONS ? IDC_BUTTON_DELETE_EXTENSION : IDC_BUTTON_DELETE_HEADER);
							int nIDEdit   = (LOWORD(wParam)==IDC_LIST_EXCLUDED_EXTENSIONS ? IDC_EDIT_EXTENSION          : IDC_EDIT_HEADER         );

							LRESULT selCount = SendMessage(hwndListBox, LB_GETSELCOUNT, 0, 0);
							if (0 >= selCount)
							{
								LeoHelpers::EnableDlgItem(hwndDlg, nIDDelete, FALSE);
							}
							else
							{
								LeoHelpers::EnableDlgItem(hwndDlg, nIDDelete, TRUE);

								int *pSelItems = new int[ selCount ];

								if (selCount == SendMessage(hwndListBox, LB_GETSELITEMS, selCount, reinterpret_cast<LPARAM>(pSelItems)))
								{
									std::basic_string<TCHAR> strLBItem;

									if (LeoHelpers::GetListBoxItemText(hwndListBox, pSelItems[0], &strLBItem) && (!strLBItem.empty()))
									{
										SetDlgItemText(hwndDlg, nIDEdit, strLBItem.c_str());
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
			}
			break;
		case IDC_BUTTON_DELETE_CONFIGEXT:
			if (NULL != pcd && BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
				bUpdatePreview = true;
				bUpdateFont = true;
				bUpdateCodePage = true;
				bUpdateAddButtons = true;
				bFlagsToGui = true;

				HWND hwndListBox = NULL;
				bool bIsDefault = true;
				LRESULT selIdx = 0;
				CTextThumbConfig::CTypeConfig typeConfig = NTextThumbConfigDlg::getCurrentTypeConfig(pcd, hwndDlg, &hwndListBox, &bIsDefault, &selIdx);

				if (hwndListBox != NULL && !typeConfig.strExt.empty() && !bIsDefault && selIdx > 0)
				{
					pcd->dialogConfig.DeleteTypeConfig(typeConfig.strExt);

					SendMessage(hwndListBox, LB_SETCURSEL, selIdx - 1, 0);
					SendMessage(hwndListBox, LB_DELETESTRING, selIdx, 0);

					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_CONFIGEXT, selIdx > 1 ? TRUE : FALSE);
				}
			}
			break;
		case IDC_BUTTON_DELETE_EXTENSION:
		case IDC_BUTTON_DELETE_HEADER:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
				bUpdateAddButtons = true;

				HWND hwndListBox = GetDlgItem(hwndDlg, (LOWORD(wParam)==IDC_BUTTON_DELETE_EXTENSION ? IDC_LIST_EXCLUDED_EXTENSIONS : IDC_LIST_EXCLUDED_HEADERS));
				if (NULL != hwndListBox)
				{
					LeoHelpers::EnableDlgItem(hwndDlg, LOWORD(wParam), FALSE);

					LRESULT selCount = SendMessage(hwndListBox, LB_GETSELCOUNT, 0, 0);
					if (0 < selCount)
					{
						int *pSelItems = new int[ selCount ];

						if (selCount == SendMessage(hwndListBox, LB_GETSELITEMS, selCount, reinterpret_cast<LPARAM>(pSelItems)))
						{
							int iDeleteCount = 0;

							for(LRESULT i = 0; i < selCount; ++i)
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
		case IDC_BUTTON_ADD_CONFIGEXT:
			if (NULL != pcd && BN_CLICKED == HIWORD(wParam))
			{
				LONG_PTR lButtonStyle;

				if ((!LeoHelpers::GetDlgItemLongPtr(hwndDlg, IDC_BUTTON_ADD_CONFIGEXT, GWL_STYLE, &lButtonStyle))
				||	(lButtonStyle & WS_DISABLED))
				{
					MessageBeep(-1);
				}
				else
				{
					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdatePreview = true;
					bUpdateFont = true;
					bUpdateCodePage = true;
					bUpdateAddButtons = true;
					bFlagsToGui = true;

					std::basic_string< TCHAR > strNewExt;
					if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_CONFIG_EXT, &strNewExt) && strNewExt.length() > 0)
					{
						if (strNewExt[0] != _T('.'))
						{
							strNewExt.insert(0, _T("."));
						}

						HWND hwndListBox = NULL;
						CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, &hwndListBox, NULL, NULL);

						if (NULL != hwndListBox)
						{
							LRESULT lrInsertIdx = 1; // Skip over the <Default> item at the top.
							LRESULT itemCount = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0);

							std::basic_string<TCHAR> strLBItem;

							// Find the correct place to insert the new item, since this list isn't sorted.
							for (lrInsertIdx = 1; lrInsertIdx < itemCount; ++lrInsertIdx)
							{
								if ((!LeoHelpers::GetListBoxItemText(hwndListBox, lrInsertIdx, &strLBItem) && (!strLBItem.empty()))
								||	0 > _tcsicmp(strNewExt.c_str(), strLBItem.c_str()))
								{
									break;
								}
							}

							lrInsertIdx = SendMessage(hwndListBox, LB_INSERTSTRING, lrInsertIdx, reinterpret_cast<LPARAM>(strNewExt.c_str()));

							if (lrInsertIdx > 0)
							{
								SendMessage(hwndListBox, LB_SETITEMDATA, lrInsertIdx, 0); // It's not the default so set ITEMDATA to 0.

								SetDlgItemText(hwndDlg, IDC_EDIT_CONFIG_EXT, _T(""));
								LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_DELETE_CONFIGEXT, TRUE);

								typeConfig.strExt = strNewExt;
								pcd->dialogConfig.SetTypeConfig(typeConfig); // Copy old TypeConfig to new.

								SendMessage(hwndListBox, LB_SETCURSEL, lrInsertIdx, 0);
							}
						}
					}
				}
			}
			break;
		case IDC_BUTTON_ADD_EXTENSION:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LONG_PTR lButtonStyle;

				if ((!LeoHelpers::GetDlgItemLongPtr(hwndDlg, IDC_BUTTON_ADD_EXTENSION, GWL_STYLE, &lButtonStyle))
				||	(lButtonStyle & WS_DISABLED))
				{
					MessageBeep(-1);
				}
				else
				{
					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdateAddButtons = true;

					std::basic_string< TCHAR > strText;
					if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_EXTENSION, &strText) && strText.length() > 0)
					{
						if (strText[0] != _T('.'))
						{
							strText.insert(0, _T("."));
						}

						HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_EXTENSIONS);
						if (NULL != hwndListBox)
						{
							SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strText.c_str()));
							SetDlgItemText(hwndDlg, IDC_EDIT_EXTENSION, _T(""));
						}
					}
				}
			}
			break;
		case IDC_BUTTON_ADD_HEADER:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LONG_PTR lButtonStyle;

				if ((!LeoHelpers::GetDlgItemLongPtr(hwndDlg, IDC_BUTTON_ADD_HEADER, GWL_STYLE, &lButtonStyle))
				||	(lButtonStyle & WS_DISABLED))
				{
					MessageBeep(-1);
				}
				else
				{
					LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, TRUE);
					bUpdateAddButtons = true;

					std::basic_string< TCHAR > strText;
					if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_HEADER, &strText))
					{
						HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_HEADERS);
						if (NULL != hwndListBox)
						{
							SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strText.c_str()));
							SetDlgItemText(hwndDlg, IDC_EDIT_HEADER, _T(""));
						}
					}
				}
			}
			break;
		case IDC_EDIT_CONFIG_EXT:
		case IDC_EDIT_EXTENSION:
		case IDC_EDIT_HEADER:
			{
				if (HIWORD(wParam) == EN_CHANGE)
				{
					bUpdateAddButtons = true;
				}
				else if (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_KILLFOCUS)
				{
					bUpdateDefButton = true;
				}
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
		case IDC_BUTTON_APPLY:
			if (BN_CLICKED == HIWORD(wParam))
			{
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_APPLY, FALSE);
				bSaveSettings = true;
			}
			break;
		case IDC_STATIC_PREVIEW:
			{
				if (HIWORD(wParam) == STN_CLICKED
				||	HIWORD(wParam) == STN_DBLCLK)
				{
					pcd->iSeed = (++(pcd->iSeed)) % (sizeof(sampleStrings)/sizeof(sampleStrings[0]));
					bUpdatePreview = true;
				}
			}
			break;
		default:
			break;
		}
		break;
	case WM_SYSCOLORCHANGE:
		bUpdatePreview = true;
		bResult = FALSE; // Pass on to standard handling.
		break;
	case WM_NCDESTROY:
		bNcDestroy = true;
		break;
	case WM_PAINT:
		if (pcd != NULL && !pcd->pGlobalConfig->IsWindowsXPOrAbove())
		{
			// Windows 2000's static control does not repaint itself if a bitmap has been set,
			// unless we set a new bitmap every time we paint.
			bUpdatePreview = true;
		}
		break; // Pass on to standard handling.
    default:
		break;
	}

	if (NULL != pcd && (bLoadSettings || bLoadDefaultSettings))
	{
		if (bLoadDefaultSettings)
		{
			pcd->dialogConfig.LoadDefaults();
		}

		LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());

		std::vector< std::basic_string<TCHAR> > vecConfigExts;
		pcd->dialogConfig.GetConfiguredExtensions(&vecConfigExts);

		HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_CONFIGURED_EXTENSIONS);

		if (NULL != hwndListBox)
		{
			// Delete existing configured extensions.
			for (LRESULT lr = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0); 0 < lr; lr = SendMessage(hwndListBox, LB_DELETESTRING, 0, 0))
			{
			}

			// Add configured extensions from the config instance.
			for (std::vector< std::basic_string<TCHAR> >::const_iterator pstrExt = vecConfigExts.begin(); pstrExt != vecConfigExts.end(); ++pstrExt)
			{
				CTextThumbConfig::CTypeConfig typeConfig = pcd->dialogConfig.GetTypeConfig( *pstrExt );

				LRESULT lrIdx = -1;

				bool bIsDefault = pstrExt->empty();

				lrIdx = SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(bIsDefault ? sl.Get(STR_TEXTTHUMB_DEFAULT_TYPE_LABEL) : pstrExt->c_str()));
				if (0 <= lrIdx)
				{
					if (bIsDefault)
					{
						SendMessage(hwndListBox, LB_SETCURSEL, lrIdx, 0);
					}
					SendMessage(hwndListBox, LB_SETITEMDATA, lrIdx, bIsDefault ? 1 : 0);
				}
			}
		}

		if (NULL != (hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_EXTENSIONS)))
		{
			std::vector< std::basic_string<TCHAR> > vecExcludedExtensions;
			pcd->dialogConfig.GetExcludedExtensions(&vecExcludedExtensions);

			for (LRESULT lr = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0); 0 < lr; lr = SendMessage(hwndListBox, LB_DELETESTRING, 0, 0))
			{
			}

			for (std::vector< std::basic_string<TCHAR> >::const_iterator pstrExExt = vecExcludedExtensions.begin(); pstrExExt != vecExcludedExtensions.end(); ++pstrExExt)
			{
				SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pstrExExt->c_str()));
			}
		}

		if (NULL != (hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_HEADERS)))
		{
			std::vector< std::basic_string<TCHAR> > vecExcludedHeaders;
			pcd->dialogConfig.GetExcludedHeaders(&vecExcludedHeaders);

			for (LRESULT lr = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0); 0 < lr; lr = SendMessage(hwndListBox, LB_DELETESTRING, 0, 0))
			{
			}

			for (std::vector< std::basic_string<TCHAR> >::const_iterator pcstrExtHead = vecExcludedHeaders.begin(); pcstrExtHead != vecExcludedHeaders.end(); ++pcstrExtHead)
			{
				SendMessage(hwndListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pcstrExtHead->c_str()));
			}
		}

		bUpdateAddButtons = true;
		bFlagsToGui = true;
		bUpdateFont = true;
		bUpdateCodePage = true;
		bUpdatePreview = true;
	}

	if (NULL != pcd && bUpdateAddButtons) // Must be after settings load
	{
		std::basic_string< TCHAR > strText;

		if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_CONFIG_EXT, &strText))
		{
			HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_CONFIGURED_EXTENSIONS);
			if (NULL != hwndListBox)
			{
				if (strText.length() > 0 && strText[0] != _T('.'))
				{
					strText.insert(0, _T("."));
				}

				BOOL bNotFound = TRUE;
				if (strText.empty()
				||	0 <= SendMessage(hwndListBox, LB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(strText.c_str())))
				{
					bNotFound = FALSE;
				}
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_CONFIGEXT, bNotFound);
			}
		}

		if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_EXTENSION, &strText))
		{
			HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_EXTENSIONS);
			if (NULL != hwndListBox)
			{
				if (strText.length() > 0 && strText[0] != _T('.'))
				{
					strText.insert(0, _T("."));
				}

				BOOL bNotFound = TRUE;
				if (strText.empty()
				||	0 <= SendMessage(hwndListBox, LB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(strText.c_str())))
				{
					bNotFound = FALSE;
				}
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_EXTENSION, bNotFound);
			}
		}

		if (LeoHelpers::GetDlgItemText(hwndDlg, IDC_EDIT_HEADER, &strText))
		{
			HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_HEADERS);
			if (NULL != hwndListBox)
			{
				BOOL bNotFound = TRUE;
				if (strText.empty()
				||	0 <= SendMessage(hwndListBox, LB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(strText.c_str())))
				{
					bNotFound = FALSE;
				}
				LeoHelpers::EnableDlgItem(hwndDlg, IDC_BUTTON_ADD_HEADER, bNotFound);
			}
		}
	}

	if (NULL != pcd && bUpdateDefButton) // Must be after settings load and after bUpdateAddButtons
	{
		HWND hwndOK                 = GetDlgItem(hwndDlg, IDOK);
		HWND hwndEditConfigExt      = GetDlgItem(hwndDlg, IDC_EDIT_CONFIG_EXT);
		HWND hwndButtonAddConfigExt = GetDlgItem(hwndDlg, IDC_BUTTON_ADD_CONFIGEXT);
		HWND hwndEditExtension      = GetDlgItem(hwndDlg, IDC_EDIT_EXTENSION);
		HWND hwndButtonAddExtension = GetDlgItem(hwndDlg, IDC_BUTTON_ADD_EXTENSION);
		HWND hwndEditHeader         = GetDlgItem(hwndDlg, IDC_EDIT_HEADER);
		HWND hwndButtonAddHeader    = GetDlgItem(hwndDlg, IDC_BUTTON_ADD_HEADER);

		HWND hwndFocus = ::GetFocus();

		// If GetFocus returns NULL then another window/thread has the focus and we shouldn't change
		// the default push button. Doing so on Vista results in IDOK retaining its blue glow if
		// you give the edit control focus and then click on another window. Avoiding the change
		// when another window/thread gets focus seems to do the trick, since when our window
		// is reactivated the edit control gets the focus again and even if the window is activated
		// by clicking on another control, the edit control seems to get and then lose the focus,
		// rather than not getting it back in the first place, which results in the desired effect.
		if (NULL != hwndFocus
		&&	NULL != hwndOK
		&&	NULL != hwndEditConfigExt
		&&	NULL != hwndButtonAddConfigExt
		&&	NULL != hwndEditExtension
		&&	NULL != hwndButtonAddExtension
		&&	NULL != hwndEditHeader
		&&	NULL != hwndButtonAddHeader)
		{
			WPARAM defId;
			HWND hwndDef;

			if (hwndFocus == hwndEditConfigExt)
			{
				defId = IDC_BUTTON_ADD_CONFIGEXT;
				hwndDef = hwndButtonAddConfigExt;
			}
			else if (hwndFocus == hwndEditExtension)
			{
				defId = IDC_BUTTON_ADD_EXTENSION;
				hwndDef = hwndButtonAddExtension;
			}
			else if (hwndFocus == hwndEditHeader)
			{
				defId = IDC_BUTTON_ADD_HEADER;
				hwndDef = hwndButtonAddHeader;
			}
			else
			{
				defId = IDOK;
				hwndDef = hwndOK;
			}

			::SendMessage(hwndDlg, DM_SETDEFID, defId, 0);
			::SendMessage(hwndOK,                 BM_SETSTYLE, (hwndDef == hwndOK)                 ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);
			::SendMessage(hwndButtonAddConfigExt, BM_SETSTYLE, (hwndDef == hwndButtonAddConfigExt) ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);
			::SendMessage(hwndButtonAddExtension, BM_SETSTYLE, (hwndDef == hwndButtonAddExtension) ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);
			::SendMessage(hwndButtonAddHeader,    BM_SETSTYLE, (hwndDef == hwndButtonAddHeader)    ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);
		}
	}

	if (NULL != pcd && bFlagsToGui)
	{
		CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

		CheckDlgButton(hwndDlg, IDC_RADIO_ICONPREFS,        (!((typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_ON)||(typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_OFF))) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_RADIO_ICONON,           (typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_ON              ) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_RADIO_ICONOFF,          (typeConfig.dwFlags&CTextThumbConfig::TTF_ICON_OFF             ) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_DESCRIPTION,      (typeConfig.dwFlags&CTextThumbConfig::TTF_DESCRIPTION_ON       ) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_FOLDERTHUMBNAILS, (typeConfig.dwFlags&CTextThumbConfig::TTF_FOLDERTHUMBNAILS_OFF ) ? BST_UNCHECKED : BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_WRAP,             (typeConfig.dwFlags&CTextThumbConfig::TTF_WRAP_ON              ) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHECK_REMOVEBLANKLINES, (typeConfig.dwFlags&CTextThumbConfig::TTF_REMOVE_BLANK_LINES_ON) ? BST_CHECKED : BST_UNCHECKED);
	}

	if (NULL != pcd && bGuiToFlags)
	{
		CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

		typeConfig.dwFlags = 0;

		typeConfig.dwFlags |= ((BST_CHECKED   == IsDlgButtonChecked(hwndDlg, IDC_CHECK_WRAP)            ) ? CTextThumbConfig::TTF_WRAP_ON               : 0);
		typeConfig.dwFlags |= ((BST_CHECKED   == IsDlgButtonChecked(hwndDlg, IDC_CHECK_REMOVEBLANKLINES)) ? CTextThumbConfig::TTF_REMOVE_BLANK_LINES_ON : 0);
		typeConfig.dwFlags |= ((BST_CHECKED   == IsDlgButtonChecked(hwndDlg, IDC_RADIO_ICONON)          ) ? CTextThumbConfig::TTF_ICON_ON               : 0); // tri-state
		typeConfig.dwFlags |= ((BST_CHECKED   == IsDlgButtonChecked(hwndDlg, IDC_RADIO_ICONOFF)         ) ? CTextThumbConfig::TTF_ICON_OFF              : 0); // tri-state
		typeConfig.dwFlags |= ((BST_CHECKED   == IsDlgButtonChecked(hwndDlg, IDC_CHECK_DESCRIPTION)     ) ? CTextThumbConfig::TTF_DESCRIPTION_ON        : 0);
		typeConfig.dwFlags |= ((BST_UNCHECKED == IsDlgButtonChecked(hwndDlg, IDC_CHECK_FOLDERTHUMBNAILS)) ? CTextThumbConfig::TTF_FOLDERTHUMBNAILS_OFF  : 0);

		pcd->dialogConfig.SetTypeConfig(typeConfig);
	}

	if (NULL != pcd && bUpdateFont)
	{
		LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());

		HDC hDC = GetDC(hwndDlg);

		if (NULL != hDC)
		{
			CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

			// Build font name and size string
			int iPoint = abs(MulDiv(typeConfig.logFont.lfHeight,72,GetDeviceCaps(hDC,LOGPIXELSY)));
			TCHAR *szFontBuffer = LeoHelpers::StringAllocAndFormat(sl.Get(STR_TEXTTHUMB_FONT_AND_SIZE), typeConfig.logFont.lfFaceName, iPoint);

			if (NULL != szFontBuffer)
			{
				SetDlgItemText(hwndDlg, IDC_EDIT_FONT, szFontBuffer);

				delete [] szFontBuffer;
			}

			ReleaseDC(hwndDlg, hDC);
		}
	}

	if (NULL != pcd && bUpdateCodePage)
	{
		LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());

		CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

		DWORD dwCurrentCP = typeConfig.dwCodePage;

		if (dwCurrentCP == CP_ACP)
		{
			dwCurrentCP = ::GetACP();
		}

		bool bClear = true;

		for(std::map< UINT, std::basic_string<TCHAR> >::const_iterator miter = pcd->mapCodePages.begin(); miter != pcd->mapCodePages.end(); ++miter)
		{
			LeoHelpers::SetupComboItem(hwndDlg, IDC_COMBO_CODEPAGE, bClear, miter->first, dwCurrentCP, miter->second.c_str());
			bClear = false;
		}

		if (!LeoHelpers::GetComboItem(hwndDlg, IDC_COMBO_CODEPAGE, &dwCurrentCP))
		{
			TCHAR *szName = LeoHelpers::StringAllocAndFormat(sl.Get(STR_TEXTTHUMB_CODEPAGE_NOT_INSTALLED), dwCurrentCP);

			if (szName != NULL)
			{
				LeoHelpers::SetupComboItem(hwndDlg, IDC_COMBO_CODEPAGE, bClear, dwCurrentCP, dwCurrentCP, szName);
				delete [] szName;
			}
		}
	}

	if (NULL != pcd && bUpdatePreview)
	{
		HWND hwndPreview = GetDlgItem(hwndDlg, IDC_STATIC_PREVIEW);

		if (NULL != hwndPreview && NULL != pcd->pGenerator)
		{
			CTextThumbConfig::CTypeConfig typeConfig = getCurrentTypeConfig(pcd, hwndDlg, NULL, NULL, NULL);

			RECT rectPreview;
			GetClientRect(hwndPreview, &rectPreview);
			SIZE sizePreview;
			sizePreview.cx = rectPreview.right;
			sizePreview.cy = rectPreview.bottom;

			const char *szSample = sampleStrings[ pcd->iSeed ];

			if (typeConfig.dwCodePage == 437)
			{
				szSample = dosSampleString; // Special sample text for code-page 437 (DOS).
			}
			else
			{
				typeConfig.dwCodePage = 1252; // Code page of all our other sample text (ANSI Latin 1). Note that we're changing a temp copy of the TypeConfig here.
			}

			CMemoryTextFile textFile( reinterpret_cast<const BYTE *>( szSample ), static_cast<ULONG>(strlen( szSample )) );

			HBITMAP hbmpPreview = NULL;

			bool bSimulateIcon = pcd->dialogConfig.ShouldDrawThumbnailIcon(typeConfig.dwFlags);

			hbmpPreview = pcd->pGenerator->GenerateThumbnail(pcd->dialogConfig, hwndDlg, &textFile, NULL,
															 typeConfig, true, bSimulateIcon, sizePreview.cx, sizePreview.cy);

			if (NULL != hbmpPreview)
			{
				// Note that on XP and above, if the image contains an alpha channel then this code will leak resources.
				// See the "Important" note at the bottom of the STM_SETIMAGE documentation. Note that the image does not
				// use the alpha channel as we fill the background ourselves (for compatibility with older versions of
				// Windows whose static controls do not support the alpha channel).

				HBITMAP hbmpOldPreview = reinterpret_cast<HBITMAP>(SendMessage(hwndPreview, STM_SETIMAGE, IMAGE_BITMAP,
																	reinterpret_cast<LPARAM>(hbmpPreview)));
				if (NULL != hbmpOldPreview)
				{
					// The static control made a copy of the previous image which must be deleted when we set a new one.
					DeleteObject(hbmpOldPreview);
				}
				// Delete our original copy of the new image.
				DeleteObject(hbmpPreview);
			}
		}
	}

	if (NULL != pcd && bSaveSettings)
	{
		HWND hwndListBox;

		if (NULL != (hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_EXTENSIONS)))
		{
			LRESULT itemCount = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0);
			if (0 <= itemCount)
			{
				std::vector< std::basic_string<TCHAR> > vecStrings;
				std::basic_string<TCHAR> strLBItem;

				for (LRESULT i = 0; i < itemCount; i++)
				{
					if (LeoHelpers::GetListBoxItemText(hwndListBox, i, &strLBItem) && (!strLBItem.empty()))
					{
						vecStrings.push_back( strLBItem );
					}
				}
				pcd->dialogConfig.SetExcludedExtensions(vecStrings);
			}
		}

		if (NULL != (hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_EXCLUDED_HEADERS)))
		{
			LRESULT itemCount = SendMessage(hwndListBox, LB_GETCOUNT, 0, 0);
			if (0 <= itemCount)
			{
				std::vector< std::basic_string<TCHAR> > vecStrings;
				std::basic_string<TCHAR> strLBItem;

				for (LRESULT i = 0; i < itemCount; i++)
				{
					if (LeoHelpers::GetListBoxItemText(hwndListBox, i, &strLBItem) && (!strLBItem.empty()))
					{
						vecStrings.push_back( strLBItem );
					}
				}
				pcd->dialogConfig.SetExcludedHeaders(vecStrings);
			}
		}

		bool bThumbnailSettingsDiffer = true; // Assume they always differ for now. Coding up the comparison doesn't seem worth it.

		*pcd->pGlobalConfig = pcd->dialogConfig;

		if (!pcd->dialogConfig.Save()) // Doesn't matter which one we save as they're identical. Saving our one means we don't need to guard against any external modifications being saved by mistake (not that anything else modifies the config at the moment).
		{
			LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());
			MessageBox(hwndDlg, sl.Get(STR_ACTIVEX_MESSAGE_BOX_CONFIG_SAVE_ERROR), sl.Get(STR_TEXTTHUMB_PLUGIN_DESCRIPTION), MB_OK|MB_ICONEXCLAMATION);
		}

		if (NULL != pcd->hWndNotify)
		{
			PostMessage(pcd->hWndNotify, DVPLUGINMSG_REINITIALIZE, 0, pcd->dwNotifyData);

			if (bThumbnailSettingsDiffer)
			{
				// Old way: PostMessage(pcd->hWndNotify, DVPLUGINMSG_THUMBSCHANGED, DVPTCF_REDRAW | DVPTCF_FLUSHCACHE, 0);

				// Ask Opus to refresh all visible thumbnails, but leave the cache alone. Since we tell Opus to never
				// cache our thumbnails that is all we need to do.
				THUMBCACHECONTROLDATA ccd = {0};
				ccd.cbSize             = sizeof(ccd);
				ccd.iOperation         = TCCOP_EMPTY;
				ccd.pszPath            = NULL;
				ccd.pszFileNamePattern = NULL;
				ccd.hEvent             = NULL;
				ccd.dwFlags            = TCCF_REGENERATEONLY;
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
		// No need to delete the preview image that was displayed by the static as it will have done that itself during destruction.
		delete pcd;
		SetWindowLongPtr(hwndDlg, DWLP_USER, NULL);
	}

	return(bResult);
}

BOOL NTextThumbConfigDlg::configInitDialog(ConfigDlgData *pcd, HWND hwndDlg)
{
	BOOL bResult = TRUE;

	LeoHelpers::OpusStringLoader sl(pcd->pGlobalConfig->GetOpusPluginHelper());
	::SetWindowText(hwndDlg, sl.Get(STR_TEXTTHUMB_CONFIG_TITLE));

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

	HWND hwndListBoxConfigExt;
	HWND hwndEditConfigExt;
	HWND hwndButtonAddConfigExt;
	HWND hwndButtonDeleteConfigExt;
	HWND hwndCheckboxWrap;
	HWND hwndCheckboxRemoveBlankLines;
	HWND hwndRadioIconPrefs;
	HWND hwndRadioIconForceOn;
	HWND hwndRadioIconForceOff;
	HWND hwndCheckboxDescription;
	HWND hwndCheckboxFolderThumbs;
	HWND hwndButtonBackColor;
	HWND hwndButtonTextColor;
	HWND hwndButtonFont;
	HWND hwndEditFont;
	HWND hwndStaticCodePageLabel;
	HWND hwndComboCodePage;
	HWND hwndStaticPreview;
	HWND hwndListBoxExtensions;
	HWND hwndEditExtension;
	HWND hwndButtonAddExtension;
	HWND hwndButtonDeleteExtension;
	HWND hwndListBoxHeaders;
	HWND hwndEditHeader;
	HWND hwndButtonAddHeader;
	HWND hwndButtonDeleteHeader;
	HWND hwndGroupBoxFileTypeSettings;
	HWND hwndGroupBoxExtensionsIgnore;
	HWND hwndGroupBoxContentsIgnore;
	HWND hwndDefaults;
	HWND hwndOK;
	HWND hwndCancel;
	HWND hwndApply;

	RECT rcEditConfigExt;
	RECT rcButtonAddConfigExt;
	RECT rcButtonDeleteConfigExt;
	RECT rcCheckboxWrap;
	RECT rcCheckboxRemoveBlankLines;
	RECT rcRadioIconPrefs;
	RECT rcRadioIconForceOn;
	RECT rcRadioIconForceOff;
	RECT rcCheckboxDescription;
	RECT rcCheckboxFolderThumbs;
	RECT rcButtonBackColor;
	RECT rcButtonTextColor;
	RECT rcButtonFont;
	RECT rcEditFont;
	RECT rcStaticCodePageLabel;
	RECT rcComboCodePage;
	RECT rcEditExtension;
	RECT rcButtonAddExtension;
	RECT rcButtonDeleteExtension;
	RECT rcEditHeader;
	RECT rcButtonAddHeader;
	RECT rcButtonDeleteHeader;
	RECT rcDefaults;
	RECT rcOK;
	RECT rcCancel;
	RECT rcApply;

	RECT rcStaticPreview            = {0};
	RECT rcListBoxConfigExt         = {0};
	RECT rcListBoxExtensions        = {0};
	RECT rcListBoxHeaders           = {0};
	RECT rcGroupBoxFileTypeSettings = {0};
	RECT rcGroupBoxExtensionsIgnore = {0};
	RECT rcGroupBoxContentsIgnore   = {0};

	if (maker.CreateListBox(&hwndListBoxConfigExt,                                        IDC_LIST_CONFIGURED_EXTENSIONS,                                                      false, false)
	&&	maker.CreateEdit(   &hwndEditConfigExt,            &rcEditConfigExt,              IDC_EDIT_CONFIG_EXT,                                                                 false, layout.GetButtonWidthXPixels(), true, false, false, false)
	&&	maker.CreateButton( &hwndButtonAddConfigExt,       &rcButtonAddConfigExt,         IDC_BUTTON_ADD_CONFIGEXT,            STR_ACTIVEX_CONFIG_BUTTON_ADD_NOSHORTCUT,       false, false)
	&&	maker.CreateButton( &hwndButtonDeleteConfigExt,    &rcButtonDeleteConfigExt,      IDC_BUTTON_DELETE_CONFIGEXT,         STR_ACTIVEX_CONFIG_BUTTON_REMOVE_NOSHORTCUT,    false, false)
	&&	maker.CreateCheck(  &hwndCheckboxWrap,             &rcCheckboxWrap,               IDC_CHECK_WRAP,                      STR_TEXTTHUMB_CONFIG_WRAP_LINES,                false, false, false, true, true)
	&&	maker.CreateCheck(  &hwndCheckboxRemoveBlankLines, &rcCheckboxRemoveBlankLines,   IDC_CHECK_REMOVEBLANKLINES,          STR_TEXTTHUMB_CONFIG_REMOVE_BLANK_LINES,        false, false, false, true, true)
	&&	maker.CreateCheck(  &hwndRadioIconPrefs,           &rcRadioIconPrefs,             IDC_RADIO_ICONPREFS,                 STR_TEXTTHUMB_CONFIG_ICON_PER_PREFS,            false, true,  false, true, true)
	&&	maker.CreateCheck(  &hwndRadioIconForceOn,         &rcRadioIconForceOn,           IDC_RADIO_ICONON,                    STR_TEXTTHUMB_CONFIG_ICON_FORCE_ON,             false, true,  false, true, false)
	&&	maker.CreateCheck(  &hwndRadioIconForceOff,        &rcRadioIconForceOff,          IDC_RADIO_ICONOFF,                   STR_TEXTTHUMB_CONFIG_ICON_FORCE_OFF,            false, true,  false, true, false)
	&&	maker.CreateCheck(  &hwndCheckboxDescription,      &rcCheckboxDescription,        IDC_CHECK_DESCRIPTION,               STR_TEXTTHUMB_CONFIG_POPULATE_DESCRIPTION,      false, false, false, true, true)
	&&	maker.CreateCheck(  &hwndCheckboxFolderThumbs,     &rcCheckboxFolderThumbs,       IDC_CHECK_FOLDERTHUMBNAILS,          STR_TEXTTHUMB_CONFIG_FOLDER_THUMBS,             false, false, false, true, true)
	&&	maker.CreateButton( &hwndButtonBackColor,          &rcButtonBackColor,            IDC_BUTTON_COLOR,                    STR_TEXTTHUMB_CONFIG_BACK_COLOR,                false, false)
	&&	maker.CreateButton( &hwndButtonTextColor,          &rcButtonTextColor,            IDC_BUTTON_TEXTCOLOR,                STR_TEXTTHUMB_CONFIG_TEXT_COLOR,                false, false)
	&&	maker.CreateButton( &hwndButtonFont,               &rcButtonFont,                 IDC_BUTTON_FONT,                     STR_TEXTTHUMB_CONFIG_FONT,                      false, false)
	&&	maker.CreateEdit(   &hwndEditFont,                 &rcEditFont,                   IDC_EDIT_FONT,                                                                       false, layout.GetButtonWidthXPixels(), true, false, false, true)
	&&	maker.CreateLabel(  &hwndStaticCodePageLabel,      &rcStaticCodePageLabel,        IDC_STATIC_CODEPAGE,                 STR_TEXTTHUMB_CONFIG_CODE_PAGE,                 false, true, true)
	&&	maker.CreateCombo(  &hwndComboCodePage,            &rcComboCodePage,              IDC_COMBO_CODEPAGE,                                                                  false, true, false)
	&&	maker.CreateBitmap( &hwndStaticPreview,                                           IDC_STATIC_PREVIEW,                                                                  true)
	&&	maker.CreateListBox(&hwndListBoxExtensions,                                       IDC_LIST_EXCLUDED_EXTENSIONS,                                                        true, true)
	&&	maker.CreateEdit(   &hwndEditExtension,            &rcEditExtension,              IDC_EDIT_EXTENSION,                                                                  false, layout.GetButtonWidthXPixels(), true, false, false, false)
	&&	maker.CreateButton( &hwndButtonAddExtension,       &rcButtonAddExtension,         IDC_BUTTON_ADD_EXTENSION,            STR_ACTIVEX_CONFIG_BUTTON_ADD_NOSHORTCUT,       false, false)
	&&	maker.CreateButton( &hwndButtonDeleteExtension,    &rcButtonDeleteExtension,      IDC_BUTTON_DELETE_EXTENSION,         STR_ACTIVEX_CONFIG_BUTTON_REMOVE_NOSHORTCUT,    false, false)
	&&	maker.CreateListBox(&hwndListBoxHeaders,                                          IDC_LIST_EXCLUDED_HEADERS,                                                           true, true)
	&&	maker.CreateEdit(   &hwndEditHeader,               &rcEditHeader,                 IDC_EDIT_HEADER,                                                                     false, layout.GetButtonWidthXPixels(), true, false, false, false)
	&&	maker.CreateButton( &hwndButtonAddHeader,          &rcButtonAddHeader,            IDC_BUTTON_ADD_HEADER,               STR_ACTIVEX_CONFIG_BUTTON_ADD_NOSHORTCUT,       false, false)
	&&	maker.CreateButton( &hwndButtonDeleteHeader,       &rcButtonDeleteHeader,         IDC_BUTTON_DELETE_HEADER,            STR_ACTIVEX_CONFIG_BUTTON_REMOVE_NOSHORTCUT,    false, false)
	&&	maker.CreateGroup(  &hwndGroupBoxFileTypeSettings,                                IDC_GROUPBOX_FILE_TYPE_SETTINGS,     STR_TEXTTHUMB_CONFIG_FILE_TYPE_SETTINGS)
	&&	maker.CreateGroup(  &hwndGroupBoxExtensionsIgnore,                                IDC_GROUPBOX_FILE_EXTENSIONS_IGNORE, STR_TEXTTHUMB_CONFIG_EXTENSIONS_IGNORE)
	&&	maker.CreateGroup(  &hwndGroupBoxContentsIgnore,                                  IDC_GROUPBOX_FILE_CONTENTS_IGNORE,   STR_TEXTTHUMB_CONFIG_CONTENTS_IGNORE)
	&&	maker.CreateButton( &hwndDefaults,                 &rcDefaults,                   IDC_BUTTON_DEFAULTS,                 STR_ACTIVEX_CONFIG_BUTTON_DEFAULTS,             false, false)
	&&	maker.CreateButton( &hwndOK,                       &rcOK,                         IDOK,                                STR_ACTIVEX_CONFIG_BUTTON_OK,                   false, true)
	&&	maker.CreateButton( &hwndCancel,                   &rcCancel,                     IDCANCEL,                            STR_ACTIVEX_CONFIG_BUTTON_CANCEL,               false, false)
	&&	maker.CreateButton( &hwndApply,                    &rcApply,                      IDC_BUTTON_APPLY,                    STR_ACTIVEX_CONFIG_BUTTON_APPLY,                false, false))
	{
		RECT rcListButton;
		::UnionRect(&rcListButton, &rcButtonAddConfigExt, &rcButtonDeleteConfigExt);
		LeoHelpers::UnionRect(&rcListButton, &rcButtonAddExtension);
		LeoHelpers::UnionRect(&rcListButton, &rcButtonDeleteExtension);
		LeoHelpers::UnionRect(&rcListButton, &rcButtonAddHeader);
		LeoHelpers::UnionRect(&rcListButton, &rcButtonDeleteHeader);
		rcButtonAddConfigExt    = rcListButton;
		rcButtonDeleteConfigExt = rcListButton;
		rcButtonAddExtension    = rcListButton;
		rcButtonDeleteExtension = rcListButton;
		rcButtonAddHeader       = rcListButton;
		rcButtonDeleteHeader    = rcListButton;

		LONG lListWidth = (rcListButton.right - rcListButton.left) * 2 + layout.GetRelatedControlGapXPixels();
		rcEditConfigExt.right     = rcEditConfigExt.left     + lListWidth;
		rcEditExtension.right     = rcEditExtension.left     + lListWidth;
		rcEditHeader.right        = rcEditHeader.left        + lListWidth;
		rcListBoxConfigExt.right  = rcListBoxConfigExt.left  + lListWidth;
		rcListBoxExtensions.right = rcListBoxExtensions.left + lListWidth;
		rcListBoxHeaders.right    = rcListBoxHeaders.left    + lListWidth;

		LONG lWidthSettingsButton = rcButtonBackColor.right - rcButtonBackColor.left;
		if (lWidthSettingsButton < (rcButtonTextColor.right - rcButtonTextColor.left))
		{
			lWidthSettingsButton = (rcButtonTextColor.right - rcButtonTextColor.left);
		}
		if (lWidthSettingsButton < (rcButtonFont.right - rcButtonFont.left))
		{
			lWidthSettingsButton = (rcButtonFont.right - rcButtonFont.left);
		}
		if (lWidthSettingsButton < (rcStaticCodePageLabel.right - rcStaticCodePageLabel.left))
		{
			lWidthSettingsButton = (rcStaticCodePageLabel.right - rcStaticCodePageLabel.left);
		}
		rcButtonBackColor.right = rcButtonBackColor.left + lWidthSettingsButton;
		rcButtonTextColor.right = rcButtonTextColor.left + lWidthSettingsButton;
		rcButtonFont.right      = rcButtonFont.left      + lWidthSettingsButton;

		rcStaticCodePageLabel.bottom = rcStaticCodePageLabel.top + (rcComboCodePage.bottom - rcComboCodePage.top);

		::OffsetRect(&rcRadioIconPrefs, lListWidth + 3 * layout.GetMarginPixels(), layout.GetMarginPixels() + layout.GetGroupBoxFirstControlYPixels());
		LeoHelpers::BelowRect(&rcRadioIconForceOn,           &rcRadioIconPrefs,           layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcRadioIconForceOff,          &rcRadioIconForceOn,         layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxWrap,               &rcRadioIconForceOff,        layout.GetMarginPixels());
		LeoHelpers::BelowRect(&rcCheckboxRemoveBlankLines,   &rcCheckboxWrap,             layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxDescription,        &rcCheckboxRemoveBlankLines, layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcCheckboxFolderThumbs,       &rcCheckboxDescription,      layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcButtonBackColor,            &rcCheckboxFolderThumbs,     layout.GetMarginPixels());
		LeoHelpers::BelowRect(&rcButtonTextColor,            &rcButtonBackColor,          layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRect(&rcButtonFont,                 &rcButtonTextColor,          layout.GetRelatedControlGapYPixels());
		LeoHelpers::BelowRightRect(&rcStaticCodePageLabel,   &rcButtonFont,               layout.GetRelatedControlGapYPixels());

		LeoHelpers::RightRect(&rcEditFont,                   &rcButtonFont,               layout.GetRelatedControlGapXPixels());
		LeoHelpers::RightRect(&rcComboCodePage,              &rcStaticCodePageLabel,      layout.GetRelatedControlGapXPixels());

		rcButtonAddConfigExt.left   = layout.GetMarginPixels() * 2;
		rcButtonAddConfigExt.right  = rcButtonAddConfigExt.left + (rcListButton.right - rcListButton.left);
		rcButtonAddConfigExt.bottom = rcComboCodePage.bottom;
		rcButtonAddConfigExt.top    = rcButtonAddConfigExt.bottom - (rcListButton.bottom - rcListButton.top);
		LeoHelpers::RightRect(&rcButtonDeleteConfigExt,      &rcButtonAddConfigExt,       layout.GetRelatedControlGapXPixels());
		LeoHelpers::AboveRect(&rcEditConfigExt,              &rcButtonAddConfigExt,       layout.GetRelatedControlGapXPixels() / 2);
		LeoHelpers::AboveRect(&rcListBoxConfigExt,           &rcEditConfigExt,            layout.GetRelatedControlGapXPixels() / 2);
		rcListBoxConfigExt.top = rcRadioIconPrefs.top;

		LONG lMaxSettingsRight = rcRadioIconPrefs.right;
		if (lMaxSettingsRight < rcRadioIconForceOn.right        ) { lMaxSettingsRight = rcRadioIconForceOn.right;         }
		if (lMaxSettingsRight < rcRadioIconForceOff.right       ) { lMaxSettingsRight = rcRadioIconForceOff.right;        }
		if (lMaxSettingsRight < rcCheckboxWrap.right            ) { lMaxSettingsRight = rcCheckboxWrap.right;             }
		if (lMaxSettingsRight < rcCheckboxRemoveBlankLines.right) { lMaxSettingsRight = rcCheckboxRemoveBlankLines.right; }
		if (lMaxSettingsRight < rcCheckboxDescription.right     ) { lMaxSettingsRight = rcCheckboxDescription.right;      }
		if (lMaxSettingsRight < rcCheckboxFolderThumbs.right    ) { lMaxSettingsRight = rcCheckboxFolderThumbs.right;     }

		rcStaticPreview.top    = rcRadioIconPrefs.top;
		rcStaticPreview.bottom = rcCheckboxFolderThumbs.bottom + layout.GetMarginPixels();
		rcStaticPreview.left   = lMaxSettingsRight + layout.GetMarginPixels();
		rcStaticPreview.right  = rcStaticPreview.left + (rcStaticPreview.bottom - rcStaticPreview.top);

		rcEditFont.right      = rcStaticPreview.right;
		rcComboCodePage.right = rcStaticPreview.right;

		rcGroupBoxFileTypeSettings.left   = layout.GetMarginPixels();
		rcGroupBoxFileTypeSettings.top    = layout.GetMarginPixels();
		rcGroupBoxFileTypeSettings.right  = rcComboCodePage.right  + layout.GetMarginPixels();
		rcGroupBoxFileTypeSettings.bottom = rcComboCodePage.bottom + layout.GetMarginPixels();

		LONG lRightGroupsHeight = ((rcGroupBoxFileTypeSettings.bottom - rcGroupBoxFileTypeSettings.top) - layout.GetMarginPixels()) / 2;
		rcGroupBoxExtensionsIgnore.left   = 0;
		rcGroupBoxExtensionsIgnore.right  = layout.GetMarginPixels() * 2 + lListWidth;
		rcGroupBoxExtensionsIgnore.top    = 0;
		rcGroupBoxExtensionsIgnore.bottom = lRightGroupsHeight;
		LeoHelpers::RightRect(&rcGroupBoxExtensionsIgnore, &rcGroupBoxFileTypeSettings, layout.GetMarginPixels());

		rcGroupBoxContentsIgnore = rcGroupBoxExtensionsIgnore;
		LeoHelpers::RightBelowRect(&rcGroupBoxContentsIgnore, &rcGroupBoxFileTypeSettings, layout.GetMarginPixels());

		LeoHelpers::RightRect(&rcButtonAddHeader,    &rcComboCodePage,   3 * layout.GetMarginPixels());
		LeoHelpers::RightRect(&rcButtonDeleteHeader, &rcButtonAddHeader, layout.GetRelatedControlGapXPixels());
		LeoHelpers::AboveRect(&rcEditHeader,         &rcButtonAddHeader, layout.GetRelatedControlGapXPixels() / 2);
		LeoHelpers::AboveRect(&rcListBoxHeaders,     &rcEditHeader,      layout.GetRelatedControlGapXPixels() / 2);
		rcListBoxHeaders.top = rcGroupBoxContentsIgnore.top + layout.GetGroupBoxFirstControlYPixels();
		rcListBoxConfigExt.top = rcRadioIconPrefs.top;

		rcListBoxExtensions     = rcListBoxHeaders;
		rcEditExtension         = rcEditHeader;
		rcButtonAddExtension    = rcButtonAddHeader;
		rcButtonDeleteExtension = rcButtonDeleteHeader;
		LONG lRightGroupYOffset = rcGroupBoxContentsIgnore.top - rcGroupBoxExtensionsIgnore.top;
		::OffsetRect(&rcListBoxExtensions,     0, -lRightGroupYOffset);
		::OffsetRect(&rcEditExtension,         0, -lRightGroupYOffset);
		::OffsetRect(&rcButtonAddExtension,    0, -lRightGroupYOffset);
		::OffsetRect(&rcButtonDeleteExtension, 0, -lRightGroupYOffset);

		LeoHelpers::BelowRect(&rcDefaults, &rcGroupBoxFileTypeSettings, layout.GetMarginPixels());
		LeoHelpers::BelowRightRect(&rcApply, &rcGroupBoxContentsIgnore, layout.GetMarginPixels());
		LeoHelpers::LeftRect(&rcCancel, &rcApply, layout.GetButtonGapXPixels());
		LeoHelpers::LeftRect(&rcOK, &rcCancel, layout.GetButtonGapXPixels());

		RECT rcEverything = rcGroupBoxFileTypeSettings;
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

		// Special change for combo controls: Their unexpanded height is fixed and their window height determines the height of the drop-down (at least on earlier versions of Windows).
		rcComboCodePage.bottom = rcComboCodePage.top + (rcGroupBoxFileTypeSettings.bottom - rcGroupBoxFileTypeSettings.top);

		LeoHelpers::MoveWindowRect(hwndListBoxConfigExt,         &rcListBoxConfigExt,         FALSE);
		LeoHelpers::MoveWindowRect(hwndEditConfigExt,            &rcEditConfigExt,            FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonAddConfigExt,       &rcButtonAddConfigExt,       FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonDeleteConfigExt,    &rcButtonDeleteConfigExt,    FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxWrap,             &rcCheckboxWrap,             FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxRemoveBlankLines, &rcCheckboxRemoveBlankLines, FALSE);
		LeoHelpers::MoveWindowRect(hwndRadioIconPrefs,           &rcRadioIconPrefs,           FALSE);
		LeoHelpers::MoveWindowRect(hwndRadioIconForceOn,         &rcRadioIconForceOn,         FALSE);
		LeoHelpers::MoveWindowRect(hwndRadioIconForceOff,        &rcRadioIconForceOff,        FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxDescription,      &rcCheckboxDescription,      FALSE);
		LeoHelpers::MoveWindowRect(hwndCheckboxFolderThumbs,     &rcCheckboxFolderThumbs,     FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonBackColor,          &rcButtonBackColor,          FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonTextColor,          &rcButtonTextColor,          FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonFont,               &rcButtonFont,               FALSE);
		LeoHelpers::MoveWindowRect(hwndEditFont,                 &rcEditFont,                 FALSE);
		LeoHelpers::MoveWindowRect(hwndStaticCodePageLabel,      &rcStaticCodePageLabel,      FALSE);
		LeoHelpers::MoveWindowRect(hwndComboCodePage,            &rcComboCodePage,            FALSE);
		LeoHelpers::MoveWindowRect(hwndStaticPreview,            &rcStaticPreview,            FALSE);
		LeoHelpers::MoveWindowRect(hwndListBoxExtensions,        &rcListBoxExtensions,        FALSE);
		LeoHelpers::MoveWindowRect(hwndEditExtension,            &rcEditExtension,            FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonAddExtension,       &rcButtonAddExtension,       FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonDeleteExtension,    &rcButtonDeleteExtension,    FALSE);
		LeoHelpers::MoveWindowRect(hwndListBoxHeaders,           &rcListBoxHeaders,           FALSE);
		LeoHelpers::MoveWindowRect(hwndEditHeader,               &rcEditHeader,               FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonAddHeader,          &rcButtonAddHeader,          FALSE);
		LeoHelpers::MoveWindowRect(hwndButtonDeleteHeader,       &rcButtonDeleteHeader,       FALSE);
		LeoHelpers::MoveWindowRect(hwndGroupBoxFileTypeSettings, &rcGroupBoxFileTypeSettings, FALSE);
		LeoHelpers::MoveWindowRect(hwndGroupBoxExtensionsIgnore, &rcGroupBoxExtensionsIgnore, FALSE);
		LeoHelpers::MoveWindowRect(hwndGroupBoxContentsIgnore,   &rcGroupBoxContentsIgnore,   FALSE);
		LeoHelpers::MoveWindowRect(hwndDefaults,                 &rcDefaults,                 FALSE);
		LeoHelpers::MoveWindowRect(hwndOK,                       &rcOK,                       FALSE);
		LeoHelpers::MoveWindowRect(hwndCancel,                   &rcCancel,                   FALSE);
		LeoHelpers::MoveWindowRect(hwndApply,                    &rcApply,                    FALSE);
	}

	// Window setup.

	LeoHelpers::CenterWindow(hwndDlg, GetParent(hwndDlg));

	if (hwndListBoxConfigExt != NULL)
	{
		// Set focus. (Do not use SetFocus in dialogs. Use WM_NEXTDLGCTL instead.)
		SendMessage(hwndDlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(hwndListBoxConfigExt), TRUE);

		bResult = FALSE; // Do *not* allow keyboard focus to be set to whatever is in wParam. We set focus to something more suitable.
	}
	else
	{
		bResult = TRUE; // Allow keyboard focus to be set to whatever is in wParam.
	}

	return bResult;
}

CTextThumbConfig::CTypeConfig NTextThumbConfigDlg::getCurrentTypeConfig(ConfigDlgData *pcd, HWND hwndDlg, HWND *phwndListBox, bool *pbIsDefault, LRESULT *pSelIdx)
{
	HWND hwndListBox = GetDlgItem(hwndDlg, IDC_LIST_CONFIGURED_EXTENSIONS);

	if (NULL != phwndListBox)
	{
		*phwndListBox = hwndListBox;
	}

	if (NULL != hwndListBox)
	{
		LRESULT selIdx = SendMessage(hwndListBox, LB_GETCURSEL, 0, 0);

		if (selIdx < 0)
		{
			selIdx = 0;
			SendMessage(hwndListBox, LB_SETCURSEL, selIdx, 0);
		}

		if (pSelIdx != NULL)
		{
			*pSelIdx = selIdx;
		}

		LRESULT bIsDefault = SendMessage(hwndListBox, LB_GETITEMDATA, selIdx, 0);

		if (!bIsDefault)
		{
			if (pbIsDefault != NULL)
			{
				*pbIsDefault = false;
			}
			
			std::basic_string< TCHAR > strName;
			if (LeoHelpers::GetListBoxItemText(hwndListBox, selIdx, &strName))
			{
				return pcd->dialogConfig.GetTypeConfig(strName);
			}
		}
	}
	else if (pSelIdx != NULL)
	{
		*pSelIdx = 0;
	}

	if (pbIsDefault != NULL)
	{
		*pbIsDefault = true;
	}

	return pcd->dialogConfig.GetTypeConfig(_T(""));
}
