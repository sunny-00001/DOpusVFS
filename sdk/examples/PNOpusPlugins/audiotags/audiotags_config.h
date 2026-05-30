#pragma once

class DOpusPluginHelperAudioTagsPlugin : public DOpusPluginHelperUtil
{
};

// This is just a shell of a config class since the plugin currently has no settings

class AudioTagsConfig
{
private:
	// Don't forget to update copy constructor and assignment operator when adding members.
//	HMODULE                       m_hModuleDll;
//	mutable CRITICAL_SECTION      m_cs;
	DWORD64                       m_dw64OpusVersion;
//	DOpusPluginHelperDCRawPlugin *m_pDOpusPluginHelper;
	LeoHelpers::OpusStringLoader  m_sl;
//	bool                          m_bWindowsXPOrAbove;

public:
	AudioTagsConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperAudioTagsPlugin *pDOpusPluginHelper);
	AudioTagsConfig(const AudioTagsConfig &rhs); // copy constructor can be used.
	AudioTagsConfig &operator=(const AudioTagsConfig &rhs); // assignment operator can be used.
	~AudioTagsConfig(); // Warning: Non-virtual destructor.

//	HMODULE GetInstance() const { return m_hModuleDll; }
//	DOpusPluginHelperAudioTagsPlugin *GetOpusPluginHelper() const { return m_pDOpusPluginHelper; }

	const wchar_t *GetString(UINT id, const wchar_t *szFallback = NULL)
	{
		const wchar_t *szResult = m_sl.Get(id);

		return szResult ? szResult : szFallback;
	}

	enum OpusAbility
	{
		ATA_RUN	= 0,
	};

	bool CheckOpusAbility(OpusAbility ability) const
	{
		switch(ability)
		{
		case(ATA_RUN):
			return (m_dw64OpusVersion >= MAKE64BITVERSIONNUMBER(9,1,3,0));
		default:
			return false;
		}
	}
};
