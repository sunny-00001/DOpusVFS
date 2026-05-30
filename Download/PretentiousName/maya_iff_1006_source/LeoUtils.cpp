/* Utility code
 * Copyright (C) 2008 Leo Davidson
 * (email: leo@ox.compsoc.net, WWW: http://www.pretentiousname.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "StdAfx.h"
#include "LeoUtils.h"

// delete[] the result.
// Returns NULL on failure.
TCHAR *LeoUtils::StringAllocAndFormat(const TCHAR *format, ...)
{
	va_list args;
	va_start(args, format);
	TCHAR *result = VStringAllocAndFormat(format, args);
	va_end(args);

	return(result);
}

// delete[] the result.
// Returns NULL on failure.
TCHAR *LeoUtils::VStringAllocAndFormat(const TCHAR *format, va_list args)
{
	size_t bufferSize = 64;

	TCHAR *szResult = new(std::nothrow) TCHAR[bufferSize];

	while(true)
	{
		if (NULL == szResult)
		{
			::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			break;
		}

		int formatRes = _vsntprintf_s(szResult, bufferSize, _TRUNCATE, format, args);

		// formatRes negative on error; (bufferSize == formatRes) should be redundant for _vsntprintf.

		if ((0 > formatRes) || (bufferSize == formatRes))
		{
			delete [] szResult;
			bufferSize *= 2;
			szResult = new(std::nothrow) TCHAR[bufferSize];
		}
		else
		{
			break;
		}
	}

	return(szResult);
}

bool LeoUtils::CopyFile(const TCHAR *szSource, const TCHAR *szDest, HANDLE hAbortEvent, bool bClearReadOnlyAttrib, bool bFailIfExists)
{
	class CopyFileDummyInner
	{
	public:
		static DWORD CALLBACK CopyProgressRoutine(
			LARGE_INTEGER TotalFileSize,
			LARGE_INTEGER TotalBytesTransferred,
			LARGE_INTEGER StreamSize,
			LARGE_INTEGER StreamBytesTransferred,
			DWORD dwStreamNumber,
			DWORD dwCallbackReason,
			HANDLE hSourceFile,
			HANDLE hDestinationFile,
			LPVOID lpData)
		{
			if (lpData == NULL)
			{
				return PROGRESS_QUIET;
			}

			if (WAIT_TIMEOUT != ::WaitForSingleObject(static_cast<HANDLE>(lpData), 0))
			{
				return PROGRESS_CANCEL;
			}

			return PROGRESS_CONTINUE;
		}
	};

	BOOL bCancel = FALSE; // Never used.
	DWORD dwCopyFlags = (bFailIfExists ? COPY_FILE_FAIL_IF_EXISTS : 0);

	if (0 == ::CopyFileEx(szSource, szDest, CopyFileDummyInner::CopyProgressRoutine, hAbortEvent, &bCancel, dwCopyFlags))
	{
		return false;
	}

	if (bClearReadOnlyAttrib)
	{
		DWORD dwAttribs = ::GetFileAttributes(szDest);

		if (dwAttribs != INVALID_FILE_ATTRIBUTES
		&&	(dwAttribs&FILE_ATTRIBUTE_READONLY))
		{
			dwAttribs -= FILE_ATTRIBUTE_READONLY;
			::SetFileAttributes(szDest, dwAttribs);
		}
	}

	return true;
}

bool LeoUtils::CopyVersionResourceToViewerPluginInfo(HMODULE g_hDllModule, WORD iconId, LPVIEWERPLUGININFO lpVPInfo)
{
	bool bResult = false;

	TCHAR szModName[_MAX_PATH + 1];
	DWORD dwVerSize;
	DWORD dwUnused;

	if (NULL != g_hDllModule
	&&	GetModuleFileName(g_hDllModule, szModName, _MAX_PATH)
	&&	(0 != (dwVerSize = GetFileVersionInfoSize(szModName, &dwUnused))))
	{
		BYTE *pVerData = new BYTE[dwVerSize];

		if (GetFileVersionInfo(szModName, NULL, dwVerSize, pVerData))
		{
			LPVOID pBuffer;
			UINT bufLen;

			// Find out language of version resource
			if (VerQueryValue(pVerData, _T("\\VarFileInfo\\Translation"), &pBuffer, &bufLen)
			&&	4 == bufLen)
			{
				BYTE *pLang = reinterpret_cast<BYTE*>(pBuffer);

				if (LeoUtils::CopyVersionNumber(&lpVPInfo->dwVersionHigh,	&lpVPInfo->dwVersionLow,		_T("FileVersion"),		pLang, pVerData)
				&&	LeoUtils::CopyVersionString(lpVPInfo->lpszCopyright,	lpVPInfo->cchCopyrightMax,		_T("LegalCopyright"),	pLang, pVerData)
				&&	LeoUtils::CopyVersionString(lpVPInfo->lpszName,			lpVPInfo->cchNameMax,			_T("InternalName"),		pLang, pVerData)
				&&	LeoUtils::CopyVersionString(lpVPInfo->lpszDescription,	lpVPInfo->cchDescriptionMax,	_T("FileDescription"),	pLang, pVerData)
				&&	LeoUtils::CopyVersionString(lpVPInfo->lpszURL,			lpVPInfo->cchURLMax,			_T("CompanyName"),		pLang, pVerData))
				{
					// If anything fails after the next line then the icon must be destroyed.
					if (0 != iconId && lpVPInfo->cbSize >= sizeof(VIEWERPLUGININFO))
					{
						lpVPInfo->hIconSmall = reinterpret_cast<HICON>(LoadImage(g_hDllModule, MAKEINTRESOURCE(iconId), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR));
					}

					bResult = true;
				}
			}
		}

		delete [] pVerData;
	}
	
	return bResult;
}

bool LeoUtils::CopyVersionNumber(DWORD *pdwHigh, DWORD *pdwLow, TCHAR *szItemName, BYTE *pLang, const LPVOID pVerData)
{
	bool bResult = false;

	TCHAR szVerPath[128];
	_stprintf_s(szVerPath, 128, _T("\\StringFileInfo\\%02x%02x%02x%02x\\%s"), pLang[1], pLang[0], pLang[3], pLang[2], szItemName);

	LPVOID pBuffer;
	UINT bufLen;
	int v0,v1,v2,v3;

	if (VerQueryValue(pVerData, szVerPath, &pBuffer, &bufLen)
	&&	4 == _stscanf_s(reinterpret_cast<TCHAR *>(pBuffer), _T("%d, %d, %d, %d"), &v0, &v1, &v2, &v3))
	{
		*pdwHigh = ((v0<<16) + v1);
		*pdwLow  = ((v2<<16) + v3);

		bResult = true;
	}

	return(bResult);
}

bool LeoUtils::CopyVersionString(TCHAR *szDestBuffer, UINT uiDestBufferSize, TCHAR *szItemName, BYTE *pLang, const LPVOID pVerData)
{
	bool bResult = false;

	if (NULL == szDestBuffer)
	{
		bResult = true;
	}
	else if (2 < uiDestBufferSize)
	{
		TCHAR szVerPath[128];
		_stprintf_s(szVerPath, 128, _T("\\StringFileInfo\\%02x%02x%02x%02x\\%s"), pLang[1], pLang[0], pLang[3], pLang[2], szItemName);

		LPVOID pBuffer;
		UINT bufLen;

		if (VerQueryValue(pVerData, szVerPath, &pBuffer, &bufLen))
		{
			_tcsncpy_s(szDestBuffer, uiDestBufferSize, reinterpret_cast<TCHAR *>(pBuffer), _TRUNCATE);

			bResult = true;
		}
	}

	return(bResult);
}

const TCHAR *LeoUtils::GetLastPathPart(const TCHAR *szIn)
{
	if (NULL == szIn)
	{
		return(szIn);
	}

	size_t len = _tcslen(szIn);

	if (1 >= len)
	{
		return(szIn);
	}

	const TCHAR *szOut = szIn + (len - 1);

	// Ignore any slashes at the very end of the string.
	while (szIn < szOut && (*szOut == _T('\\') || *szOut == _T('/')))
	{
		szOut--;
	}

	// Stop at the next slash we find.
	while (szIn < szOut && (*szOut != _T('\\') && *szOut != _T('/')))
	{
		szOut--;
	}

	if (*szOut == _T('\\') || *szOut == _T('/'))
	{
		szOut++;
	}

	return(szOut);
}

const TCHAR *LeoUtils::GetExtensionPart(bool bIncludeDot, const TCHAR *szIn)
{
	if (NULL == szIn)
	{
		return(szIn);
	}

	size_t len = _tcslen(szIn);

	if (1 >= len)
	{
		return(NULL);
	}

	const TCHAR *szOut = szIn + (len - 1);

	while (szIn < szOut && (*szOut != _T('.')))
	{
		szOut--;
	}

	if (*szOut != _T('.'))
	{
		return(NULL);
	}

	if (!bIncludeDot)
	{
		szOut++;
	}

	return(szOut);
}

LeoUtils::CFileAndStream::CFileAndStream(const wchar_t *szFilePath, HANDLE hAbortEvent, bool bOpenTempCopy)
: m_strFileName(LeoUtils::GetLastPathPart(szFilePath))
, m_strFilePath(szFilePath)
, m_bDeleteFilePathOnClose(false)
, m_bOpenTempCopyFilePath(bOpenTempCopy)
, m_pStream(NULL)
, m_bNoRandomSeekInStream(false)
, m_hAbortEvent(hAbortEvent)
, m_bStreamIsFile(false)
{
}

// pStream can be NULL but only if you are never going to call methods which read data or get the steam or file path.
LeoUtils::CFileAndStream::CFileAndStream(const wchar_t *szFileName, IStream *pStream, bool bNoRandomSeek)
: m_strFileName(szFileName)
, m_bDeleteFilePathOnClose(false)
, m_bOpenTempCopyFilePath(false)
, m_pStream(pStream)
, m_bNoRandomSeekInStream(bNoRandomSeek)
, m_hAbortEvent(NULL)
, m_bStreamIsFile(false)
{
	if (m_pStream != NULL)
	{
		m_pStream->AddRef();
	}
}

LeoUtils::CFileAndStream::CFileAndStream(const wchar_t *szFileName, const BYTE *pData, UINT cbDataSize)
: m_strFileName(szFileName)
, m_bDeleteFilePathOnClose(false)
, m_bOpenTempCopyFilePath(false)
, m_pStream(NULL)
, m_bNoRandomSeekInStream(false)
, m_hAbortEvent(NULL)
, m_bStreamIsFile(false)
{
	HGLOBAL hGlobal = ::GlobalAlloc(GMEM_MOVEABLE, cbDataSize);

	if (hGlobal != NULL)
	{
		void *pBuffer = ::GlobalLock(hGlobal);

		if (pBuffer != NULL)
		{
			::memcpy_s(pBuffer, cbDataSize, pData, cbDataSize);

			::GlobalUnlock(hGlobal);

			if (S_OK != ::CreateStreamOnHGlobal(hGlobal, TRUE, &m_pStream))
			{
				m_pStream = NULL;
			}
		}
	}
}

LeoUtils::CFileAndStream::~CFileAndStream()
{
	if (m_pStream != NULL)
	{
		m_pStream->Release();
		m_pStream = NULL;
	}

	if (m_bDeleteFilePathOnClose && !m_strFilePath.empty())
	{
		for(int i = 0; i < 5; ++i)
		{
			if (DeleteFile(m_strFilePath.c_str()))
			{
				break;
			}

			Sleep(1000);
		}
	}

	m_bDeleteFilePathOnClose = false;
	m_bOpenTempCopyFilePath = false;
	m_bNoRandomSeekInStream = false;
	m_bStreamIsFile = false;
	m_strFilePath.clear();
	m_strFileName.clear();
	m_hAbortEvent = NULL;
}

// This allows you to set the OpenTempCopy flag after construction.
// Any subsequent call to GetFilePath will result in the file being copied to a temp path if
// it had not already been done. Any preceeding call to GetFilePath may, of course, have alredy
// received the real filename. If the data is in a stream then calling this has no effect as
// GetFilePath will always cause the data to be saved to a temp file regardless of the flag.
void LeoUtils::CFileAndStream::SetOpenTempCopy(bool bOpenTemp)
{
	m_bOpenTempCopyFilePath = bOpenTemp;
}

// The file name may not match the name of the actual file. It is indicative of the original file's name
// but the file path returned by GetFilePath may be a temp-file with a different name. GetFileName should
// be used for display purposes and for any logic which depends on the original file's name.
// Calling GetFileName will not trigger any files or streams to be created.

bool LeoUtils::CFileAndStream::GetFileName(std::basic_string< TCHAR > *pstrFileName)
{
	if (m_strFileName.empty())
	{
		return false;
	}

	*pstrFileName = m_strFileName;
	return true;
}

bool LeoUtils::CFileAndStream::GetFileExtension(std::basic_string< TCHAR > *pstrFileExtension, bool bIncludeDot)
{
	if (m_strFileName.empty())
	{
		return false;
	}

	const TCHAR *szExt = LeoUtils::GetExtensionPart(bIncludeDot, m_strFileName.c_str());

	if (szExt == NULL)
	{
		return false;
	}

	*pstrFileExtension = szExt;
	return true;
}

bool LeoUtils::CFileAndStream::HasFilePath()
{
	return !m_strFilePath.empty();
}

bool LeoUtils::CFileAndStream::HasStream()
{
	return m_pStream != NULL;
}

bool LeoUtils::CFileAndStream::GetNominalFilePath(std::basic_string< TCHAR > *pstrNominalFilePath)
{
	if (pstrNominalFilePath == NULL)
	{
		return false;
	}

	pstrNominalFilePath->clear();

	if (m_strFileName.empty())
	{
		return false;
	}

	if ((m_bOpenTempCopyFilePath && !m_bDeleteFilePathOnClose)
	||	(m_strFilePath.empty() && m_pStream != NULL))
	{
		return GetNominalTempFilePath(pstrNominalFilePath);
	}

	if (!m_strFilePath.empty())
	{
		*pstrNominalFilePath = m_strFilePath;
		return true;
	}

	return false;
}

bool LeoUtils::CFileAndStream::GetNominalTempFilePath(std::basic_string< TCHAR > *pstrNominalTempFilePath)
{
	if (pstrNominalTempFilePath == NULL)
	{
		return false;
	}

	pstrNominalTempFilePath->clear();

	if (m_strFileName.empty())
	{
		return false;
	}

	// Work out a path that will be similar to the real temp file path if the file ever gets written to disk.
	// We tell OpenTempFileNamePreserveExtension to not really create the file.
	OpenTempFileNamePreserveExtension(NULL, _T("dnvt"), m_strFileName.c_str(), pstrNominalTempFilePath, true);

	return !pstrNominalTempFilePath->empty();
}

bool LeoUtils::GetTempPath(std::basic_string< TCHAR > *pstrTempPath)
{
	// If we ask for the size first then we have to worry about the temp dir
	// being redefined between calls. In theory we still have to worry about
	// that with the code below but only if the temp path is already
	// longer than MAX_PATH and it then gets redefined, between calls, and
	// the new definition is even longer than the old one. In which case we
	// will fail gracefully. Since a lot of other software will fail just because
	// the temp path is longer than MAX_PATH it doesn't seem worth looping to
	// handle the worst case scenario.

	DWORD dwTempBufferSize = MAX_PATH;
	TCHAR szTempPath1[MAX_PATH];

	DWORD dwTempRes = ::GetTempPath(dwTempBufferSize, szTempPath1);

	if (dwTempRes == 0)
	{
		return false;
	}
	else if (dwTempRes < dwTempBufferSize)
	{
		*pstrTempPath = szTempPath1;
		return true;
	}

	dwTempBufferSize = dwTempRes + 1;
	TCHAR *szTempPath2 = new TCHAR[dwTempBufferSize];

	dwTempRes = ::GetTempPath(dwTempBufferSize, szTempPath2);

	bool bResult = false;

	if (dwTempRes != 0 && dwTempRes < dwTempBufferSize)
	{
		*pstrTempPath = szTempPath2;
		bResult = true;
	}

	delete[] szTempPath2;

	return bResult;
}

// If bDontReallyOpen is true then the function won't really create a file and always returns INVALID_HANDLE_VALUE, but will still set *pstrFilenameOut to a name that is similar to what you'd get normally.
// On failure *pstrFilenameOut will be left empty so you can check that when bDontReallyOpen is true.
// If you set bDontReallyOpen then you absolutely must not create a file with the generated path because such a file may already exist and be used for other purposes. The generated path should only be used as a sample, for example if you want to test whether calling this function for real will result in a path that contains any non-ASCII characters.
HANDLE LeoUtils::OpenTempFileNamePreserveExtension(const TCHAR *szTempPath, const TCHAR *szPrefix, const TCHAR *szFilenameIn, std::basic_string< TCHAR > *pstrFilenameOut, bool bDontReallyOpen)
{
	HANDLE hFileResult = INVALID_HANDLE_VALUE;

	if (NULL != pstrFilenameOut)
	{
		pstrFilenameOut->clear();
	}

	std::basic_string< TCHAR > strFileStart;
	
	if (szTempPath != NULL)
	{
		strFileStart = szTempPath;
	}
	else if (!LeoUtils::GetTempPath(&strFileStart))
	{
		return hFileResult;
	}

	if (strFileStart.length() > 0 && '\\' != *(strFileStart.rbegin()) && '/' != *(strFileStart.rbegin()))
	{
		strFileStart += '\\';
	}

	strFileStart += szPrefix;

	std::basic_string< TCHAR > strExtension = _T(".tmp");

	if (NULL != szFilenameIn)
	{
		const TCHAR *szExt = _tcsrchr(szFilenameIn, _T('.'));

		if (NULL != szExt)
		{
			strExtension = szExt;
		}
	}

	SYSTEMTIME systime;
	GetSystemTime(&systime);

	int i1 = systime.wYear;
	int i2 = systime.wMonth;
	int i3 = systime.wDay;
	int i4 = systime.wHour;
	int i5 = systime.wMinute;
	int i6 = systime.wSecond;
	int i7a = systime.wMilliseconds;

	for (int i = 0; i < 100; i++)
	{
		int i7 = i7a + i;

		TCHAR *szFilename = StringAllocAndFormat(_T("%s%04d%02d%02d%02d%02d%02d%04d%s"),
													strFileStart.c_str(), i1,i2,i3,i4,i5,i6,i7, strExtension.c_str());

		if (NULL == szFilename)
		{
			break;
		}
		else
		{
			if (!bDontReallyOpen)
			{
				hFileResult = ::CreateFile(szFilename, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
			}

			if (bDontReallyOpen || hFileResult != INVALID_HANDLE_VALUE)
			{
				if (NULL != pstrFilenameOut)
				{
					*pstrFilenameOut = szFilename;
				}
				delete [] szFilename;
				break;
			}
			delete [] szFilename;
		}
	}

	return(hFileResult);
}

// These calls will cause a file or stream to be created if there isn't one already.
// Call HasFilePath and HasStream to see what already exists if you can work with both and want
// to avoid the overhead of conversion.

bool LeoUtils::CFileAndStream::GetFilePath(std::basic_string< TCHAR > *pstrFilePath)
{
	bool bResult = false;

	if (PrepareFileForReading()
	||	WriteStreamToTempFile())
	{
		*pstrFilePath = m_strFilePath;

		bResult = true;
	}
/*
	if (m_bStreamIsFile && m_pStream != NULL)
	{
		m_pStream->Release();
		m_pStream = NULL;

		m_bNoRandomSeekInStream = false;

		m_bStreamIsFile = false;
	}
*/
	return bResult;
}

// The stream will not be AddRef'd by this call. It will exist as long as the CFileAndSteam does
// so you should not normally need to AddRef it.
// If a stream is returned it will always be seekable.
// Whenever GetStream is called the stream's position is reset to the start.
bool LeoUtils::CFileAndStream::GetStream(IStream **ppStream)
{
	bool bResult = false;

	if (m_pStream != NULL)
	{
		if (m_bNoRandomSeekInStream)
		{
			if (WriteStreamToTempFile() && m_pStream != NULL)
			{
				*ppStream = m_pStream;
				bResult = true;
			}
		}
		else
		{
			*ppStream = m_pStream;
			bResult = true;
		}
	 }
	 else if (PrepareFileForReading())
	 {
		m_bNoRandomSeekInStream = false;

		if (S_OK != SHCreateStreamOnFile(m_strFilePath.c_str(), STGM_READ|STGM_SHARE_DENY_NONE, &m_pStream)
		||	m_pStream == NULL)
		{
			m_pStream = NULL;
		}
		else
		{
			m_bStreamIsFile = true;

			*ppStream = m_pStream;
			bResult = true;
		}
	}

	if (bResult)
	{
		LARGE_INTEGER liZero;
		liZero.QuadPart = 0;

		if (S_OK != m_pStream->Seek(liZero, STREAM_SEEK_SET, NULL))
		{
			bResult = false;
		}
	 }

	return bResult;
}

bool LeoUtils::CFileAndStream::PrepareFileForReading()
{
	bool bResult = false;

	if (!m_strFileName.empty()
	&&	!m_strFilePath.empty())
	{
		if (!m_bOpenTempCopyFilePath || m_bDeleteFilePathOnClose)
		{
			bResult = true;
		}
		else
		{
			// Copy the file to a temporary file and refer to that from now on.

			std::basic_string< TCHAR > strTempFilePath;

			HANDLE hTempFile = OpenTempFileNamePreserveExtension(NULL, _T("dnvt"), m_strFileName.c_str(), &strTempFilePath, false);

			if (hTempFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hTempFile);

				if (!LeoUtils::CopyFile(m_strFilePath.c_str(), strTempFilePath.c_str(), m_hAbortEvent, true, false))
				{
					DeleteFile(strTempFilePath.c_str());
				}
				else
				{
					m_bOpenTempCopyFilePath = false;
					m_bDeleteFilePathOnClose = true;
					m_strFilePath = strTempFilePath;

					bResult = true;
				}
			}
		}
	}

	return bResult;
}

bool LeoUtils::CFileAndStream::WriteStreamToTempFile()
{
	bool bResult = false;

	if (m_pStream != NULL && !m_strFileName.empty())
	{
		std::basic_string< TCHAR > strTempFilePath;

		HANDLE hTempFile = OpenTempFileNamePreserveExtension(NULL, _T("dnvt"), m_strFileName.c_str(), &strTempFilePath, false);

		if (hTempFile != INVALID_HANDLE_VALUE)
		{
			bool bWriteError = false;
			DWORD dwSize;
			BYTE bBuf[8192];

			LARGE_INTEGER liZero;
			ULARGE_INTEGER uliPrevious;

			liZero.QuadPart = 0;
			uliPrevious.QuadPart = 0;

			if (!m_bNoRandomSeekInStream)
			{
				m_pStream->Seek(liZero, STREAM_SEEK_CUR, &uliPrevious);
				m_pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
			}

			while (S_OK == m_pStream->Read(bBuf, sizeof(bBuf), &dwSize))
			{
				if (0 == dwSize)
				{
					// Some IStream implementations return S_OK and set dwSize to zero to indicate
					// the end of the stream. (Some use S_FALSE instead.)
					break;
				}

				if (0 < dwSize)
				{
					DWORD dwTemp = 0;

					if (0 == WriteFile(hTempFile,bBuf,dwSize,&dwTemp,0))
					{
						bWriteError = true;
						break;
					}
				}
			}

			if (!m_bNoRandomSeekInStream)
			{
				// When using STREAM_SEEK_SET the first argument to Seek is treated as unsigned.

				LARGE_INTEGER liPrevious;
				liPrevious.LowPart  = uliPrevious.LowPart;
				liPrevious.HighPart = uliPrevious.HighPart;

				m_pStream->Seek(liPrevious, STREAM_SEEK_SET, NULL);
			}

			// Close temporary file
			CloseHandle(hTempFile);

			if (bWriteError)
			{
				DeleteFile(strTempFilePath.c_str());
			}
			else
			{
				m_bOpenTempCopyFilePath = false;
				m_bDeleteFilePathOnClose = true;
				m_strFilePath = strTempFilePath;

				m_pStream->Release();
				m_bStreamIsFile = false;
				m_bNoRandomSeekInStream = false;

				if (S_OK != SHCreateStreamOnFile(m_strFilePath.c_str(), STGM_READ|STGM_SHARE_DENY_NONE, &m_pStream))
				{
					m_pStream = NULL;
				}
				else
				{
					m_bStreamIsFile = true;
					bResult = true;
				}
			}
		}
	}

	return bResult;
}
