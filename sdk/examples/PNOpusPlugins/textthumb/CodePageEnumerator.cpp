#include "StdAfx.h"
#include "LeoHelpers.h"
#include "CodePageEnumerator.h"

CRITICAL_SECTION CCodePageEnumerator::s_cs;
std::vector< std::basic_string<TCHAR> > *CCodePageEnumerator::s_pvecStrIdentifiers;

bool CCodePageEnumerator::StaticInitialize()
{
	InitializeCriticalSection(&s_cs);
	s_pvecStrIdentifiers = NULL;
	return true;
}

void CCodePageEnumerator::StaticDestroy()
{
	DeleteCriticalSection(&s_cs);
}

bool CCodePageEnumerator::Enumerate(std::map< UINT, std::basic_string<TCHAR> > &mapCodePages, DWORD dwFlags)
{
	mapCodePages.clear();

	std::vector< std::basic_string<TCHAR> > vecStrIdentifiers;

	BOOL bEnumRes = FALSE;

	{
		LeoHelpers::CriticalSectionScoper css(&s_cs);

		s_pvecStrIdentifiers = &vecStrIdentifiers;

		bEnumRes = EnumSystemCodePages(CCodePageEnumerator::codePageEnumProc, dwFlags);

		s_pvecStrIdentifiers = NULL;
	}

	if (!bEnumRes)
	{
		return(false);
	}

	CPINFOEX info;

	for(std::vector< std::basic_string<TCHAR> >::const_iterator pStrIdentifier = vecStrIdentifiers.begin(); pStrIdentifier != vecStrIdentifiers.end(); ++pStrIdentifier)
	{
		unsigned long ulCodePage = _tcstoul(pStrIdentifier->c_str(), NULL, 10);

		if (ulCodePage != 0 && ulCodePage != ULONG_MAX && GetCPInfoEx(ulCodePage, 0, &info) && info.CodePageName[0] != _T('\0'))
		{
			mapCodePages[ ulCodePage ] = info.CodePageName;
		}
	}

	return(true);
}

BOOL CALLBACK CCodePageEnumerator::codePageEnumProc(LPTSTR lpCodePageString)
{
	if (s_pvecStrIdentifiers == NULL)
	{
		return FALSE;
	}

	s_pvecStrIdentifiers->push_back(lpCodePageString);

	return TRUE;
}
