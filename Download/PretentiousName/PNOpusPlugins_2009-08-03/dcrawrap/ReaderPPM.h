#pragma once

namespace ReaderPNM
{
	// The Win32IOWrapper does not need to support seeking.
	// The pVPFileInfo and phBitmap parameters can be NULL if not wanted.
	bool Process(Win32IOWrapper *pIO, CRITICAL_SECTION *pReadCS, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, HBITMAP *phBitmap);
};
