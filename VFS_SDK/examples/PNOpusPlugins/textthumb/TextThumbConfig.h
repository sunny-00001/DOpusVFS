#pragma once

class DOpusPluginHelperTextThumbPlugin : public DOpusPluginHelperConfig, public DOpusPluginHelperXML, public DOpusPluginHelperUtil
{
};

class CTextThumbConfig
{
public:

	enum OpusAbility
	{
		TTA_RUN = 0,
	};

	enum TypeConfigFlags
	{
		// Changing the values/meaning of these flags will break the Registry settings import (but not XML).
		TTF_WRAP_ON					= 1<<0,
		TTF_REMOVE_BLANK_LINES_ON	= 1<<1,
		TTF_ICON_ON					= 1<<2,
		TTF_ICON_OFF				= 1<<3,
		TTF_DESCRIPTION_ON			= 1<<4,
		TTF_FOLDERTHUMBNAILS_OFF	= 1<<5,
		// Any flags added after this point were never saved to the registry and so can be changed when needed. Registry imports will default them to off at the moment but that can be changed by masking and combining with default flags, if needed.
	};

	class CTypeConfig
	{
	public:
		// Don't forget to update copy constructor and assignment operator when adding members.
		std::basic_string<TCHAR> strExt;
		DWORD    dwFlags;
		LOGFONT  logFont;
		COLORREF crText;
		COLORREF crBackground;
		DWORD    dwCodePage;
	public:
		CTypeConfig();
		~CTypeConfig();
		CTypeConfig(const CTypeConfig &rhs);
		CTypeConfig &operator=(const CTypeConfig &rhs);
		CTypeConfig(std::basic_string<TCHAR> strExtIn, DWORD dwFlagsIn, COLORREF crTextIn, COLORREF crBackgroundIn, DWORD dwCodePageIn, const LOGFONT &logFontIn);
	};

private:

	// Don't forget to update copy constructor and assignment operator when adding members.

	mutable CRITICAL_SECTION m_cs;
	HMODULE m_hModuleDll;
	DWORD64 m_dw64OpusVersion;
	DOpusPluginHelperTextThumbPlugin *m_pDOpusPluginHelper;
	LeoHelpers::OpusStringLoader m_sl;
	bool m_bWindowsXPOrAbove;

	std::vector< std::basic_string<TCHAR> > m_vecExcludedExtensions;
	std::vector< std::basic_string<TCHAR> > m_vecExcludedHeaders;
	std::map< std::basic_string<TCHAR>, CTypeConfig > m_mapExtTypeConfigs; // keys must be lower-case. use CTypeConfig::strExt for display name.

	static bool growCharBuffer(wchar_t **pCharBuffer, INT *pCharBufferSize, INT reqSize);
	bool xmlGetNodeName(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const;
	bool xmlGetNodeValue(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const;
	bool xmlGetNodeAttribute(HANDLE hNode, const wchar_t *szAttributeName, wchar_t **pCharBuffer, INT *pCharBufferSize) const;

	void clear();
	bool loadXml();
	bool loadXmlContent(HANDLE hRootNode, TCHAR **pCharBuffer, INT *pCharBufferSize);
	bool saveXml();
	bool saveXmlContent(HANDLE hRootNode);
	bool loadRegistry();
	bool getConfigPath(std::basic_string<TCHAR> *pStrPath) const;
	void loadDefaultExtTypeConfigs(std::map< std::basic_string<TCHAR>, CTypeConfig > *pmapExtTypeConfigs) const;
public:

	CTextThumbConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperTextThumbPlugin *pDOpusPluginHelper);
	~CTextThumbConfig();
	CTextThumbConfig(const CTextThumbConfig &rhs);
	CTextThumbConfig &operator=(const CTextThumbConfig &rhs);

	HMODULE GetDllModule() const { return m_hModuleDll; }
	bool CheckOpusAbility(OpusAbility ability) const;
	DWORD64 GetOpusVersion() const { return m_dw64OpusVersion; }
	DOpusPluginHelperTextThumbPlugin *GetOpusPluginHelper() const { return m_pDOpusPluginHelper; }
	const wchar_t *CacheString(UINT uiMsg) { return m_sl.Get(uiMsg); }
	inline bool IsWindowsXPOrAbove() const { return m_bWindowsXPOrAbove; }
	CRITICAL_SECTION *GetCS() const { return &m_cs; }

	void LoadDefaults();
	bool Load();
	bool Save();

	bool ShouldDrawThumbnailIcon(DWORD dwFlags) const;
	static bool GetDefaultFont(DWORD dwCodePage, LOGFONT *pLogFont);

	void GetExcludedExtensions(std::vector< std::basic_string<TCHAR> > *pvecResult) const;
	void GetExcludedHeaders(std::vector< std::basic_string<TCHAR> > *pvecResult) const;
	void GetConfiguredExtensions(std::vector< std::basic_string<TCHAR> > *pvecResult) const;
	CTypeConfig GetTypeConfig(const std::basic_string<TCHAR> &strExt) const;

	void SetExcludedExtensions(const std::vector< std::basic_string<TCHAR> > &vecExcludedExtensions);
	void SetExcludedHeaders(const std::vector< std::basic_string<TCHAR> > &vecExcludedHeaders);
	void SetTypeConfig(const CTypeConfig &typeConfig);
	void DeleteTypeConfig(const std::basic_string<TCHAR> &strExt);
};
