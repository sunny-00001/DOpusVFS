#pragma once

//#define _WIN32_DCOM

#define WINVER         0x0500	// Win2k and above
#define _WIN32_WINNT   0x0500	// Win2k and above
#define _WIN32_WINDOWS 0x0410	// Win98 and above
#define _WIN32_IE      0x0600	// IE 6.0 and above

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
//#include <commctrl.h>
#include <WININET.H>
#include <Shlobj.h>
#include <stdio.h>
#include <tchar.h>
#include <locale.h>
#include <process.h>
#include <math.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <assert.h>

#ifndef APPCOMMAND_MEDIA_PLAY
#define APPCOMMAND_MEDIA_PLAY             46
#endif

#ifndef APPCOMMAND_MEDIA_PAUSE
#define APPCOMMAND_MEDIA_PAUSE            47
#endif

#ifndef APPCOMMAND_MEDIA_FAST_FORWARD
#define APPCOMMAND_MEDIA_FAST_FORWARD     49
#endif

#ifndef APPCOMMAND_MEDIA_REWIND
#define APPCOMMAND_MEDIA_REWIND           50
#endif

#include "../common/viewer plugins.h"
#include "../common/plugin support.h"
#include "../common/messages.hpp"

#ifndef VIEWPIC_SHADOWSIZE
#define VIEWPIC_SHADOWSIZE	4
#endif

#ifndef VIEWPIC_FRAMESIZE
#define VIEWPIC_FRAMESIZE	3
#endif
