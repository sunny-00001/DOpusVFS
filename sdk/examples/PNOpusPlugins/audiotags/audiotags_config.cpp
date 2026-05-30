#include "stdafx.h"
#include "LeoHelpers.h"
#include "audiotags_config.h"

AudioTagsConfig::AudioTagsConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperAudioTagsPlugin *pDOpusPluginHelper)
//: m_hModuleDll(hModuleDll)
: m_dw64OpusVersion(dw64OpusVersion)
//, m_pDOpusPluginHelper(pDOpusPluginHelper)
, m_sl(pDOpusPluginHelper)
//, m_bWindowsXPOrAbove(LeoHelpers::IsWindowsXPOrAbove())
{
//	InitializeCriticalSection(&m_cs);
}

AudioTagsConfig::~AudioTagsConfig()
{
//	DeleteCriticalSection(&m_cs);
}

AudioTagsConfig::AudioTagsConfig(const AudioTagsConfig &rhs)
: m_sl(rhs.m_sl.GetOpusPluginHelperUtil()) // This is safe to do outside of the critical section.
{
//	InitializeCriticalSection(&m_cs);

//	LeoHelpers::CriticalSectionScoper css(&rhs.m_cs);

//	m_hModuleDll            = rhs.m_hModuleDll;
	m_dw64OpusVersion       = rhs.m_dw64OpusVersion;
//	m_pDOpusPluginHelper    = rhs.m_pDOpusPluginHelper;
//	m_bWindowsXPOrAbove     = rhs.m_bWindowsXPOrAbove;
}

AudioTagsConfig &AudioTagsConfig::operator=(const AudioTagsConfig &rhs)
{
	if (this != &rhs)
	{
//		LeoHelpers::CriticalSectionScoper css1(&m_cs);
//		LeoHelpers::CriticalSectionScoper css2(&rhs.m_cs);

//		assert(m_hModuleDll         == rhs.m_hModuleDll);
		assert(m_dw64OpusVersion    == rhs.m_dw64OpusVersion);
//		assert(m_pDOpusPluginHelper == rhs.m_pDOpusPluginHelper); // If this isn't true then we have a problem with m_sl as well.
//		assert(m_bWindowsXPOrAbove  == rhs.m_bWindowsXPOrAbove);
	}

	return *this;
}
