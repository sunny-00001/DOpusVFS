# ServiceVFS

Windows 服务管理虚拟文件系统插件，为 Directory Opus 提供服务管理功能。

## 功能

- 查看所有 Windows 服务
- 启动/停止/暂停/继续服务
- 查看服务状态、启动类型、描述
- 修改服务启动类型
- 查看服务依赖关系

## 构建

### 使用 build.bat

```batch
build.bat
```

### 使用 CMake

```batch
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 安装

将生成的 `ServiceVFS.dll` 复制到 Directory Opus 的 VFS 插件目录：

```
%APPDATA%\GPSoftware\Directory Opus\VFS\
```

## 目录结构

```
ServiceVFS/
├── include/          # SDK 头文件
│   ├── vfs plugins.h
│   └── plugin support.h
├── src/              # 源代码
│   ├── ServiceVFS.cpp
│   ├── ServiceVFS.def
│   ├── resource.h
│   └── resource.rc
├── CMakeLists.txt    # CMake 构建配置
├── build.bat         # 批处理构建脚本
└── README.md         # 本文件
```

## 使用方法

在 Directory Opus 中访问：

```
ServiceVFS://
```

将显示所有 Windows 服务列表，可以右键菜单进行操作。