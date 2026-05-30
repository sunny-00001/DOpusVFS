#pragma once

#define DCRAW_CONFIG_NAME L"DCRaw"

class DOpusPluginHelperDCRawPlugin : public DOpusPluginHelperConfig, public DOpusPluginHelperXML, public DOpusPluginHelperUtil
{
};

class DCRawConfig
{
public:
	struct DCR_RawProfile
	{
		// Default copy-const/assignment-op must work or be overridden.

		DCR_RawProfile()
		: rsThumbs(DCR_RawSettings::DP_THUMBS)
		, rsViewers(DCR_RawSettings::DP_VIEWERS)
		, rsConverter(DCR_RawSettings::DP_CONVERTER)
		{
		}

		std::wstring	strDisplayName;
		std::wstring	strSafeName;
		DCR_RawSettings rsThumbs;
		DCR_RawSettings rsViewers;
		DCR_RawSettings rsConverter;
	};

	// Map key is the "safe" version of the display-name.
	// Converting the display name into a "safe" name was needed when we stored settings in the registry and
	// remains for compatibility with settings that have been converted to XML from the old registry data.
	class DCR_ProfileMap : public std::map< std::wstring, DCR_RawProfile >
	{
	public:
		DCR_RawProfile *GetCameraProfileFromMakeModel(const wchar_t *szMake, const wchar_t *szModel)
		{
			return GetCameraProfileFromDisplayName(GenerateDisplayName(szMake, szModel).c_str());
		}

		DCR_RawProfile *GetCameraProfileFromDisplayName(const wchar_t *szDisplayName)
		{
			return GetCameraProfileFromSafeName(LeoHelpers::GenerateSafeName(szDisplayName));
		}

		DCR_RawProfile *GetCameraProfileFromSafeName(const std::wstring &strSafeName)
		{
			iterator pPair = find(strSafeName);

			if (pPair == end())
			{
				pPair = find(L""); // Find the default profile.
			}

			if (pPair == end())
			{
				assert(false);
				return NULL;
			}

			return &pPair->second;
		}

		const DCR_RawProfile *GetCameraProfileFromSafeName(const std::wstring &strSafeName) const
		{
			const_iterator pPair = find(strSafeName);

			if (pPair == end())
			{
				pPair = find(L""); // Find the default profile.
			}

			if (pPair == end())
			{
				assert(false);
				return NULL;
			}

			return &pPair->second;
		}

		static std::wstring GenerateSafeName(const wchar_t *szMake, const wchar_t *szModel)
		{
			return LeoHelpers::GenerateSafeName(GenerateDisplayName(szMake,szModel).c_str());
		}

		static std::wstring GenerateDisplayName(const wchar_t *szMake, const wchar_t *szModel)
		{
			std::wstring strOutDisplayName = szMake;
			if (!strOutDisplayName.empty()) { strOutDisplayName += L" / "; }
			strOutDisplayName += szModel;
			return strOutDisplayName;
		}
	};

private:
	// Don't forget to update copy constructor and assignment operator when adding members.
	HMODULE                       m_hModuleDll;
	mutable CRITICAL_SECTION      m_cs;
	DWORD64                       m_dw64OpusVersion;
	DOpusPluginHelperDCRawPlugin *m_pDOpusPluginHelper;
	LeoHelpers::OpusStringLoader  m_sl;
	bool                          m_bWindowsXPOrAbove;

	bool                          m_bRawEnabled;
	bool                          m_bPnmEnabled;
	std::set< std::wstring >      m_setRawExtensionsLower;
	DCR_ProfileMap                m_mapProfiles;

public:
	DCRawConfig(HMODULE hModuleDll, DWORD64 dw64OpusVersion, DOpusPluginHelperDCRawPlugin *pDOpusPluginHelper);
	DCRawConfig(const DCRawConfig &rhs); // copy constructor can be used.
	DCRawConfig &operator=(const DCRawConfig &rhs); // assignment operator can be used.
	~DCRawConfig(); // Warning: Non-virtual destructor.

	HMODULE GetInstance() const { return m_hModuleDll; }
	DOpusPluginHelperDCRawPlugin *GetOpusPluginHelper() const { return m_pDOpusPluginHelper; }

	const wchar_t *GetString(UINT id)
	{
		return m_sl.Get(id);
	}

	enum OpusAbility
	{
		DCRA_RUN	= 0,
	};

	bool CheckOpusAbility(OpusAbility ability) const
	{
		switch(ability)
		{
		case(DCRA_RUN):
			return (m_dw64OpusVersion >= MAKE64BITVERSIONNUMBER(9,1,2,0));
		default:
			return false;
		}
	}

	static void ProcessExtension(std::wstring *pStrExt, bool bWantDot)
	{
		if (pStrExt == NULL)     { return; }
		if (pStrExt->empty())    { return; }
		if ((*pStrExt)[0]==L'.') { pStrExt->erase(0,1); }
		if (pStrExt->empty())    { return; }
		LeoHelpers::GenerateLegalFileName(pStrExt, false, false, 0); // Replace illegal chars and extra dots with _
		LeoHelpers::ToLower(pStrExt);
		if (bWantDot)            { pStrExt->insert(0, L"."); }
	}

	void SetRawEnabled(bool bEnabled) { m_bRawEnabled = bEnabled; }
	void SetPnmEnabled(bool bEnabled) { m_bPnmEnabled = bEnabled; }
	void SetRawExtensions(const std::vector< std::wstring > &vecExtensions);

	bool IsRawEnabled() const { return m_bRawEnabled; } // No need for critical section as bool read/write is atomic.
	bool IsPnmEnabled() const { return m_bPnmEnabled; } // No need for critical section as bool read/write is atomic.
	void GetExtensions(std::vector< std::wstring > *pvecExtensions, bool bRawIfEnabled, bool bRawAlways, bool bPnmIfEnabled, bool bPnmAlways) const;

	enum ExtensionType
	{
		DCRET_UNHANDLED,
		DCRET_RAW,
		DCRET_PNM
	};

	ExtensionType GetExtensionType(const std::wstring &strExtNoDotLower, bool bRawIfEnabled, bool bRawAlways, bool bPnmIfEnabled, bool bPnmAlways) const;

	DCR_ProfileMap &GetProfileMapNoLock()
	{
		return m_mapProfiles;
	}

	void RemoveProfileNoLock(const std::wstring &strProfileSafeName)
	{
		m_mapProfiles.erase(strProfileSafeName);
	}

	DCR_RawSettings GetRawSettings(const std::wstring &strSafeName, DCR_RawSettings::Purpose purp) const;

	void LoadDefaults();
	bool Load();
	bool Save() const;

	bool RawSettingsToClipboardString(std::wstring *pstrOut, const DCR_RawSettings &rs) const;
	bool RawSettingsFromClipboardString(DCR_RawSettings *prs, const std::wstring &strIn, DCR_RawSettings::Purpose purp) const;

private:
	static bool growCharBuffer(wchar_t **pCharBuffer, INT *pCharBufferSize, INT reqSize);
	bool xmlGetNodeName(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const;
	bool xmlGetNodeValue(HANDLE hNode, wchar_t **pCharBuffer, INT *pCharBufferSize) const;
	bool xmlGetNodeAttribute(HANDLE hNode, const wchar_t *szAttributeName, wchar_t **pCharBuffer, INT *pCharBufferSize) const;

	bool getConfigPath(std::wstring *pStrPath) const;

	bool loadXml();
	bool loadXmlContent(HANDLE hRootNode, wchar_t **pCharBuffer, INT *pCharBufferSize);
	void loadXmlRawSettings(HANDLE hNodeSettings, DCR_RawSettings *prs, wchar_t **pCharBuffer, INT *pCharBufferSize) const;
	bool saveXml() const;
	bool saveXmlContent(HANDLE hRootNode) const;
	bool saveXmlRawSettings(HANDLE hProfileNode, const wchar_t *szSettingsNodeName, const DCR_RawSettings &rs, DCR_RawSettings::Purpose purp) const;
	bool loadRegistry();
	void loadRegistryRawSettings(const std::wstring &strKeyPath, DCR_RawSettings *prs);
	bool isRedundantRawSettingsFromRegistry(const DCR_RawSettings &rs);
};
