#include "stdafx.h"
#include "LeoHelpers.h"
#include "dcraw_settings.h"
#include "dcrawrap_config.h"

#define JP2RAW_REG_PATH L"Software\\GPSoftware\\Directory Opus\\Config\\User\\Viewers\\JP2Raw"

#define DCRAW_DCRAWCLIP_NAME           L"DCRawClip"

#define DCRC_ATTRIBUTE_RAW             L"Raw"
#define DCRC_ATTRIBUTE_PNM             L"PNM"
#define DCRC_NODE_RAWEXTENSION         L"RawExt"
#define DCRC_NODE_RAWEXTENSION_REMOVE  L"RawExtRemove"
#define DCRC_NODE_PROFILE              L"CamProfile"
#define DCRC_ATTRIBUTE_SAFENAME        L"SafeName"
#define DCRC_ATTRIBUTE_DISPLAYNAME     L"DisplayName"
#define DCRC_NODE_THUMBS               L"Thumbs"
#define DCRC_NODE_VIEWERS              L"Viewers"
#define DCRC_NODE_CONVERTER            L"Converter"
#define DCRC_NODE_GENERICSETTINGS      L"Generic"
#define DCRC_ATTRIBUTE_TRYPREVIEW      L"TryPreview"
#define DCRC_ATTRIBUTE_TRYFULL         L"TryFull"
#define DCRC_ATTRIBUTE_ROTATION        L"Rotation"
#define DCRC_ATTRIBUTE_CORRECTGEOMETRY L"CorrectGeometry"
#define DCRC_ATTRIBUTE_INTERPQUALITY   L"InterpQuality"
#define DCRC_ATTRIBUTE_RGGB            L"RGGB"
#define DCRC_ATTRIBUTE_DOCMODENOCOL    L"DocModeNoCol"
#define DCRC_ATTRIBUTE_DOCMODERAW      L"DocModeRaw"
#define DCRC_ATTRIBUTE_HALFSIZECOLOR   L"HalfSizeColor"
#define DCRC_ATTRIBUTE_CAMWHITE        L"CamWhite"
#define DCRC_ATTRIBUTE_SETWHITE        L"SetWhite"
#define DCRC_ATTRIBUTE_USERMUL1        L"UserMul1"
#define DCRC_ATTRIBUTE_USERMUL2        L"UserMul2"
#define DCRC_ATTRIBUTE_USERMUL3        L"UserMul3"
#define DCRC_ATTRIBUTE_USERMUL4        L"UserMul4"
#define DCRC_ATTRIBUTE_BRIGHTNESS      L"Brightness"
#define DCRC_ATTRIBUTE_FIXEDWHITE      L"FixedWhite"
#define DCRC_ATTRIBUTE_HIGHLIGHTMODE   L"HighlightMode"
#define DCRC_ATTRIBUTE_GAMMAPOWER      L"GammaPower"
#define DCRC_ATTRIBUTE_GAMMATOESLOPE   L"GammaToeSlope"
#define DCRC_ATTRIBUTE_CHROMARED       L"ChromaRed"
#define DCRC_ATTRIBUTE_CHROMABLUE      L"ChromaBlue"
#define DCRC_ATTRIBUTE_OUTICCTYPE      L"OutIccType"
#define DCRC_ATTRIBUTE_CAMICCTYPE      L"CamIccType"
#define DCRC_ATTRIBUTE_OUTICC          L"OutIcc"
#define DCRC_ATTRIBUTE_CAMICC          L"CamIcc"
#define DCRC_ATTRIBUTE_BADPIXELS       L"BadPixels"
#define DCRC_ATTRIBUTE_BADPIXELSPATH   L"BadPixelsPath"
#define DCRC_ATTRIBUTE_NOISEFILTER     L"NoiseFilter"
#define DCRC_ATTRIBUTE_NOISETHRESHOLD  L"NoiseThreshold"
#define DCRC_ATTRIBUTE_MEDIANFILTER    L"MedianFilter"
#define DCRC_ATTRIBUTE_MEDIANPASSES    L"MedianPasses"

static const wchar_t * const arraySzPnmExtensionsSortedLower[] =
{
//	L"pam",
//	L"pbm",
	L"pgm",
	L"pnm",
	L"ppm"
};

static const wchar_t * const arraySzRawExtensionsSortedLower[] =
{
	L"3fr",
	L"arw",
	L"bay", // untested
	L"cap", // untested
	L"cr2",
	L"crw",
	L"dc2",
	L"dcr",
	L"dcs",
	L"dng",
	L"eip", // untested
	L"erf",
	L"fff", // untested
	L"iiq", // untested
	L"kdc",
	L"mdc",
	L"mef",
	L"mos", // untested
	L"mrw",
	L"nef",
	L"nrw",
	L"orf",
	L"pef",
	L"ptx",
	L"pxn", // untested
	L"raf",
	L"raw",
	L"rw2",
	L"rwz", // untested
	L"sr2",
	L"srf",
//	L"tif", <-- NO! Will wrongly identify TIFF images that happen to be a certain size as raw images, whether or not they are.
	L"x3f"
};

DCRawConfig::DCRawConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperDCRawPlugin *pDOpusPluginHelper)
: m_hModuleDll(hModuleDll)
, m_dw64OpusVersion(dw64OpusVersion)
, m_pDOpusPluginHelper(pDOpusPluginHelper)
, m_sl(pDOpusPluginHelper)
, m_bWindowsXPOrAbove(LeoHelpers::IsWindowsXPOrAbove())
, m_bRawEnabled(true)
, m_bPnmEnabled(true)
{
	InitializeCriticalSection(&m_cs);

#ifdef _DEBUG
	for (size_t i = 0; i < _countof(arraySzPnmExtensionsSortedLower); ++i)
	{
		assert(i == 0 || 0 > wcscmp(arraySzPnmExtensionsSortedLower[i - 1], arraySzPnmExtensionsSortedLower[i]));
		for (const wchar_t *pc = arraySzPnmExtensionsSortedLower[i]; *pc; ++pc) { assert(!iswupper(*pc)); }
	}

	for (size_t i = 0; i < _countof(arraySzRawExtensionsSortedLower); ++i)
	{
		assert(i == 0 || 0 > wcscmp(arraySzRawExtensionsSortedLower[i - 1], arraySzRawExtensionsSortedLower[i]));
		for (const wchar_t *pc = arraySzRawExtensionsSortedLower[i]; *pc; ++pc) { assert(!iswupper(*pc)); }
	}
#endif
}

DCRawConfig::~DCRawConfig()
{
	DeleteCriticalSection(&m_cs);
}

DCRawConfig::DCRawConfig(const DCRawConfig &rhs)
: m_sl(rhs.m_pDOpusPluginHelper) // This is safe to do outside of the critical section.
{
	InitializeCriticalSection(&m_cs);

	LeoHelpers::CriticalSectionScoper css(&rhs.m_cs);

	m_hModuleDll            = rhs.m_hModuleDll;
	m_dw64OpusVersion       = rhs.m_dw64OpusVersion;
	m_pDOpusPluginHelper    = rhs.m_pDOpusPluginHelper;
	m_bWindowsXPOrAbove     = rhs.m_bWindowsXPOrAbove;

	m_bRawEnabled           = rhs.m_bRawEnabled;
	m_bPnmEnabled           = rhs.m_bPnmEnabled;
	m_setRawExtensionsLower = rhs.m_setRawExtensionsLower;
	m_mapProfiles           = rhs.m_mapProfiles;
}

DCRawConfig &DCRawConfig::operator=(const DCRawConfig &rhs)
{
	if (this != &rhs)
	{
		LeoHelpers::CriticalSectionScoper css1(&m_cs);
		LeoHelpers::CriticalSectionScoper css2(&rhs.m_cs);

		assert(m_hModuleDll         == rhs.m_hModuleDll);
		assert(m_dw64OpusVersion    == rhs.m_dw64OpusVersion);
		assert(m_pDOpusPluginHelper == rhs.m_pDOpusPluginHelper); // If this isn't true then we have a problem with m_sl as well.
		assert(m_bWindowsXPOrAbove  == rhs.m_bWindowsXPOrAbove);

		m_bRawEnabled			= rhs.m_bRawEnabled;
		m_bPnmEnabled           = rhs.m_bPnmEnabled;
		m_setRawExtensionsLower = rhs.m_setRawExtensionsLower;
		m_mapProfiles           = rhs.m_mapProfiles;
	}

	return *this;
}

void DCRawConfig::SetRawExtensions(const std::vector< std::wstring > &vecExtensions)
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_setRawExtensionsLower.clear();

	std::wstring strExtLower;

	for (std::vector< std::wstring >::const_iterator viter = vecExtensions.begin(); viter != vecExtensions.end(); ++viter)
	{
		strExtLower = *viter;
		ProcessExtension(&strExtLower, false); // Should be redundant but it doesn't hurt and this isn't a critical path for speed.

		m_setRawExtensionsLower.insert(strExtLower);
	}
}

void DCRawConfig::GetExtensions(std::vector< std::wstring > *pvecExtensions, bool bRawIfEnabled, bool bRawAlways, bool bPnmIfEnabled, bool bPnmAlways) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	pvecExtensions->clear();

	if (bRawAlways || (bRawIfEnabled && IsRawEnabled()))
	{
		for (std::set< std::wstring >::const_iterator siter = m_setRawExtensionsLower.begin(); siter != m_setRawExtensionsLower.end(); ++siter)
		{
			pvecExtensions->push_back(*siter);
		}
	}

	if (bPnmAlways || (bPnmIfEnabled && IsPnmEnabled()))
	{
		for (size_t i = 0; i < _countof(arraySzPnmExtensionsSortedLower); ++i)
		{
			pvecExtensions->push_back(arraySzPnmExtensionsSortedLower[i]);
		}
	}
}

DCRawConfig::ExtensionType DCRawConfig::GetExtensionType(const std::wstring &strExtNoDotLower, bool bRawIfEnabled, bool bRawAlways, bool bPnmIfEnabled, bool bPnmAlways) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	if (bRawAlways || (bRawIfEnabled && IsRawEnabled()))
	{
		if (m_setRawExtensionsLower.find(strExtNoDotLower) != m_setRawExtensionsLower.end())
		{
			return DCRET_RAW;
		}
	}

	if (bPnmAlways || (bPnmIfEnabled && IsPnmEnabled()))
	{
		const wchar_t *szKey = strExtNoDotLower.c_str();

		if (NULL != bsearch_s(&szKey, arraySzPnmExtensionsSortedLower, _countof(arraySzPnmExtensionsSortedLower), sizeof(const wchar_t *), LeoHelpers::WideStrBSearchCmp, NULL))
		{
			return DCRET_PNM;
		}
	}

	return DCRET_UNHANDLED;
}

DCR_RawSettings DCRawConfig::GetRawSettings(const std::wstring &strSafeName, DCR_RawSettings::Purpose purp) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	const DCR_RawProfile *pProfile = m_mapProfiles.GetCameraProfileFromSafeName(strSafeName);

	if (pProfile == NULL)
	{
		assert(false);
		return DCR_RawSettings(purp);
	}

	switch(purp)
	{
	case DCR_RawSettings::DP_VIEWERS:
		return pProfile->rsViewers;

	case DCR_RawSettings::DP_THUMBS:
		return pProfile->rsThumbs;

	case DCR_RawSettings::DP_CONVERTER:
		return pProfile->rsConverter;

	default:
		assert(false);
		return pProfile->rsViewers;
	}
}

inline bool DCRawConfig::growCharBuffer(wchar_t **pCharBuffer, INT *pCharBufferSize, INT reqSize)
{
	if (reqSize > *pCharBufferSize)
	{
		delete[] *pCharBuffer;
		*pCharBuffer = new(std::nothrow) wchar_t[reqSize];
		*pCharBufferSize = (*pCharBuffer != NULL) ? reqSize : 0;
	}
	return(NULL != *pCharBuffer);
}

bool DCRawConfig::xmlGetNodeName(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeName(hNode, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeName(hNode, *pCharBuffer, &reqSize));
}

bool DCRawConfig::xmlGetNodeValue(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeValue(hNode, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeValue(hNode, *pCharBuffer, &reqSize));
}

bool DCRawConfig::xmlGetNodeAttribute(HANDLE hNode, const wchar_t *szAttributeName, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	INT reqSize = 0;

	return (m_pDOpusPluginHelper->XMLGetNodeAttribute(hNode, szAttributeName, NULL, &reqSize)
	  &&	growCharBuffer(pCharBuffer, pCharBufferSize, reqSize)
	  &&	m_pDOpusPluginHelper->XMLGetNodeAttribute(hNode, szAttributeName, *pCharBuffer, &reqSize));
}

bool DCRawConfig::getConfigPath(std::wstring *pStrPath) const
{
	wchar_t szConfigPath[MAX_PATH];

	if (!m_pDOpusPluginHelper->GetConfigPath(OPUSPATH_CONFIG, szConfigPath, _countof(szConfigPath)))
	{
		pStrPath->clear();
		return false;
	}
	else
	{
		*pStrPath = szConfigPath;
		LeoHelpers::AppendPathString(pStrPath, DCRAW_CONFIG_NAME);
		pStrPath->append(L".oxc");
		return true;
	}
}

void DCRawConfig::LoadDefaults()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_bRawEnabled = true;
	m_bPnmEnabled = true;
	m_setRawExtensionsLower.clear();
	m_mapProfiles.clear();

	for (size_t i = 0; i < _countof(arraySzRawExtensionsSortedLower); ++i)
	{
		m_setRawExtensionsLower.insert(arraySzRawExtensionsSortedLower[i]);
	}

	// Cause the default profile to be inserted if it's missing.
	m_mapProfiles[ L"" ];
}

bool DCRawConfig::Load()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	if (!loadXml() && !loadRegistry())
	{
		LoadDefaults();
	}

	// There must always be a default profile. It should be impossible not to have one.
	assert(m_mapProfiles.find(L"") != m_mapProfiles.end());

	// Cause the default profile to be inserted if it's missing.
	m_mapProfiles[ L"" ];

	return true;
}

bool DCRawConfig::Save() const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	return saveXml();
}

bool DCRawConfig::RawSettingsToClipboardString(std::wstring *pstrOut, const DCR_RawSettings &rs) const
{
	bool bResult = false;

	pstrOut->clear();

	HANDLE hRootNode = m_pDOpusPluginHelper->XMLCreateFile();
	
	if (hRootNode != NULL)
	{
		LPWSTR szXmlCTMA = NULL;

		if (m_pDOpusPluginHelper->XMLSetNodeName(hRootNode, DCRAW_DCRAWCLIP_NAME)
		&&	saveXmlRawSettings(hRootNode, DCRC_NODE_GENERICSETTINGS, rs, DCR_RawSettings::DP_UNKNOWN)
		&&	m_pDOpusPluginHelper->XMLSaveData(hRootNode, &szXmlCTMA)
		&&	szXmlCTMA != NULL)
		{
			*pstrOut = szXmlCTMA;
			bResult = true;

			::CoTaskMemFree(szXmlCTMA);
			szXmlCTMA = NULL;
		}

		m_pDOpusPluginHelper->XMLFreeFile(hRootNode);
	}

	return bResult;
}

bool DCRawConfig::RawSettingsFromClipboardString(DCR_RawSettings *prs, const std::wstring &strIn, DCR_RawSettings::Purpose purp) const
{
	bool bResult = false;

	// We'll load into a temporary object, initialised to the defaults. Only if we fully succeed will we modify *prs.
	DCR_RawSettings newSettings(purp);

	HANDLE hRootNode = m_pDOpusPluginHelper->XMLLoadData(strIn.c_str(), static_cast<DWORD>(strIn.length() * sizeof(wchar_t)));

	if (hRootNode != NULL)
	{
		INT charBufferSize = 1024;
		wchar_t *charBuffer = new wchar_t[charBufferSize];

		if (xmlGetNodeName(hRootNode, &charBuffer, &charBufferSize)
		&&	0 == wcscmp(charBuffer, DCRAW_DCRAWCLIP_NAME))
		{
			for(HANDLE hNode = m_pDOpusPluginHelper->XMLFirstChildNode(hRootNode); hNode != NULL; hNode = m_pDOpusPluginHelper->XMLNextNode(hNode))
			{
				if (xmlGetNodeName(hNode, &charBuffer, &charBufferSize))
				{
					if (0 == wcscmp(charBuffer, DCRC_NODE_GENERICSETTINGS))
					{
						loadXmlRawSettings(hNode, &newSettings, &charBuffer, &charBufferSize);
						*prs = newSettings;
						bResult = true;
						break;
					}
				}
			}
		}

		delete [] charBuffer;

		m_pDOpusPluginHelper->XMLFreeFile(hRootNode);
	}

	return bResult;
}

bool DCRawConfig::loadXml()
{
	bool bResult = false;

	// Load over the defaults.
	// The config file only stores what was changed from the defaults. This lets us push out new, improved defaults
	// which everyone will pick up, without forcing them to wipe out any customisations they have made.
	LoadDefaults();

	std::wstring strConfigPath;
	HANDLE hRootNode;

	if (getConfigPath(&strConfigPath)
	&&	NULL != (hRootNode = m_pDOpusPluginHelper->XMLLoadFile(strConfigPath.c_str())))
	{
		INT charBufferSize = 1024;
		wchar_t *charBuffer = new wchar_t[charBufferSize];

		if (loadXmlContent(hRootNode, &charBuffer, &charBufferSize))
		{
			bResult = true;
		}

		delete [] charBuffer;

		m_pDOpusPluginHelper->XMLFreeFile(hRootNode);
	}

	return bResult;
}

bool DCRawConfig::saveXml() const
{
	bool bResult = false;

	std::wstring strConfigPath;
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

bool DCRawConfig::loadXmlContent(HANDLE hRootNode, wchar_t **pCharBuffer, INT *pCharBufferSize)
{
	if (!xmlGetNodeName(hRootNode, pCharBuffer, pCharBufferSize)
	||	0 != wcscmp(*pCharBuffer, DCRAW_CONFIG_NAME))
	{
		return false;
	}

	BOOL bTemp = FALSE;
	std::wstring strTemp;

	if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hRootNode, DCRC_ATTRIBUTE_RAW, &bTemp))
	{
		m_bRawEnabled = (bTemp ? true : false);
	}

	if (m_pDOpusPluginHelper->XMLGetNodeBoolAttribute(hRootNode, DCRC_ATTRIBUTE_PNM, &bTemp))
	{
		m_bPnmEnabled = (bTemp ? true : false);
	}

	for(HANDLE hNode = m_pDOpusPluginHelper->XMLFirstChildNode(hRootNode); hNode != NULL; hNode = m_pDOpusPluginHelper->XMLNextNode(hNode))
	{
		if (xmlGetNodeName(hNode, pCharBuffer, pCharBufferSize))
		{
			if (0 == wcscmp(*pCharBuffer, DCRC_NODE_RAWEXTENSION))
			{
				if (!xmlGetNodeValue(hNode, pCharBuffer, pCharBufferSize))
				{
					return false;
				}
				else
				{
					strTemp = *pCharBuffer; // In case the config was edited by hand and is now invalid.
					ProcessExtension(&strTemp, false);
					m_setRawExtensionsLower.insert(strTemp);
				}
			}
			else if (0 == wcscmp(*pCharBuffer, DCRC_NODE_RAWEXTENSION_REMOVE))
			{
				if (!xmlGetNodeValue(hNode, pCharBuffer, pCharBufferSize))
				{
					return false;
				}
				else
				{
					strTemp = *pCharBuffer; // In case the config was edited by hand and is now invalid.
					ProcessExtension(&strTemp, false);
					m_setRawExtensionsLower.erase(strTemp);
				}
			}
			else if (0 == wcscmp(*pCharBuffer, DCRC_NODE_PROFILE))
			{
				if (!xmlGetNodeAttribute(hNode, DCRC_ATTRIBUTE_SAFENAME, pCharBuffer, pCharBufferSize))
				{
					return false;
				}

				strTemp = *pCharBuffer;

				// Get or create the profile, then update it with the loaded settings.
				DCR_RawProfile &prof = m_mapProfiles[ strTemp ];

				prof.strSafeName = strTemp;

				if (!xmlGetNodeAttribute(hNode, DCRC_ATTRIBUTE_DISPLAYNAME, pCharBuffer, pCharBufferSize))
				{
					return false;
				}

				prof.strDisplayName = *pCharBuffer;

				for(HANDLE hNodeChild = m_pDOpusPluginHelper->XMLFirstChildNode(hNode); hNodeChild != NULL; hNodeChild = m_pDOpusPluginHelper->XMLNextNode(hNodeChild))
				{
					if (xmlGetNodeName(hNodeChild, pCharBuffer, pCharBufferSize))
					{
						if (0 == wcscmp(*pCharBuffer, DCRC_NODE_THUMBS))
						{
							loadXmlRawSettings(hNodeChild, &prof.rsThumbs, pCharBuffer, pCharBufferSize);
						}
						else if (0 == wcscmp(*pCharBuffer, DCRC_NODE_VIEWERS))
						{
							loadXmlRawSettings(hNodeChild, &prof.rsViewers, pCharBuffer, pCharBufferSize);
						}
						else if (0 == wcscmp(*pCharBuffer, DCRC_NODE_CONVERTER))
						{
							loadXmlRawSettings(hNodeChild, &prof.rsConverter, pCharBuffer, pCharBufferSize);
						}
					}
				}
			}
		}
	}

	return true;
}

bool DCRawConfig::saveXmlContent(HANDLE hRootNode) const
{
	if (!m_pDOpusPluginHelper->XMLSetNodeName(hRootNode, DCRAW_CONFIG_NAME))
	{
		return false;
	}

	// We only save differences from the default, so that we can retroactively change the default if needed.

	DCRawConfig defCfg(m_hModuleDll, m_dw64OpusVersion, m_pDOpusPluginHelper);
	defCfg.LoadDefaults();

	if (m_bRawEnabled != defCfg.m_bRawEnabled)
	{
		if (!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hRootNode, DCRC_ATTRIBUTE_RAW, m_bRawEnabled))
		{
			return false;
		}
	}

	if (m_bPnmEnabled != defCfg.m_bPnmEnabled)
	{
		if (!m_pDOpusPluginHelper->XMLSetNodeBoolAttribute(hRootNode, DCRC_ATTRIBUTE_PNM, m_bPnmEnabled))
		{
			return false;
		}
	}

	for(std::set< std::wstring >::const_iterator rawExtIter = m_setRawExtensionsLower.begin(); rawExtIter != m_setRawExtensionsLower.end(); ++rawExtIter)
	{
		if (defCfg.m_setRawExtensionsLower.find( *rawExtIter ) == defCfg.m_setRawExtensionsLower.end())
		{
			HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, DCRC_NODE_RAWEXTENSION);
			if (hNode == NULL
			||	!m_pDOpusPluginHelper->XMLSetNodeValue(hNode, rawExtIter->c_str()))
			{
				return false;
			}
		}
	}

	for(std::set< std::wstring >::const_iterator rawExtIter = defCfg.m_setRawExtensionsLower.begin(); rawExtIter != defCfg.m_setRawExtensionsLower.end(); ++rawExtIter)
	{
		if (m_setRawExtensionsLower.find( *rawExtIter ) == m_setRawExtensionsLower.end())
		{
			HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, DCRC_NODE_RAWEXTENSION_REMOVE);
			if (hNode == NULL
			||	!m_pDOpusPluginHelper->XMLSetNodeValue(hNode, rawExtIter->c_str()))
			{
				return false;
			}
		}
	}

	// If the default config ever contains profiles other than the default profile then we'll need to
	// modify the loading and saving code so that those profiles can be deleted by the user. The default
	// profile itself cannot be deleted so we don't have to worry about that one.
	assert(defCfg.m_mapProfiles.size() == 1);

	// Save each profile, but only the non-default parts of it. If a profile is all defaults we still save
	// it -- just its name really -- so that it's still there next time the user opens the config dialog.
	// Otherwise if you added a camera profile but saved when it was at defaults, it would vanish next time
	// which would be confusing.
	for(DCR_ProfileMap::const_iterator profIter = m_mapProfiles.begin(); profIter != m_mapProfiles.end(); ++profIter)
	{
		HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hRootNode, DCRC_NODE_PROFILE);
		if (hNode == NULL
		||	!m_pDOpusPluginHelper->XMLSetNodeAttribute(hNode, DCRC_ATTRIBUTE_SAFENAME,    profIter->second.strSafeName.c_str())
		||	!m_pDOpusPluginHelper->XMLSetNodeAttribute(hNode, DCRC_ATTRIBUTE_DISPLAYNAME, profIter->second.strDisplayName.c_str())
		||	!saveXmlRawSettings(hNode, DCRC_NODE_THUMBS,    profIter->second.rsThumbs,    DCR_RawSettings::DP_THUMBS)
		||	!saveXmlRawSettings(hNode, DCRC_NODE_VIEWERS,   profIter->second.rsViewers,   DCR_RawSettings::DP_VIEWERS)
		||	!saveXmlRawSettings(hNode, DCRC_NODE_CONVERTER, profIter->second.rsConverter, DCR_RawSettings::DP_CONVERTER))
		{
			return false;
		}
	}

	return true;
}

void DCRawConfig::loadXmlRawSettings(HANDLE hNodeSettings, DCR_RawSettings *prs, wchar_t **pCharBuffer, INT *pCharBufferSize) const
{
	DOpusPluginHelperDCRawPlugin * const &h = m_pDOpusPluginHelper; // Just to make the lines thinner.

	BOOL bTemp = FALSE;
	DWORD dwTemp = 0;

	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_TRYPREVIEW,      &bTemp  )) { prs->bTryPreview      = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_TRYFULL,         &bTemp  )) { prs->bTryFull         = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_ROTATION,        &dwTemp )) { prs->iRotation        = dwTemp;                 }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_CORRECTGEOMETRY, &bTemp  )) { prs->bCorrectGeometry = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_INTERPQUALITY,   &dwTemp )) { prs->iInterpQuality   = dwTemp;                 }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_DOCMODENOCOL,    &bTemp  )) { prs->bDocModeNoCol    = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_DOCMODERAW,      &bTemp  )) { prs->bDocModeRaw      = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_HALFSIZECOLOR,   &bTemp  )) { prs->bHalfSizeColor   = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_RGGB,            &bTemp  )) { prs->bRGGB            = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_CAMWHITE,        &bTemp  )) { prs->bCamWhite        = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_SETWHITE,        &bTemp  )) { prs->bSetWhite        = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_USERMUL1,        &dwTemp )) { prs->iUserMul1        = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_USERMUL2,        &dwTemp )) { prs->iUserMul2        = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_USERMUL3,        &dwTemp )) { prs->iUserMul3        = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_USERMUL4,        &dwTemp )) { prs->iUserMul4        = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_BRIGHTNESS,      &dwTemp )) { prs->iBrightness      = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_GAMMAPOWER,      &dwTemp )) { prs->iGammaPower      = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_GAMMATOESLOPE,   &dwTemp )) { prs->iGammaToeSlope   = dwTemp;                 }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_FIXEDWHITE,      &bTemp  )) { prs->bFixedWhite      = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_HIGHLIGHTMODE,   &dwTemp )) { prs->iHighlightMode   = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_CHROMARED,       &dwTemp )) { prs->iChromaRed       = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_CHROMABLUE,      &dwTemp )) { prs->iChromaBlue      = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_OUTICCTYPE,      &dwTemp )) { prs->iOutIccType      = dwTemp;                 }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_CAMICCTYPE,      &dwTemp )) { prs->iCamIccType      = dwTemp;                 }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_BADPIXELS,       &bTemp  )) { prs->bBadPixels       = (bTemp ? true : false); }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_NOISEFILTER,     &bTemp  )) { prs->bNoiseFilter     = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_NOISETHRESHOLD,  &dwTemp )) { prs->iNoiseThreshold  = dwTemp;                 }
	if (h->XMLGetNodeBoolAttribute(  hNodeSettings, DCRC_ATTRIBUTE_MEDIANFILTER,    &bTemp  )) { prs->bMedianFilter    = (bTemp ? true : false); }
	if (h->XMLGetNodeDWORDAttribute( hNodeSettings, DCRC_ATTRIBUTE_MEDIANPASSES,    &dwTemp )) { prs->iMedianPasses    = dwTemp;                 }

	if (xmlGetNodeAttribute(hNodeSettings, DCRC_ATTRIBUTE_OUTICC,        pCharBuffer, pCharBufferSize)) { prs->strOutIcc        = *pCharBuffer; }
	if (xmlGetNodeAttribute(hNodeSettings, DCRC_ATTRIBUTE_CAMICC,        pCharBuffer, pCharBufferSize)) { prs->strCamIcc        = *pCharBuffer; }
	if (xmlGetNodeAttribute(hNodeSettings, DCRC_ATTRIBUTE_BADPIXELSPATH, pCharBuffer, pCharBufferSize)) { prs->strBadPixelsPath = *pCharBuffer; }
}

bool DCRawConfig::saveXmlRawSettings(HANDLE hProfileNode, const wchar_t *szSettingsNodeName, const DCR_RawSettings &rs, DCR_RawSettings::Purpose purp) const
{
	bool bResult = true;

	DCR_RawSettings *pd = NULL;
	
	if (purp != DCR_RawSettings::DP_UNKNOWN) // If it's unknown we want to save all settings as it's for the clipboard and can be applied to any purpose.
	{
		pd = new DCR_RawSettings(purp); // We only save what's different from the defaults.
	}

	DOpusPluginHelperDCRawPlugin * const &h = m_pDOpusPluginHelper; // Just to make the lines thinner.

	HANDLE hNode = m_pDOpusPluginHelper->XMLAddChildNode(hProfileNode, szSettingsNodeName);

	if (hNode == NULL
	||	( (pd==NULL || rs.bTryPreview      != pd->bTryPreview                ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_TRYPREVIEW,      rs.bTryPreview              ) )
	||	( (pd==NULL || rs.bTryFull         != pd->bTryFull                   ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_TRYFULL,         rs.bTryFull                 ) )
	||	( (pd==NULL || rs.iRotation        != pd->iRotation                  ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_ROTATION,        rs.iRotation                ) )
	||	( (pd==NULL || rs.bCorrectGeometry != pd->bCorrectGeometry           ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_CORRECTGEOMETRY, rs.bCorrectGeometry         ) )
	||	( (pd==NULL || rs.iInterpQuality   != pd->iInterpQuality             ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_INTERPQUALITY,   rs.iInterpQuality           ) )
	||	( (pd==NULL || rs.bDocModeNoCol    != pd->bDocModeNoCol              ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_DOCMODENOCOL,    rs.bDocModeNoCol            ) )
	||	( (pd==NULL || rs.bDocModeRaw      != pd->bDocModeRaw                ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_DOCMODERAW,      rs.bDocModeRaw              ) )
	||	( (pd==NULL || rs.bHalfSizeColor   != pd->bHalfSizeColor             ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_HALFSIZECOLOR,   rs.bHalfSizeColor           ) )
	||	( (pd==NULL || rs.bRGGB            != pd->bRGGB                      ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_RGGB,            rs.bRGGB                    ) )
	||	( (pd==NULL || rs.bCamWhite        != pd->bCamWhite                  ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_CAMWHITE,        rs.bCamWhite                ) )
	||	( (pd==NULL || rs.bSetWhite        != pd->bSetWhite                  ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_SETWHITE,        rs.bSetWhite                ) )
	||	( (pd==NULL || rs.iUserMul1        != pd->iUserMul1                  ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_USERMUL1,        rs.iUserMul1                ) )
	||	( (pd==NULL || rs.iUserMul2        != pd->iUserMul2                  ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_USERMUL2,        rs.iUserMul2                ) )
	||	( (pd==NULL || rs.iUserMul3        != pd->iUserMul3                  ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_USERMUL3,        rs.iUserMul3                ) )
	||	( (pd==NULL || rs.iUserMul4        != pd->iUserMul4                  ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_USERMUL4,        rs.iUserMul4                ) )
	||	( (pd==NULL || rs.iBrightness      != pd->iBrightness                ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_BRIGHTNESS,      rs.iBrightness              ) )
	||	( (pd==NULL || rs.iGammaPower      != pd->iGammaPower                ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_GAMMAPOWER,      rs.iGammaPower              ) )
	||	( (pd==NULL || rs.iGammaToeSlope   != pd->iGammaToeSlope             ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_GAMMATOESLOPE,   rs.iGammaToeSlope           ) )
	||	( (pd==NULL || rs.bFixedWhite      != pd->bFixedWhite                ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_FIXEDWHITE,      rs.bFixedWhite              ) )
	||	( (pd==NULL || rs.iHighlightMode   != pd->iHighlightMode             ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_HIGHLIGHTMODE,   rs.iHighlightMode           ) )
	||	( (pd==NULL || rs.iChromaRed       != pd->iChromaRed                 ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_CHROMARED,       rs.iChromaRed               ) )
	||	( (pd==NULL || rs.iChromaBlue      != pd->iChromaBlue                ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_CHROMABLUE,      rs.iChromaBlue              ) )
	||	( (pd==NULL || rs.iOutIccType      != pd->iOutIccType                ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_OUTICCTYPE,      rs.iOutIccType              ) )
	||	( (pd==NULL || rs.iCamIccType      != pd->iCamIccType                ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_CAMICCTYPE,      rs.iCamIccType              ) )
	||	( (pd==NULL || rs.bBadPixels       != pd->bBadPixels                 ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_BADPIXELS,       rs.bBadPixels               ) )
	||	( (pd==NULL || rs.bNoiseFilter     != pd->bNoiseFilter               ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_NOISEFILTER,     rs.bNoiseFilter             ) )
	||	( (pd==NULL || rs.iNoiseThreshold  != pd->iNoiseThreshold            ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_NOISETHRESHOLD,  rs.iNoiseThreshold          ) )
	||	( (pd==NULL || rs.bMedianFilter    != pd->bMedianFilter              ) && !h->XMLSetNodeBoolAttribute(  hNode, DCRC_ATTRIBUTE_MEDIANFILTER,    rs.bMedianFilter            ) )
	||	( (pd==NULL || rs.iMedianPasses    != pd->iMedianPasses              ) && !h->XMLSetNodeDWORDAttribute( hNode, DCRC_ATTRIBUTE_MEDIANPASSES,    rs.iMedianPasses            ) )
	||	( (pd==NULL || rs.strOutIcc.compare(       pd->strOutIcc       ) != 0) && !h->XMLSetNodeAttribute(      hNode, DCRC_ATTRIBUTE_OUTICC,          rs.strOutIcc.c_str()        ) )
	||	( (pd==NULL || rs.strCamIcc.compare(       pd->strCamIcc       ) != 0) && !h->XMLSetNodeAttribute(      hNode, DCRC_ATTRIBUTE_CAMICC,          rs.strCamIcc.c_str()        ) )
	||	( (pd==NULL || rs.strBadPixelsPath.compare(pd->strBadPixelsPath) != 0) && !h->XMLSetNodeAttribute(      hNode, DCRC_ATTRIBUTE_BADPIXELSPATH,   rs.strBadPixelsPath.c_str() ) ))
	{
		bResult = false;
	}

	delete pd;

	return bResult;
}

bool DCRawConfig::loadRegistry()
{
	// Don't read and especially don't write/delete registry settings when in USB mode.
	// We may be loaded from Opus 9 USB Mode on a machine with Opus 8 installed.
	if (m_pDOpusPluginHelper->IsUSBInstall())
	{
		return false;
	}

	// Apply the registry settings over the defaults, rather than on their own.
	LoadDefaults();

	DWORD dwFlags;

	if (!LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, JP2RAW_REG_PATH, L"Flags", 0, &dwFlags))
	{
		// If the flags value doesn't exist then assume there's no registry config to convert.
		return false;
	}

	enum DCR_Flags
	{
		// Changing the values/meaning of these flags will break the Registry settings import (but not XML).
		DCRF_DISABLE_DCRAW		= 1<<0,
	//	DCRF_DISABLE_TGA		= 1<<1,
	//	DCRF_DISABLE_JP2		= 1<<2,
		DCRF_DISABLE_PNM		= 1<<3,
	//	DCRF_DISABLE_RAS		= 1<<4,
	//	DCRF_DISABLE_TIF		= 1<<5,
	//	DCRF_DISABLE_TGA_ALPHA	= 1<<6,
	};

	if (dwFlags & DCRF_DISABLE_DCRAW)
	{
		m_bRawEnabled = false;
	}

	if (dwFlags & DCRF_DISABLE_PNM)
	{
		m_bPnmEnabled = false;
	}

	std::vector< std::wstring > vecRawExtensions;

	if (LeoHelpers::LeetRegQueryMultiStringValue(HKEY_CURRENT_USER, JP2RAW_REG_PATH, L"RawExtensions", 0, &vecRawExtensions))
	{
		// Add an extra extensions; don't remove any of our defaults.
		for (std::vector< std::wstring >::iterator rawExtIter = vecRawExtensions.begin(); rawExtIter != vecRawExtensions.end(); ++rawExtIter)
		{
			ProcessExtension(&(*rawExtIter),false);
			m_setRawExtensionsLower.insert(*rawExtIter);
		}
	}

	std::vector< std::wstring > vecProfileNames;
	std::wstring strDisplayName;

	if (LeoHelpers::LeetRegEnumKey(HKEY_CURRENT_USER, JP2RAW_REG_PATH, NULL, 0, &vecProfileNames))
	{
		for (std::vector< std::wstring >::const_iterator profKeyIter = vecProfileNames.begin(); profKeyIter != vecProfileNames.end(); ++profKeyIter)
		{
			const wchar_t *szKeyName = profKeyIter->c_str();
			std::wstring strFullKeyPath = JP2RAW_REG_PATH;
			strFullKeyPath += L"\\";
			strFullKeyPath += szKeyName;

			if (szKeyName[0] == L'C'
			&&	szKeyName[1] == L'f'
			&&	szKeyName[2] == L'g')
			{
				const wchar_t *szSafeName = szKeyName + 3;

				// "Cfg" is the default. Everything else must be "Cfg_safename"
				if (szSafeName[0] == L'_')
				{
					// Skip the _ and ensure there's a safe-name after it.
					if ((++szSafeName)[0] == L'\0')
					{
						continue;
					}
				}
				else if (szSafeName[0] != L'\0')
				{
					continue; // It's "CfgX" where X is not _ or null. Invalid.
				}

				// Don't load the default profile's display name. The config dialog looks it up in the language file.
				strDisplayName.clear();

				if (szSafeName[0] == L'\0'
				||	(LeoHelpers::LeetRegQueryStringValue(HKEY_CURRENT_USER, strFullKeyPath.c_str(), L"DisplayName", 0, &strDisplayName)
				&&	 !strDisplayName.empty()))
				{
					// Get or create the profile, then update it with the loaded settings.
					DCR_RawProfile &prof = m_mapProfiles[ szSafeName ];

					prof.strSafeName = szSafeName;
					prof.strDisplayName = strDisplayName;

					loadRegistryRawSettings(strFullKeyPath + L"\\Thumbs",  &prof.rsThumbs);
					loadRegistryRawSettings(strFullKeyPath + L"\\Viewers", &prof.rsViewers);
					loadRegistryRawSettings(strFullKeyPath + L"\\Viewers", &prof.rsConverter); // Apply old viewer settings to the converter. (Since it's just the ICC profile stuff, this makes sense.)
				}
			}
		}
	}

	// Special case: The old plugin's default config included a Kodak___DCS_Pro_14N profile which
	// existed just to disable thumbnail previews (since they didn't work for that camera at the time).
	// They work now so if we've imported that profile and it's all default values then we can dump it.

	DCR_ProfileMap::iterator kodakIter = m_mapProfiles.find(L"Kodak___DCS_Pro_14N");

	if (kodakIter != m_mapProfiles.end()
	&&	isRedundantRawSettingsFromRegistry(kodakIter->second.rsThumbs)
	&&	isRedundantRawSettingsFromRegistry(kodakIter->second.rsViewers))
	{
		m_mapProfiles.erase(kodakIter);
	}

	if (Save())
	{
		SHDeleteKey(HKEY_CURRENT_USER, JP2RAW_REG_PATH);
	}

	return true;
}

void DCRawConfig::loadRegistryRawSettings(const std::wstring &strKeyPath, DCR_RawSettings *prs)
{
	const wchar_t *p = strKeyPath.c_str();

	// Ignore TryPreview and TryFull settings from the registry.
	// The newer code can get more types of preview images so if they were turned off
	// for something in the past it's probably worth turning them on again now.
	// It's also felt that we should opt for a quick decode by default, not a full decode,
	// even in the viewer.

	// ...Sod it. So many of the old defaults were bad, or no longer make sense, that the only thing
	// we actually import is the camera name and the ICC profiles.

//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"TryPreview",     0, &prs->bTryPreview);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"TryFull",        0, &prs->bTryFull);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"Rotation",       0, &prs->iRotation);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"FujiTurn",       0, &prs->bFujiTurn);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"InterpQuality",  0, &prs->iInterpQuality);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"DocModeNoCol",   0, &prs->bDocModeNoCol);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"DocModeRaw",     0, &prs->bDocModeRaw);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"HalfSizeColor",  0, &prs->bHalfSizeColor);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"RGGB",           0, &prs->bRGGB);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"UseSecondary",   0, &prs->bUseSecondary);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"Bilateral",      0, &prs->bBilateral);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"BilatDomain",    0, &prs->iBilatDomain);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"BilatRange",     0, &prs->iBilatRange);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"AutoWhite",      0, &prs->bAutoWhite);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"CamWhite",       0, &prs->bCamWhite);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"SetWhite",       0, &prs->bSetWhite);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"UserMul1",       0, &prs->iUserMul1);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"UserMul2",       0, &prs->iUserMul2);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"UserMul3",       0, &prs->iUserMul3);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"UserMul4",       0, &prs->iUserMul4);
//	LeoHelpers::LeetRegQueryBoolValue(  HKEY_CURRENT_USER, p, L"SetBlackPoint",  0, &prs->bSetBlackPoint);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"BlackPoint",     0, &prs->iBlackPoint);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"Brightness",     0, &prs->iBrightness);
//	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"HighlightMode",  0, &prs->iHighlightMode);
	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"OutIccType",     0, &prs->iOutIccType);
	LeoHelpers::LeetRegQueryStringValue(HKEY_CURRENT_USER, p, L"OutIcc",         0, &prs->strOutIcc);
	LeoHelpers::LeetRegQueryIntValue(   HKEY_CURRENT_USER, p, L"CamIccType",     0, &prs->iCamIccType);
	LeoHelpers::LeetRegQueryStringValue(HKEY_CURRENT_USER, p, L"CamIcc",         0, &prs->strCamIcc);
}

bool DCRawConfig::isRedundantRawSettingsFromRegistry(const DCR_RawSettings &rs)
{
	// Test against the old JP2Raw.dll defaults, not the current defaults.

	return (
//		rs.bTryPreview
//	&&	rs.bTryFull
//	&&	rs.iRotation      == -1
//	&&	rs.bFujiTurn
//	&&	rs.iInterpQuality == -1
//	&&	!rs.bDocModeNoCol
//	&&	!rs.bDocModeRaw
//	&&	!rs.bHalfSizeColor
//	&&	!rs.bRGGB
//	&&	!rs.bUseSecondary
//	&&	!rs.bBilateral
//	&&	rs.iBilatDomain   == 2000
//	&&	rs.iBilatRange    == 4000
//	&&	!rs.bAutoWhite
//	&&	!rs.bCamWhite
//	&&	!rs.bSetWhite
//	&&	rs.iUserMul1      == 1000
//	&&	rs.iUserMul2      == 1000
//	&&	rs.iUserMul3      == 1000
//	&&	rs.iUserMul4      == 1000
//	&&	!rs.bSetBlackPoint
//	&&	rs.iBlackPoint    == 0
//	&&	rs.iBrightness    == 1000
//	&&	rs.iHighlightMode == 0
		rs.iOutIccType    == 1
	&&	rs.strOutIcc.empty()
	&&	rs.iCamIccType    == 0
	&&	rs.strCamIcc.empty());
}
