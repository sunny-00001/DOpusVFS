#pragma once

#define NTDDI_VERSION	NTDDI_WIN2K
#define WINVER			0x0500
#define _WIN32_WINNT	0x0500
#define _WIN32_WINDOWS	0x0500
#define _WIN32_IE		0x0500

#include <windows.h>
//#include <windowsx.h>
//#include <commctrl.h>

#include <stdio.h>
#include <tchar.h>

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

#define DOPUS_PLUGIN_HELPER
#define DOPUS_PLUGIN_LEO_NO_COPYVERSION

#include "../common/viewer plugins.h"
#include "../common/plugin support.h"
#include "../common/messages.hpp"
