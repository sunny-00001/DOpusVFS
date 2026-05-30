#pragma once

class DOpusPluginHelperGifPlugin : public DOpusPluginHelperConfig, public DOpusPluginHelperXML, public DOpusPluginHelperUtil
{
};

class CGifConfig
{
protected:

	// Don't forget to update copy constructor and assignment operator when adding members.

	mutable CRITICAL_SECTION m_cs;
	HMODULE m_hModuleDll;
	DWORD64 m_dw64OpusVersion;
	DOpusPluginHelperGifPlugin *m_pDOpusPluginHelper;
	LeoHelpers::OpusStringLoader m_sl;

	BOOL m_bAnimationControlsViewer;
	BOOL m_bAnimationControlsPreview;
	BOOL m_bThumbnailSprockets;
	DWORD m_dwMinimumFrameDelay;
	DWORD m_dwMaximumFrameDelay;
	BOOL m_bOnlyIncreseZeroDelay;

public:

	CGifConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperGifPlugin *pDOpusPluginHelper);
	CGifConfig(const CGifConfig &rhs);
	CGifConfig &operator=(const CGifConfig &rhs);
	virtual ~CGifConfig();

	enum OpusAbility
	{
		GAA_RUN = 0,
	};

	HMODULE GetDllModule() const { return m_hModuleDll; }
	bool CheckOpusAbility(OpusAbility ability) const;
	DWORD64 GetOpusVersion() const { return m_dw64OpusVersion; }
	DOpusPluginHelperGifPlugin *GetOpusPluginHelper() const { return m_pDOpusPluginHelper; }
	const wchar_t *CacheString(UINT uiMsg) { return m_sl.Get(uiMsg); }
	CRITICAL_SECTION *GetCS() const { return &m_cs; }

	void LoadDefaults();
	bool Load();
	bool Save();

	bool DoThumbnailSettingsDiffer(const CGifConfig &rhs) const;

	bool  GetAnimationControlsViewer()  const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_bAnimationControlsViewer  ? true : false; }
	bool  GetAnimationControlsPreview() const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_bAnimationControlsPreview ? true : false; }
	bool  GetThumbnailSprockets()       const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_bThumbnailSprockets       ? true : false; }
	DWORD GetMinimumFrameDelay()        const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_dwMinimumFrameDelay; }
	DWORD GetMaximumFrameDelay()        const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_dwMaximumFrameDelay; }
	bool  GetOnlyIncreaseZeroDelay()    const { LeoHelpers::CriticalSectionScoper css(&m_cs); return m_bOnlyIncreseZeroDelay     ? true : false; }

	void SetAnimationControlsViewer(bool bValue)  { LeoHelpers::CriticalSectionScoper css(&m_cs); m_bAnimationControlsViewer  = bValue ? TRUE : FALSE; }
	void SetAnimationControlsPreview(bool bValue) { LeoHelpers::CriticalSectionScoper css(&m_cs); m_bAnimationControlsPreview = bValue ? TRUE : FALSE; }
	void SetThumbnailSprockets(bool bValue)       { LeoHelpers::CriticalSectionScoper css(&m_cs); m_bThumbnailSprockets       = bValue ? TRUE : FALSE; }
	void SetMinimumFrameDelay(DWORD dwValue)      { LeoHelpers::CriticalSectionScoper css(&m_cs); m_dwMinimumFrameDelay       = dwValue; }
	void SetMaximumFrameDelay(DWORD dwValue)      { LeoHelpers::CriticalSectionScoper css(&m_cs); m_dwMaximumFrameDelay       = dwValue; }
	void SetOnlyIncreaseZeroDelay(bool bValue)    { LeoHelpers::CriticalSectionScoper css(&m_cs); m_bOnlyIncreseZeroDelay     = bValue ? TRUE : FALSE; }

protected:
	void buildOpusConfigStruct(DOPUSPLUGINCONFIGDATA &configData, std::vector<DOPUSPLUGINCONFIGITEM> &vecConfigItems);
};
