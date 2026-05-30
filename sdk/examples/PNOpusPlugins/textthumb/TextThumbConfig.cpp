#include "StdAfx.h"
#include "LeoHelpers.h"
#include "TextThumbConfig.h"

#define TEXTTHUMB_REG_PATH _T("Software\\GPSoftware\\Directory Opus\\Config\\User\\Viewers\\TextThumb")
#define TEXTTHUMB_CONFIG_NAME _T("TextThumb")

#define TTC_NODE_EXCLUDEDEXTENSION _T("ExcludedExtension")
#define TTC_NODE_EXCLUDEDHEADER _T("ExcludedHeader")
#define TCC_NODE_TYPECONFIG _T("TypeConfig")
#define TCC_NODE_BACKGROUNDCOLOR _T("BackgroundColor")
#define TCC_NODE_TEXTCOLOR _T("TextColor")
#define TCC_NODE_CODEPAGE _T("CodePage")
#define TCC_NODE_FLAGS _T("Flags")
#define TCC_NODE_FONT _T("Font")
#define TCC_ATTRIBUTE_NAME _T("Name")
#define TCC_ATTRIBUTE_WRAPLINES _T("WrapLines")
#define TCC_ATTRIBUTE_REMOVEBLANKLINES _T("RemoveBlankLines")
#define TCC_ATTRIBUTE_FORCEICONON _T("ForceIconOn")
#define TCC_ATTRIBUTE_FORCEICONOFF _T("ForceIconOff")
#define TCC_ATTRIBUTE_CONTENTSINDESCRIPTION _T("ContentsInDescription")
#define TCC_ATTRIBUTE_INCLUDEINFOLDERTHUMBNAILS _T("IncludeInFolderThumbnails")

CTextThumbConfig::CTextThumbConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperTextThumbPlugin *pDOpusPluginHelper)
	: m_hModuleDll(hModuleDll)
	, m_dw64OpusVersion(dw64OpusVersion)
	, m_pDOpusPluginHelper(pDOpusPluginHelper)
	, m_sl(pDOpusPluginHelper)
	, m_bWindowsXPOrAbove(LeoHelpers::IsWindowsXPOrAbove())
{
	InitializeCriticalSection(&m_cs);
}

CTextThumbConfig::~CTextThumbConfig(void)
{
	DeleteCriticalSection(&m_cs);
}

CTextThumbConfig::CTextThumbConfig(const CTextThumbConfig &rhs)
: m_sl(rhs.m_pDOpusPluginHelper) // This is safe to do outside of the critical section.
{
	InitializeCriticalSection(&m_cs);

	LeoHelpers::CriticalSectionScoper css(&rhs.m_cs);

	m_hModuleDll         = rhs.m_hModuleDll;
	m_dw64OpusVersion    = rhs.m_dw64OpusVersion;
	m_pDOpusPluginHelper = rhs.m_pDOpusPluginHelper;
	m_bWindowsXPOrAbove  = rhs.m_bWindowsXPOrAbove;

	m_vecExcludedExtensions = rhs.m_vecExcludedExtensions;
	m_vecExcludedHeaders    = rhs.m_vecExcludedHeaders;
	m_mapExtTypeConfigs     = rhs.m_mapExtTypeConfigs;
}

CTextThumbConfig &CTextThumbConfig::operator=(const CTextThumbConfig &rhs)
{
	if (this != &rhs)
	{
		LeoHelpers::CriticalSectionScoper css1(&m_cs);
		LeoHelpers::CriticalSectionScoper css2(&rhs.m_cs);

		assert(m_hModuleDll         == rhs.m_hModuleDll);
		assert(m_dw64OpusVersion    == rhs.m_dw64OpusVersion);
		assert(m_pDOpusPluginHelper == rhs.m_pDOpusPluginHelper); // If this isn't true then we have a problem with m_sl as well.
		assert(m_bWindowsXPOrAbove  == rhs.m_bWindowsXPOrAbove);

		m_vecExcludedExtensions = rhs.m_vecExcludedExtensions;
		m_vecExcludedHeaders    = rhs.m_vecExcludedHeaders;
		m_mapExtTypeConfigs     = rhs.m_mapExtTypeConfigs;
	}

	return *this;
}

bool CTextThumbConfig::CheckOpusAbility(OpusAbility ability) const
{
	switch(ability)
	{
	case(TTA_RUN):
		return(m_dw64OpusVersion >= MAKE64BITVERSIONNUMBER(9,1,1,3));
	default:
		return(false);
	}
}

inline bool CTextThumbConfig::growCharBuffer(wchar_t **pCharBuffer, INT *pCharBufferSize, INT reqSize)
{
	if (reqSize > *pCharBufferSize)
	{
		delete[] *pCharBuffer;
		*pCharBuffer = new(std::nothrow) wchar_t[reqSize];
		*pCharBufferSize = (*pCharBuffer != NULL) ? reqSize : 0;
	}
	return(NULL != *pCharBuffer);
}

bool CTextThumbConfig::xmlGetNodeName(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeName(hNode, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeName(hNode, *pCharBuffer, &reqSize));
}

bool CTextThumbConfig::xmlGetNodeValue(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeValue(hNode, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeValue(hNode, *pCharBuffer, &reqSize));
}

bool CTextThumbConfig::xmlGetNodeAttribute(HANDLE hNode, const wchar_t *szAttributeName, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeAttribute(hNode, szAttributeName, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeAttribute(hNode, szAttributeName, *pCharBuffer, &reqSize));
}

void CTextThumbConfig::clear()
{
	m_vecExcludedExtensions.clear();
	m_vecExcludedHeaders.clear();
	m_mapExtTypeConfigs.clear();
}

bool CTextThumbConfig::getConfigPath(std::basic_string<TCHAR> *pStrPath) const
{
	TCHAR szConfigPath[MAX_PATH];

	if (!m_pDOpusPluginHelper->GetConfigPath(OPUSPATH_CONFIG, szConfigPath, _countof(szConfigPath)))
	{
		pStrPath->clear();
		return false;
	}
	else
	{
		*pStrPath = szConfigPath;
		LeoHelpers::AppendPathString(pStrPath, TEXTTHUMB_CONFIG_NAME);
		pStrPath->append(_T(".oxc"));
		return true;
	}
}

bool CTextThumbConfig::Load()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	clear();

	if (!loadXml() && !loadRegistry())
	{
		LoadDefaults();
	}

	if (m_mapExtTypeConfigs.find(_T("")) == m_mapExtTypeConfigs.end())
	{
		// There must always be a default config. Only an invalid configuration would be missing one. Copy the default-default over.
		std::map< std::basic_string<TCHAR>, CTypeConfig > mapExtTypeConfigDefaults;
		loadDefaultExtTypeConfigs(&mapExtTypeConfigDefaults);

		m_mapExtTypeConfigs[ _T("") ] = mapExtTypeConfigDefaults[ _T("") ];
	}

	return true;
}

bool CTextThumbConfig::Save()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	return saveXml();
}

void CTextThumbConfig::LoadDefaults()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	clear();

	m_vecExcludedExtensions.push_back( _T(".html") );
	m_vecExcludedExtensions.push_back( _T(".htm") );
	m_vecExcludedExtensions.push_back( _T(".mht") );
	m_vecExcludedExtensions.push_back( _T(".mhtml") );
	m_vecExcludedExtensions.push_back( _T(".svg") );

	m_vecExcludedHeaders.push_back( _T("%PDF-") );
	m_vecExcludedHeaders.push_back( _T("%!PS-") );
	m_vecExcludedHeaders.push_back( _T("{\\rtf") );

	loadDefaultExtTypeConfigs(&m_mapExtTypeConfigs);
}

void CTextThumbConfig::loadDefaultExtTypeConfigs(std::map< std::basic_string<TCHAR>, CTypeConfig > *pmapExtTypeConfigs) const
{
	LOGFONT logFontNormal; GetDefaultFont(CP_ACP, &logFontNormal);
	LOGFONT logFontDOS;    GetDefaultFont(437,    &logFontDOS);

	// These extensions must be lower-case.
	(*pmapExtTypeConfigs)[ _T("")     ] = CTypeConfig(_T(""),     TTF_ICON_ON,                          RGB(0,0,0), RGB(218,218,218), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".bat") ] = CTypeConfig(_T(".bat"), TTF_ICON_ON|TTF_FOLDERTHUMBNAILS_OFF, RGB(0,0,0), RGB(184,184,220), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".cpp") ] = CTypeConfig(_T(".cpp"), TTF_ICON_ON,                          RGB(0,0,0), RGB(201,165,241), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".ini") ] = CTypeConfig(_T(".ini"), TTF_ICON_ON|TTF_FOLDERTHUMBNAILS_OFF, RGB(0,0,0), RGB(181,235,253), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".reg") ] = CTypeConfig(_T(".reg"), TTF_ICON_ON,                          RGB(0,0,0), RGB(181,235,253), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".sql") ] = CTypeConfig(_T(".sql"), TTF_ICON_ON,                          RGB(0,0,0), RGB(255,151,151), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".txt") ] = CTypeConfig(_T(".txt"), TTF_ICON_ON,                          RGB(0,0,0), RGB(205,205,188), CP_ACP, logFontNormal);
	(*pmapExtTypeConfigs)[ _T(".nfo") ] = CTypeConfig(_T(".nfo"), 0,                                    RGB(0,0,0), RGB(218,218,218), 437,    logFontDOS);
	(*pmapExtTypeConfigs)[ _T(".diz") ] = CTypeConfig(_T(".diz"), 0,                                    RGB(0,0,0), RGB(218,218,218), 437,    logFontDOS);
	(*pmapExtTypeConfigs)[ _T(".asc") ] = CTypeConfig(_T(".asc"), 0,                                    RGB(0,0,0), RGB(218,218,218), 437,    logFontDOS);
}

bool CTextThumbConfig::loadXml()
{
	bool bResult = false;

	clear();

	std::basic_string<TCHAR> strConfigPath;
	HANDLE hRootNode;

	if (getConfigPath(&strConfigPath)
	&&	NULL != (hRootNode = m_pDOpusPluginHelper->XMLLoadFile(strConfigPath.c_str())))
	{
		INT charBufferSize = 1024;
		TCHAR *charBuffer = new TCHAR[charBufferSize];

		if (loadXmlContent(hRootNode, &charBuffer, &charBufferSize))
		{
			bResult = true;
		}

		delete [] charBuffer;

		m_pDOpusPluginHelper->XMLFreeFile(hRootNode);
	}

	return bResult;
}

bool CTextThumbConfig::saveXml()
{
	bool bResult = false;

	std::basic_string<TCHAR> strConfigPath;
	HANDLE hRootNode;

	if (getConfigPath(&strConfigPath)
	&&	NULL != (hRootNode = m_pDOpusPluginHelper->XMLCreateFile()))
	{
		if (saveXmlContent(hRootNode)
		&&	m_pDOpusPluginHelper->XMLSaveFile(hRootNode, strConfigPath.c_str()))
		{
			bResult = true;
		}

		m_pDOpusPluginHelper->XMLFreeFile(hRootNode);
	}

	return bResult;
}

bool CTextThumbConfig::loadXmlContent(HANDLE hRootNode, TCHAR **pCharBuffer, INT *pCharBufferSize)
{
	// Create a default config to fill in any blanks in the config data.
	CTextThumbConfig defaultConfig(m_hModuleDll, m_dw64OpusVersion, m_pDOpusPluginHelper);
	defaultConfig.LoadDefaults();

	if (!xmlGetNodeName(hRootNode, pCharBuffer, pCharBufferSize)
	||	0 != _tcscmp(*pCharBuffer, TEXTTHUMB_CONFIG_NAME))
	{
		return false;
	}

	for(HANDLE hNode = m_pDOpusPluginHelper->XMLFirstChildNode(hRootNode); hNode != NULL; hNode = m_pDOpusPluginHelper->XMLNextNode(hNode))
	{
		if (xmlGetNodeName(hNode, pCharBuffer, pCharBufferSize))
		{
			if (0 == _tcscmp(*pCharBuffer, TTC_NODE_EXCLUDEDEXTENSION))
			{
				if (!xmlGetNodeValue(hNode, pCharBuffer, pCharBufferSize))
				{
					return false;
				}
				else
				{
					m_vecExcludedExtensions.push_back(*pCharBuffer);
				}
			}
			else if (0 == _tcscmp(*pCharBuffer, TTC_NODE_EXCLUDEDHEADER))
			{
				if (!xmlGetNodeValue(hNode, pCharBuffer, pCharBufferSize))
				{
					return false;
				}
				else
				{
					m_vecExcludedHeaders.push_back(*pCharBuffer);
				}
			}
			else if (0 == _tcscmp(*pCharBuffer, TCC_NODE_TYPECONFIG))
			{
				if (!xmlGetNodeAttribute(hNode, TCC_ATTRIBUTE_NAME, pCharBuffer, pCharBufferSize))
				{
					return false;
				}
				else
				{
					std::basic_string< TCHAR > strExt = *pCharBuffer;
					std::basic_string< TCHAR > strExtLower;
					LeoHelpers::ToLower(&strExtLower, strExt);

					CTypeConfig typeConfig = defaultConfig.GetTypeConfig(strExtLower); // Inherit from the defaults for this file extension, or the default-default.
					typeConfig.strExt = strExt;

					HANDLE hNodeBackgroundColor = m_pDOpusPluginHelper->XMLFindChildNode(hNode, TCC_NODE_BACKGROUNDCOLOR);
					HANDLE hNodeTextColor       = m_pDOpusPluginHelper->XMLFindChildNode(hNode, TCC_NODE_TEXTCOLOR);
					HANDLE hNodeCodePage        = m_pDOpusPluginHelper->XMLFindChildNode(hNode, TCC_NODE_CODEPAGE);
					HANDLE hNodeFont            = m_pDOpusPluginHelper->XMLFindChildNode(hNode, TCC_NODE_FONT);
					HANDLE hNodeFlags           = m_pDOpusPluginHelper->XMLFindChildNode(hNode, TCC_NODE_FLAGS);

					DWORD dwTemp;

					if (hNodeBackgroundColor != NULL && m_pDOpusPluginHelper->XMLGetNodeDWORDValue(hNodeBackgroundColor, &dwTemp)) { typeConfig.crBackground = dwTemp; }
					if (hNodeTextColor       != NULL && m_pDOpusPluginHelper->XMLGetNodeDWORDValue(hNodeTextColor,       &dwTemp)) { typeConfig.crText       = dwTemp; }
					if (hNodeCodePage        != NULL && m_pDOpusPluginHelper->XMLGetNodeDWORDValue(hNodeCodePage,        &dwTemp)) { typeConfig.dwCodePage   = dwTemp; }

					LOGFONT logFontTemp;

					if ((hNodeFont != NULL && m_pDOpusPluginHelper->XMLGetNodeLOGFONTValue(hNodeFont, &logFontTemp))
					||	GetDefaultFont(typeConfig.dwCodePage, &logFontTemp))
					{
						typeConfig.logFont = logFontTemp;
					}

					if (hNodeFlags != NULL)
					{
						BOOL bTemp;					

						bool bWrapLines                 = (typeConfig.dwFlags & TTF_WRAP_ON)               ? true  : false;
						bool bRemoveBlankLines          = (typeConfig.dwFlags & TTF_REMOVE_BLANK_LINES_ON) ? true  : false;
						bool bForceIconOn               = (typeConfig.dwFlags & TTF_ICON_ON)               ? true  : false;
						bool bForceIconOff              = (typeConfig.dwFlags & TTF_ICON_OFF)              ? true  : false;
						bool bContentsInDescription     = (typeConfig.dwFlags & TTF_DESCRIPTION_ON)        ? true  : false;
						bool bIncludeInFolderThumbnails = (typeConfig.dwFlags & TTF_FOLDERTHUMBNAILS_OFF)  ? false : true;

						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_WRAPLINES,                 &bTemp)) { bWrapLines                 = (bTemp ? true : false); }
						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_REMOVEBLANKLINES,          &bTemp)) { bRemoveBlankLines          = (bTemp ? true : false); }
						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_FORCEICONON,               &bTemp)) { bForceIconOn               = (bTemp ? true : false); }
						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_FORCEICONOFF,              &bTemp)) { bForceIconOff              = (bTemp ? true : false); }
						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_CONTENTSINDESCRIPTION,     &bTemp)) { bContentsInDescription     = (bTemp ? true : false); }
						if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_INCLUDEINFOLDERTHUMBNAILS, &bTemp)) { bIncludeInFolderThumbnails = (bTemp ? true : false); }

						typeConfig.dwFlags = 0;
						if ( bWrapLines)                 { typeConfig.dwFlags += TTF_WRAP_ON;               }
						if ( bRemoveBlankLines)          { typeConfig.dwFlags += TTF_REMOVE_BLANK_LINES_ON; }
						if ( bForceIconOn)               { typeConfig.dwFlags += TTF_ICON_ON;               }
						if ( bForceIconOff)              { typeConfig.dwFlags += TTF_ICON_OFF;              }
						if ( bContentsInDescription)     { typeConfig.dwFlags += TTF_DESCRIPTION_ON;        }
						if (!bIncludeInFolderThumbnails) { typeConfig.dwFlags += TTF_FOLDERTHUMBNAILS_OFF;  }
					}

					m_mapExtTypeConfigs[strExtLower] = typeConfig;
				}
			}
		}
	}

	return true;
}

bool CTextThumbConfig::saveXmlContent(HANDLE hRootNode)
{
	if (!m_pDOpusPluginHelper->XMLSetNodeName(hRootNode, TEXTTHUMB_CONFIG_NAME))
	{
		return false;
	}

	for(std::vector< std::basic_string<TCHAR> >::const_iterator pExcludedExtension = m_vecExcludedExtensions.begin(); pExcludedExtension != m_vecExcludedExtensions.end(); ++pExcludedExtension)
	{
		HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, TTC_NODE_EXCLUDEDEXTENSION);
		if (hNode == NULL
		||	!m_pDOpusPluginHelper->XMLSetNodeValue(hNode, pExcludedExtension->c_str()))
		{
			return false;
		}
	}

	for(std::vector< std::basic_string<TCHAR> >::const_iterator pExcludedHeader = m_vecExcludedHeaders.begin(); pExcludedHeader != m_vecExcludedHeaders.end(); ++pExcludedHeader)
	{
		HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, TTC_NODE_EXCLUDEDHEADER);
		if (NULL == hNode
		||	!m_pDOpusPluginHelper->XMLSetNodeValue(hNode, pExcludedHeader->c_str()))
		{
			return false;
		}
	}

	for(std::map< std::basic_string<TCHAR>, CTypeConfig >::const_iterator miter = m_mapExtTypeConfigs.begin(); miter != m_mapExtTypeConfigs.end(); ++miter)
	{
		const CTypeConfig & typeConfig = miter->second;

		bool bWrapLines                 = (typeConfig.dwFlags & TTF_WRAP_ON)               ? true  : false;
		bool bRemoveBlankLines          = (typeConfig.dwFlags & TTF_REMOVE_BLANK_LINES_ON) ? true  : false;
		bool bForceIconOn               = (typeConfig.dwFlags & TTF_ICON_ON)               ? true  : false;
		bool bForceIconOff              = (typeConfig.dwFlags & TTF_ICON_OFF)              ? true  : false;
		bool bContentsInDescription     = (typeConfig.dwFlags & TTF_DESCRIPTION_ON)        ? true  : false;
		bool bIncludeInFolderThumbnails = (typeConfig.dwFlags & TTF_FOLDERTHUMBNAILS_OFF)  ? false : true;

		HANDLE hNode;
		HANDLE hNodeBackgroundColor;
		HANDLE hNodeTextColor;
		HANDLE hNodeCodePage;
		HANDLE hNodeFont;
		HANDLE hNodeFlags;
		if (NULL == (hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, TCC_NODE_TYPECONFIG))
		||	!m_pDOpusPluginHelper->XMLSetNodeAttribute(hNode, TCC_ATTRIBUTE_NAME, typeConfig.strExt.c_str())
		||	NULL == (hNodeBackgroundColor = m_pDOpusPluginHelper->XMLAddChildNode(hNode, TCC_NODE_BACKGROUNDCOLOR))
		||	NULL == (hNodeTextColor       = m_pDOpusPluginHelper->XMLAddChildNode(hNode, TCC_NODE_TEXTCOLOR))
		||	NULL == (hNodeCodePage        = m_pDOpusPluginHelper->XMLAddChildNode(hNode, TCC_NODE_CODEPAGE))
		||	NULL == (hNodeFont            = m_pDOpusPluginHelper->XMLAddChildNode(hNode, TCC_NODE_FONT))
		||	NULL == (hNodeFlags           = m_pDOpusPluginHelper->XMLAddChildNode(hNode, TCC_NODE_FLAGS))
		||	!m_pDOpusPluginHelper->XMLSetNodeDWORDValue(hNodeBackgroundColor, typeConfig.crBackground)
		||	!m_pDOpusPluginHelper->XMLSetNodeDWORDValue(hNodeTextColor,       typeConfig.crText)
		||	!m_pDOpusPluginHelper->XMLSetNodeDWORDValue(hNodeCodePage,        typeConfig.dwCodePage)
		||	!m_pDOpusPluginHelper->XMLSetNodeLOGFONTValue(hNodeFont,          &typeConfig.logFont)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_WRAPLINES,                 bWrapLines)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_REMOVEBLANKLINES,          bRemoveBlankLines)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_FORCEICONON,               bForceIconOn)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_FORCEICONOFF,              bForceIconOff)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_CONTENTSINDESCRIPTION,     bContentsInDescription)
		||	!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hNodeFlags, TCC_ATTRIBUTE_INCLUDEINFOLDERTHUMBNAILS, bIncludeInFolderThumbnails))
		{
			return false;
		}
	}

	return true;
}

bool CTextThumbConfig::loadRegistry()
{
	// Don't read and especially don't write/delete registry settings when in USB mode.
	// We may be loaded from Opus 9 USB Mode on a machine with Opus 8 installed.
	if (m_pDOpusPluginHelper->IsUSBInstall())
	{
		return false;
	}

	assert(sizeof(DWORD) == sizeof(COLORREF));

	if (sizeof(DWORD) != sizeof(COLORREF))
	{
		return false;
	}

	clear();

	std::vector< std::basic_string<TCHAR> > vecExcludedExtensions;
	std::vector< std::basic_string<TCHAR> > vecExcludedHeaders;
	std::vector< std::basic_string<TCHAR> > vecExtensions;

	LeoHelpers::LeetRegQueryMultiStringValue(HKEY_CURRENT_USER, TEXTTHUMB_REG_PATH, _T("ExcludedExtensions"), 0, &vecExcludedExtensions);
	LeoHelpers::LeetRegQueryMultiStringValue(HKEY_CURRENT_USER, TEXTTHUMB_REG_PATH, _T("ExcludedHeaders"),    0, &vecExcludedHeaders);
	LeoHelpers::LeetRegEnumKey(HKEY_CURRENT_USER, TEXTTHUMB_REG_PATH, _T("Cfg"), 0, &vecExtensions);

	if (vecExcludedExtensions.empty()
	&&	vecExcludedHeaders.empty()
	&&	vecExtensions.empty())
	{
		return false;
	}

	m_vecExcludedExtensions = vecExcludedExtensions;

	m_vecExcludedHeaders = vecExcludedHeaders;

	// Create a default config to fill in any blanks in the registry data.
	CTextThumbConfig defaultConfig(m_hModuleDll, m_dw64OpusVersion, m_pDOpusPluginHelper);
	defaultConfig.LoadDefaults();

	std::basic_string< TCHAR > extLower;
	std::basic_string< TCHAR > sDefTypePath = TEXTTHUMB_REG_PATH;
	std::basic_string< TCHAR > sTypePath;
	sDefTypePath += _T("\\Cfg");

	for(std::vector< std::basic_string<TCHAR> >::const_iterator pExtMixed = vecExtensions.begin(); pExtMixed != vecExtensions.end(); ++pExtMixed)
	{
		sTypePath = sDefTypePath + *pExtMixed;

		LeoHelpers::ToLower(&extLower, *pExtMixed);

		DWORD dwFlags;

		if (!LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, sTypePath.c_str(), _T("Flags"), 0, &dwFlags))
		{
			dwFlags = defaultConfig.GetTypeConfig(extLower).dwFlags;
		}

		DWORD dwCodePage;

		if (!LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, sTypePath.c_str(), _T("CodePage"), 0, &dwCodePage))
		{
			dwCodePage = defaultConfig.GetTypeConfig(extLower).dwCodePage;
		}

		COLORREF crText;

		if (!LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, sTypePath.c_str(), _T("TextColor"), 0, &crText))
		{
			crText = defaultConfig.GetTypeConfig(extLower).crText;
		}

		COLORREF crBackground;

		if (!LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, sTypePath.c_str(), _T("BackgroundColor"), 0, &crBackground))
		{
			crBackground = defaultConfig.GetTypeConfig(extLower).crBackground;
		}

		LOGFONT logFont;
		ZeroMemory(&logFont, sizeof(logFont));

		void *pVoid = NULL;
		DWORD dwType;
		DWORD dwSize;

		bool bGotFont = false;

		if (LeoHelpers::LeetRegQueryValue(HKEY_CURRENT_USER, sTypePath.c_str(),_T("Font"), 0, &dwType, &pVoid, &dwSize))
		{
			if (REG_BINARY == dwType)
			{
				if (dwSize == sizeof(LOGFONT))
				{
					LOGFONT *pRegLogFont = reinterpret_cast<LOGFONT*>(pVoid);
					pRegLogFont->lfFaceName[LF_FACESIZE-1] = _T('\0'); // Ensure face name is null terminated.
					logFont = *pRegLogFont; // Copy the structure.
					bGotFont = true;
				}
#ifdef UNICODE
				else if (dwSize == sizeof(LOGFONTA))
				{
					LOGFONTA *pRegLogFont = reinterpret_cast<LOGFONTA*>(pVoid);
					pRegLogFont->lfFaceName[LF_FACESIZE-1] = '\0'; // Ensure face name is null terminated.
					logFont.lfHeight			= pRegLogFont->lfHeight;
					logFont.lfWidth				= pRegLogFont->lfWidth;
					logFont.lfEscapement		= pRegLogFont->lfEscapement;
					logFont.lfOrientation		= pRegLogFont->lfOrientation;
					logFont.lfWeight			= pRegLogFont->lfWeight;
					logFont.lfItalic			= pRegLogFont->lfItalic;
					logFont.lfUnderline			= pRegLogFont->lfUnderline;
					logFont.lfStrikeOut			= pRegLogFont->lfStrikeOut;
					logFont.lfCharSet			= pRegLogFont->lfCharSet;
					logFont.lfOutPrecision		= pRegLogFont->lfOutPrecision;
					logFont.lfClipPrecision		= pRegLogFont->lfClipPrecision;
					logFont.lfQuality			= pRegLogFont->lfQuality;
					logFont.lfPitchAndFamily	= pRegLogFont->lfPitchAndFamily;
					WCHAR *wszFontName = LeoHelpers::LeetMBtoWC(pRegLogFont->lfFaceName);
					if (NULL != wszFontName)
					{
						LeoHelpers::StringCopy(logFont.lfFaceName, wszFontName, sizeof(logFont.lfFaceName)/sizeof(logFont.lfFaceName[0]));
						delete [] wszFontName;
					}
					else
					{
						logFont.lfFaceName[0] = L'\0';
					}
					bGotFont = true;
				}
#else
				else if (dwSize == sizeof(LOGFONTW))
				{
					LOGFONTW *pRegLogFont = reinterpret_cast<LOGFONTW*>(pVoid);
					pRegLogFont->lfFaceName[LF_FACESIZE-1] = L'\0'; // Ensure face name is null terminated.
					logFont.lfHeight			= pRegLogFont->lfHeight;
					logFont.lfWidth				= pRegLogFont->lfWidth;
					logFont.lfEscapement		= pRegLogFont->lfEscapement;
					logFont.lfOrientation		= pRegLogFont->lfOrientation;
					logFont.lfWeight			= pRegLogFont->lfWeight;
					logFont.lfItalic			= pRegLogFont->lfItalic;
					logFont.lfUnderline			= pRegLogFont->lfUnderline;
					logFont.lfStrikeOut			= pRegLogFont->lfStrikeOut;
					logFont.lfCharSet			= pRegLogFont->lfCharSet;
					logFont.lfOutPrecision		= pRegLogFont->lfOutPrecision;
					logFont.lfClipPrecision		= pRegLogFont->lfClipPrecision;
					logFont.lfQuality			= pRegLogFont->lfQuality;
					logFont.lfPitchAndFamily	= pRegLogFont->lfPitchAndFamily;
					char *szFontName = LeoHelpers::LeetWCtoMB(pRegLogFont->lfFaceName);
					if (NULL != szFontName)
					{
						LeoHelpers::StringCopy(logFont.lfFaceName, szFontName, sizeof(logFont.lfFaceName)/sizeof(logFont.lfFaceName[0]));
						delete [] szFontName;
					}
					else
					{
						logFont.lfFaceName[0] = '\0';
					}
					bGotFont = false;
				}
#endif
			}
			delete [] pVoid; // Delete the data loaded from the regsitry.
		}

		if (!bGotFont)
		{
			if (!GetDefaultFont(dwCodePage, &logFont))
			{
				CTypeConfig def = defaultConfig.GetTypeConfig(extLower);
				memcpy(&logFont, &def.logFont, sizeof(LOGFONT));
			}
		}

		m_mapExtTypeConfigs[ extLower ] = CTypeConfig(*pExtMixed, dwFlags, crText, crBackground, dwCodePage, logFont);
	}

	if (Save())
	{
		SHDeleteKey(HKEY_CURRENT_USER, TEXTTHUMB_REG_PATH);
	}

	return true;
}

bool CTextThumbConfig::ShouldDrawThumbnailIcon(DWORD dwFlags) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs); // Not needed now but might be in the future. Harmless, either way.

	if (dwFlags&TTF_ICON_ON)
	{
		return(true);
	}

	if (dwFlags&TTF_ICON_OFF)
	{
		return(false);
	}

	DOPUSTHUMBNAILPREFSDATA thumbPrefs;
	ZeroMemory(&thumbPrefs, sizeof(thumbPrefs));
	thumbPrefs.cbSize = sizeof(thumbPrefs);

	if (m_pDOpusPluginHelper->GetThumbnailPrefs(&thumbPrefs)
	&&	0 != (thumbPrefs.dwFlags&DTHUMBF_SHOWICON))
	{
		return(true);
	}

	return(false);
}

void CTextThumbConfig::GetExcludedExtensions(std::vector< std::basic_string<TCHAR> > *pvecResult) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	*pvecResult = m_vecExcludedExtensions;
}

void CTextThumbConfig::SetExcludedExtensions(const std::vector< std::basic_string<TCHAR> > &vecExcludedExtensions)
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_vecExcludedExtensions = vecExcludedExtensions;
}

void CTextThumbConfig::GetExcludedHeaders(std::vector< std::basic_string<TCHAR> > *pvecResult) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	*pvecResult = m_vecExcludedHeaders;
}

void CTextThumbConfig::SetExcludedHeaders(const std::vector< std::basic_string<TCHAR> > &vecExcludedHeaders)
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_vecExcludedHeaders = vecExcludedHeaders;
}

void CTextThumbConfig::GetConfiguredExtensions(std::vector< std::basic_string<TCHAR> > *pvecResult) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	pvecResult->clear();
	pvecResult->reserve(m_mapExtTypeConfigs.size());

	for(std::map< std::basic_string<TCHAR>, CTypeConfig >::const_iterator miter = m_mapExtTypeConfigs.begin(); miter != m_mapExtTypeConfigs.end(); ++miter)
	{
		pvecResult->push_back(miter->second.strExt);
	}
}

CTextThumbConfig::CTypeConfig CTextThumbConfig::GetTypeConfig(const std::basic_string<TCHAR> &strExt) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	std::basic_string<TCHAR> strExtLower;
	LeoHelpers::ToLower(&strExtLower, strExt);

	std::map< std::basic_string<TCHAR>, CTypeConfig >::const_iterator miter = m_mapExtTypeConfigs.find(strExtLower);

	if (miter != m_mapExtTypeConfigs.end())
	{
		return miter->second;
	}
	else
	{
		miter = m_mapExtTypeConfigs.find(_T(""));

		if (miter != m_mapExtTypeConfigs.end())
		{
			return miter->second;
		}
		else
		{
			// This should never happen. It means Load failed to create a default.
			std::map< std::basic_string<TCHAR>, CTypeConfig > mapExtTypeConfigDefaults;
			loadDefaultExtTypeConfigs(&mapExtTypeConfigDefaults);

			return mapExtTypeConfigDefaults[ _T("" ) ];
		}
	}
}

void CTextThumbConfig::SetTypeConfig(const CTypeConfig &typeConfig)
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	std::basic_string<TCHAR> strExtLower;
	LeoHelpers::ToLower(&strExtLower, typeConfig.strExt);

	m_mapExtTypeConfigs[strExtLower] = typeConfig;
}

void CTextThumbConfig::DeleteTypeConfig(const std::basic_string<TCHAR> &strExt)
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	std::basic_string<TCHAR> strExtLower;
	LeoHelpers::ToLower(&strExtLower, strExt);

	std::map< std::basic_string<TCHAR>, CTypeConfig >::iterator miter = m_mapExtTypeConfigs.find(strExtLower);

	if (miter != m_mapExtTypeConfigs.end())
	{
		m_mapExtTypeConfigs.erase(miter);
	}
}

// static
bool CTextThumbConfig::GetDefaultFont(DWORD dwCodePage, LOGFONT *pLogFont)
{
//	LeoHelpers::CriticalSectionScoper css(&m_cs);  <-- Not needed in static.

	bool bResult = false;

	ZeroMemory(pLogFont, sizeof(LOGFONT));

	if (GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), pLogFont))
	{
		HDC hDC = GetDC(NULL);
		if (NULL != hDC)
		{
			pLogFont->lfHeight = -MulDiv(7, GetDeviceCaps(hDC, LOGPIXELSY), 72); // Default to 7-point font.
			ReleaseDC(NULL, hDC);
		}

		//if (pLogFont->lfFaceName[0] == _T('\0'))
		if (dwCodePage == 437) // If DOS code page use monospace font with the DOS symbols in it.
		{
#ifdef UNICODE
			LeoHelpers::StringCopy(pLogFont->lfFaceName, _T("Lucida Console"), LF_FACESIZE);
#else
#pragma error("Saving these defaults from an ANSI build would mean things don't work when people upgrade to Unicode. Luckily we don't support ANSI anymore.")
			LeoHelpers::StringCopy(pLogFont->lfFaceName, _T("IBMPC"), LF_FACESIZE);
#endif
		}
		else
		{
			LeoHelpers::StringCopy(pLogFont->lfFaceName, _T("Arial"), LF_FACESIZE);
		}

		bResult = true;
	}

	return(bResult);
}



CTextThumbConfig::CTypeConfig::CTypeConfig()
	: dwFlags(0)
	, crText(RGB(0,0,0))
	, crBackground(RGB(164,164,164))
	, dwCodePage(CP_ACP)
{
	ZeroMemory(&logFont, sizeof(LOGFONT));
}

CTextThumbConfig::CTypeConfig::~CTypeConfig()
{
}

CTextThumbConfig::CTypeConfig::CTypeConfig(const CTypeConfig &rhs)
{
	strExt       = rhs.strExt;
	dwFlags      = rhs.dwFlags;
	crText       = rhs.crText;
	crBackground = rhs.crBackground;
	dwCodePage   = rhs.dwCodePage;
	memcpy(&logFont, &rhs.logFont, sizeof(LOGFONT));
}

CTextThumbConfig::CTypeConfig &CTextThumbConfig::CTypeConfig::operator=(const CTypeConfig &rhs)
{
	if (this != &rhs)
	{
		strExt       = rhs.strExt;
		dwFlags      = rhs.dwFlags;
		crText       = rhs.crText;
		crBackground = rhs.crBackground;
		dwCodePage   = rhs.dwCodePage;
		memcpy(&logFont, &rhs.logFont, sizeof(LOGFONT));
	}
	return *this;
}

CTextThumbConfig::CTypeConfig::CTypeConfig(std::basic_string<TCHAR> strExtIn, DWORD dwFlagsIn, COLORREF crTextIn, COLORREF crBackgroundIn, DWORD dwCodePageIn, const LOGFONT &logFontIn)
	: strExt(strExtIn)
	, dwFlags(dwFlagsIn)
	, crText(crTextIn)
	, crBackground(crBackgroundIn)
	, dwCodePage(dwCodePageIn)
{
	memcpy(&logFont, &logFontIn, sizeof(LOGFONT));
}
