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

#pragma once

#define MAKE64BITVERSIONNUMBER(a,b,c,d) ((((DWORD64)(a))<<48) + (((DWORD64)(b))<<32) + (((DWORD64)(c))<<16) + (((DWORD64)(d))<<00))

struct LeoUtils
{
	// delete[] the result.
	// Returns NULL on failure.
	static TCHAR *StringAllocAndFormat(const TCHAR *format, ...);

	// delete[] the result.
	// Returns NULL on failure.
	static TCHAR *VStringAllocAndFormat(const TCHAR *format, va_list args);

	static bool CopyFile(const TCHAR *szSource, const TCHAR *szDest, HANDLE hAbortEvent, bool bClearReadOnlyAttrib, bool bFailIfExists);

	// If initialisation fails after the call to CopyVersionResourceToViewerPluginInfo then the lpVPInfo->hIconSmall icon must be destroyed.
	static bool CopyVersionResourceToViewerPluginInfo(HMODULE g_hDllModule, WORD iconId, LPVIEWERPLUGININFO lpVPInfo);
	static bool CopyVersionNumber(DWORD *pdwHigh, DWORD *pdwLow, TCHAR *szItemName, BYTE *pLang, const LPVOID pVerData);
	static bool CopyVersionString(TCHAR *szDestBuffer, UINT uiDestBufferSize, TCHAR *szItemName, BYTE *pLang, const LPVOID pVerData);

	static const TCHAR *GetLastPathPart(const TCHAR *szIn);
	static const TCHAR *GetExtensionPart(bool bIncludeDot, const TCHAR *szIn);

	static bool GetTempPath(std::basic_string< TCHAR > *pstrTempPath);

	// If bDontReallyOpen is true then the function won't really create a file and always returns INVALID_HANDLE_VALUE, but will still set *pstrFilenameOut to a name that is similar to what you'd get normally.
	// On failure *pstrFilenameOut will be left empty so you can check that when bDontReallyOpen is true.
	// If you set bDontReallyOpen then you absolutely must not create a file with the generated path because such a file may already exist and be used for other purposes. The generated path should only be used as a sample, for example if you want to test whether calling this function for real will result in a path that contains any non-ASCII characters.
	static HANDLE OpenTempFileNamePreserveExtension(const TCHAR *szTempPath, const TCHAR *szPrefix, const TCHAR *szFilenameIn, std::basic_string< TCHAR > *pstrFilenameOut, bool bDontReallyOpen);

	class CFileAndStream
	{
	public:
		// hAbortEvent can be null.
		CFileAndStream(const wchar_t *szFilePath, HANDLE hAbortEvent, bool bOpenTempCopy);

		// pStream can be NULL but only if you are never going to call methods which read data or get the steam or file path.
		CFileAndStream(const wchar_t *szFileName, IStream *pStream, bool bNoRandomSeek);

		CFileAndStream(const wchar_t *szFileName, const BYTE *pData, UINT cbDataSize);

		~CFileAndStream(); // Warning: Non-virtual destructor.

		// This allows you to set the OpenTempCopy flag after construction.
		// Any subsequent call to GetFilePath will result in the file being copied to a temp path if
		// it had not already been done. Any preceeding call to GetFilePath may, of course, have alredy
		// received the real filename. If the data is in a stream then calling this has no effect as
		// GetFilePath will always cause the data to be saved to a temp file regardless of the flag.
		void SetOpenTempCopy(bool bOpenTemp);

		// The file name may not match the name of the actual file. It is indicative of the original file's name
		// but the file path returned by GetFilePath may be a temp-file with a different name. GetFileName should
		// be used for display purposes and for any logic which depends on the original file's name.
		// Calling GetFileName will not trigger any files or streams to be created.
		bool GetFileName(std::basic_string< TCHAR > *pstrFileName);
		bool GetFileExtension(std::basic_string< TCHAR > *pstrFileExtension, bool bIncludeDot);

		// GetNominalFilePath gets a full path that will be similar to, but may not be the same as,
		// the file path returned by GetFilePath. You can use this to test for non-ASCII characters
		// in the file path without triggering a stream to be written to a file as you would if you
		// called GetFilePath.
		bool GetNominalFilePath(std::basic_string< TCHAR > *pstrNominalFilePath);
		bool GetNominalTempFilePath(std::basic_string< TCHAR > *pstrNominalTempFilePath);

		bool HasFilePath();
		bool HasStream();

		// These calls will cause a file or stream to be created if there isn't one already.
		// Call HasFilePath and HasStream to see what already exists if you can work with both and want
		// to avoid the overhead of conversion.

		bool GetFilePath(std::basic_string< TCHAR > *pstrFilePath);

		// The stream will not be AddRef'd by this call. It will exist as long as the CFileAndSteam does
		// so you should not normally need to AddRef it.
		// If a stream is returned it will always be seekable.
		// Whenever GetStream is called the stream's position is reset to the start.
		bool GetStream(IStream **ppStream);

	private:
		bool PrepareFileForReading();
		bool WriteStreamToTempFile();

	private:
		std::wstring m_strFileName;

		std::wstring m_strFilePath;
		bool m_bDeleteFilePathOnClose;
		bool m_bOpenTempCopyFilePath;

		IStream *m_pStream;
		bool m_bNoRandomSeekInStream;

		bool m_bStreamIsFile;

		HANDLE m_hAbortEvent;
	};

};
