#pragma once

class CTextThumbGenerator
{
public:
	CTextThumbGenerator();
	/* virtual */ ~CTextThumbGenerator();

	HBITMAP GenerateThumbnail(const CTextThumbConfig &config, HWND hWnd, CBaseTextFile *pTextFile, LPVIEWERPLUGINFILEINFO lpVPFileInfo,
						      const CTextThumbConfig::CTypeConfig &typeConfig,
							  const bool bFillAlphaForDlg, const bool bSimulateIcon, LONG lWidth, LONG lHeight);

protected:
	static const BYTE *GetShadowArray(LONG &lShadow);
	static HRGN GenerateRegion(const LONG lWidth, const LONG lHeight, const LONG lAASize, bool bForText);

	static RGBQUAD *GenerateBackground(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground);
	static HBITMAP GenerateShadowMaskDIB(RGBQUAD **ppPixels, HDC hDC, HRGN hRgn, const LONG lWidth, const LONG lHeight, const LONG lAASize);

	static bool DrawTextIntoCurrentRegion(HDC hDC, const LONG lWidth, const LONG lHeight, const CTextThumbConfig::CTypeConfig &typeConfig, CBaseTextFile *pTextFile);
	static bool GradientFillAllWithCurrentRegion(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground);
	static bool FillBackgroundForDialog(HDC hDC, HWND hWnd, const LONG lWidth, const LONG lHeight, RGBQUAD *pPixels);
	static bool DrawSampleIcon(HDC hDC, HINSTANCE hInstance, RGBQUAD *pPixels, const LONG lWidth, const LONG lHeight, const std::wstring &strExt);

	const RGBQUAD *CacheBackground(HDC hDC, const LONG lWidth, const LONG lHeight, const COLORREF crBackground);
	void ReleaseBackground(const LONG lWidth, const LONG lHeight, const COLORREF crBackground);

protected:
	enum { GISCALE_PIXBYTES		= 4 }; // 4 bytes per pixel; preserves alpha channel.

protected:
	CRITICAL_SECTION m_cs;

	class ColorCache
	{
	public:
		// There must be a zero-argument constructor.
		ColorCache() : m_iUserCount(0), m_dwLastUse(0) { }
		~ColorCache()
		{
			for (std::map< COLORREF, LeoHelpers::CriticalObject< RGBQUAD * > * >::iterator mapIter = m_mapColorToPixels.begin(); mapIter != m_mapColorToPixels.end(); ++mapIter)
			{
				delete [] mapIter->second->object;
				mapIter->second->object = NULL;
				delete mapIter->second;
				mapIter->second = NULL;
			}
		}

		void FlagInUse() { m_iUserCount++; }
		void FlagDone() { m_dwLastUse = GetTickCount(); m_iUserCount--; }
		bool IsExpired(DWORD dwTimeoutMS) { return (m_iUserCount == 0 && GetTickCount() - m_dwLastUse >= dwTimeoutMS); } // Handles 49.7 day timer wrap-around.

		// We use the default copy-constructor and assignment operator with the assumption they're only called before the ColorCache is ever used.
	//	ColorCache(const ColorCache &rhs);
	//	ColorCache &operator=(const ColorCache &rhs);
	public:
		std::map< COLORREF, LeoHelpers::CriticalObject< RGBQUAD * > * > m_mapColorToPixels;
	protected:
		DWORD m_dwLastUse;
		int m_iUserCount;
	};

	std::map< std::pair< LONG, LONG >, ColorCache > m_mapSizeToColors;

	class CBackgroundReleaser
	{
	public:
		CBackgroundReleaser(CTextThumbGenerator *pGenerator, const LONG lWidth, const LONG lHeight, const COLORREF crBackground)
		: m_pGenerator(pGenerator), m_lWidth(lWidth), m_lHeight(lHeight), m_crBackground(crBackground)
		{
		}

		/*virtual*/ ~CBackgroundReleaser()
		{
			if (NULL != m_pGenerator)
			{
				m_pGenerator->ReleaseBackground(m_lWidth, m_lHeight, m_crBackground);
			}
		}

		void SetGenerator(CTextThumbGenerator *pGenerator)
		{
			m_pGenerator = pGenerator;
		}

	protected:
		CTextThumbGenerator *m_pGenerator;
		const LONG m_lWidth;
		const LONG m_lHeight;
		const COLORREF m_crBackground;
	};

	class CCacheCleanupThread : public LeoHelpers::HousekeepingThread
	{
	public:
		CCacheCleanupThread(CTextThumbGenerator *pGenerator);
		virtual ~CCacheCleanupThread();

	protected:
		// Once the thread has started, MainTask is called every m_dwThreadIntervalMS until Stop or RequestStop has been called.
		virtual void MainTask();

	protected:
		CTextThumbGenerator *m_pGenerator;
	};

	CCacheCleanupThread *m_pCacheCleanupThread;
};
