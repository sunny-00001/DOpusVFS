# DOpus VFS Plugins

Directory Opus VFS (Virtual File System) 插件集合，为 Directory Opus 文件管理器扩展虚拟文件系统支持。

## 插件列表

| 插件 | 说明 | 状态 |
|------|------|------|
| **CodeupVFS** | 云效 Git 仓库虚拟文件系统 | 完成 |
| **RcloneVFS** | Rclone 云存储虚拟文件系统 | 完成 |
| **GitHubVFS** | GitHub 仓库虚拟文件系统 | 完成 |
| **ServiceVFS** | Windows 服务管理虚拟文件系统 | 完成 |
| **ProcessVFS** | 进程管理虚拟文件系统 | 完成 |
| **EnvVFS** | 环境变量虚拟文件系统 | 完成 |
| **RegVFS** | 注册表虚拟文件系统 | 完成 |
| **FirewallVFS** | Windows 防火墙管理虚拟文件系统 | 完成 |
| **WIFIVFS** | WiFi 网络管理虚拟文件系统 | 完成 |
| **StockVFS** | 股票行情虚拟文件系统 | 完成 |
| **TaskSchedulerVFS** | 任务计划虚拟文件系统 | 完成 |
| **DOpusWebDAV** | WebDAV 虚拟文件系统 | 完成 |
| **Dopusrclone** | Rclone 简化版插件 | 完成 |
| **IPFSVFS** | IPFS 分布式文件系统 | 开发中 |

## 目录结构

```
VFS/
├── scripts/              # DOpus 脚本集合
├── toolbars/             # 工具栏配置文件
├── docs/                 # 项目文档
├── sdk/                  # VFS Plugin SDK
│   ├── docs/             # SDK 文档
│   ├── headers/          # SDK 头文件
│   └── examples/         # 示例插件
├── examples/             # 第三方示例和资源
├── CodeupVFS/            # 云效插件
├── RcloneVFS/            # Rclone 插件
├── [其他插件]/           # 更多 VFS 插件
└── README.md             # 本文件
```

## 构建说明

### 前置要求

- Visual Studio 2022 或 Build Tools
- CMake 3.15+
- Directory Opus 12+

### 构建单个插件

```batch
cd PluginName
build.bat
```

### 使用 CMake 构建

```batch
cd PluginName
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

输出 DLL 将生成在项目根目录。

## 安装说明

将编译好的 `.dll` 文件复制到 Directory Opus 的 VFSPlugins 目录：

```
C:\Program Files\GPSoftware\Directory Opus\VFSPlugins\
```

或在 DOpus 设置中配置自定义插件路径。

## SDK 文档

VFS Plugin SDK 位于 `sdk/docs/` 目录：

- [VFS Plugin SDK.pdf](sdk/docs/VFS%20Plugin%20SDK.pdf) - 官方 SDK 文档
- [VFS_Plugin_SDK.md](sdk/docs/VFS_Plugin_SDK.md) - Markdown 版本

## 相关脚本

`scripts/` 目录包含配套的 DOpus 脚本：

| 脚本 | 说明 |
|------|------|
| ServicesVFS.js | 服务管理脚本 |
| EnvVFS2.js | 环境变量脚本 |
| WIFIVFS.js | WiFi 管理脚本 |

## 工具栏配置

`toolbars/` 目录包含预配置的工具栏：

| 文件 | 说明 |
|------|------|
| Rclone.dop | Rclone 操作工具栏 |
| ServiceToolbar.dop | 服务管理工具栏 |
| EnvToolbar.dop | 环境变量工具栏 |
| WIFIToolbar.dop | WiFi 管理工具栏 |

## 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

各插件可能包含第三方代码，请查看各插件目录下的 LICENSE 文件。

## 贡献指南

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/new-plugin`)
3. 提交改动 (`git commit -am 'Add new plugin'`)
4. 推送分支 (`git push origin feature/new-plugin`)
5. 创建 Pull Request

## 作者

sunny-00001

## 链接

- [Directory Opus 官网](https://www.gpsoft.com.au/)
- [VFS Plugin SDK](https://www.gpsoft.com.au/)
- [GitHub 仓库](https://github.com/sunny-00001/DOpusVFS)