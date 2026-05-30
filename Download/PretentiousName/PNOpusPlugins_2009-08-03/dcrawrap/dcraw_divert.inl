// This should be included into dcraw.cpp directly rather than compiled by itself.

// This file is not part of the original DCRaw code; it's part of
// the changes by Leo Davidson to turn DCRaw into thread-safe C++
// for the Win32 platform. The aim is to move static/global data
// into instance members and to divert calls to CRT I/O functions
// to our own replacements. The replacements handle things like
// UTF-16 filenames (which we substitute for the dummy names that
// we pass the original code) which are important for Win32. The
// replacement functions also allow us to record all open files
// and allocated memory so that on error we can clean-up. (The
// original code assumes the program will exit and clean itself
// up but I wish to use it in a process that is long-lived and
// multi-threaded.) An important aim of replacing the functions
// in this way is to modify the original code as litle as possible.
// (Otherwise it takes a lot of manual effort to merge changes.)

#ifndef __cplusplus
#error Leo Davidson's dcraw_divert.inl only makes sense for C++ projects.
#endif


#ifdef _DEBUG
#ifdef WIN32

#define OUTPUT_DCRAW_DIVERT_DEBUG
//#define OUTPUT_DCRAW_PRINT_DEBUG

#endif
#endif


// Leo Davidson 04/Sep/2008: Changes to make compilation smoother with Microsoft Visual Studio 2008.
#ifdef _MSC_VER
// If we're using Microsoft's compiler then turn off lots of warnings so we can get a clean build of this code.
// In the past I added casts to fix this type of stuff but doing that really just adds a lot of noise to the diff vs the original code
// and it had the potential to introduce errors if I misinterpreted Dave's code. I've still fixed one or two things but I'm aiming to
// change as little as possible these days.
#define _CRT_SECURE_NO_WARNINGS // Warnings about strcpy vs strcpy_s and so on.
#pragma warning(disable:4996) // 'swab': The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name: _swab. See online help for details.
#pragma warning(disable:4018) // '<' : signed/unsigned mismatch [and similar for other operators]
#pragma warning(disable:4305) // 'initializing' : truncation from 'double' to 'float' [and similar for other type pairs]
#pragma warning(disable:4244) // '=' : conversion from 'double' to 'int', possible loss of data [and similar for UINT64->unsigned int and other type pairs]
#pragma warning(disable:4267) // '=' : conversion from 'size_t' to 'unsigned int', possible loss of data
#pragma warning(disable:4101) // 'len' : unreferenced local variable [which happens if you define the NO_BADPIXEL_FILE_SEARCH thing I added]
#ifndef __cplusplus
#pragma warning(disable:4133) // 'function' : incompatible types - from 'UshORt *' to 'char *' [due to swab() being called on the first 2 bytes of a 16-bit buffer]
#else
#pragma warning(disable:4309) // 'initializing' : truncation of constant value [due to signed char values being initialised with constants like 0x80]
#endif
#ifndef LOCALTIME
#pragma message("***********************************************************************************************************************************")
#pragma message("***********************************************************************************************************************************")
#pragma message("***********************************************************************************************************************************")
#pragma message("***                                                                                                                             ***")
#pragma message("*** LOCALTIME is not defined. Beware that the code will set the env var TZ=UTC which may cause problems when used as a library. ***")
#pragma message("***                                                                                                                             ***")
#pragma message("***********************************************************************************************************************************")
#pragma message("***********************************************************************************************************************************")
#pragma message("***********************************************************************************************************************************")
#endif // LOCALTIME
#endif // _MSC_VER

// Leo Davidson 04/Sep/2008: Changed from strnicmp to _strnicmp to avoid warning with newer versions of Visual Studio, if we're using MSVC.
#ifdef WIN32

#ifdef _MSC_VER
#undef strncasecmp
#define strncasecmp _strnicmp
#else
#define strncasecmp strnicmp
#endif

typedef __int64 INT64;
typedef unsigned __int64 UINT64;

// Leo Davidson 04/Sep/2008: Use fseek and ftell for fseeko and ftell, like when DJGPP is defined. (Previously these were left as fseeko and ftello which MSVC doesn't know about.)
#ifdef _MSC_VER
#define fseeko fseek
#define ftello ftell
// Note: MSVC has _fseeki64 and _ftelli64 but I'm not using them because:
// 1) fseeko is only used in one place where it's given an off_t argument in SEEK_SET mode.
//    As far as I can tell off_t is always 32-bit, even with a WIN64 compile, and SEEK_SET means
//    the argument is treated as unsigned, so using _fseeki64 instead of fseek would be pointless.
//    I'm surprised that off_t doesn't grow to 64-bits like size_t does with WIN64 but it doesn't
//    and it looks like making it do so would cause problems in <wchar.h>.
// 2) ftello is only used in one place where the current file position is output as a debug string.
//    As I doubt the rest of the code copes with files >2gig (see 1) and I've never seen a Raw image
//    that big, it doesn't seem worth making people provide a 64-bit tell function in their IO
//    wrappers just for this.
//#define fseeko _fseeki64
//#define ftello _ftelli64
#endif
#endif

#include <string>
#include <map>
#include <set>
#include <vector>

// Leo Davidson 04/Sep/2008: Since C++ is more strict about types we need to overload the swab function. If we're compiling as C then this isn't needed.
inline void swab (UshORt *buf1, UshORt *buf2, int sizeInBytes)
{
	swab((char *)buf1, (char *)buf2, sizeInBytes);
}

inline void swab (uchar *buf1, uchar *buf2, int sizeInBytes)
{
	swab((char *)buf1, (char *)buf2, sizeInBytes);
}

// Leo Davidson 04/Sep/2008: When compiling as C++ we need to ensure that only the C-style "double pow(double,double)" and "double sqrt(double)" are used.
#define pow powl
#define sqrt sqrtl

// Leo Davidson 04/Sep/2008: Replace setjmp/longjmp with C++ exceptions.
#undef longjmp
#undef setjmp
#define longjmp(x,y) throw("")
#define setjmp(x) 0

// Leo Davidson 04/Sep/2008: Divert malloc, calloc and free so that we can clean-up any mess.
// This is needed as there are lots of error states where the code leaks memory (since it's
// written to assume the program will then exit and the OS will clean up for it).

// Leo Davidson 04/Sep/2008: Divert stdio so that:
// (a) All errors are detected (the original code has lots of unchecked fseek calls).
// (b) Writes to stdout and stderr can be captured if desired. (Not implemented at the time of writing.)
// (c) We can close & delete any files left around after an error.
// (d) We can handle UTF-16 paths without modifying the code, using a substitution map.

#ifdef _CRTDBG_MAP_ALLOC
#error Something has defined _CRTDBG_MAP_ALLOC which may break our function diverts. Needs investigation.
#endif

class DCRawDivert
{
private:
	// Memory allocations that we need to free.
	std::set< void * > m_setToFree;

	// Files that we need to close and possibly delete.
	std::set< FILE * > m_setToClose;
	std::set< std::wstring > m_setToDelete;

	 // This exists to let us substitute wchar_t paths for char paths without having to change all the original code to use whcar_t.
	std::map< std::string, std::wstring > m_mapFilePathOverrides;

	// This exists to support parse_external_jpeg() in DCRaw.cpp. If DCRaw tries to open a file and the full path isn't in
	// m_mapFilePathOverrides then we will see if the parent directory's path is in m_mapDirectoryPathOverrides; if it is then
	// we will attempt to load a file of the same name (which is generated in parse_external_jpeg()) in the folder which the
	// map points to. Note that m_mapDirectoryPathOverrides may be empty. For example, if the Opus plugin is trying to view a
	// raw image from a zip file then Opus will have given the plugin an IStream which it will have written to a temp file and
	// that temp file is what gets passed to the DCRaw code. If the DCRaw code then asks for another file in the same directory
	// (i.e. the zip file), the plugin (currently) has no way of asking Opus to get that out of the zip and the input path
	// (which is really just a temp directory with only one relevant file in it) will not be in m_mapDirectoryPathOverrides.
	// Support for this could probably be added to Opus but it doesn't seem worth it unless another plugin, and a more likely
	// scenario, requires it.
	std::map< std::string, std::wstring > m_mapDirectoryPathOverrides;

	// Data we collect about the file.
//	std::string m_strVersion;
	std::string m_strMake;
	std::string m_strModel;
	int m_iWidth;
	int m_iHeight;
	int m_iFlip;

#ifdef WIN32
	HANDLE m_hAbortEvent;
	CRITICAL_SECTION *m_pCritSecTempFOpen;
#endif

public:
#ifdef WIN32
	DCRawDivert(HANDLE hAbortEvent, CRITICAL_SECTION *pCritSecTempFOpen)
	: m_hAbortEvent(hAbortEvent)
	, m_pCritSecTempFOpen(pCritSecTempFOpen)
	, m_iWidth(0)
	, m_iHeight(0)
	, m_iFlip(0)
	{
	}
#else
	DCRawDivert()
	{
	}
#endif

	virtual ~DCRawDivert()
	{
		freeAllMemory();
		closeAllFiles();
		deleteOutputFiles();
	}

	void AddFilePathOverride(const char *szFakePath, const wchar_t *szRealPath)
	{
		m_mapFilePathOverrides.insert( std::make_pair< std::string, std::wstring >( szFakePath, szRealPath ) );
	}

	void AddDirectoryPathOverride(const char *szFakePath, const wchar_t *szRealPath)
	{
		m_mapDirectoryPathOverrides.insert( std::make_pair< std::string, std::wstring >( szFakePath, szRealPath ) );
	}

//	const std::string &GetVersion()    const { return m_strVersion; }
	const std::string &GetMake()       const { return m_strMake; }
	const std::string &GetModel()      const { return m_strModel; }
	int                GetWidth()      const { return m_iWidth; }
	int                GetHeight()     const { return m_iHeight; }
//	int                GetFlip()       const { return m_iFlip; }

	int GetRotation() const
	{
		switch(m_iFlip)
		{
		default: return 0;
		case 5:  return 270;
		case 3:  return 180;
		case 6:  return 90;
		}
	}

	bool GetOutputFilePath(std::wstring *pOutPath) const
	{
		if (m_setToDelete.size() != 1)
		{
			assert(false);
			pOutPath->clear();
			return false;
		}

		*pOutPath = *m_setToDelete.begin();
		return (!pOutPath->empty());
	}

private:
	DCRawDivert(const DCRawDivert &rhs); // Disallow
	DCRawDivert &operator=(const DCRawDivert &rhs); // Disallow

	// Unused and undefined. Just here so there is a compiler error if any of them are used by the main code in the future.
	void *realloc(void * _Memory, size_t _NewSize);
	int remove(const char *_Filename);
#undef unlink
	int unlink(const char *_Filename);
	int _unlink(const char *_Filename);
	int rmtmp(void); 
	int _rmtmp(void); 
	int fcloseall(void);
	int _fcloseall(void);
	FILE *freopen(const char *_Filename, const char *_Mode, FILE *_File);
	FILE *fsopen(const char *_Filename, const char *_Mode, int _ShFlag);
	FILE *_fsopen(const char *_Filename, const char *_Mode, int _ShFlag);
	int setmaxstdio(int _Max);
	int _setmaxstdio(int _Max);
	char *tempnam(const char *dir, const char *prefix);
	char *_tempnam(const char *dir, const char *prefix);
	char *tmpnam(char *_Buffer);
	int fputs(const char *str, FILE *stream);

protected:
	inline void *malloc(size_t _Size)
	{
		void *p = ::malloc(_Size);

		if (!p) { throw(""); }
		
		m_setToFree.insert(p);
		return p;
	}

	inline void *calloc(size_t _NumOfElements, size_t _SizeOfElements)
	{
		void *p = ::calloc(_NumOfElements, _SizeOfElements);

		if (!p) { throw(""); }
		
		m_setToFree.insert(p);
		return p;
	}

	inline void free(void * _Memory)
	{
		if (_Memory)
		{
			m_setToFree.erase(_Memory);
			::free(_Memory);
		}
	}

	void freeAllMemory()
	{
		for (std::set< void * >::const_iterator memIter = m_setToFree.begin(); memIter != m_setToFree.end(); ++memIter)
		{
			::free(*memIter);
		}
		m_setToFree.clear();
	}

	void closeAllFiles()
	{
		for (std::set< FILE * >::const_iterator fileIter = m_setToClose.begin(); fileIter != m_setToClose.end(); ++fileIter)
		{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
			OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"closeAllFiles", L"Closing \"%p\"", *fileIter);
#endif
			::fclose(*fileIter);
		}
		m_setToClose.clear();
	}

	void deleteOutputFiles()
	{
		for (std::set< std::wstring >::const_iterator pathIter = m_setToDelete.begin(); pathIter != m_setToDelete.end(); ++pathIter)
		{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
			OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"deleteOutputFiles", L"Deleting \"%s\"", pathIter->c_str());
#endif
			::_wremove(pathIter->c_str());
		}
		m_setToDelete.clear();
	}

	FILE *fopen(const char *_Filename, const char *_Mode)
	{
		// Validate the inputs. At the time of writing the dcraw code never uses append mode.
		// I could arbitrarily include appended files into the deletion map on the assumption they
		// will be outputs of some kind, if they're ever used, but it seems safer to reject any
		// calls using append mode to force a proper decision when and if such code appears.

		if (_Filename == NULL || _Mode == NULL || strchr(_Mode, 'a') != NULL)
		{
			assert(false);
			throw("");
		}

#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
		OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fopen", L"Inputs: \"%S\", \"%S\"", _Filename, _Mode);
#endif

		// At the time of writing the only output file opened via fopen is the main output
		// file. (There's also a temp file DCRaw may create but that goes via tmpfile().)
		// So if mode 'w' is set, divert the output to the temp directory instead of the input
		// directory.
		if (strchr(_Mode, 'w') != NULL)
		{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
			OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fopen", L"Output file so diverting to OpenTempFile");
#endif
#ifndef WIN32
#error It you want output file redirection do something appropriate here. --Leo
#else
			FILE *pFileResult = NULL;

			const char *ascExtension = GetExtensionPartA(true, _Filename);

			assert(ascExtension);

			if (ascExtension)
			{
				wchar_t *wideExtension = DCRawDivert::MBtoWC(ascExtension);
				wchar_t *wideMode = DCRawDivert::MBtoWC(_Mode);

				assert(wideExtension && wideMode);

				if (wideExtension && wideMode)
				{
					pFileResult = OpenTempFile(L"rawout", wideExtension, wideMode);
				}

				delete[] wideExtension;
				delete[] wideMode;
			}

			return pFileResult;
#endif
		}

		// See if the filename is in the substitution map. If it isn't convert the input to UFT-16.
		std::map< std::string, std::wstring >::const_iterator iterPathOver = \
			m_mapFilePathOverrides.find(_Filename);

		std::wstring utfString;
		wchar_t *utfBuffer = NULL;
		const wchar_t *pathToOpen = NULL;

		if (iterPathOver != m_mapFilePathOverrides.end())
		{
			// Full file path is in the map, so substitute the whole thing.
			pathToOpen = iterPathOver->second.c_str();
		}

		if (pathToOpen == NULL && !m_mapDirectoryPathOverrides.empty())
		{
			// See if the file is in a directory that's in m_mapDirectoryPathOverrides;
			// if it is then look for the same filename in the place the map points to.

			std::string strParentIn;
			GetParentPathStringA(&strParentIn, _Filename);

			if (!strParentIn.empty())
			{
				std::map< std::string, std::wstring >::const_iterator iterParentOver = \
					m_mapDirectoryPathOverrides.find(strParentIn);

				if (iterParentOver != m_mapDirectoryPathOverrides.end())
				{
					wchar_t *utfNameOnly = MBtoWC(GetLastPathPartA(_Filename));

					utfString = iterParentOver->second;
					AppendPathString(&utfString, utfNameOnly);
					pathToOpen = utfString.c_str();

					delete[] utfNameOnly;
				}
			}
		}

		assert(pathToOpen != NULL); // I don't think this should ever happen. If it does I want to know about it in debug builds.

		if (pathToOpen == NULL)
		{
			utfBuffer = DCRawDivert::MBtoWC(_Filename);
			pathToOpen = utfBuffer;
		}

		wchar_t *wideMode = DCRawDivert::MBtoWC(_Mode);

		FILE *pFileResult = NULL;

#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
		OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fopen", L"Diverted to \"%s\", \"%s\"", pathToOpen, wideMode);
#endif

		if (pathToOpen == NULL
		||	wideMode == NULL
		||	0 != ::_wfopen_s(&pFileResult, pathToOpen, wideMode)
		||	pFileResult == NULL)
		{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
			OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fopen", L"Failed");
#endif

			pFileResult = NULL;
		}
		else
		{
			m_setToClose.insert(pFileResult);

			// At the moment all output files get diverted to OpenFileTemp before this point.
			assert(strchr(_Mode, 'w') == NULL && strchr(_Mode, 'D') == NULL);
		}

		delete[] utfBuffer;
		delete[] wideMode;

		// Don't throw an exception if fopen fails. All the code paths in DCRaw check the result.

		return pFileResult;
	}

	int fclose(FILE *_File)
	{
		m_setToClose.erase(_File);
		return ::fclose(_File);
	}

#ifndef WIN32
	FILE *tmpfile(void)
	{
		FILE *pFileResult = tmpfile();
	}
#else
	FILE *tmpfile(void)
	{
		// On Win32 with the Visual Studio CRT, tmpfile() doesn't really work because it tries to create
		// the temp file in the root (i.e. C:\) which most users don't have permission to do (and which
		// creates a nasty mess anyway).

		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}

		// Mode D means it'll be deleted when closed, like tmpfile should, so we don't need to put the name in the delete map.
		return OpenTempFile(L"rawdiv", L".tmp", L"w+bD");
	}

	FILE *OpenTempFile(const wchar_t *szBaseName, const wchar_t *szExtension, const wchar_t *szMode)
	{
		FILE *pFileResult = NULL;

		std::wstring strTempFilePath;
		std::wstring strFileStart;

		if (!DCRawDivert::GetTempPath(&strFileStart))
		{
			throw("");
		}

		if (strFileStart.length() > 0 && '\\' != *(strFileStart.rbegin()) && '/' != *(strFileStart.rbegin()))
		{
			strFileStart += '\\';
		}

		strFileStart += szBaseName;

		SYSTEMTIME systime;
		::GetSystemTime(&systime);

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

			wchar_t *szFilepath = DCRawDivert::StringAllocAndFormatW(
				L"%s%04d%02d%02d%02d%02d%02d%04d%s",
				strFileStart.c_str(), i1,i2,i3,i4,i5,i6,i7, szExtension);

			if (NULL == szFilepath)
			{
				break;
			}
			else
			{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
				OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"OpenTempFile", L"Trying \"%s\", \"%s\"", szFilepath, szMode);
#endif

				errno_t opRes = 1;
				{
					// Use a critical section here because there's no way to ensure fopen (or _wfopen_s in our case)
					// creates a file. If one thread just created and closed a temp file, but hasn't deleted it yet,
					// then another thread can open that same temp-file and overwrite it with a different image.
					// Assumption: Nothing else is going to write temp files with the same prefix as us.
					CriticalSectionScoper css(m_pCritSecTempFOpen);

					// Only try to open the file if it doesn't exist.
					if (INVALID_FILE_ATTRIBUTES == ::GetFileAttributes(szFilepath))
					{
						opRes = ::_wfopen_s(&pFileResult, szFilepath, szMode);
					}
				}

				if (0 != opRes || pFileResult == NULL)
				{
					opRes = 1;
					pFileResult = NULL;
				}
				else
				{
					// Need to close it regardless of mode.
					m_setToClose.insert(pFileResult);

					// If it's an output file and not already set to auto-delete on closure then we need to delete it, too.
					if (wcschr(szMode, L'w') != NULL
					&&	wcschr(szMode, L'D') == NULL)
					{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
						OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"OpenTempFile", L"Succeeded and put in delete set");
#endif
						m_setToDelete.insert(szFilepath);
					}
					else
					{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
						OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"OpenTempFile", L"Succeeded (delete not needed)");
#endif
					}
				}

				delete [] szFilepath;

				if (0 == opRes) 
				{
					break;
				}
			}
		}
#endif // WIN32

		// Don't throw an exception if fopen or tmpfile fails. All the code paths in DCRaw check the result.

		return pFileResult;
	}

	int fseek(FILE * _File, long _Offset, int _Origin, bool bThrowOnError=true)
	{
		int result = ::fseek(_File, _Offset, _Origin);

		if (result != 0 && bThrowOnError)
		{
			throw("");
		}

		return result;
	}

	long ftell(FILE * _File)
	{
		long result = ::ftell(_File);

		if (result < 0)
		{
			throw("");
		}

		return result;
	}

	size_t fread(void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		size_t result = ::fread(_DstBuf, _ElementSize, _Count, _File);

		if (result != _Count)
		{
			throw("");
		}

		return result;
	}

	size_t fwrite(const void * _Str, size_t _Size, size_t _Count, FILE * _File)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		size_t result = ::fwrite(_Str, _Size, _Count, _File);

		if (result != _Count)
		{
			throw("");
		}

		return result;
	}
	
	char * fgets(char * _Buf, int _MaxCount, FILE * _File, bool bThrowOnError=true)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		char *result = ::fgets(_Buf, _MaxCount, _File);

		if (result == NULL && bThrowOnError)
		{
			throw("");
		}

		return result;

	}

	int fgetc(FILE * _File)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		int result = ::fgetc(_File);

		if (result == EOF)
		{
			throw("");
		}

		return result;
	}

	int fputc(int _Ch, FILE * _File)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		int result = ::fputc(_Ch, _File);

		if (result == EOF)
		{
			throw("");
		}

		return result;
	}

#ifdef vfscanf

#ifdef _Scanf_format_string_
	int fscanf(FILE * _File, _Scanf_format_string_ const char * _Format, ...)
#else
	int fscanf(FILE * _File, const char * _Format, ...)
#endif
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent))
		{
			throw("");
		}
#endif

		int result = EOF;

		va_list args;
		va_start(args, _Format);
		
		result = ::vfscanf(_File, _Format, args);

		va_end(args);

		if (result <= 0)
		{
			throw("");
		}

		return result;
	}

#else

	// Visual Studio 2008 doesnt't provide a vfscanf function. Luckily, the DCRaw code
	// (at least at the time of writing), only ever scans for one thing at a time,
	// so we don't actualy need vfscanf. :)

	int fscanf(FILE * _File, const char * _Format, void *pv)
	{
#ifdef WIN32
		if (m_hAbortEvent && WAIT_TIMEOUT!=WaitForSingleObject(m_hAbortEvent,0))
		{
			throw("");
		}
#endif

		int result = EOF;

		result = ::fscanf(_File, _Format, pv);

		if (result <= 0)
		{
			throw("");
		}

		return result;
	}

#endif

	int fprintf(FILE * _File, const char * _Format, ...)
	{
		int result = -1;

		va_list args;
		va_start(args, _Format);

		if (_File != stdout && _File != stderr)
		{
			result = ::vfprintf(_File, _Format, args);

			if (result < 0)
			{
				throw("");
			}
		}
		else
		{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
#ifdef OUTPUT_DCRAW_PRINT_DEBUG
			char *szAscii = VStringAllocAndFormatA(_Format, args);
			if (szAscii != NULL)
			{
				OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fprintf", L"%S", szAscii);
				delete[] szAscii;
			}
#endif
#endif

			if (0 == strcmp(_Format, "Cannot decode file %s\n"))
			{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
				OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"fprintf", L"*** throwing ***");
#endif
				throw("");
			}

			result = 1;
		}

		va_end(args);

		return result;
	}

	int printf(const char *_Format, ...)
	{
		int result;

		va_list args;
		va_start(args, _Format);

#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
#ifdef OUTPUT_DCRAW_PRINT_DEBUG
		char *szAscii = VStringAllocAndFormatA(_Format, args);
		if (szAscii != NULL)
		{
			OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"printf", L"%S", szAscii);
			delete[] szAscii;
		}
#endif
#endif

		if (0 == strcmp(_Format, "Output size: %4d x %d\n"))
		{
			m_iWidth = va_arg(args,int);
			m_iHeight = va_arg(args,int);
		}
		else if (0 == strcmp(_Format, "Camera: %s %s\n"))
		{
			m_strMake = va_arg(args,char *);
			m_strModel = va_arg(args,char *);
		}
		else if (0 == strcmp(_Format, "Flip: %d\n")) // Flip isn't output by the original DCRaw code; I added it.
		{
			m_iFlip = va_arg(args,int);
		}
//		else if (0 == strcmp(_Format, "\nRaw photo decoder \"dcraw\" v%s"))
//		{
//			m_strVersion = va_args(args,char *);
//		}

		va_end(args);

		result = 1;

		return result;
	}

	void perror(const char *_ErrMsg)
	{
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
#ifdef OUTPUT_DCRAW_PRINT_DEBUG
		OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"perror", L"%S", _ErrMsg);
#endif
#endif
	}

	int puts(const char *_Str)
	{
		// Reminder: The real puts would add a return to the end of the string.
#ifdef OUTPUT_DCRAW_DIVERT_DEBUG
#ifdef OUTPUT_DCRAW_PRINT_DEBUG
		OutputDebugFormat(L"[DCRaw] ", L"DCRawDivert", L"puts", L"%S", _Str);
#endif
#endif
		return 1;
	}

#ifndef isatty
	int isatty(int fd)
	{
		throw(""); // This even being called does not make sense in C++ mode with captured (or not) stdout.
		return 0;
	}
#endif

#ifndef setmode
	int setmode(int fd, int mode)
	{
		throw(""); // This even being called does not make sense in C++ mode with captured (or not) stdout.
		return -1;
	}
#endif

	// Leo Davidson 04/Sep/2008: Critical section and lock count around LCMS code.
	// I'm not certain this critical section is needed but at least
	// some parts of LCMS are not thread safe. From Marti Maria:
	//
	// lcms itself is thread safe under certain conditions:
	// - You have to use a different color transform on each thread.
	// - Functions that retrieve info, like cmsTakeDescription and so are not thread safe
	// - Some functions affects global settings, like cmsSetAlarmCodes().
	//
	// While I'm not certain if that applies to the usage in DCRaw, it doesn't really
	// hurt to protect it in a CS just in case and the majority of execution time will
	// be spent doing other things, leaving lots of scope for two decoders to run in parallel.
#ifndef WIN32
#error You may need to provide some kind of critical section or mutex for your platform around the LCMS code. I don't know how this is done on other platforms, sorry! --Leo
#else
	class CriticalSectionScoper
	{
	public:
		CriticalSectionScoper(CRITICAL_SECTION *pCS) : m_pCS(pCS) { assert(m_pCS != NULL); if (m_pCS != NULL) { EnterCriticalSection(m_pCS); } }
		/*not virtual*/ ~CriticalSectionScoper()                  { if (m_pCS != NULL) { LeaveCriticalSection(m_pCS); } }
	private:
		CriticalSectionScoper(const CriticalSectionScoper &rhs);
		CriticalSectionScoper &operator=(const CriticalSectionScoper &rhs);
	private:
		CRITICAL_SECTION *m_pCS;
	};

	CRITICAL_SECTION *pCritSecLcms;
#endif

private:

#ifndef WIN32
#error Please define an ASCII-to-UFT16 function for your OS, or remove the m_mapFilePathOverrides stuff if you don't need UTF16 at all.
#else
	// Returns NULL on failure (use GetLastError())
	// delete[] the result when finished with it.
	static wchar_t *MBtoWC(const char *mbString)
	{
		WCHAR *result = NULL;

		int wcBufSize = MultiByteToWideChar(CP_ACP, 0, mbString, -1, NULL, 0);

		result = new(std::nothrow) WCHAR[wcBufSize];

		if (result == NULL)
		{
			::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		}
		else if (0 == MultiByteToWideChar(CP_ACP, 0, mbString, -1, result, wcBufSize))
		{
			delete [] result;
			result = NULL;
		}

		return(result);
	}
#endif

#ifdef WIN32
	// This stuff is used by other WIN32-only code. Nothing in this WIN32 block should throw exceptions.

	static bool GetTempPath(std::wstring *pstrTempPath)
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
		wchar_t szTempPath1[MAX_PATH];

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
		wchar_t *szTempPath2 = new wchar_t[dwTempBufferSize];

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

	// delete[] the result.
	// Returns NULL on failure.
	static wchar_t *VStringAllocAndFormatW(const wchar_t *format, va_list args)
	{
		// Leo 16/Jan/2009: Instead of defaulting to 64 bytes for the initial buffer size, make it 1.5* the format string length.
		size_t bufferSize = (wcslen(format) * 3) / 2;

		if (bufferSize < 64)
		{
			bufferSize = 64;
		}

		wchar_t *szResult = new(std::nothrow) wchar_t[bufferSize];

		while(true)
		{
			if (NULL == szResult)
			{
				::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
				break;
			}

			int formatRes = _vsnwprintf_s(szResult, bufferSize, _TRUNCATE, format, args);

			// formatRes negative on error; (bufferSize == formatRes) should be redundant for _vsntprintf.

			if ((0 > formatRes) || (bufferSize == formatRes))
			{
				delete [] szResult;
				bufferSize *= 2;
				szResult = new(std::nothrow) wchar_t[bufferSize];
			}
			else
			{
				break;
			}
		}

		return(szResult);
	}

	// delete[] the result.
	// Returns NULL on failure.
	static wchar_t *StringAllocAndFormatW(const wchar_t *format, ...)
	{
		va_list args;
		va_start(args, format);
		wchar_t *result = DCRawDivert::VStringAllocAndFormatW(format, args);
		va_end(args);

		return(result);
	}

	// delete[] the result.
	// Returns NULL on failure.
	static char *VStringAllocAndFormatA(const char *format, va_list args)
	{
		// Leo 16/Jan/2009: Instead of defaulting to 64 bytes for the initial buffer size, make it 1.5* the format string length.
		size_t bufferSize = (strlen(format) * 3) / 2;

		if (bufferSize < 64)
		{
			bufferSize = 64;
		}

		char *szResult = new(std::nothrow) char[bufferSize];

		while(true)
		{
			if (NULL == szResult)
			{
				::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
				break;
			}

			int formatRes = _vsnprintf_s(szResult, bufferSize, _TRUNCATE, format, args);

			// formatRes negative on error; (bufferSize == formatRes) should be redundant for _vsntprintf.

			if ((0 > formatRes) || (bufferSize == formatRes))
			{
				delete [] szResult;
				bufferSize *= 2;
				szResult = new(std::nothrow) char[bufferSize];
			}
			else
			{
				break;
			}
		}

		return(szResult);
	}

	// delete[] the result.
	// Returns NULL on failure.
	static char *StringAllocAndFormatA(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		char *result = DCRawDivert::VStringAllocAndFormatA(format, args);
		va_end(args);

		return(result);
	}

	static std::string *GetParentPathStringA(std::string *pstrResult, const char *path)
	{
		if (NULL == pstrResult || NULL == path)
		{
			return NULL;
		}

		const char *lastBackslash  = strrchr(path, '\\');
		const char *lastFrontslash = strrchr(path, '/' );
		const char *lastSlash = NULL;

		if ((NULL != lastBackslash) && (NULL != lastFrontslash))
		{
			if (lastBackslash > lastFrontslash)
			{
				lastSlash = lastBackslash;
			}
			else
			{
				lastSlash = lastFrontslash;
			}
		}
		else if (NULL != lastBackslash)
		{
			lastSlash = lastBackslash;
		}
		else if (NULL != lastFrontslash)
		{
			lastSlash = lastFrontslash;
		}

		if (NULL == lastSlash)
		{
			pstrResult->clear();
		}
		else
		{
			std::wstring::size_type resLen = (lastSlash - path);

			pstrResult->assign(path, resLen);
		}

		return pstrResult;
	}

	static std::wstring *AppendPathString(std::wstring *pstrPath, const wchar_t *szAppendage)
	{
		if (pstrPath == NULL)
		{
			return NULL;
		}

		if (szAppendage != NULL)
		{
			while (szAppendage[0] == L'\\' || szAppendage[0] == L'/')
			{
				++szAppendage;
			}
		}

		if (szAppendage != NULL && szAppendage[0] != L'\0')
		{
			if (!pstrPath->empty())
			{
				wchar_t lastChar = (*pstrPath)[pstrPath->length() - 1];

				if (lastChar != L'\\'
				&&	lastChar != L'/')
				{
					pstrPath->append(L"\\");
				}
			}

			pstrPath->append(szAppendage);
		}

		return pstrPath;
	}

	static const char *GetLastPathPartA(const char *szIn)
	{
		if (NULL == szIn)
		{
			return(szIn);
		}

		size_t len = strlen(szIn);

		if (1 >= len)
		{
			return(szIn);
		}

		const char *szOut = szIn + (len - 1);

		// Ignore any slashes at the very end of the string.
		while (szIn < szOut && (*szOut == '\\' || *szOut == '/'))
		{
			szOut--;
		}

		// Stop at the next slash we find.
		while (szIn < szOut && (*szOut != '\\' && *szOut != '/'))
		{
			szOut--;
		}

		if (*szOut == '\\' || *szOut == '/')
		{
			szOut++;
		}

		return(szOut);
	}

	static const char *GetExtensionPartA(bool bIncludeDot, const char *szIn)
	{
		if (NULL == szIn)
		{
			return(szIn);
		}

		size_t len = strlen(szIn);

		if (1 >= len)
		{
			return(NULL);
		}

		const char *szOut = szIn + (len - 1);

		while (szIn < szOut && (*szOut != '.'))
		{
			szOut--;
		}

		if (*szOut != '.')
		{
			return(NULL);
		}

		if (!bIncludeDot)
		{
			szOut++;
		}

		return(szOut);
	}

	/*
	static const wchar_t *GetExtensionPartW(bool bIncludeDot, const wchar_t *szIn)
	{
		if (NULL == szIn)
		{
			return(szIn);
		}

		size_t len = wcslen(szIn);

		if (1 >= len)
		{
			return(NULL);
		}

		const wchar_t *szOut = szIn + (len - 1);

		while (szIn < szOut && (*szOut != L'.'))
		{
			szOut--;
		}

		if (*szOut != L'.')
		{
			return(NULL);
		}

		if (!bIncludeDot)
		{
			szOut++;
		}

		return(szOut);
	}
	*/

#ifdef OUTPUT_DCRAW_DIVERT_DEBUG

	static void OutputDebug(const wchar_t *szPrefix, const wchar_t *szClass, const wchar_t *szMethod, const wchar_t *szMessage)
	{
		std::wstring strMsg = szPrefix;

		if (szClass != NULL)
		{
			strMsg += szClass;

			if (szMethod != NULL)
			{
				strMsg += L"::";
			}
			else
			{
				strMsg += L": ";
			}
		}

		if (szMethod != NULL)
		{
			strMsg += szMethod;
			strMsg += L": ";
		}

		if (szMessage != NULL)
		{
			strMsg += szMessage;
		}

		while(!strMsg.empty() && iswspace(strMsg[strMsg.length() - 1]))
		{
			strMsg.resize(strMsg.length() - 1);
		}

		strMsg += L"\r\n";

		::OutputDebugString(strMsg.c_str());
	}

	static void VOutputDebugFormat(const wchar_t *szPrefix, const wchar_t *szClass, const wchar_t *szMethod, const wchar_t *szFormat, va_list args)
	{
		wchar_t *szMainMsg = VStringAllocAndFormatW(szFormat, args);

		if (szMainMsg != NULL)
		{
			OutputDebug(szPrefix, szClass, szMethod, szMainMsg);

			delete[] szMainMsg;
		}
	}
public:
	static void OutputDebugFormat(const wchar_t *szPrefix, const wchar_t *szClass, const wchar_t *szMethod, const wchar_t *szFormat, ...)
	{
		va_list args;
		va_start(args, szFormat);

		VOutputDebugFormat(szPrefix, szClass, szMethod, szFormat, args);

		va_end(args);
	}
private:

#endif // OUTPUT_DCRAW_DIVERT_DEBUG

#endif // WIN32
};
