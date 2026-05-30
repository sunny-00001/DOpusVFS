# Directory Opus 插件资源集合

## 简介
这是 Directory Opus 相关插件、脚本和开发资源的集合。

## VFS 插件项目

### C++ VFS 插件
- **directory_opus_amiga_plugin** - 为 Directory Opus 12 添加 ADF/HDF 支持的 VFS 插件
  - 位置: `directory_opus_amiga_plugin/`
  - 语言: C++
  - 功能: 支持 Amiga 磁盘镜像浏览

- **DOpusWebDAV** - WebDAV VFS 插件
  - 位置: `dopus-webdav-vfs/`
  - 语言: C++
  - 功能: 支持 WebDAV 协议访问

- **RegVFS** - 注册表 VFS 插件（已在上层目录）
  - 位置: `../RegVFS/`
  - 语言: C++
  - 功能: 将 Windows 注册表作为文件系统浏览

### 查看器/其他 C++ 插件
- **DirectoryOpus-CSV-viewer-plugin** - CSV 查看器插件
  - 位置: `DirectoryOpus-CSV-viewer-plugin/`
  - 语言: C++
  - 功能: 预览 CSV 文件

- **IbDOpusExt** - Directory Opus 扩展
  - 位置: `IbDOpusExt/`
  - 语言: C# / C++
  - 功能: 多种增强功能，包括查看器插件和 VFS 插件修复

## 脚本插件

### JavaScript 脚本
- **DirectoryOpus-TabLabelizer-plugin** - 标签页命名插件
  - 位置: `DirectoryOpus-TabLabelizer-plugin/`
  - 语言: JavaScript
  - 功能: 为标签页显示更详细的路径信息

- **DirectoryOpus-FileLockingInfo-columns-and-dialog** - 文件锁定信息插件
  - 位置: `DirectoryOpus-FileLockingInfo-columns-and-dialog/`
  - 语言: JavaScript
  - 功能: 显示文件锁定信息

- **DOpus.ext** - 扩展脚本集合
  - 位置: `DOpus.ext/`
  - 语言: JavaScript
  - 功能: 多个实用脚本，包括标签页管理、命令增强等

- **IbDOpusScripts** - 脚本集合
  - 位置: `IbDOpusScripts/`
  - 语言: JavaScript / VBScript
  - 功能: 大量实用脚本和按钮

### VBScript 脚本
- **DOpus-Script** - 旧版脚本集合
  - 位置: `DOpus-Script/`
  - 语言: VBScript
  - 功能: 各种按钮、主题和脚本

## 其他集成
- **Listary.FileAppPlugin.DirectoryOpus** - Listary 集成插件
  - 位置: `Listary.FileAppPlugin.DirectoryOpus/`
  - 语言: C#
  - 功能: Listary 与 Directory Opus 集成

## 开发资源

### 官方资源
- **Directory Opus 官网**: https://www.gpsoft.com.au/
- **官方文档**: https://docs.dopus.com/
- **资源中心**: https://resource.dopus.com/
- **开发者论坛**: https://resource.dopus.com/c/viewer-vfs-plugins/34
- **官方支持**: https://support.gpsoft.com.au/

### 社区资源
- **Pretentious Name 插件源码**: https://www.pretentiousname.com/pnop_source/
  - 包含 Audio Tags、Raw Digital Camera、GIFAnim、NFO、Targa 等插件源码
- **插件列表**: https://www.pretentiousname.com/opus_plugin_list/
  - 完整的 Directory Opus 插件列表

## 内置 VFS 插件参考
Directory Opus 内置支持的归档格式：
- Zip（内置）
- 7z、RAR、ISO 等（通过 opus7zip.dll）

## 学习资源
1. SDK 文档（modules.doc、hooks.doc）
2. 研究现有插件源码
3. 加入开发者论坛讨论

## 开发最佳实践
- 同时提供 32 位和 64 位版本
- 支持 Unicode
- 正确的错误处理和资源管理
- 提供配置界面（如需要）
