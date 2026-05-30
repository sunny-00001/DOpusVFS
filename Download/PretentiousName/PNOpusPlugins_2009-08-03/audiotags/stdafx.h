// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WINVER         0x0500	// Win2k and above
#define _WIN32_WINNT   0x0500	// Win2k and above
#define _WIN32_WINDOWS 0x0410	// Win98 and above
#define _WIN32_IE      0x0600	// IE 6.0 and above

//#define WIN32_LEAN_AND_MEAN	<-- AWWW HELL NAWW!!

// Windows Header Files:
#include <windows.h>

#include <stdio.h>
#include <tchar.h>
//#include <Commctrl.h>
//#include <richedit.h>

//#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>

#include <assert.h>

// Makes plugin_support.h compile if we're not including Commctrl.h
#ifndef HIMAGELIST
struct _IMAGELIST;
typedef struct _IMAGELIST* HIMAGELIST;
#endif
#ifndef CLR_NONE
#define CLR_NONE                0xFFFFFFFFL
#endif

#include "../common/viewer plugins.h"
#include "../common/plugin support.h"
#include "../common/messages.hpp"

