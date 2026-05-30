#pragma once

class DCRawStateData
{
private:
	const DCRawConfig *m_pConfig;

	wchar_t m_szCfgDlgPage[64];
	wchar_t m_szCfgDlgRawProfile[1024];

	void clear()
	{
		m_szCfgDlgPage[0] = L'\0';
		m_szCfgDlgRawProfile[0] = L'\0';
	}

	void buildOpusConfigStruct(DOPUSPLUGINCONFIGDATA &configData, std::vector< DOPUSPLUGINCONFIGITEM > &vecConfigItems)
	{
		DOPUSPLUGINCONFIGITEM tempItem;

		tempItem.pszName    = L"CfgDlgPage";
		tempItem.iType      = DPCITYPE_LPWSTR;
		tempItem.pData      = m_szCfgDlgPage;
		tempItem.dwDataSize = _countof(m_szCfgDlgPage)*sizeof(m_szCfgDlgPage[0]); // Size should be in bytes, not chararacters. Used _countof to ensure compiler error if the array becomes a pointer.
		vecConfigItems.push_back(tempItem);

		tempItem.pszName    = L"CfgDlgRawProfile";
		tempItem.iType      = DPCITYPE_LPWSTR;
		tempItem.pData      = m_szCfgDlgRawProfile;
		tempItem.dwDataSize = _countof(m_szCfgDlgRawProfile)*sizeof(m_szCfgDlgRawProfile[0]); // Size should be in bytes, not chararacters. Used _countof to ensure compiler error if the array becomes a pointer.
		vecConfigItems.push_back(tempItem);

		configData.cbSize        = sizeof(DOPUSPLUGINCONFIGDATA);
		configData.pszName       = DCRAW_CONFIG_NAME;
		configData.fStateData    = TRUE;
		configData.iNumCfgItems  = static_cast<int>(vecConfigItems.size());
		configData.pCfgItemArray = &vecConfigItems[0];
	}

public:
	DCRawStateData(const DCRawConfig *pConfig)
	: m_pConfig(pConfig)
	{
		m_szCfgDlgPage[0] = L'\0';
		m_szCfgDlgRawProfile[0] = L'\0';
	}

	bool Load()
	{
		clear();

		DOPUSPLUGINCONFIGDATA configData;
		std::vector< DOPUSPLUGINCONFIGITEM > vecConfigItems;
		buildOpusConfigStruct(configData, vecConfigItems);

		if (!m_pConfig->GetOpusPluginHelper()->LoadOrSaveConfig(OPUSCFG_LOAD, &configData))
		{
			clear();
			return false;
		}

		return true;
	}

	bool Save()
	{
		DOPUSPLUGINCONFIGDATA configData;
		std::vector< DOPUSPLUGINCONFIGITEM > vecConfigItems;
		buildOpusConfigStruct(configData, vecConfigItems);

		if (!m_pConfig->GetOpusPluginHelper()->LoadOrSaveConfig(OPUSCFG_SAVE, &configData))
		{
			return false;
		}

		return true;
	}

	const wchar_t *GetCfgDlgPage()       const { return m_szCfgDlgPage;       }
	const wchar_t *GetCfgDlgRawProfile() const { return m_szCfgDlgRawProfile; }

	bool SetCfgDlgPage(const std::wstring &strCfgDlgPage)
	{
		if (strCfgDlgPage.length() >= _countof(m_szCfgDlgPage))
		{
			assert(false);
			return false;
		}

		LeoHelpers::StringCopy(m_szCfgDlgPage, strCfgDlgPage.c_str(), _countof(m_szCfgDlgPage));

		return true;
	}

	bool SetCfgDlgRawProfile(const std::wstring &strCfgDlgRawProfile)
	{
		if (strCfgDlgRawProfile.length() >= _countof(m_szCfgDlgRawProfile))
		{
			assert(false);
			return false;
		}

		LeoHelpers::StringCopy(m_szCfgDlgRawProfile, strCfgDlgRawProfile.c_str(), _countof(m_szCfgDlgRawProfile));

		return true;
	}
};
