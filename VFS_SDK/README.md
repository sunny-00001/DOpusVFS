# VFS_SDK - Directory Opus VFS 插件开发 SDK

本目录包含开发 Directory Opus VFS（虚拟文件系统）插件所需的 SDK 及参考资源。

## 目录结构

```
VFS_SDK/
├── headers/              # SDK 头文件（核心）
│   ├── plugin support.h   # 插件支持库头文件
│   ├── vfs plugins.h      # VFS 插件 API 头文件
│   └── viewer plugins.h   # Viewer 插件 API 头文件
├── docs/                 # SDK 文档
│   └── VFS Plugin SDK.pdf # 官方 VFS 插件开发文档（PDF）
└── examples/             # 示例 VFS 插件源码
    ├── firy_plugin/      # Firy VFS 插件（ADF/D64/FAT，C++/CMake）
    ├── PNOpusPlugins/    # PNOpus 多插件示例（audiotags/dcrawrap 等）
    └── maya_iff_source/  # Maya IFF VFS 插件示例
```

## SDK 头文件说明

### plugin support.h
插件支持库，提供插件与 Directory Opus 之间的通信基础设施，包括：
- 插件初始化/清理函数
- Opus 字符串管理
- 插件对话框支持
- 内存管理辅助函数

### vfs plugins.h
VFS（虚拟文件系统）插件 API，定义：
- `GetPluginInfo`、`GetNextFormat`、`Identify`、`GetProperty` 等导出函数
- `VFSData` 结构体（插件与 Opus 之间的主要数据结构）
- 文件操作函数（OpenDir、NextDirEntry、CloseDir 等）
- 架构标志和属性定义

### viewer plugins.h
Viewer 插件 API（与 VFS 插件共用同一套基础设施）

## 示例插件说明

### firy_plugin
- GitHub: https://github.com/segrax/directory_opus_firy_plugin
- 功能: ADF/D64/FAT12/16/32 文件系统 VFS 插件
- 构建: CMake + Visual Studio 2022
- 特点: 现代化构建系统，代码结构清晰

### PNOpusPlugins (from pretentiousname.com)
- 包含多个插件示例：
  - **audiotags**: 音频文件标签 VFS 插件（FLAC/Vorbis）
  - **dcrawrap**: RAW 数码相机图像 Viewer 插件
- 特点: 完整的 Visual Studio 项目文件，老牌插件参考

### maya_iff_source
- Maya IFF 图像文件 Viewer 插件
- 特点: 简洁的单一源文件实现

## 官方 SDK 获取

官方 Plugin SDK 可从 GPSoftware 官网获取：
- 官网下载页: https://www.gpsoft.com.au/DScripts/Download.asp
- Resource Centre 讨论帖: https://resource.dopus.com/t/plugins-opus-plugin-sdk-updated-and-now-available/4373

注：官方 SDK 包含本文档已覆盖的头文件和 PDF 文档，以及更多示例源码。

## 开发参考

- Directory Opus 官方手册: https://docs.dopus.com
- Resource Centre（插件分享/讨论论坛）: https://resource.dopus.com/c/viewer-plugins/34
- 开发者论坛: https://resource.dopus.com（Developer 板块）

## 已有资源备注

- `D:/VFS/DOpusHelp.pdf` - Directory Opus 完整帮助文档（已存在于 VFS 目录）
- `D:/VFS/RcloneVFS/headers/` - 实际项目使用的 SDK 头文件（与本 SDK 一致）

## 更新时间

2026-05-08
