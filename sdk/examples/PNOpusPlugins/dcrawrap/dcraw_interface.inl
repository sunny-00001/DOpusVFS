#include "dcraw_settings.h"
#include "dcraw_interface.h"

 // Private constructor. Using code should call static DoFile to get results.
DCRawResult::DCRawResult()
: m_pDCRaw(NULL)
{
}

DCRawResult::~DCRawResult()
{
	delete m_pDCRaw;
	m_pDCRaw = NULL;
}

const std::string &DCRawResult::GetMake()     const { return m_pDCRaw->GetMake(); }
const std::string &DCRawResult::GetModel()    const { return m_pDCRaw->GetModel(); }
int                DCRawResult::GetWidth()    const { return m_pDCRaw->GetWidth(); }
int                DCRawResult::GetHeight()   const { return m_pDCRaw->GetHeight(); }
int                DCRawResult::GetRotation() const { return m_pDCRaw->GetRotation(); }

bool DCRawResult::GetOutputFilePath(std::wstring *pOutPath) const { return m_pDCRaw->GetOutputFilePath(pOutPath); }


// static
bool DCRawResult::GetDCRawVersion(std::string *pstrOutVersion)
{
	if (pstrOutVersion == NULL)
	{
		return false;
	}

	*pstrOutVersion = VERSION;

	/*
	pstrOutVersion->clear();

	DCRawResult *pResult = new DCRawResult();

	pResult->m_pDCRaw = new DCRaw(NULL, NULL);

	// Run with no args to cause it to output the help text from which we can grab the version string.

	std::vector< const char * > vecArgs;
	vecArgs.push_back("dcraw.exe");

	// As DCRaw starts it does ( argv[argc] = ""; ) so we need to put something at the end of argv for it to overwrite
	// but which is not counted as part of argc (which is why we use vecArgs.size()-1 in the call to main).
	vecArgs.push_back("");

	try
	{
		status = pResult->m_pDCRaw->main(vecArgs.size()-1, const_cast< char ** >( &vecArgs[0] ));
	}
	catch(...)
	{
	}

	*pstrOutVersion = pResult->m_pDCRaw->GetVersion();

	delete pResult;
	pResult = NULL;
	*/

	return !pstrOutVersion->empty();
}

// static
DCRawResult *DCRawResult::DoFile(CRITICAL_SECTION *pcsLcms,
								 CRITICAL_SECTION *pcsTempFOpen,
								 HANDLE hAbortEvent,
								 DCRAW_OPERATION dcrOper,
								 const wchar_t *szInputFilePath,
								 const char    *szInputFileNameIfAscii,
								 const wchar_t *szInputDirIfReal,
								 const DCR_RawSettings *prs,
								 std::vector< std::string > *pvecCmdLineDebug)
{
	if (pvecCmdLineDebug)
	{
		pvecCmdLineDebug->clear();
	}

	DCRawResult *pResult = new DCRawResult();

	pResult->m_pDCRaw = new DCRaw(pcsLcms, pcsTempFOpen, hAbortEvent);

	const char *szFakeDirIn      = "!:\\in";
	const char *szFakeDirInSlash = "!:\\in\\";
	std::string strFakeFileIn    = szFakeDirInSlash;
	strFakeFileIn += (szInputFileNameIfAscii ? szInputFileNameIfAscii : "infile"); // Preserve the name if we can as some names have special meaning.
	const char *szFakeFileOut = "!:\\out\\outfile";

	pResult->m_pDCRaw->AddFilePathOverride(strFakeFileIn.c_str(), szInputFilePath);

	if (szInputDirIfReal)
	{
		pResult->m_pDCRaw->AddDirectoryPathOverride(szFakeDirIn, szInputDirIfReal); // Allow DCRaw to look for other files in the same directory.
	}

	bool bValidOp = false;

	std::vector< std::string > vecArgs;
	vecArgs.push_back("dcraw.exe");

	if (dcrOper == DCRO_IDENTIFY)
	{
		vecArgs.push_back("-i");
		vecArgs.push_back("-v");
		bValidOp = true;
	}
	else if (dcrOper == DCRO_PREVIEW)
	{
		vecArgs.push_back("-e");
		bValidOp = true;
	}
	else if (dcrOper == DCRO_DECODE || dcrOper == DCRO_DEBUGCMDLINE)
	{
		assert(prs != NULL);

		char szTemp[128];

		if (prs != NULL)
		{
			if (prs->iRotation >= 0)
			{
				vecArgs.push_back("-t");
				sprintf_s(szTemp, "%d", prs->iRotation);
				vecArgs.push_back(szTemp);
			}

			if (!prs->bCorrectGeometry)
			{
				vecArgs.push_back("-j");
			}

			if (prs->bDocModeNoCol)
			{
				vecArgs.push_back("-d");
			}
			else if (prs->bDocModeRaw)
			{
				vecArgs.push_back("-D");
			}
			else if (prs->bHalfSizeColor)
			{
				vecArgs.push_back("-h");
			}
			else if (prs->iInterpQuality >= 0)
			{
				vecArgs.push_back("-q");
				sprintf_s(szTemp, "%d", prs->iInterpQuality);
				vecArgs.push_back(szTemp);
			}

			if (prs->bRGGB)
			{
				vecArgs.push_back("-f");
			}

			if (prs->bCamWhite)
			{
				vecArgs.push_back("-w");
			}

			if (prs->bAutoWhite)
			{
				vecArgs.push_back("-a");
			}
			else if (prs->bSetWhite)
			{
				vecArgs.push_back("-r");
				sprintf_s(szTemp, "%d.%03d", prs->iUserMul1 / 1000, prs->iUserMul1 % 1000);
				vecArgs.push_back(szTemp);
				sprintf_s(szTemp, "%d.%03d", prs->iUserMul2 / 1000, prs->iUserMul2 % 1000);
				vecArgs.push_back(szTemp);
				sprintf_s(szTemp, "%d.%03d", prs->iUserMul3 / 1000, prs->iUserMul3 % 1000);
				vecArgs.push_back(szTemp);
				sprintf_s(szTemp, "%d.%03d", prs->iUserMul4 / 1000, prs->iUserMul4 % 1000);
				vecArgs.push_back(szTemp);
			}

			if (prs->iBrightness != 1000)
			{
				vecArgs.push_back("-b");
				sprintf_s(szTemp, "%d.%03d", prs->iBrightness / 1000, prs->iBrightness % 1000);
				vecArgs.push_back(szTemp);
			}

			if (prs->bFixedWhite)
			{
				vecArgs.push_back("-W");
			}

			if (prs->iHighlightMode > 0)
			{
				vecArgs.push_back("-H");
				sprintf_s(szTemp, "%d", prs->iHighlightMode);
				vecArgs.push_back(szTemp);
			}

			if (prs->iGammaPower    != DCR_RawSettings::GAMMA_BT709_POWER
			||	prs->iGammaToeSlope != DCR_RawSettings::GAMMA_BT709_TOESLOPE)
			{
				vecArgs.push_back("-g");
				sprintf_s(szTemp, "%d.%03d", prs->iGammaPower / 1000, prs->iGammaPower % 1000);
				vecArgs.push_back(szTemp);
				sprintf_s(szTemp, "%d.%03d", prs->iGammaToeSlope / 1000, prs->iGammaToeSlope % 1000);
				vecArgs.push_back(szTemp);
			}

			if (prs->iChromaRed != 1000 || prs->iChromaBlue != 1000)
			{
				vecArgs.push_back("-C");
				sprintf_s(szTemp, "%d.%03d", prs->iChromaRed / 1000, prs->iChromaRed % 1000);
				vecArgs.push_back(szTemp);
				sprintf_s(szTemp, "%d.%03d", prs->iChromaBlue / 1000, prs->iChromaBlue % 1000);
				vecArgs.push_back(szTemp);
			}

			if (prs->iCamIccType == 1)
			{
				vecArgs.push_back("-p");
				vecArgs.push_back("embed");
			}
			else if (prs->iCamIccType == -1 && !prs->strCamIcc.empty())
			{
				std::string strFakePath = szFakeDirInSlash;
				strFakePath += "camicc";
				pResult->m_pDCRaw->AddFilePathOverride(strFakePath.c_str(), prs->strCamIcc.c_str());

				vecArgs.push_back("-p");
				vecArgs.push_back(strFakePath.c_str());
			}

			if (prs->iOutIccType != -1 && prs->iOutIccType != 1)
			{
				vecArgs.push_back("-o");
				sprintf_s(szTemp, "%d", prs->iOutIccType);
				vecArgs.push_back(szTemp);
			}
			else if (prs->iOutIccType == -1 && !prs->strOutIcc.empty())
			{
				std::string strFakePath = szFakeDirInSlash;
				strFakePath += "outicc";
				pResult->m_pDCRaw->AddFilePathOverride(strFakePath.c_str(), prs->strOutIcc.c_str());

				vecArgs.push_back("-o");
				vecArgs.push_back(strFakePath.c_str());
			}

			if (prs->bBadPixels && !prs->strBadPixelsPath.empty())
			{
				std::string strFakePath = szFakeDirInSlash;
				strFakePath += ".badpixels";
				pResult->m_pDCRaw->AddFilePathOverride(strFakePath.c_str(), prs->strBadPixelsPath.c_str());

				vecArgs.push_back("-P");
				vecArgs.push_back(strFakePath.c_str());
			}

			if (prs->bNoiseFilter)
			{
				vecArgs.push_back("-n");
				sprintf_s(szTemp, "%d", prs->iNoiseThreshold);
				vecArgs.push_back(szTemp);
			}

			if (prs->bMedianFilter)
			{
				vecArgs.push_back("-m");
				sprintf_s(szTemp, "%d", prs->iMedianPasses);
				vecArgs.push_back(szTemp);
			}
		}

		bValidOp = true;
	}

	assert(bValidOp);

	int status = 1; // 0 for success, anything else for failure.

	if (bValidOp)
	{
		vecArgs.push_back(strFakeFileIn.c_str());

		if (pvecCmdLineDebug)
		{
			*pvecCmdLineDebug = vecArgs;
		}

		if (dcrOper == DCRO_DEBUGCMDLINE)
		{
			status = 1; // Ensure that pResult is deleted. DCRO_DEBUGCMDLINE callers expect NULL result and check the vector instead.
		}
		else
		{
			// Convert string vector into char * array.
			std::vector< const char * > vecArgsChar;

			for(std::vector< std::string >::const_iterator viter = vecArgs.begin(); viter != vecArgs.end(); ++viter)
			{
				vecArgsChar.push_back( viter->c_str() );
			}

			// As DCRaw starts it does ( argv[argc] = ""; ) so we need to put something at the end of argv for it to overwrite
			// but which is not counted as part of argc (which is why we use vecArgsChar.size()-1 in the call to main).
			vecArgsChar.push_back("");

			try
			{
				status = pResult->m_pDCRaw->main(vecArgsChar.size()-1, const_cast< char ** >( &vecArgsChar[0] ));
			}
			catch(...)
			{
				status = 1; // fail
			}
		}
	}

	if (status != 0)
	{
		delete pResult;
		pResult = NULL;
	}

	return pResult;
}
