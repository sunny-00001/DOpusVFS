# EnvVFS - Directory Opus 环境变量 VFS 插件

一个用于 Directory Opus 的虚拟文件系统插件，让您可以像浏览文件一样浏览和管理 Windows 环境变量。

## 功能特性

- **直观浏览** - 在 Directory Opus 中以熟悉的文件浏览方式查看所有环境变量
- **自定义列** - 显示变量值、类型、来源等信息的自定义列
- **右键菜单** - 完整的右键菜单，支持编辑、复制、删除等操作
- **系统集成** - 可直接打开系统环境变量设置对话框
- **中文界面** - 所有文本和界面均为中文显示
- **配置对话框** - 可通过插件配置进行个性化设置
- **关于对话框** - 显示插件信息和版本

## 安装说明

### 从源码构建

1. 确保已安装 Visual Studio 2019 或更高版本
2. 打开 Visual Studio 开发者命令提示符
3. 导航到 `EnvVFS` 目录
4. 运行 `build.bat` 或使用 CMake 构建
5. 编译成功后，`EnvVFS.dll` 将输出到父目录

### 安装到 Directory Opus

1. 将 `EnvVFS.dll` 复制到 Directory Opus 的 VFS 插件目录（通常为 `C:\Program Files\GPSoftware\Directory Opus\VFSPlugins`）
2. 重启 Directory Opus
3. 在地址栏输入 `env://` 即可开始使用

## 使用方法

### 基本浏览

- 在 Directory Opus 地址栏输入 `env://` 按回车
- 您将看到所有系统、用户和临时环境变量的列表
- 双击任意变量可查看其详细信息

### 自定义列

右键点击列标题，选择"列"，您可以启用以下自定义列：
- **值** - 显示环境变量的值
- **类型** - 显示变量类型（字符串、可展开字符串等）
- **来源** - 显示变量来源（系统、用户或临时）
- **系统** - 标记是否为系统变量

### 右键菜单功能

右键点击任意环境变量，您可以：
- **编辑** - 在默认文本编辑器中打开变量值进行编辑
- **复制值** - 将变量值复制到剪贴板
- **复制变量名** - 将变量名复制到剪贴板
- **打开系统环境变量设置** - 打开 Windows 系统环境变量设置对话框
- **删除** - 删除该环境变量（需要管理员权限）

### 配置对话框

在 Directory Opus 插件配置中找到 EnvVFS，点击配置按钮可访问插件配置。

## 技术说明

### 架构

- **VFS 接口** - 完全实现 Directory Opus VFS 插件接口
- **Windows API** - 使用 Windows Registry API 读写环境变量
- **Unicode 支持** - 完全支持 Unicode 字符
- **资源文件** - 使用资源文件管理对话框和字符串

### 文件结构

```
EnvVFS/
├── include/              # VFS 插件头文件
│   ├── vfs_plugins.h
│   ├── plugin_support.h
│   └── opusplug.h
├── src/
│   └── EnvVFS.cpp      # 主实现文件
├── CMakeLists.txt       # CMake 构建配置
├── build.bat           # 批处理构建脚本
├── EnvVFS.def          # DLL 导出定义
├── resource.h          # 资源头文件
├── resource.rc         # 资源文件
└── README.md           # 本文档
```

### 开发环境

- **语言**: C++ 17
- **编译器**: MSVC (Visual Studio)
- **目标**: Windows x64
- **运行时**: Multi-threaded (MT)

## 系统要求

- Windows 7 或更高版本（64位）
- Directory Opus 12 或更高版本

## 构建方法

### 使用 build.bat（推荐）

```cmd
cd EnvVFS
build.bat
```

### 使用 CMake

```cmd
cd EnvVFS
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Release
```

## 相关项目

- **RcloneVFS** - 云存储集成插件
- **ServersVFS** - Windows 服务管理插件

## 许可证

Copyright © 2026. 保留所有权利。

## 免责声明

本插件按"原样"提供，不提供任何明示或暗示的保证。使用本插件造成的任何损失，作者不承担责任。操作环境变量时请谨慎，建议在修改前备份注册表。

## 支持与反馈

如有问题或建议，欢迎反馈。

---

**祝您使用愉快！**
