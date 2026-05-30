#include "StdAfx.h"
#include "resource.h"
#include "LeoHelpers.h"
#include "GifConfig.h"
#include "GifDecoder.h"
#include "gifanim.h"

#define GIFANIM_REG_PATH _T("Software\\GPSoftware\\Directory Opus\\Config\\User\\Viewers\\GifAnim")
#define GIFANIM_CONFIG_NAME _T("GifAnim");

CGifConfig::CGifConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperGifPlugin *pDOpusPluginHelper)
	: m_hModuleDll(hModuleDll)
	, m_dw64OpusVersion(dw64OpusVersion)
	, m_pDOpusPluginHelper(pDOpusPluginHelper)
	, m_sl(pDOpusPluginHelper)
	, m_bAnimationControlsViewer(TRUE)
	, m_bAnimationControlsPreview(TRUE)
	, m_bThumbnailSprockets(TRUE)
	, m_dwMinimumFrameDelay(75)
	, m_dwMaximumFrameDelay(0)
	, m_bOnlyIncreseZeroDelay(TRUE)
{
	InitializeCriticalSection(&m_cs);
}

CGifConfig::~CGifConfig()
{
	DeleteCriticalSection(&m_cs);
}

CGifConfig::CGifConfig(const CGifConfig &rhs)
: m_sl(rhs.m_pDOpusPluginHelper)
{
	InitializeCriticalSection(&m_cs);

	LeoHelpers::CriticalSectionScoper css(&rhs.m_cs);

	m_hModuleDll         = rhs.m_hModuleDll;
	m_dw64OpusVersion    = rhs.m_dw64OpusVersion;
	m_pDOpusPluginHelper = rhs.m_pDOpusPluginHelper;

	m_bAnimationControlsViewer  = rhs.m_bAnimationControlsViewer;
	m_bAnimationControlsPreview = rhs.m_bAnimationControlsPreview;
	m_bThumbnailSprockets       = rhs.m_bThumbnailSprockets;
	m_dwMinimumFrameDelay       = rhs.m_dwMinimumFrameDelay;
	m_dwMaximumFrameDelay       = rhs.m_dwMaximumFrameDelay;
	m_bOnlyIncreseZeroDelay     = rhs.m_bOnlyIncreseZeroDelay;
}

CGifConfig &CGifConfig::operator=(const CGifConfig &rhs)
{
	if (this != &rhs)
	{
		LeoHelpers::CriticalSectionScoper css1(&m_cs);
		LeoHelpers::CriticalSectionScoper css2(&rhs.m_cs);

		assert(m_pDOpusPluginHelper == rhs.m_pDOpusPluginHelper); // If this fails we have a problem with m_sl.

		m_hModuleDll         = rhs.m_hModuleDll;
		m_dw64OpusVersion    = rhs.m_dw64OpusVersion;
		m_pDOpusPluginHelper = rhs.m_pDOpusPluginHelper;

		m_bAnimationControlsViewer  = rhs.m_bAnimationControlsViewer;
		m_bAnimationControlsPreview = rhs.m_bAnimationControlsPreview;
		m_bThumbnailSprockets       = rhs.m_bThumbnailSprockets;
		m_dwMinimumFrameDelay       = rhs.m_dwMinimumFrameDelay;
		m_dwMaximumFrameDelay       = rhs.m_dwMaximumFrameDelay;
		m_bOnlyIncreseZeroDelay     = rhs.m_bOnlyIncreseZeroDelay;
	}

	return *this;
}

bool CGifConfig::CheckOpusAbility(OpusAbility ability) const
{
	switch(ability)
	{
	case(GAA_RUN):
		return(m_dw64OpusVersion >= MAKE64BITVERSIONNUMBER(9,1,1,0));
	default:
		return(false);
	}
}

void CGifConfig::buildOpusConfigStruct(DOPUSPLUGINCONFIGDATA &configData, std::vector<DOPUSPLUGINCONFIGITEM> &vecConfigItems)
{
	DOPUSPLUGINCONFIGITEM tempItem;

	tempItem.pszName    = _T("AnimationControlsViewer");
	tempItem.iType      = DPCITYPE_Bool;
	tempItem.pData      = &m_bAnimationControlsViewer;
	tempItem.dwDataSize = sizeof(m_bAnimationControlsViewer);
	vecConfigItems.push_back(tempItem);

	tempItem.pszName    = _T("AnimationControlsPreview");
	tempItem.iType      = DPCITYPE_Bool;
	tempItem.pData      = &m_bAnimationControlsPreview;
	tempItem.dwDataSize = sizeof(m_bAnimationControlsPreview);
	vecConfigItems.push_back(tempItem);

	tempItem.pszName    = _T("ThumbnailSprockets");
	tempItem.iType      = DPCITYPE_Bool;
	tempItem.pData      = &m_bThumbnailSprockets;
	tempItem.dwDataSize = sizeof(m_bThumbnailSprockets);
	vecConfigItems.push_back(tempItem);

	tempItem.pszName    = _T("MinimumFrameDelay");
	tempItem.iType      = DPCITYPE_DWORD;
	tempItem.pData      = &m_dwMinimumFrameDelay;
	tempItem.dwDataSize = sizeof(m_dwMinimumFrameDelay);
	vecConfigItems.push_back(tempItem);

	tempItem.pszName    = _T("MaximumFrameDelay");
	tempItem.iType      = DPCITYPE_DWORD;
	tempItem.pData      = &m_dwMaximumFrameDelay;
	tempItem.dwDataSize = sizeof(m_dwMaximumFrameDelay);
	vecConfigItems.push_back(tempItem);

	tempItem.pszName    = _T("OnlyIncreseZeroDelay");
	tempItem.iType      = DPCITYPE_Bool;
	tempItem.pData      = &m_bOnlyIncreseZeroDelay;
	tempItem.dwDataSize = sizeof(m_bOnlyIncreseZeroDelay);
	vecConfigItems.push_back(tempItem);

	configData.cbSize        = sizeof(DOPUSPLUGINCONFIGDATA);
	configData.pszName       = GIFANIM_CONFIG_NAME;
	configData.fStateData    = FALSE;
	configData.iNumCfgItems  = static_cast<int>(vecConfigItems.size());
	configData.pCfgItemArray = &vecConfigItems[0];
}

void CGifConfig::LoadDefaults()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	m_bAnimationControlsViewer  = TRUE;
	m_bAnimationControlsPreview = TRUE;
	m_bThumbnailSprockets       = TRUE;
	m_dwMinimumFrameDelay       = 75;
	m_dwMaximumFrameDelay       = 0;
	m_bOnlyIncreseZeroDelay     = TRUE;
}

bool CGifConfig::Load()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	LoadDefaults();

	DOPUSPLUGINCONFIGDATA configData;
	std::vector<DOPUSPLUGINCONFIGITEM> vecConfigItems;
	buildOpusConfigStruct(configData, vecConfigItems);

	bool bConvert = false;

	if (!m_pDOpusPluginHelper->LoadOrSaveConfig(OPUSCFG_LOAD, &configData))
	{
		LoadDefaults();

		// Don't read and especially don't write/delete registry settings when in USB mode.
		// We may be loaded from Opus 9 USB Mode on a machine with Opus 8 installed.
		if (!m_pDOpusPluginHelper->IsUSBInstall())
		{
			DWORD dwReg;

			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("AnimationControlsViewer"),  0, &dwReg)) { bConvert = true; m_bAnimationControlsViewer  = dwReg ? TRUE : FALSE; }
			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("AnimationControlsPreview"), 0, &dwReg)) { bConvert = true; m_bAnimationControlsPreview = dwReg ? TRUE : FALSE; }
			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("ThumbnailSprockets"),       0, &dwReg)) { bConvert = true; m_bThumbnailSprockets       = dwReg ? TRUE : FALSE; }
			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("MinimumFrameDelay"),        0, &dwReg)) { bConvert = true; m_dwMinimumFrameDelay       = dwReg;                }
			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("MaximumFrameDelay"),        0, &dwReg)) { bConvert = true; m_dwMaximumFrameDelay       = dwReg;                }
			if (LeoHelpers::LeetRegQueryDWORDValue(HKEY_CURRENT_USER, GIFANIM_REG_PATH, _T("OnlyIncreaseZeroDelay"),    0, &dwReg)) { bConvert = true; m_bOnlyIncreseZeroDelay     = dwReg ? TRUE : FALSE; }
		}
	}

	// Perform validation and range checking.

	if (m_dwMinimumFrameDelay > UD_MAXVAL)
	{
		m_dwMinimumFrameDelay = UD_MAXVAL;
	}

	if (m_dwMaximumFrameDelay > UD_MAXVAL)
	{
		m_dwMaximumFrameDelay = UD_MAXVAL;
	}

	if (bConvert && Save())
	{
		SHDeleteKey(HKEY_CURRENT_USER, GIFANIM_REG_PATH);
	}

	return true;
}

bool CGifConfig::Save()
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	DOPUSPLUGINCONFIGDATA configData;
	std::vector<DOPUSPLUGINCONFIGITEM> vecConfigItems;
	buildOpusConfigStruct(configData, vecConfigItems);

	if (!m_pDOpusPluginHelper->LoadOrSaveConfig(OPUSCFG_SAVE, &configData))
	{
		return false;
	}

	return true;
}

bool CGifConfig::DoThumbnailSettingsDiffer(const CGifConfig &rhs) const
{
	LeoHelpers::CriticalSectionScoper css(&m_cs);

	return(m_bThumbnailSprockets != rhs.m_bThumbnailSprockets);
}
