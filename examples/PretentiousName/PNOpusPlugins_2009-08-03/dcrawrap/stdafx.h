// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WINVER         0x0500	// Win2k and above
#define _WIN32_WINNT   0x0500	// Win2k and above
#define _WIN32_WINDOWS 0x0410	// Win98 and above
#define _WIN32_IE      0x0600	// IE 6.0 and above

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>

#include <stdio.h>
#include <tchar.h>

#include <Commctrl.h>
#include <richedit.h>

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


#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED                 0x031A
#endif

#ifndef ETDT_DISABLE
#define ETDT_DISABLE        0x00000001
#endif

#ifndef ETDT_ENABLE
#define ETDT_ENABLE         0x00000002
#endif

#ifndef ETDT_USETABTEXTURE
#define ETDT_USETABTEXTURE  0x00000004
#endif

#ifndef ETDT_USEAEROWIZARDTABTEXTURE
#define ETDT_USEAEROWIZARDTABTEXTURE    0x00000008
#endif

#ifndef ETDT_ENABLETAB
#define ETDT_ENABLETAB              (ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE)
#endif

#ifndef ETDT_ENABLEAEROWIZARDTAB
#define ETDT_ENABLEAEROWIZARDTAB    (ETDT_ENABLE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)
#endif

#ifndef ETDT_VALIDBITS
#define ETDT_VALIDBITS              (ETDT_DISABLE | \
                                     ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)
#endif

#include "../common/viewer plugins.h"
#include "../common/plugin support.h"
#include "../common/messages.hpp"
