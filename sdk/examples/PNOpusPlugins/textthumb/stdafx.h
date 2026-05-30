#pragma once

#define WINVER         0x0500	// Win2k and above
#define _WIN32_WINNT   0x0500	// Win2k and above
#define _WIN32_WINDOWS 0x0410	// Win98 and above
#define _WIN32_IE      0x0500	// IE 5.0 and above

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#include <process.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <assert.h>

#include "../common/viewer plugins.h"
#include "../common/plugin support.h"
#include "../common/messages.hpp"
