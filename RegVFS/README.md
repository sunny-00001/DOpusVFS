# Registry VFS Plugin for Directory Opus

Windows 注册表虚拟文件系统插件，允许在 Directory Opus 中以文件系统的方式浏览和管理 Windows 注册表。

## 功能特性

### 核心功能
- 将 Windows 注册表映射为虚拟文件系统
- 支持浏览所有根键：`HKEY_CLASSES_ROOT`、`HKEY_CURRENT_USER`、`HKEY_LOCAL_MACHINE`、`HKEY_USERS`、`HKEY_CURRENT_CONFIG`
- 注册表项显示为文件夹，注册表值显示为文件
- 使用 regedit.exe 图标作为插件图标

### 完整读写支持
- **读取注册表值**：双击值文件即可查看内容
- **写入注册表值**：编辑值文件并保存，自动写入注册表
- **创建注册表项**：像创建文件夹一样创建新的注册表项
- **删除操作**：删除注册表值和注册表项
- **重命名功能**：重命名注册表项和值
- **拖拽支持**：复制和移动注册表项

### 自定义列显示

插件提供 8 个自定义列，方便查看注册表信息：

| 列名 | 键名 | 说明 |
|------|------|------|
| 值类型 | `regtype` | 注册表值类型（REG_SZ, REG_DWORD 等），注册表项显示为"注册表项" |
| 数据大小 | `regsize` | 数据大小（字节） |
| 值预览 | `regpreview` | 值预览（字符串、数值、十六进制等） |
| 所属根键 | `regroot` | 所属根键名称（如 HKEY_LOCAL_MACHINE） |
| 完整路径 | `regfullpath` | 注册表完整路径 |
| 子项数量 | `regsubkeys` | 注册表项的子项数量 |
| 值数量 | `regvaluecount` | 注册表项的值数量 |
| 修改时间 | `regmodified` | 注册表项的最后修改时间 |

**查看自定义列方法**：在文件列表中右键点击列标题 → 选择"更多..." → 找到 Registry VFS 相关的列 → 勾选需要的列

### 右键菜单

| 菜单项 | 功能 |
|--------|------|
| 在注册表编辑器中打开 | 启动 regedit 并导航到当前项 |
| 搜索... | 搜索键名、值名、值内容 |
| 复制注册表路径 | 复制当前项的标准注册表路径到剪贴板 |
| 复制注册表项... | 复制当前注册表项（创建副本） |
| 添加到收藏夹 | 将当前路径添加到收藏夹 |
| 查看收藏夹 | 查看已收藏的注册表路径 |
| 清空收藏夹 | 清除所有收藏项 |
| 批量导出子项... | 将所有子项导出为 .reg 文件 |
| 批量复制子项... | 复制所有子项（创建副本） |
| 批量删除子项... | 删除所有子项（需双重确认） |
| 统计子项信息 | 显示当前目录和子项的统计信息 |
| 新建注册表项 | 创建新的注册表子项 |
| 新建字符串值 | 创建 REG_SZ 类型的值 |
| 新建 DWORD 值 | 创建 REG_DWORD 类型的值 |
| 新建 QWORD 值 | 创建 REG_QWORD 类型的值 |
| 新建二进制值 | 创建 REG_BINARY 类型的值 |
| 新建可扩展字符串值 | 创建 REG_EXPAND_SZ 类型的值 |
| 导出注册表项... | 将当前项导出为 .reg 文件 |
| 属性 | 显示注册表项/值的详细属性和权限信息 |

### 智能特性
- **自动类型检测**：写入时自动判断数据类型（REG_DWORD、REG_QWORD、REG_SZ、REG_BINARY）
- **值预览格式化**：根据类型智能显示值内容
  - REG_SZ/REG_EXPAND_SZ：显示字符串
  - REG_DWORD：显示十六进制和十进制值
  - REG_QWORD：显示64位数值
  - REG_MULTI_SZ：显示多个字符串（分号分隔）
  - REG_BINARY：显示十六进制数据
- **文件描述**：鼠标悬停显示值预览
- **属性对话框**：显示注册表项/值的详细信息和权限

## 项目结构

```
RegVFS/
├── include/
│   ├── vfs_plugins.h       # Directory Opus VFS 插件 API 头文件
│   ├── plugin_support.h    # 插件支持库头文件
│   └── resource.h          # 资源定义头文件
├── src/
│   ├── RegistryVFS.cpp     # 注册表 VFS 插件主程序
│   └── RegistryVFS.def     # DLL 导出定义
├── CMakeLists.txt          # CMake 构建配置
├── compile.bat             # 编译脚本
└── README.md               # 本文档
```

## 系统要求

- Windows 7/8/10/11 (64位)
- Directory Opus 12 或更高版本
- Visual Studio 2019 或更高版本（用于编译）
- CMake 3.16 或更高版本

## 编译方法

### 使用编译脚本

```powershell
cd RegVFS
compile.bat
```

编译完成后，`RegistryVFS.dll` 将生成在上级目录中。

### 使用 CMake

```powershell
cd RegVFS
mkdir build
cd build
cmake -G Ninja -A x64 ..
cmake --build . --config Release
```

## 安装

1. 编译项目生成 `RegistryVFS.dll`
2. 将 DLL 复制到 Directory Opus 的 VFS 插件目录：
   ```
   D:\Dopus\VFSPlugins\
   ```
3. 重启 Directory Opus

## 使用方法

在 Directory Opus 地址栏中输入 `reg://` 前缀的路径即可浏览注册表：

| 路径 | 说明 |
|------|------|
| `reg://` | 显示所有根键列表 |
| `reg://HKEY_LOCAL_MACHINE` | 浏览 HKEY_LOCAL_MACHINE 根键 |
| `reg://HKEY_CURRENT_USER/Software` | 浏览当前用户软件配置 |
| `reg://HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion` | 浏览 Windows 版本信息 |

## 映射规则

| 注册表元素 | 文件系统映射 | 操作支持 |
|-----------|-------------|---------|
| 根键 (HKEY_*) | 顶级目录 | 只读 |
| 注册表项 (Key) | 文件夹 | 创建、删除、重命名、复制、移动 |
| 注册表值 (Value) | 文件 | 读取、写入、删除、重命名 |
| 默认值 ((Default)) | 文件名为 "(Default)" | 读取、写入 |

## 支持的注册表值类型

| 类型 | 说明 | 预览格式 |
|------|------|---------|
| REG_NONE | 无类型 | (空) |
| REG_SZ | 字符串 | 文本内容 |
| REG_EXPAND_SZ | 可扩展字符串 | 文本内容 |
| REG_BINARY | 二进制数据 | 十六进制显示 |
| REG_DWORD | 32位整数 | 0xXXXXXXXX (十进制) |
| REG_DWORD_BIG_ENDIAN | 大端32位整数 | 十六进制显示 |
| REG_LINK | 符号链接 | 十六进制显示 |
| REG_MULTI_SZ | 多字符串 | 字符串1; 字符串2; ... |
| REG_QWORD | 64位整数 | 0xXXXXXXXXXXXXXXXX (十进制) |

## 技术实现

### 核心 API

插件实现了完整的 VFS 接口：

| 函数 | 功能 |
|------|------|
| `VFS_Init` / `VFS_Uninit` | 插件初始化/反初始化 |
| `VFS_Create` / `VFS_Destroy` | 创建/销毁插件实例 |
| `VFS_IdentifyW` | 返回插件标识信息 |
| `VFS_GetPrefixListW` | 返回支持的路径前缀 |
| `VFS_GetCustomColumnsW` | 返回自定义列定义（8列） |
| `VFS_ReadDirectoryW` | 枚举注册表项和值（含自定义列数据） |
| `VFS_GetFileInformationW` | 获取文件信息 |
| `VFS_GetFileDescriptionW` | 获取文件描述（值预览） |
| `VFS_CreateFileW` | 打开注册表值 |
| `VFS_ReadFile` / `VFS_WriteFile` | 读写注册表值数据 |
| `VFS_SeekFile` | 定位读写位置 |
| `VFS_CloseFile` | 关闭文件（保存修改） |
| `VFS_DeleteFileW` | 删除注册表值 |
| `VFS_CreateDirectoryW` | 创建注册表项 |
| `VFS_RemoveDirectoryW` | 删除注册表项 |
| `VFS_RenameFileW` | 重命名注册表项 |
| `VFS_MoveFileW` | 移动注册表项 |
| `VFS_GetPathDisplayNameW` | 获取路径显示名称 |
| `VFS_GetPathParentRootW` | 获取父路径 |
| `VFS_PropGetW` | 获取插件属性 |
| `VFS_PropertiesW` | 显示属性对话框 |
| `VFS_GetFileSizeW` | 获取文件大小 |
| `VFS_GetLastError` | 获取最后错误 |
| `VFS_GetContextMenuW` | 返回右键菜单项 |
| `VFS_ContextVerbW` | 执行右键菜单命令 |
| `VFS_Configure` | 显示配置对话框 |
| `VFS_About` | 显示关于对话框 |
| `VFS_USBSafe` | USB 安全模式支持 |

### 路径解析

插件使用 `reg://` 前缀标识注册表路径，格式为：

```
reg://HKEY_ROOT_KEY/subkey1/subkey2/value_name
```

### 值类型自动检测

写入注册表值时，插件会自动检测数据类型：

| 数据特征 | 推断类型 |
|---------|---------|
| 4 字节数据 | REG_DWORD |
| 8 字节数据 | REG_QWORD |
| Unicode 字符串（以空字符结尾） | REG_SZ |
| 其他 | REG_BINARY |

## 权限说明

- 修改 `HKEY_LOCAL_MACHINE` 需要管理员权限
- 修改 `HKEY_CURRENT_USER` 通常不需要额外权限
- 某些系统关键注册表项可能受保护，无法修改
- 属性对话框中可查看注册表项的权限信息

## 安全警告

⚠ **警告**：不当修改注册表可能导致系统不稳定或无法启动。请确保：
- 修改前备份重要数据
- 了解修改的注册表项的作用
- 不要随意删除系统关键注册表项
- 批量删除操作需要双重确认

## 已知限制

1. 不支持跨根键移动（如从 HKEY_CURRENT_USER 移动到 HKEY_LOCAL_MACHINE）
2. 重命名注册表项时，只能修改项名，不能移动到其他路径
3. 删除注册表项时，如果项内有子项，可能需要先删除子项
4. 搜索功能可能在大范围搜索时耗时较长
5. 收藏夹数据保存在内存中，重启 DOpus 后会丢失

## 版本历史

### v5.0.0 (2026-05-08)

- ✅ 新增：8个自定义列（值类型、数据大小、值预览、所属根键、完整路径、子项数量、值数量、修改时间）
- ✅ 新增：右键菜单增强（搜索、收藏夹、批量操作、属性）
- ✅ 新增：拖拽支持（复制和移动注册表项）
- ✅ 新增：属性对话框（详细信息和权限）
- ✅ 新增：搜索功能（搜索键名、值名、值内容）
- ✅ 新增：收藏夹功能
- ✅ 新增：批量操作（导出、复制、删除、统计）
- ✅ 新增：VFS_PropertiesW 导出函数
- ✅ 修复：VFS_ContextVerbW 双击处理（正确返回 VFSCVRES_CHANGEDIR）
- ✅ 修复：DLL 版本同步到 DOpus 插件目录

### v3.0.0 (2026-05-06)

- ✅ 完全重写使用官方 VFS API
- ✅ 新增：自定义列支持
- ✅ 新增：文件描述（值预览）支持
- ✅ 新增：路径显示名称和父路径支持
- ✅ 新增：配置和关于对话框
- ✅ 新增：USB 安全模式支持

### v2.0.0 (2026-05-06)

- 新增：写入注册表值功能
- 新增：创建注册表项功能
- 新增：删除注册表值和项功能
- 新增：重命名注册表项和值功能
- 新增：智能值类型自动检测

### v1.0.0 (2026-05-06)

- 初始版本
- 支持浏览注册表
- 支持读取注册表值
- 只读模式

## 参考资料

- [Directory Opus 官方文档](https://docs.dopus.com/)
- [Directory Opus 资源中心](https://resource.dopus.com/)
- [Opus Plugins SDK](https://www.gpsoft.com.au/)
- [Windows Registry API](https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry)
