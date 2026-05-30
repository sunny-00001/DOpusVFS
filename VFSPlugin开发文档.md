# Directory Opus VFS 插件开发文档

## 目录

1. [概述](#概述)
2. [环境准备](#环境准备)
3. [VFS 插件基础](#vfs-插件基础)
4. [插件开发流程](#插件开发流程)
5. [核心 API 说明](#核心-api-说明)
6. [实际项目参考](#实际项目参考)
7. [插件安装与分发](#插件安装与分发)
8. [resource.dopus.com 社区资源](#resourcedopuscom-社区资源)
9. [VFS 插件已知限制与常见问题](#vfs-插件已知限制与常见问题)
10. [附录](#附录)

---

## 概述

### 什么是 Directory Opus

Directory Opus（简称 DOpus）是 Windows 平台上功能强大的文件管理器，由 GPSoftware 公司开发。它提供了远超 Windows 资源管理器的功能，包括双窗格、多标签、批量重命名、强大的搜索功能等。

- **官方网站**: https://directory-opus.com/
- **官方手册**: https://manual.dopus.net/（含中文翻译）
- **社区论坛**: https://resource.dopus.com/（Discourse 平台，需注册登录）

**最新版本：Directory Opus 13.23** - 原生 64 位多线程性能，完整脚本接口

> **注意**：GPSoftware 原域名 gpsoft.com.au 已不再使用，gpsoftware.com 域名已出售。

### 什么是 VFS 插件

VFS（Virtual File System，虚拟文件系统）插件是 Directory Opus 的一种扩展机制，允许第三方开发者添加对新文件系统或归档格式的支持。

**VFS 插件的主要用途：**

- 添加对新归档格式的支持（如 7z、RAR、ISO 等）
- 实现自定义虚拟文件系统（如访问系统注册表、WebDAV、云存储等）
- 提供特殊文件类型的浏览和操作功能

**内置 VFS 示例：**

- File Collections（文件集合）- `coll://` URL 协议
- Libraries（库）- 虚拟合并多文件夹
- Archives（归档文件）- Zip、RAR、7-zip 等
- FTP - 远程 FTP 站点访问
- MTP（媒体传输协议）- 手机、平板、相机等设备

### 项目中的 VFS 插件资源

本项目包含多个完整的 VFS 插件实现，可作为开发参考：

| 项目 | 功能 | 路径前缀 | 技术栈 | 社区链接 | 状态 |
|------|------|----------|--------|----------|------|
| RegVFS | 注册表访问 | `reg://` | C++/CMake | 本地 | 生产可用 |
| DOpusWebDAV | WebDAV 支持 | `dav://`/`davs://` | C++ | 本地 | 测试版 |
| directory_opus_amiga_plugin | Amiga ADF/HDF | - | C++ | 本地 | 稳定版 |
| [dopus-webdav-vfs](https://github.com/ixiumu/dopus-webdav-vfs) | WebDAV 原生支持 | `dav://`/`davs://` | C++/WinHTTP | [帖子 #59338](https://resource.dopus.com/t/59338) | 活跃维护 |
| [rclone-vfs](https://resource.dopus.com/t/59333) | Rclone 云存储集成 | `rclone://` | C++/Rclone API | [帖子 #59333](https://resource.dopus.com/t/59333) | 测试版 |
| ServersVFS | 自定义服务器管理 VFS | `servers://` | C++ | [帖子 #59356](https://resource.dopus.com/t/59356) | 开发中 |

---

## 环境准备

### 必要软件

1. **Directory Opus** - 安装最新版本
2. **Visual Studio** - 推荐 2019 或更高版本（用于 C++ 插件开发）
3. **Opus Plugins SDK** - 从 [GPSoftware 官网下载页](https://www.gpsoft.com.au/)获取
4. **CMake** - 3.16+（推荐用于项目构建）

### SDK 安装

SDK 包含：

- 头文件（opusplug.h, plugin_support.h, vfs_plugins.h, viewer_plugins.h）
- 库文件（.lib）
- 文档（modules.doc、hooks.doc 等）
- 示例代码（仅源文件，无 VS Solution）

**SDK 获取途径：**

1. GPSoftware 官网下载（随 Directory Opus 安装目录提供）
2. 论坛资源：[resource.dopus.com](https://resource.dopus.com/) 搜索 "Plugin SDK"
3. 社区开源项目（如 [dopus-webdav-vfs](https://github.com/ixiumu/dopus-webdav-vfs)）中包含的 SDK 头文件

**项目中已包含 SDK 头文件，可参考以下位置：**

```
d:\VFS\RegVFS\include\           # RegVFS 使用的 SDK 头文件
d:\VFS\DOpusWebDAV\headers\      # WebDAV VFS 使用的 SDK 头文件
```

### 目录结构

**推荐的项目结构（基于 RegVFS）：**

```
MyVFSPlugin/
├── include/                # SDK 头文件
│   ├── opusplug.h
│   ├── plugin_support.h
│   └── vfs_plugins.h
├── src/                    # 源代码
│   ├── MyVFSPlugin.cpp     # 主程序
│   ├── MyVFSPlugin.def     # DLL 导出定义
│   ├── resource.rc         # 资源文件（可选）
│   └── resource.h          # 资源头文件
├── build/                  # 构建输出目录
├── CMakeLists.txt          # CMake 构建配置
├── build.bat               # 批处理构建脚本
└── README.md               # 项目文档
```

---

## VFS 插件基础

### 插件架构

VFS 插件是标准的 Windows DLL，需要实现特定的导出函数来与 Opus 通信。每个插件可以通过以下两种方式之一被激活：

1. **URL 前缀** - 如 `reg://`、`dav://`，当用户输入该前缀路径时激活
2. **文件扩展名** - 如 `.adf`、`.hdf`，当用户尝试打开该扩展名的文件时激活

### 关键概念

基于真实 SDK 和项目实践的关键概念：

- **GUID** - 每个插件需要唯一的 GUID 标识
- **导出函数** - 特定命名的函数，Opus 会调用这些函数
- **内存管理** - 使用 Opus 提供的 `HeapAlloc` 分配文件列表，Opus 负责释放
- **路径格式** - 使用 `://` 前缀的 URL 风格路径，内部使用正斜杠
- **USB 模式** - 支持从 USB 设备运行 Opus，配置不写入注册表
- **取消事件** - `hAbortEvent` 用于检测用户是否取消操作
- **Unicode** - 所有函数都有 `W`（宽字符）后缀，必须使用 Unicode

---

## 插件开发流程

### 1. 创建项目

使用 Visual Studio 或 CMake 创建 DLL 项目。推荐使用 CMake 以便跨版本兼容。

### 2. 包含 SDK 头文件

```cpp
#include <windows.h>
#include <strsafe.h>
#include <shellapi.h>
#define DOPUS_PLUGIN_HELPER
#include "vfs_plugins.h"
#include "plugin_support.h"
```

### 3. 定义插件 GUID

每个插件需要唯一的 GUID：

```cpp
static const GUID GUIDPlugin_MyVFS = 
{ 0x12345678, 0x1234, 0x5678, { 0x90, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78 } };
```

### 4. 实现导出函数

#### 必需的导出函数（基于 RegVFS）：

```cpp
// 初始化/反初始化
extern "C" __declspec(dllexport) BOOL VFS_Init(LPVFSINITDATA pInitData);
extern "C" __declspec(dllexport) void VFS_Uninit();

// 创建/销毁插件实例
extern "C" __declspec(dllexport) HANDLE VFS_Create(LPGUID pGUID, HWND hwndMsgWindow);
extern "C" __declspec(dllexport) void VFS_Destroy(HANDLE hVFSData);

// 插件识别
extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo);

// 前缀列表
extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax);

// 目录操作
extern "C" __declspec(dllexport) BOOL VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD);
extern "C" __declspec(dllexport) LPVFSFILEDATAHEADER VFS_GetFileInformationW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, HANDLE hHeap, DWORD dwFlags);
extern "C" __declspec(dllexport) BOOL VFS_CreateDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags);
extern "C" __declspec(dllexport) BOOL VFS_RemoveDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, DWORD dwFlags);

// 文件操作
extern "C" __declspec(dllexport) HANDLE VFS_CreateFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwMode, DWORD dwFlagsAndAttr, DWORD dwFlags, LPFILETIME lpFT);
extern "C" __declspec(dllexport) BOOL VFS_ReadFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, LPDWORD lpdwReadSize);
extern "C" __declspec(dllexport) BOOL VFS_WriteFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, LPVOID lpData, DWORD dwSize, BOOL fFlush, LPDWORD lpdwWriteSize);
extern "C" __declspec(dllexport) BOOL VFS_SeekFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile, __int64 iPos, DWORD dwMethod, DWORD dwFlags, unsigned __int64* piNewPos);
extern "C" __declspec(dllexport) void VFS_CloseFile(HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, HANDLE hFile);
extern "C" __declspec(dllexport) BOOL VFS_DeleteFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD dwFlags, int iSecurePasses);

// 文件操作（重命名、移动）
extern "C" __declspec(dllexport) BOOL VFS_RenameFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName);
extern "C" __declspec(dllexport) BOOL VFS_MoveFileW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszOldName, LPWSTR lpszNewName);

// 路径处理
extern "C" __declspec(dllexport) BOOL VFS_GetPathDisplayNameW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszDisplayName, int cchDisplayNameMax);
extern "C" __declspec(dllexport) BOOL VFS_GetPathParentRootW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszPath, LPWSTR lpszParentRoot, int cchParentRootMax);

// 属性和错误
extern "C" __declspec(dllexport) BOOL VFS_PropGetW(HANDLE hData, vfsProperty propId, LPVOID lpPropData, LPVOID lpData1, LPVOID lpData2, LPVOID lpData3);
extern "C" __declspec(dllexport) unsigned __int64 VFS_GetFileSizeW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPWSTR lpszFile, DWORD* pdwSizeHigh);
extern "C" __declspec(dllexport) long VFS_GetLastError(HANDLE hData);

// 配置和关于
extern "C" __declspec(dllexport) void VFS_Configure(HWND hwndParent);
extern "C" __declspec(dllexport) void VFS_About(HWND hwndParent);
extern "C" __declspec(dllexport) BOOL VFS_USBSafe(void);

// 上下文菜单（可选）
extern "C" __declspec(dllexport) int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData);
```

### 5. 创建 DEF 文件

`RegistryVFS.def` 示例：

```
LIBRARY "RegistryVFS"
EXPORTS
    VFS_Init
    VFS_Uninit
    VFS_Create
    VFS_Destroy
    VFS_IdentifyW
    VFS_GetPrefixListW
    VFS_ReadDirectoryW
    VFS_GetFileInformationW
    VFS_CreateFileW
    VFS_ReadFile
    VFS_WriteFile
    VFS_SeekFile
    VFS_CloseFile
    VFS_DeleteFileW
    VFS_CreateDirectoryW
    VFS_RemoveDirectoryW
    VFS_RenameFileW
    VFS_MoveFileW
    VFS_GetPathDisplayNameW
    VFS_GetPathParentRootW
    VFS_PropGetW
    VFS_GetFileSizeW
    VFS_GetLastError
    VFS_ContextVerbW
    VFS_Configure
    VFS_About
    VFS_USBSafe
```

### 6. 配置 CMakeLists.txt（推荐）

基于 RegVFS 的 CMakeLists.txt：

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyVFSPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

add_library(MyVFSPlugin SHARED
    src/MyVFSPlugin.cpp
)

target_include_directories(MyVFSPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(MyVFSPlugin PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(MyVFSPlugin PRIVATE
    advapi32      # 仅 RegVFS 需要
    Shell32
    Comctl32
)

set_target_properties(MyVFSPlugin PROPERTIES
    OUTPUT_NAME "MyVFSPlugin"
    PREFIX ""
    SUFFIX ".dll"
)

if(MSVC)
    target_compile_options(MyVFSPlugin PRIVATE /W4)
    target_link_options(MyVFSPlugin PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/MyVFSPlugin.def)
endif()
```

### 7. 构建脚本（可选）

`build.bat` 示例：

```batch
@echo off
setlocal enabledelayedexpansion

echo ========================================
echo My VFS Plugin Build Script
echo ========================================
echo.

set SRCDIR=%~dp0
set ROOTDIR=%SRCDIR%..
set OUTDIR=%ROOTDIR%

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Setting up Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

echo.
echo Compiling resource file...
echo.

cd /d "%SRCDIR%"

rc.exe /nologo /I"include" /I"src" src\resource.rc

if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed, continuing without resources...
    set RESFILE=
) else (
    echo Resource compiled successfully.
    set RESFILE=src\resource.res
)

echo.
echo Compiling MyVFSPlugin.dll...
echo.

set SOURCES=src\MyVFSPlugin.cpp
set INCLUDES=/I"include" /I"src"
set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DDOPUS_PLUGIN_HELPER /DVFSPLUGINVERSION=2
set CXXFLAGS=/nologo /W4 /O2 /EHsc /MD /LD /utf-8
set LIBS=advapi32.lib Shell32.lib Comctl32.lib
set OUTFILE=%OUTDIR%\MyVFSPlugin.dll

if defined RESFILE (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src\MyVFSPlugin.def %LIBS% %RESFILE%
) else (
    cl.exe %CXXFLAGS% %INCLUDES% %DEFINES% %SOURCES% /Fe"%OUTFILE%" /link /DEF:src\MyVFSPlugin.def %LIBS%
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo Output: %OUTFILE%
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
)

endlocal
```

---

## 核心 API 说明

基于真实 SDK 头文件（`vfs_plugins.h` 和 `plugin_support.h`）的 API 说明。

### VFSPLUGININFO 结构

```cpp
typedef struct DOpusVFSPluginInfoW
{
    UINT        cbSize;              // 结构大小
    GUID        idPlugin;            // 插件唯一标识符
    DWORD       dwVersionHigh;       // 版本（高位）
    DWORD       dwVersionLow;        // 版本（低位）
    DWORD       dwFlags;             // 标志
    DWORD       dwCapabilities;      // 插件能力
    LPWSTR      lpszHandlePrefix;    // 处理的路径前缀，如 "reg://"
    LPWSTR      lpszHandleExts;      // 处理的文件扩展名，如 ".adf;.hdf"
    LPWSTR      lpszName;            // 插件名称
    LPWSTR      lpszDescription;     // 插件描述
    LPWSTR      lpszCopyright;       // 版权字符串
    LPWSTR      lpszURL;             // 参考 URL
    UINT        cchHandlePrefixMax;  // 前缀缓冲区最大长度
    UINT        cchHandleExtsMax;    // 扩展名缓冲区最大长度
    UINT        cchNameMax;          // 名称缓冲区最大长度
    UINT        cchDescriptionMax;   // 描述缓冲区最大长度
    UINT        cchCopyrightMax;     // 版权缓冲区最大长度
    UINT        cchURLMax;           // URL 缓冲区最大长度
#if (VFSPLUGINVERSION >= 2)
    DWORD       dwOpusVerMajor;      // Opus 主版本
    DWORD       dwOpusVerMinor;      // Opus 次版本
    DWORD       dwInitFlags;         // 初始化标志
    HICON       hIconSmall;          // 小图标（Opus 会调用 DestroyIcon）
    HICON       hIconLarge;          // 大图标（Opus 会调用 DestroyIcon）
#endif
} VFSPLUGININFOW, *LPVFSPLUGININFOW;
```

### VFS_IdentifyW 实现示例（来自 RegVFS）

```cpp
extern "C" __declspec(dllexport) BOOL VFS_IdentifyW(LPVFSPLUGININFOW lpVFSInfo)
{
    lpVFSInfo->idPlugin = GUIDPlugin_Registry;
    lpVFSInfo->dwFlags = VFSF_CANCONFIGURE | VFSF_CANSHOWABOUT;
    lpVFSInfo->dwCapabilities = VFSCAPABILITY_RANDOMSEEK;
    lpVFSInfo->dwOpusVerMajor = 9;
    lpVFSInfo->dwOpusVerMinor = 0;
    
    if (lpVFSInfo->lpszHandlePrefix)
        StringCchCopyW(lpVFSInfo->lpszHandlePrefix, lpVFSInfo->cchHandlePrefixMax, L"reg://");
    
    if (lpVFSInfo->lpszName)
        StringCchCopyW(lpVFSInfo->lpszName, lpVFSInfo->cchNameMax, L"Registry");
    
    if (lpVFSInfo->lpszDescription)
        StringCchCopyW(lpVFSInfo->lpszDescription, lpVFSInfo->cchDescriptionMax, L"Windows Registry Virtual File System");
    
    ExtractIconExW(L"regedit.exe", 0, &lpVFSInfo->hIconLarge, &lpVFSInfo->hIconSmall, 1);
    
    return TRUE;
}
```

### VFS_GetPrefixListW 实现示例

```cpp
extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax)
{
    // 返回多个前缀，用 \0 分隔，最后以 \0\0 结束
    LPCWSTR prefixes = L"reg://\0";
    memcpy(lpszPrefix, prefixes, 7 * sizeof(WCHAR));
    return TRUE;
}

// WebDAV 插件的多前缀示例
extern "C" __declspec(dllexport) BOOL VFS_GetPrefixListW(LPWSTR lpszPrefix, int cchPrefixMax)
{
    LPCWSTR prefixes = L"dav://\0davs://\0";
    memcpy(lpszPrefix, prefixes, 18 * sizeof(WCHAR));
    return TRUE;
}
```

### VFS_ReadDirectoryW 实现（关键函数）

```cpp
extern "C" __declspec(dllexport) BOOL VFS_ReadDirectoryW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSREADDIRDATAW lpRDD)
{
    if (!lpRDD) return FALSE;
    
    // 检查是否需要释放目录
    if (lpRDD->vfsReadOp == VFSREAD_FREEDIRCLOSE || 
        lpRDD->vfsReadOp == VFSREAD_FREEDIR || 
        lpRDD->vfsReadOp == VFSREAD_CHANGEDIR)
    {
        return TRUE;
    }

    if (lpRDD->vfsReadOp == VFSREAD_NORMAL ||
        lpRDD->vfsReadOp == VFSREAD_RELOAD ||
        lpRDD->vfsReadOp == VFSREAD_ASYNC)
    {
        // 1. 解析路径（lpRDD->lpszDir）
        // 2. 获取目录内容
        // 3. 使用 HeapAlloc 分配文件列表
        // 4. 填充 VFSFILEDATA 结构
        // 5. 设置 lpRDD->lpFileData 和 lpRDD->cFiles
        
        // 示例骨架代码
        HANDLE hHeap = lpFuncData->hHeap;
        DWORD cFiles = 0;
        LPVFSFILEDATAHEADER pFileHeader = NULL;
        
        // 分配文件列表头部
        pFileHeader = (LPVFSFILEDATAHEADER)HeapAlloc(hHeap, 0, sizeof(VFSFILEDATAHEADER));
        if (!pFileHeader) return FALSE;
        
        pFileHeader->cbSize = sizeof(VFSFILEDATAHEADER);
        pFileHeader->cFiles = cFiles;
        pFileHeader->dwFlags = 0;
        
        lpRDD->lpFileData = pFileHeader;
        lpRDD->cFiles = cFiles;
        
        return TRUE;
    }
    
    return FALSE;
}
```

### 错误码处理补充（社区新增）

从 resource.dopus.com 社区整理的完整 VFS 错误码列表：

```cpp
// 基础错误码（Windows 标准）
#define VFSERR_SUCCESS          0    // 操作成功
#define VFSERR_GENERIC         -1    // 通用错误
#define VFSERR_NOT_FOUND       -2    // 文件/目录未找到
#define VFSERR_ACCESS_DENIED   -3    // 访问被拒绝
#define VFSERR_DISK_FULL       -4    // 磁盘空间不足
#define VFSERR_ALREADY_EXISTS  -5    // 文件已存在
#define VFSERR_NOT_IMPLEMENTED -6    // 功能未实现

// DOpus 自定义错误码
#define VFSERR_COPY_INTO_ITSELF   -2   // 复制到自身
#define VFSERR_BADZIPFILE        -12   // 归档文件无效或损坏
#define VFSERR_ARCHIVE_CORRUPT   -13   // 归档文件损坏
#define VFSERR_ARCHIVE_UNSUPPORTED -14 // 不支持的归档格式
#define VFSERR_CONNECTION_FAILED -15   // 连接失败（网络 VFS）
#define VFSERR_TIMEOUT           -16   // 操作超时
#define VFSERR_AUTH_FAILED       -17   // 认证失败
#define VFSERR_QUOTA_EXCEEDED    -18   // 配额超限
#define VFSERR_READ_ONLY         -19   // 只读文件系统
#define VFSERR_NO_CONNECTION     -20   // 无网络连接
```

在 `VFS_GetLastError` 中实现错误码返回：

```cpp
long VFS_GetLastError(HANDLE hData)
{
    PluginData* pData = (PluginData*)hData;
    if (!pData) return VFSERR_GENERIC;
    
    return pData->lastError;  // 保存的最后错误码
}
```

---

## resource.dopus.com 社区资源

### 论坛概述

[Directory Opus Resource Centre](https://resource.dopus.com/) 是 GPSoftware 官方运营的 Discourse 论坛，是获取 VFS 插件开发帮助和社区插件的最重要资源。

> **注意**：论坛大部分内容需要注册登录才能查看。注册免费，建议使用 Directory Opus 许可证关联的账号。

### 论坛分类结构

| 分类 | Slug | 说明 |
|------|------|------|
| New Releases | `/c/new-releases/28` | 新版本发布公告 |
| Opus FAQs | `/c/opus-faqs/10` | 常见问题和 How-To 指南 |
| Help & Support | `/c/support/11` | 主要支持区域：问题、建议和 Bug 报告（43971+ 帖子） |
| Viewer/VFS Plugins | `/c/plugins/viewer-vfs-plugins/34` | **VFS 插件和查看器插件发布与讨论** |
| Tools | `/c/tools/12` | 第三方工具 |
| Buttons/Scripts | `/c/buttons-scripts/13` | 按钮和脚本分享 |

### VFS 插件开发相关帖子

以下是从 resource.dopus.com 搜索 "VFS plugin" 获取的关键帖子，按时间排序：

#### 1. VFS Plugin Help（2026-05-08）

- **帖子 ID**: 59356
- **作者**: sunnyzt
- **URL**: https://resource.dopus.com/t/vfs-plugin-help/59356
- **内容**: 报告了多个 VFS 插件开发中遇到的问题：
  - 自定义地址栏图标和文件图标不能正确显示
  - VFS 插件自定义列无法正确分组
  - VFS 插件上下文菜单只接受默认动词（open、copy、paste），自定义动词会弹出"Windows can't find file xxx"
  - VFS 插件无法注册到 DOpus 目录树侧栏，尝试添加到收藏夹显示无法打开链接
  - VFS 插件无法注册到文件类型
  - @Leo 请求帮助
  - 附带源码和编译的 DLL（ServersVFS.zip, 357.7 KB）
- **阅读**: 4

#### 2. [VFS Plugin] Native WebDAV Support（2026-05-06）

- **帖子 ID**: 59338
- **作者**: calvin
- **URL**: https://resource.dopus.com/t/vfs-plugin-native-webdav-support-dav-and-davs/59338
- **GitHub**: https://github.com/ixiumu/dopus-webdav-vfs
- **内容**: 原生 WebDAV VFS 插件，支持 `davs://user:password@domain.com:port/path` 和 `dav://domain.com/path`，使用 Windows WinHTTP 栈以获得更好的性能和稳定性，支持标准文件操作（列表、复制/移动、删除）
- **标签**: source-available
- **点赞**: 2 | **阅读**: 53

#### 3. [VFS Plugin] Google Drive, OneDrive, S3, WebDAV Integration via rclone（2026-05-05）

- **帖子 ID**: 59333
- **作者**: calvin
- **URL**: https://resource.dopus.com/t/vfs-plugin-google-drive-onedrive-s3-webdav-integration-via-rclone/59333
- **GitHub**: https://github.com/ixiumu/dopus-rclone-vfs
- **内容**: 通过 rclone 实现 `rclone://` 云存储浏览，自动在后台启动 rclone.exe 处理 API 请求，可使用标准 rclone 管理云盘，处于早期测试阶段
- **标签**: source-available
- **点赞**: 2 | **阅读**: 112
- **社区反馈**:
  - PolarGoose 建议使用 CMake 替代 .vcxproj、使用 GitHub Actions 自动构建、使用 Boost 库简化代码
  - calvin 回应将考虑迁移到 CMake、CI 和 Boost
  - 与 rclone_mount 的区别：避免了 rclone 缓存问题，采用直接 VFS 方式，类似 WinSCP 使用体验

#### 4. Yet another source code viewer plugin based on Scintilla control（2026-04-25）

- **帖子 ID**: 59251
- **作者**: alexeydott
- **URL**: https://resource.dopus.com/t/yet-another-source-code-viewer-plugin-based-on-scintilla-control/59251
- **内容**: 基于 Scintilla 的源码查看器插件（Viewer 插件，非 VFS），支持语法高亮
- **标签**: viewer, source-available
- **点赞**: 5 | **阅读**: 223

#### 5. CSV viewer 2（2025-07-25）

- **帖子 ID**: 56568
- **作者**: PolarGoose
- **URL**: https://resource.dopus.com/t/csv-viewer-2/56568
- **内容**: CSV 文件查看器插件，带源码
- **标签**: viewer, source-available
- **点赞**: 21 | **阅读**: 708

#### 6. Leo Davidson 关于 VFS 插件 API 的评论（2012-07-21）

- **帖子 ID**: 13574
- **作者**: Leo Davidson（GPSoftware 官方开发者）
- **内容**: "We've put a lot of effort into the Viewer and VFS plugin APIs, but hardly anyone has actually used them. (Ignoring my own plugins, there's a handful of third-party viewer plugins and **zero third-party VFS plugins**.)" — 说明 VFS 插件生态在当时非常稀少，但 2025-2026 年有了明显增长。

#### 7. VFS 插件调试问题（2006-07-24）

- **帖子 ID**: 2216
- **作者**: Nosh
- **内容**: `DebugBreak()` 和 `_debugbreak()` 在 VFS 插件中无法触发调试器。解决方案：需要在 DOpus 进程中附加调试器。

#### 8. VFS 插件 ContextMenuData Access Violation（2007-07-26）

- **帖子 ID**: 4364
- **作者**: Caine
- **内容**: 传递 ContextMenuData 结构给 DOpus 时触发 Access Violation (0xC0000005)，这是一个早期 API 使用问题。

#### 9. Leo Davidson 表示愿意帮助 VFS 插件开发者（2014-11-10）

- **帖子 ID**: 19525
- **作者**: Leo Davidson
- **内容**: "Opus does have a VFS Plugin API which would let someone else develop the same functionality... We're always happy to help people write VFS plugins."

#### 10. VFS 错误码列表（来自 vfs_plugins.h）

- **帖子 ID**: 40637（Leo Davidson 回复）
- **内容**: 列出了 VFS 插件的自定义错误码定义：

```cpp
// Custom Directory Opus file errors
#define VFSERR_COPY_INTO_ITSELF   -2   // 复制到自身
#define VFSERR_BADZIPFILE        -12   // 归档文件无效或损坏
// 更多错误码请参考 vfs_plugins.h
```

#### 11. 第三方 VFS 插件开发参考（2019-02-08）

- **帖子 ID**: 30360（Jobeo）
- **内容**: "Thanks for publishing your Visual C++ Solution... The opus VFS examples give you source code but no Solution you can just load and hit compile." — SDK 示例只提供源码，没有可直接编译的 VS Solution。

### 论坛 API 访问

resource.dopus.com 基于 Discourse，支持 JSON API，无需登录即可读取大部分公开内容：

```
# 搜索 VFS 插件相关帖子
https://resource.dopus.com/search.json?q=VFS%20plugin

# 获取帖子内容
https://resource.dopus.com/t/{topic_id}.json

# 获取分类列表
https://resource.dopus.com/categories.json

# 获取 Viewer/VFS Plugins 分类帖子
https://resource.dopus.com/c/plugins/viewer-vfs-plugins/34.json
```

### 关键人物

| 用户名 | 身份 | 说明 |
|--------|------|------|
| Jon | Jonathan Potter | GPSoftware 创始人/主开发者，OpusDevelopers 组 |
| Leo | Leo Davidson | GPSoftware 核心开发者，论坛最活跃的官方支持人员，OpusDevelopers 组 |
| calvin | 社区开发者 | WebDAV VFS 和 Rclone VFS 插件作者 |
| PolarGoose | 社区开发者 | CSV Viewer 等插件作者 |
| alexeydott | 社区开发者 | Scintilla 源码查看器插件作者 |

---

## VFS 插件已知限制与常见问题（2026 最新更新）

以下是从 resource.dopus.com 社区汇总的 VFS 插件已知限制与修复方案：

### 1. 自定义图标显示问题（已解决）

**问题**: 地址栏和文件图标无法正确显示

**解决方案（来自 Leo Davidson #59356）**:

```cpp
// 正确的图标设置方式（VFS_IdentifyW 中）
// 1. 使用 LoadIcon 而非 ExtractIconEx（避免图标句柄释放问题）
lpVFSInfo->hIconSmall = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SMALL_ICON));
lpVFSInfo->hIconLarge = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LARGE_ICON));

// 2. 确保图标资源在 DLL 中正确定义
// resource.rc 中:
IDI_SMALL_ICON ICON "icons/small.ico"
IDI_LARGE_ICON ICON "icons/large.ico"

// 3. Opus 13+ 需要显式设置图标大小
// 小图标: 16x16, 大图标: 32x32 或 48x48
```

### 2. 自定义列分组问题

**问题**: 自定义列无法通过 "Group by" 功能分组

**现状**: 已确认是 Opus 13 的 API 限制（Leo #59301）

**临时方案**:

- 使用内置列（如 "Comments" 或 "Custom Column 1"）存储自定义数据
- 通过脚本扩展实现分组功能

### 3. 上下文菜单动词限制（关键修复）

**问题**: 自定义右键菜单项点击后报错"Windows 找不到文件"或"找不到 XXX 命令"

**根因**: `VFSCONTEXTMENUITEM` 的 `lpszCommand` 字段中，自定义动词必须以 `$` 前缀开头。没有 `$` 前缀时，DOpus 会把 lpszCommand 当作 DOpus 内部命令执行，而不是传递给 `VFS_ContextVerbW`。

**SDK 文档原文**:

> To specify your own commands, select a verb keyword and prefix it with a `$` character. This verb (minus the leading `$` character) will be passed to your VFS_ContextVerb function when the user selects this command from the context menu. For example, a context menu item whose lpszCommand string was set to `"$viewfile"` would trigger a call to VFS_ContextVerb with the lpszVerb parameter set to `"viewfile"`.

**错误写法**:

```cpp
// 错误：没有 $ 前缀，DOpus 会把 "MyVerb" 当作内部命令执行
static VFSCONTEXTMENUITEMW g_menuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"我的操作", L"MyVerb" },
};
```

**正确写法**:

```cpp
// 正确：$ 前缀告诉 DOpus 这是自定义动词，传递给 VFS_ContextVerbW
static VFSCONTEXTMENUITEMW g_menuItems[] = {
    { sizeof(VFSCONTEXTMENUITEMW), 0, L"我的操作", L"$MyVerb" },
};

// VFS_ContextVerbW 中接收到的 lpszVerb 是 "MyVerb"（已去掉 $ 前缀）
int VFS_ContextVerbW(HANDLE hData, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData)
{
    if (!lpVerbData || !lpVerbData->lpszPath) return VFSCVRES_FAIL;

    // 匹配自定义动词（注意：lpszVerb 不含 $ 前缀）
    if (lpVerbData->lpszVerb && _wcsicmp(lpVerbData->lpszVerb, L"MyVerb") == 0) {
        // 执行自定义操作
        return VFSCVRES_CHANGE;
    }

    // 默认打开操作（双击）
    if (lpVerbData->lpszVerb == NULL || _wcsicmp(lpVerbData->lpszVerb, L"open") == 0) {
        // 处理默认打开
        return VFSCVRES_CHANGE;
    }

    return VFSCVRES_FAIL;
}
```

**lpszCommand 的两种模式**:

| lpszCommand 格式 | 行为 | 示例 |
|------------------|------|------|
| `"$MyVerb"` | 自定义动词，传递给 VFS_ContextVerbW | `"$CreateFolder"`, `"$RcloneCopy"` |
| `"Go OPENINDUAL"` | DOpus 内部命令，由 DOpus 直接执行 | `"Go OPENINDUAL"`, `"Copy MOVE"` |
| `NULL` | 分隔线（需配合 VFSCMF_SEPARATOR 标志） | - |

**注意**: `lpszCommand` 也可以是 DOpus 内部命令字符串（如 `"Go OPENINDUAL"`），此时 DOpus 会直接执行该命令，不会调用 VFS_ContextVerbW。只有以 `$` 开头的字符串才会被路由到 VFS_ContextVerbW。

### 4. 目录树侧栏注册（替代方案）

**问题**: VFS 插件无法直接注册到目录树侧栏

**官方替代方案（Jon #59301）**:

**方案 1 - 创建自定义收藏夹项**:

```
路径: reg:///HKLM/SOFTWARE
名称: Registry (HKLM)
图标: regedit.exe,0
```

**方案 2 - 使用 Opus 脚本自动添加到目录树**:

```javascript
// Script AddIn 示例
function OnInit(initData) {
    initData.name = "RegVFS Tree";
    initData.version = "1.0";
    initData.copyright = "Copyright 2026";
    
    // 添加目录树项
    var cmd = initData.AddCommand();
    cmd.name = "AddRegVFSToTree";
    cmd.method = "OnAddRegVFSToTree";
}

function OnAddRegVFSToTree(scriptCmdData) {
    var tree = scriptCmdData.func.sourcetab.tree;
    tree.AddItem("reg:///HKCU", "Registry (HKCU)", "regedit.exe,0");
}
```

### 5. 文件类型注册（自动化方案）

**问题**: 无法自动注册到文件类型系统

**自动化方案**:

```cpp
// 在 VFS_Configure 中调用 Opus 命令行注册
void VFS_Configure(HWND hwndParent)
{
    // 构建注册命令
    WCHAR szCmd[MAX_PATH] = L"";
    StringCchPrintfW(szCmd, MAX_PATH, 
        L"dopusrt.exe /cmd Prefs SET \"Zip & Other Archives\\Archive and VFS Plugins\\MyVFSPlugin\"=1");
    
    // 执行注册
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpFile = L"dopusrt.exe";
    sei.lpParameters = szCmd + 10; // 跳过 "dopusrt.exe /cmd "
    sei.nShow = SW_HIDE;
    ShellExecuteExW(&sei);
}
```

### 6. 调试困难（2026 增强方案）

**增强调试方案**:

**方案 1 - 远程调试**:

```cpp
#ifdef _DEBUG
    // 在 VFS_Init 中添加远程调试连接
    DebugActiveProcess(GetCurrentProcessId());
    // 或使用远程调试器
    WCHAR szDebugger[MAX_PATH] = L"";
    StringCchPrintfW(szDebugger, MAX_PATH, 
        L"vsdebugger.exe /attach:%d", GetCurrentProcessId());
    ShellExecuteW(NULL, L"open", szDebugger, NULL, NULL, SW_SHOWNORMAL);
#endif
```

**方案 2 - 日志系统**:

```cpp
#define VFS_LOG(message, ...) \
{ \
    WCHAR szLog[1024] = L""; \
    StringCchPrintfW(szLog, 1024, L"[MyVFS] " message, __VA_ARGS__); \
    OutputDebugStringW(szLog); \
}

// 使用示例
VFS_LOG(L"ReadDirectory: %s", lpRDD->lpszDir);
```

**方案 3 - 使用 DebugView 捕获日志**:
1. 下载 DebugView（Sysinternals 工具）
2. 以管理员身份运行 DebugView
3. 勾选 Capture → Capture Win32
4. 在插件代码中使用 `OutputDebugStringW` 输出日志

### 7. SDK 示例补充（2026）

GPSoftware 已在 2026 年更新了 SDK，提供:

- Visual Studio 2022 Solution 模板
- CMake 配置示例
- 完整的 VFS 插件示例（RegVFS）
- 调试配置文件

**获取地址**: https://resource.dopus.com/t/opus-plugin-sdk-2026-update/59015

### 8. 新增已知限制（2026）

1. **异步操作限制**: VFS 插件的异步目录读取在 Opus 13 中存在竞态条件
2. **64 位兼容性**: 部分老插件在 64 位 Opus 中存在指针截断问题
3. **长路径支持**: 超过 260 字符的路径需要特殊处理
4. **权限继承**: VFS 插件默认继承 Opus 的权限，无法提升权限

---

## 附录（新增）

### VFS 插件版本兼容性矩阵

| Opus 版本 | VFS API 版本 | 支持的编译器 | 备注 |
|-----------|---------------|--------------|------|
| 10.x | 1 | VS2010+ | 仅 32 位 |
| 11.x | 1 | VS2012+ | 32/64 位 |
| 12.x | 2 | VS2015+ | 完整 64 位支持 |
| 13.x | 2 | VS2019+ | 多线程 VFS 支持 |

### 常用 VFS 标志常量

```cpp
// VFS 能力标志
#define VFSCAPABILITY_RANDOMSEEK    0x0001  // 支持随机读写
#define VFSCAPABILITY_WRITE         0x0002  // 支持写入
#define VFSCAPABILITY_DELETE        0x0004  // 支持删除
#define VFSCAPABILITY_RENAME        0x0008  // 支持重命名
#define VFSCAPABILITY_MOVE          0x0010  // 支持移动
#define VFSCAPABILITY_DIRECTORY     0x0020  // 支持目录操作

// VFS 初始化标志
#define VFSF_CANCONFIGURE           0x0001  // 支持配置
#define VFSF_CANSHOWABOUT           0x0002  // 支持关于对话框
#define VFSF_USBSAFE                0x0004  // USB 安全模式
#define VFSF_NOUNICODE              0x0008  // 不支持 Unicode（不推荐）

// 读取目录操作类型
#define VFSREAD_NORMAL              0x0000  // 正常读取
#define VFSREAD_RELOAD              0x0001  // 重新加载
#define VFSREAD_FREEDIR             0x0002  // 释放目录
#define VFSREAD_CHANGEDIR           0x0003  // 更改目录
#define VFSREAD_FREEDIRCLOSE        0x0004  // 关闭时释放
#define VFSREAD_ASYNC               0x0005  // 异步读取
```

### 插件安装与分发

#### 安装方法

**方法 1 - 手动安装**:
1. 将编译后的 DLL 复制到: `%ProgramFiles%\GPSoftware\Directory Opus\Plugins\`
2. 重启 Directory Opus
3. 在 Preferences → Zip & Other Archives → Archive and VFS Plugins 中启用插件

**方法 2 - 自动安装程序**:

```cpp
// 安装插件的示例代码
BOOL InstallVFSPlugin(LPCWSTR lpszPluginPath)
{
    WCHAR szOpusPluginsDir[MAX_PATH] = L"";
    
    // 获取 Opus 插件目录
    if (!SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, szOpusPluginsDir))
    {
        StringCchCatW(szOpusPluginsDir, MAX_PATH, L"\\GPSoftware\\Directory Opus\\Plugins\\");
        
        // 创建目录（如果不存在）
        CreateDirectoryW(szOpusPluginsDir, NULL);
        
        // 复制插件 DLL
        WCHAR szDestPath[MAX_PATH] = L"";
        StringCchCopyW(szDestPath, MAX_PATH, szOpusPluginsDir);
        StringCchCatW(szDestPath, MAX_PATH, L"MyVFSPlugin.dll");
        
        return CopyFileW(lpszPluginPath, szDestPath, FALSE);
    }
    
    return FALSE;
}
```

#### 分发建议

1. **打包格式**: 使用 ZIP 压缩包，包含:
   - 插件 DLL（32/64 位）
   - 安装说明
   - 许可证文件
   - 示例配置

2. **版本兼容性**:
   - 明确标注支持的 Opus 版本
   - 提供 32 位和 64 位版本
   - 测试 Opus 12 和 13 兼容性

3. **发布到社区**:
   - 在 resource.dopus.com 的 Viewer/VFS Plugins 分类发布
   - 提供源码（可选）
   - 包含详细的使用说明和截图
   - 维护更新日志

---

## 更新日志

- **2026-05-08**: 更新 resource.dopus.com 社区资源，从论坛获取最新帖子信息，包括 VFS Plugin Help、Native WebDAV Support 和 Rclone Integration 的详细内容，更新阅读数、点赞数和社区反馈；完善 VFS 插件已知限制描述
- **2026-05-10**: 修正"上下文菜单动词限制"章节，补充关键修复：VFSCONTEXTMENUITEM 的 lpszCommand 自定义动词必须以 `$` 前缀开头，否则 DOpus 会将其当作内部命令执行而非传递给 VFS_ContextVerbW；删除不存在的 VFSVERB_EXECUTE 标志引用
- **2026-05-08**: 新增 resource.dopus.com 社区资源章节、VFS 插件已知限制与常见问题章节，补充论坛关键帖子索引和 JSON API 访问方法