# Registry VFS Plugin Makefile for NMake

CC = cl.exe
LINK = link.exe

SRCDIR = src
INCDIR = include
BUILDDIR = build
OUTDIR = $(BUILDDIR)\Release

SOURCES = $(SRCDIR)\RegistryVFS.cpp
OBJECTS = $(BUILDDIR)\RegistryVFS.obj

INCLUDES = /I"$(INCDIR)"
DEFINES = /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
CXXFLAGS = /nologo /W4 /O2 /EHsc /MD /c

LIBS = advapi32.lib Shell32.lib Comctl32.lib
LDFLAGS = /nologo /DLL /DEF:$(SRCDIR)\RegistryVFS.def

TARGET = $(OUTDIR)\RegistryVFS.dll

all: dirs $(TARGET)

dirs:
    @if not exist "$(BUILDDIR)" mkdir "$(BUILDDIR)"
    @if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

$(BUILDDIR)\RegistryVFS.obj: $(SOURCES)
    $(CC) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $(SOURCES) /Fo$@

$(TARGET): $(OBJECTS)
    $(LINK) $(LDFLAGS) $(OBJECTS) $(LIBS) /Out:$@

clean:
    @if exist "$(BUILDDIR)" rmdir /s /q "$(BUILDDIR)"

.PHONY: all clean dirs
