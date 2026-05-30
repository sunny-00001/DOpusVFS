#pragma once

namespace DCRawConfigDlg
{
	class DropTarget : public IDropTarget
	{
	protected:
		bool	m_bDragEnabled;
		std::vector< HWND > m_vecDropWindows;
		std::vector< std::wstring > m_vecFilenames;
		HWND	m_hwnd;
	public:
		DropTarget();
		virtual ~DropTarget();
		void SetDragEnabled(bool bEnabled);
		void AddDropWindow(HWND hWnd);
		void Register(HWND hwnd);
		void Revoke();
		std::vector< std::wstring > &GetFilenames();
		HWND GetDropWindowFromScreenPoint(POINT &ptScreen);

		// IUnknown
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject);
		virtual ULONG STDMETHODCALLTYPE AddRef(void);
		virtual ULONG STDMETHODCALLTYPE Release(void);
		// IDropTarget
        virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
		virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
		virtual HRESULT STDMETHODCALLTYPE DragLeave(void);
		virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	};

	enum ComboFlagsDisplayedImage
	{
		CF_DI_PREVIEW	= 1<<0,
		CF_DI_FULL		= 1<<1,
	};

	enum ComboFlagsWhiteBalance
	{
		CF_WB_None    = 0,
		CF_WB_Cam     = 1<<0,
		CF_WB_Auto    = 1<<1,
		CF_WB_Set     = 1<<2,
		CF_WB_CamAuto = ((1<<0)|(1<<1)),
		CF_WB_CamSet  = ((1<<0)|(1<<2))
	};

	enum ComboFlagsGamma
	{
		CF_G_BT709,
		CF_G_SRGB,
		CF_G_CUSTOM
	};

	struct DCRawConfigDlg_CreationData
	{
		DCRawConfig *pConfig;
		HWND         hWndNotify;
		DWORD        dwNotifyData;

		DCRawConfigDlg_CreationData(DCRawConfig *pConfigIn, HWND hWndNotifyIn, DWORD dwNotifyDataIn)
		: pConfig(pConfigIn)
		, hWndNotify(hWndNotifyIn)
		, dwNotifyData(dwNotifyDataIn)
		{
		}

		~DCRawConfigDlg_CreationData()
		{
		}

	private:
		// Prevent calls to copy constructor and assignment operator.
		DCRawConfigDlg_CreationData(const DCRawConfigDlg_CreationData &rhs);
		DCRawConfigDlg_CreationData &operator=(const DCRawConfigDlg_CreationData &rhs);
	};

	struct DCRawConfigDlg_Data
	{
		HWND			hWndNotify;
		DWORD			dwNotifyData;
		HWND			hWndMainDlg;
		HWND			hWndFormats;
		HWND			hWndDCRawThumbs;
		HWND			hWndDCRawViewers;
		HWND			hWndDCRawConverter;
		DropTarget		dropTarget;
		bool			bProcessingWMScroll;
		std::wstring	strSelectedProfileSafeName;
		std::wstring	strActivePage;

		DCRawConfig		config;
		DCRawConfig *   pGlobalConfig;


		DCRawConfigDlg_Data(DCRawConfigDlg_CreationData *pCreationData)
		: hWndNotify(pCreationData->hWndNotify)
		, dwNotifyData(pCreationData->dwNotifyData)
		, hWndMainDlg(NULL)
		, hWndFormats(NULL)
		, hWndDCRawThumbs(NULL)
		, hWndDCRawViewers(NULL)
		, hWndDCRawConverter(NULL)
		, bProcessingWMScroll(false)
		, config(*pCreationData->pConfig)
		, pGlobalConfig(pCreationData->pConfig)
		{
		}
	};

	INT_PTR CALLBACK configDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR CALLBACK childProxyDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK editFloatFilterSubProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void configInitDialog(BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data *&pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void configNotify(    BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data * pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void configVScroll(   BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data * pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void configCommand(   BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data * pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void configDropFiles( BOOL &bResult, bool &bDeletePCD, bool &bLoadSettings, bool &bLoadDefaultSettings, bool &bSaveSettings, bool &bEnableDisable, DCRawConfigDlg_Data * pcd, HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

	void configLoadSettings( DCRawConfigDlg_Data *pcd, bool bLoadDefaultSettings, bool bInitDialog);
	void configSaveSettings( DCRawConfigDlg_Data *pcd);
	void configEnableDisable(DCRawConfigDlg_Data *pcd);

	void SetupRawDialog(DCRawConfigDlg_Data *pcd, DCR_RawSettings::Purpose purp);
	void SetupRawProfileInDialog(DCRawConfigDlg_Data *pcd, const DCRawConfig::DCR_RawProfile &rawProfile, DCR_RawSettings::Purpose purp);

	void StoreRawDialogData(DCRawConfigDlg_Data *pcd, DCR_RawSettings::Purpose purp);

	void RawToClipboard(DCRawConfigDlg_Data *pcd, HWND hwndDlg);
	void RawFromClipboard(DCRawConfigDlg_Data *pcd, HWND hwndDlg);


	void EnableDisableRawDialog(DCRawConfigDlg_Data *pcd, BOOL bRawEnabled, DCR_RawSettings::Purpose purp);

	void FilenameToEdit(DCRawConfigDlg_Data *pcd, HWND hwndDlg, int iDlgItem, const wchar_t *szTitle);
	void FilenamesToVector(DCRawConfigDlg_Data *pcd, HWND hwndDlg, std::vector< std::wstring > *pvecFiles, bool bClearVec, const wchar_t *szTitle);

	void addSupportedCameras(HWND hWndList);

	void processExtension(std::wstring &strExt);

#ifdef _DEBUG
	void debugCmdLine(DCRawConfigDlg_Data *pcd, HWND hwndDlg);
#endif
};
