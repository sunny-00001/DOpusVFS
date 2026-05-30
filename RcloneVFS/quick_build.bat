@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "d:\VFS\rclonevfs"
cl.exe /nologo /W3 /O2 /EHsc /MT /LD /I"headers" /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2 RcloneVFS.cpp RcloneClient.cpp PathParser.cpp RcloneCache.cpp RcloneFeatures.cpp ColumnManager.cpp MenuManager.cpp DaemonManager.cpp ConfigManager.cpp CheckDialog.cpp SyncDialog.cpp TrashDialog.cpp JobDialog.cpp VersionsDialog.cpp ShareDialog.cpp /Fe"d:\VFS\RcloneVFS.dll" /link /DEF:RcloneVFS.def Shell32.lib User32.lib Advapi32.lib Winhttp.lib Crypt32.lib Shlwapi.lib Gdi32.lib Comctl32.lib resource.res
