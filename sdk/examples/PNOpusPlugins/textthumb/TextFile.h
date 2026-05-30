#pragma once

class CBaseTextFile
{
public:
	CBaseTextFile() {}
	virtual ~CBaseTextFile() {}

	bool ReadLines(std::wstring *pwstrText, int iLines, unsigned int iMaxLength, WCHAR wcSeparator, bool bSkipSpaces, bool bSkipBlankLines, bool bAlwaysTruncate, DWORD dwCodePage);

	virtual bool GetByteCount(ULARGE_INTEGER *puli) = 0;

	static const unsigned int m_iMaxLength = 8192; // Max DrawText input size on Win95/98/ME and a reasonable place to stop anyway.

protected:
	virtual bool open() = 0;
	virtual bool read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *ulBytesRead) = 0;
	virtual bool close() = 0;
};

class CFileTextFile : public CBaseTextFile
{
public:
#ifdef UNICODE
	CFileTextFile(const WCHAR *wszFilename) : m_wstrFilename(wszFilename), m_hFile(INVALID_HANDLE_VALUE) {}
#else
	CFileTextFile(const char *szFilename) : m_strFilename(szFilename), m_hFile(INVALID_HANDLE_VALUE) {}
#endif
	virtual ~CFileTextFile() {}

	virtual bool GetByteCount(ULARGE_INTEGER *puli);

protected:
#ifdef UNICODE
	std::wstring m_wstrFilename;
#else
	std::string m_strFilename;
#endif
	HANDLE m_hFile;

protected:
	virtual bool open();
	virtual bool read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *pulBytesRead);
	virtual bool close();
};

class CStreamTextFile : public CBaseTextFile
{
public:
	CStreamTextFile(LPSTREAM pStream) : m_pStream(pStream) {}
	virtual ~CStreamTextFile() {}

	virtual bool GetByteCount(ULARGE_INTEGER *puli);

protected:
	LPSTREAM m_pStream;

protected:
	virtual bool open();
	virtual bool read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *ulBytesRead);
	virtual bool close();
};


class CMemoryTextFile : public CBaseTextFile
{
public:
	CMemoryTextFile(const BYTE *pBuffer, ULONG ulBufferSize) : m_pBuffer(pBuffer), m_ulBufferSize(ulBufferSize), m_i(0) {}
	virtual ~CMemoryTextFile() {}

	virtual bool GetByteCount(ULARGE_INTEGER *puli);

protected:
	const BYTE *m_pBuffer;
	ULONG m_ulBufferSize;
	ULONG m_i;

protected:
	virtual bool open();
	virtual bool read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *pulBytesRead);
	virtual bool close();
};
