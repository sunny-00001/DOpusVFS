#pragma once

class CCodePageEnumerator
{
private:
	CCodePageEnumerator();
	~CCodePageEnumerator();
protected:
	static CRITICAL_SECTION s_cs;
	static std::vector< std::basic_string<TCHAR> > *s_pvecStrIdentifiers;

	static BOOL CALLBACK codePageEnumProc(LPTSTR lpCodePageString);

public:
	static bool StaticInitialize();
	static void StaticDestroy();

	static bool Enumerate(std::map< UINT, std::basic_string<TCHAR> > &mapCodePages, DWORD dwFlags);
};
