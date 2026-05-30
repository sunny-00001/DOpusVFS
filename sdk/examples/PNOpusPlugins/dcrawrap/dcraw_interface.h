
class DCRaw;

class DCRawResult
{
private:
	DCRaw *m_pDCRaw;

	DCRawResult(); // Private constructor. Using code should call static DoFile to get results.

	DCRawResult(const DCRawResult &rhs); // Not implemented.
	DCRawResult &operator=(const DCRawResult &rhs); // Not implemented.

public:

	~DCRawResult(); // Warning: non-virtual destructor. I assume nothing will extend this class.

	enum DCRAW_OPERATION
	{
		DCRO_UNKNOWN,
		DCRO_IDENTIFY,
		DCRO_PREVIEW,
		DCRO_DECODE,
		DCRO_DEBUGCMDLINE
	};

	static DCRawResult *DoFile(	CRITICAL_SECTION *pcsLcms,
								CRITICAL_SECTION *pcsTempFOpen,
								HANDLE hAbortEvent,
								DCRAW_OPERATION dcrOper,
								const wchar_t *szInputFilePath,
								const char    *szInputFileNameIfAscii,
								const wchar_t *szInputDirIfReal,
								const DCR_RawSettings *prs,
								std::vector< std::string > *pvecCmdLineDebug);


	static bool GetDCRawVersion(std::string *pstrOutVersion);

	const std::string &GetMake()     const;
	const std::string &GetModel()    const;
	int                GetWidth()    const;
	int                GetHeight()   const;
	int                GetRotation() const;

	bool GetOutputFilePath(std::wstring *pOutPath) const;
};
