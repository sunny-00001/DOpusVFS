# RcloneVFS 更新日志

## v1.0.2 (2026-05-06)

### 🐛 Bug 修复

#### 1. 修复双击文件夹无反应问题
- **问题**: 双击文件夹没有反应，无法进入子目录
- **修复**: 
  - 改进 `ParseRclonePath` 函数，正确处理路径末尾的斜杠
  - 确保路径格式正确，避免路径解析错误
  - 代码位置: `ParseRclonePath` 函数

#### 2. 修复导航返回一直回退到桌面问题
- **问题**: 无论处于哪一级目录，点击导航返回都会一直回退到桌面
- **修复**:
  - 重新实现 `VFS_GetPathParentRootW` 函数的路径判断逻辑
  - 当路径已经是 `rclone://` 时返回 FALSE（表示已到达根路径）
  - 当路径为 `rclone://remote` 时返回 `rclone://`（停留在 rclone 根目录）
  - 当路径为 `rclone://remote/path` 时正确返回父路径
  - 代码位置: `VFS_GetPathParentRootW` 函数

### 🔧 技术改进

- 改进路径解析函数，正确处理各种路径格式
- 优化导航逻辑，确保路径层级正确
- 修改编译输出路径到 VFS 根目录 (`d:\VFS\`)

### 📦 编译信息

- **编译器**: Visual Studio 2022 Build Tools
- **目标平台**: x64
- **文件大小**: 329,216 字节 (321 KB)
- **输出路径**: `d:\VFS\RcloneVFS.dll`
- **导出函数**: 26 个 VFS API

---

## v1.0.1 (2026-05-06)

### 🐛 Bug 修复

#### 1. 修复地址栏图标显示问题
- **问题**: 地址栏显示的是系统默认图标，而不是 rclone 原生图标
- **修复**: 
  - 优先从 rclone.exe 提取图标资源
  - 如果 rclone.exe 没有图标资源，则回退到系统云存储图标
  - 代码位置: `VFS_IdentifyW` 函数

#### 2. 修复双击文件无反应问题
- **问题**: 双击文件没有反应，但右键打开按钮正常工作
- **修复**:
  - 改进 `VFS_CreateFileW` 函数的错误处理
  - 添加详细的错误代码设置 (ERROR_PATH_NOT_FOUND, ERROR_INVALID_PATH, ERROR_OPEN_FAILED)
  - 确保文件打开失败时返回正确的错误信息
  - 代码位置: `VFS_CreateFileW` 函数

#### 3. 修复导航返回一直回退到桌面问题
- **问题**: 点击导航返回按钮时，一直回退到桌面而不是停留在 rclone 根目录
- **修复**:
  - 修正 `VFS_GetPathParentRootW` 函数的路径处理逻辑
  - 当路径为 `rclone://remote` 时，返回 `rclone://` 而不是继续向上导航
  - 当路径为 `rclone://` 时，返回 FALSE 表示已到达根目录
  - 代码位置: `VFS_GetPathParentRootW` 函数

### 🔧 技术改进

- 添加 `ERROR_INVALID_PATH` 和 `ERROR_OPEN_FAILED` 错误代码定义
- 改进错误处理和状态返回机制
- 优化路径解析和导航逻辑

### 📦 编译信息

- **编译器**: Visual Studio 2022 Build Tools
- **目标平台**: x64
- **文件大小**: 328,704 字节 (321 KB)
- **导出函数**: 26 个 VFS API

---

## v1.0.0 (2026-05-06)

### ✨ 初始版本

- 实现完整的 Directory Opus VFS 插件接口
- 集成 rclone 命令行工具
- 支持所有 rclone 兼容的云存储服务
- 实现文件浏览、复制、移动、删除、重命名等操作
- 支持目录创建和删除
- 完整的路径管理和导航支持
