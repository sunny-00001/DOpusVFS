# RcloneVFS 插件 — 完整设计文档

> 版本: 1.0  
> 日期: 2026-05-11  
> 状态: 需求分析完成 / 方案设计完成  
> 基于: 多轮需求讨论的完整结论  
> 参考: GitHubVFS 完整设计文档（部分借鉴设计理念）

---

## 目录

1. [项目概述](#1-项目概述)
2. [设计原则](#2-设计原则)
3. [架构决策记录](#3-架构决策记录)
4. [当前代码状态](#4-当前代码状态)
5. [路径体系设计](#5-路径体系设计)
6. [自定义列体系设计](#6-自定义列体系设计)
7. [右键菜单体系设计](#7-右键菜单体系设计)
8. [能力检测体系设计](#8-能力检测体系设计)
9. [缓存策略设计](#9-缓存策略设计)
10. [DLL-脚本通信设计](#10-dll-脚本通信设计)
11. [存储配额功能设计](#11-存储配额功能设计)
12. [守护进程管理设计](#12-守护进程管理设计)
13. [数据结构定义](#13-数据结构定义)
14. [一期/二期/三期划分](#14-一期二期三期划分)
15. [rclone RC API 能力映射](#15-rclone-rc-api-能力映射)

---

## 1. 项目概述

### 1.1 目标

为 Directory Opus 开发 `rclone://` 虚拟文件系统插件，实现：

- 简洁直觉的路径体系（保持 `rclone://{remote}/{path}` 简单格式）
- 上下文感知的自定义列（原始数据 + 创造数据）
- 动态适配的右键菜单（根据云存储类型显示/隐藏功能）
- 智能缓存 + 预加载（多层缓存策略减少 API 调用）
- DLL + JScript 混合架构

### 1.2 核心设计理念

| 原则 | 说明 | 与 GitHubVFS 的区别 |
|------|------|---------------------|
| 列显示 + 菜单操作 | 只读数据用自定义列展示，操作用右键菜单执行 | 相同理念 |
| 路径即上下文 | 路径自动决定列集、菜单、行为 | rclone 路径更简单，只有两层上下文 |
| 动态能力适配 | 根据云存储类型动态显示/隐藏功能 | GitHubVFS 无此需求（GitHub API 统一） |
| 简单路径 | 不增加虚拟目录层，保持 `rclone://{remote}/{path}` | GitHubVFS 有复杂的虚拟目录体系 |
| 双击 = 最直觉动作 | 双击文件下载并打开，双击文件夹进入 | 相同理念 |

### 1.3 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                     Directory Opus UI                            │
│              (列显示 / 右键菜单 / 地址栏导航)                      │
├──────────────────────────────┬──────────────────────────────────┤
│    RcloneVFS.dll (核心)       │    RcloneVFS.js (脚本)           │
│  ┌────────────────────────┐  │  ┌────────────────────────────┐  │
│  │ 路径解析 ParsePath     │  │  │ 创造数据列计算             │  │
│  │ 目录枚举 ReadDir       │  │  │ 高级命令执行               │  │
│  │ 原始数据列填充         │  │  │ 分享链接管理（二期）       │  │
│  │ 右键菜单(核心操作)     │  │  │ 同步/校验对话框（二期）    │  │
│  │ RC API 通信            │  │  │ 回收站浏览（二期）         │  │
│  │ 缓存管理               │  │  │ 配置管理对话框（三期）     │  │
│  │ 守护进程管理           │  │  │ rclone CLI fallback        │  │
│  │ 存储配额查询           │  │  └────────────────────────────┘  │
│  │ 能力检测               │  │                                  │
│  └────────────────────────┘  │                                  │
├──────────────────────────────┴──────────────────────────────────┤
│                      通信层                                      │
│     JSON 缓存文件 │ INI 配置文件 │ 注册表                        │
├─────────────────────────────────────────────────────────────────┤
│                   rclone RC Daemon                               │
│           127.0.0.57:8657 │ WinHTTP 通信 │ Basic Auth            │
├─────────────────────────────────────────────────────────────────┤
│                   Cloud Storage APIs                             │
│        Google Drive │ OneDrive │ Dropbox │ S3 │ 50+ others      │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 设计原则

### 2.1 列显示 + 菜单操作

```
只读数据 → 自定义列显示 → 无需打开任何文件
操作执行 → 右键菜单 → 执行后列自动刷新

文件只用于两种场景：
1. 真实内容（下载到本地使用的文件）
2. 编辑后自动上传（智能编辑 + 自动同步）
```

### 2.2 路径即上下文

```
路径自动决定一切：

rclone://                    → 远程信息列集 + 远程管理菜单
rclone://gdrive/             → 文件信息列集 + 文件操作菜单
rclone://gdrive/Documents/   → 文件信息列集 + 文件操作菜单
```

### 2.3 动态能力适配

```
不同云存储类型决定可用功能：

Google Drive  → 分享链接 ✅ 回收站 ✅ 版本历史 ✅ 存储配额 ✅
OneDrive      → 分享链接 ✅ 回收站 ✅ 版本历史 ❌ 存储配额 ✅
Amazon S3     → 分享链接 ❌ 回收站 ❌ 版本历史 ❌ 存储配额 ❌
Dropbox       → 分享链接 ✅ 回收站 ❌ 版本历史 ✅ 存储配额 ✅

菜单和列根据能力动态显示/隐藏
```

### 2.4 双击 = 最直觉动作

| 项类型 | 双击行为 |
|--------|----------|
| 远程存储文件夹 | 进入远程根目录 |
| 云端文件夹 | 进入子目录 |
| 云端文件 | 下载并打开（关联程序），启动自动同步监控 |

---

## 3. 架构决策记录

### 3.1 DLL vs 脚本职责划分

| 决策项 | 方案 | 理由 |
|--------|------|------|
| DLL 职责 | 路径解析、目录枚举、自定义列（原始数据）、右键菜单（核心操作）、RC API 通信、缓存管理、守护进程管理、存储配额查询、能力检测 | 性能关键路径，需要直接与 DOpus VFS 接口交互 |
| 脚本职责 | 创造数据列、高级命令（同步/校验对话框）、分享链接管理、回收站浏览、配置管理对话框 | 灵活迭代、无需重编译、JScript 可直接调用 rclone CLI |
| 数据源 | DLL 为唯一与 rclone RC API 通信的角色 | 避免重复请求、保证一致性 |
| 认证 | rclone config 管理，插件不直接处理 OAuth | rclone 已有完善的认证体系 |
| 通信 | JSON 缓存文件 + INI 配置文件 | 进程间解耦，脚本通过删缓存+刷新通知 DLL |

### 3.2 DLL vs 脚本判断标准

**DLL 实现的条件（满足任一）**：
1. 需要 VFS 接口回调（路径解析、目录枚举、文件操作）
2. 需要高性能（大量数据填充列、缓存查询）
3. 需要直接 WinHTTP 通信（RC API 调用）
4. 操作频率高、需要即时响应

**脚本实现的条件（满足任一）**：
1. 需要频繁修改/迭代（创造数据算法、菜单文案）
2. 需要调用外部命令（rclone CLI fallback）
3. 需要弹对话框（同步、配置、分享链接）
4. 操作频率低、可容忍延迟

### 3.3 自定义列实现策略

| 来源 | 数量 | 说明 |
|------|------|------|
| DLL | 14 列 | 原始数据，同步填充，VFS_GetCustomColumnsW 接口 |
| 脚本 | 2 列 | 创造数据，异步填充，OnRcloneColumn 回调 |

DLL 填充原始数据列（大小、时间、MIME 等），脚本读取 DLL 写入的 JSON 缓存计算创造数据列（使用率可视化、分享状态）。

### 3.4 右键菜单实现策略

| 来源 | 命令 | 说明 |
|------|------|------|
| DLL | rc_refresh, rc_about, rc_mkdir, rc_download, rc_copy_path, rc_copy_id, rc_open_browser | 核心操作，VFS_GetContextMenuW 接口 |
| 脚本 | RcloneShare, RcloneTrash, RcloneSync, RcloneCheck, RcloneVersions, RcloneConfig, RcloneAddRemote | 高级命令，执行后删除缓存文件 + Go REFRESH 通知 DLL |

### 3.5 路径体系决策

| 决策项 | 方案 | 理由 |
|--------|------|------|
| 路径格式 | `rclone://{remote}/{path}` | 简单直觉，与 rclone 命令行路径一致 |
| 虚拟目录 | 不增加 | rclone 是通用文件系统，虚拟目录会干扰正常文件操作 |
| 上下文切换 | 基于路径层级 | 根目录 vs 远程目录，两层上下文足够 |

---

## 4. 当前代码状态

### 4.1 当前文件结构

```
RcloneVFS/
├── headers/
│   ├── vfs plugins.h
│   └── plugin support.h
├── RcloneClient.h          ← rclone RC API 通信
├── RcloneClient.cpp         ← rclone RC API 实现
├── RcloneVFS.cpp            ← VFS 插件主实现（所有逻辑都在这里）
├── RcloneVFS.def            ← DLL 导出定义
├── Utils.h                  ← 工具函数
├── json.hpp                 ← JSON 解析
├── resource.h / resource.rc ← 资源文件
└── build.bat                ← 构建脚本
```

### 4.2 当前 RcloneClient API（已实现）

| 方法 | 状态 | 说明 |
|------|------|------|
| GetRcloneExePath | ✅ | 查找 rclone.exe 路径 |
| StartDaemon / StopDaemon | ✅ | 启动/停止 rclone rcd 守护进程 |
| EnsureDaemonStarted | ✅ | 确保守护进程运行 |
| IsDaemonRunning | ✅ | 检查守护进程状态 |
| ListRemotes | ✅ | 列出远程存储 |
| ListRemotesWithType | ✅ | 列出远程存储（含类型） |
| ListDirectory | ✅ | 列出目录内容 |
| Stat | ✅ | 获取文件信息 |
| CopyFileToLocal | ✅ | 从云端下载文件 |
| CopyLocalToRemote | ✅ | 上传文件到云端 |
| DeleteFile | ✅ | 删除文件 |
| RemoveDir | ✅ | 删除目录 |
| MakeDir | ✅ | 创建目录 |
| Move | ✅ | 移动文件/目录 |
| CopyFileRemote | ✅ | 云端到云端复制文件 |
| CopyDir | ✅ | 云端到云端复制目录 |
| MoveFileRemote | ✅ | 云端到云端移动文件 |
| MoveDir | ✅ | 云端到云端移动目录 |
| InvalidateCache | ✅ | 清除缓存 |
| SendHttpPost | ✅ | HTTP POST 请求 |

### 4.3 待新增 RC API 方法

| 方法 | RC API | 阶段 | 说明 |
|------|--------|------|------|
| About | `operations/about` | 一期 | 存储配额信息 |
| PublicLink | `operations/publiclink` | 二期 | 生成分享链接 |
| BackendCommand | `backend/command` | 二期 | 后端特定命令（回收站/版本等） |
| Check | `operations/check` | 二期 | 校验文件 |
| SyncCopy | `sync/copy` | 二期 | 同步复制 |
| SyncMove | `sync/move` | 二期 | 同步移动 |
| ConfigCreate | `config/create` | 三期 | 创建远程配置 |
| ConfigDelete | `config/delete` | 三期 | 删除远程配置 |
| ConfigUpdate | `config/update` | 三期 | 更新远程配置 |
| ConfigProviders | `config/providers` | 三期 | 列出可用存储类型 |
| CoreCommand | `core/command` | 三期 | 执行 rclone 命令 |
| CoreBwlimit | `core/bwlimit` | 三期 | 带宽限制 |
| JobStatus | `job/status` | 三期 | 任务状态查询 |
| JobStop | `job/stop` | 三期 | 停止任务 |
| CacheExpire | `cache/expire` | 三期 | 清除 rclone 缓存 |
| Purge | `operations/purge` | 一期 | 递归删除目录 |

### 4.4 当前自定义列（8列）

| 列 Key | 标签 | 类型 | 说明 |
|--------|------|------|------|
| `rcRemote` | 远程存储 | 文本 | 远程名称 |
| `rcStorageType` | 存储类型 | 文本 | Google Drive / S3 等 |
| `rcItemType` | 项目类型 | 文本 | 文件夹/文件 |
| `rcMimeType` | MIME类型 | 文本 | MIME 类型 |
| `rcFileID` | 文件ID | 文本 | 云端文件标识 |
| `rcRemotePath` | 远程路径 | 文本 | 完整远程路径 |
| `rcFormattedSize` | 大小 | 尺寸 | 格式化文件大小 |
| `rcModTime` | 修改时间 | 文本 | 修改时间 |

### 4.5 当前右键菜单（4项）

```
目录上：新建文件夹 / rclone 复制 / rclone 移动 / 默认打开
```

### 4.6 当前缓存策略

| 类型 | 实现 | TTL | 问题 |
|------|------|-----|------|
| Stat 缓存 | `unordered_map<wstring, CacheEntry>` | 3 秒 | 过短，频繁重复请求 |
| 目录缓存 | 无 | - | 每次导航都重新请求 |
| 远程列表缓存 | 无 | - | 每次进入根目录都重新请求 |

---

## 5. 路径体系设计

### 5.1 路径格式

```
rclone://                          ← 根目录：显示所有已配置远程存储
rclone://{remote}                  ← 远程根目录：显示该远程的根文件
rclone://{remote}/{path}           ← 远程子目录：显示该路径下的文件
```

### 5.2 路径解析结构

```cpp
struct RclonePathInfo {
    std::wstring remote;       // 远程名称（不含冒号），如 "gdrive"
    std::wstring remotePath;   // 远程路径，如 "Documents/Work"
    std::wstring fs;           // 完整 fs 标识，如 "gdrive:"
    bool isRoot;               // 是否为 rclone:// 根目录
    bool isRemoteRoot;         // 是否为远程根目录 rclone://{remote}
};
```

### 5.3 路径解析逻辑

```cpp
bool ParseRclonePath(std::wstring fullPath, RclonePathInfo& info) {
    NormalizePath(fullPath);  // 统一斜杠、去除末尾斜杠
    
    if (fullPath.length() <= 9) {
        // rclone:// 根目录
        info.isRoot = true;
        info.isRemoteRoot = false;
        info.remote = L"";
        info.remotePath = L"";
        info.fs = L"";
        return true;
    }
    
    std::wstring pathWithoutProto = fullPath.substr(9);  // 去掉 "rclone://"
    size_t firstSlash = pathWithoutProto.find(L'/');
    
    if (firstSlash == std::wstring::npos) {
        // rclone://{remote} 远程根目录
        info.isRoot = false;
        info.isRemoteRoot = true;
        info.remote = pathWithoutProto;
        info.remotePath = L"";
        info.fs = pathWithoutProto + L":";
    } else {
        // rclone://{remote}/{path}
        info.isRoot = false;
        info.isRemoteRoot = false;
        info.remote = pathWithoutProto.substr(0, firstSlash);
        info.remotePath = pathWithoutProto.substr(firstSlash + 1);
        info.fs = pathWithoutProto.substr(0, firstSlash) + L":";
    }
    
    return true;
}
```

### 5.4 路径导航

| 操作 | 当前路径 | 目标路径 | 说明 |
|------|----------|----------|------|
| 进入远程 | `rclone://` | `rclone://gdrive/` | 双击远程文件夹 |
| 进入子目录 | `rclone://gdrive/Docs` | `rclone://gdrive/Docs/Sub` | 双击子文件夹 |
| 返回上级 | `rclone://gdrive/Docs/Sub` | `rclone://gdrive/Docs` | 导航返回 |
| 返回远程根 | `rclone://gdrive/Docs` | `rclone://gdrive/` | 导航返回 |
| 返回根目录 | `rclone://gdrive/` | `rclone://` | 导航返回 |

---

## 6. 自定义列体系设计

### 6.1 设计原则

- DLL 填充原始数据列，脚本填充创造数据列
- 按上下文自动切换列集（根目录 vs 文件目录）
- 所有数据都在列中展示，没有需要"打开文件"才能看到的信息

### 6.2 根目录列集（远程信息）

在 `rclone://` 根目录显示，每个条目代表一个远程存储：

| # | 列 Key | 标签 | 类型 | 来源 | 说明 |
|---|--------|------|------|------|------|
| 1 | `rcRemote` | 远程存储 | 文本 | DLL | 远程名称 |
| 2 | `rcStorageType` | 存储类型 | 文本 | DLL | Google Drive / OneDrive / S3 等 |
| 3 | `rcQuotaTotal` | 总配额 | 尺寸 | DLL | `operations/about` → total |
| 4 | `rcQuotaUsed` | 已用空间 | 尺寸 | DLL | `operations/about` → used |
| 5 | `rcQuotaFree` | 可用空间 | 尺寸 | DLL | `operations/about` → free |
| 6 | `rcQuotaUsage` | 使用率 | 文本 | 脚本 | "43%" 或可视化进度条 |
| 7 | `rcTrashedSize` | 回收站 | 尺寸 | DLL | `operations/about` → trashed |
| 8 | `rcStatus` | 状态 | 文本 | DLL | 在线/离线/错误 |

### 6.3 文件目录列集（文件信息）

在 `rclone://{remote}/**` 路径下显示，每个条目代表一个文件或文件夹：

| # | 列 Key | 标签 | 类型 | 来源 | 说明 |
|---|--------|------|------|------|------|
| 1 | `rcRemote` | 远程存储 | 文本 | DLL | 所属远程名称 |
| 2 | `rcItemType` | 项目类型 | 文本 | DLL | 文件夹/文件 |
| 3 | `rcFormattedSize` | 大小 | 尺寸 | DLL | 格式化文件大小 |
| 4 | `rcModTime` | 修改时间 | 日期 | DLL | 修改时间 |
| 5 | `rcMimeType` | MIME类型 | 文本 | DLL | MIME 类型 |
| 6 | `rcFileID` | 文件ID | 文本 | DLL | 云端文件唯一标识 |
| 7 | `rcRemotePath` | 远程路径 | 文本 | DLL | 完整远程路径 |
| 8 | `rcHash` | 哈希 | 文本 | DLL | 文件哈希值 |
| 9 | `rcShared` | 分享状态 | 文本 | 脚本 | 是否已分享（二期） |

### 6.4 列集切换实现

```cpp
extern "C" __declspec(dllexport) LPVFSCUSTOMCOLUMNW VFS_GetCustomColumnsW(HANDLE hData) {
    // 根据当前路径上下文返回不同列集
    // DOpus 会在目录切换时调用此函数
    // 实现方式：维护两套静态列定义，根据路径选择返回
}
```

### 6.5 存储类型中文名映射

```cpp
static std::wstring GetRemoteTypeCN(const std::wstring& type) {
    static const std::unordered_map<std::wstring, std::wstring> typeMap = {
        { L"drive", L"Google Drive" },
        { L"onedrive", L"OneDrive" },
        { L"dropbox", L"Dropbox" },
        { L"s3", L"Amazon S3" },
        { L"swift", L"OpenStack Swift" },
        { L"azureblob", L"Azure Blob" },
        { L"local", L"本地磁盘" },
        { L"ftp", L"FTP" },
        { L"sftp", L"SFTP" },
        { L"webdav", L"WebDAV" },
        { L"pcloud", L"pCloud" },
        { L"mega", L"Mega" },
        { L"box", L"Box" },
        { L"crypt", L"加密存储" },
        { L"union", L"联合存储" },
        { L"cache", L"缓存存储" },
        { L"chunker", L"分块存储" },
        { L"compress", L"压缩存储" },
        { L"alias", L"别名存储" },
        { L"googlecloudstorage", L"Google Cloud" },
        { L"b2", L"Backblaze B2" },
        { L"qingstor", L"青云 QingStor" },
        { L"koofr", L"Koofr" },
        { L"mailru", L"Mail.ru Cloud" },
        { L"yandex", L"Yandex Disk" },
        { L"hdfs", L"HDFS" },
        { L"fichier", L"1Fichier" },
        { L"premiumizeme", L"Premiumize.me" },
        { L"putio", L"Put.io" },
        { L"sharefile", L"ShareFile" },
        { L"storj", L"Storj" },
        { L"uptobox", L"Uptobox" },
        { L"jottacloud", L"Jottacloud" },
    };
    auto it = typeMap.find(type);
    return (it != typeMap.end()) ? it->second : type;
}
```

---

## 7. 右键菜单体系设计

### 7.1 设计原则

- **上下文感知**：根据当前路径和选中项类型动态生成菜单
- **能力适配**：根据远程存储类型显示/隐藏功能
- **核心操作在 DLL，高级命令在脚本**

### 7.2 根目录菜单（`rclone://`）

选中远程存储时：

| 菜单项 | 命令 | 来源 | 阶段 | 说明 |
|--------|------|------|------|------|
| 存储信息 | `rc_about` | DLL | 一期 | 显示配额面板 |
| 刷新远程列表 | `rc_refresh` | DLL | 一期 | 重新获取远程列表 |
| 在浏览器中管理 | `rc_open_browser` | DLL | 一期 | 打开 rclone Web GUI |
| 添加远程存储... | `rc_add_remote` | 脚本 | 三期 | 打开配置对话框 |
| 删除远程配置 | `rc_delete_remote` | 脚本 | 三期 | 删除远程配置 |

### 7.3 远程根目录菜单（`rclone://{remote}`）

| 菜单项 | 命令 | 来源 | 条件 | 阶段 | 说明 |
|--------|------|------|------|------|------|
| 新建文件夹 | `rc_mkdir` | DLL | 始终 | 一期 | 创建新文件夹 |
| 存储信息 | `rc_about` | DLL | 支持 about | 一期 | 显示配额面板 |
| 刷新 | `rc_refresh` | DLL | 始终 | 一期 | 清除缓存并刷新 |
| 查看回收站 | `rc_trash` | 脚本 | 支持 trash | 二期 | 浏览回收站 |
| 同步到... | `rc_sync` | 脚本 | 始终 | 二期 | 同步操作 |
| 校验文件 | `rc_check` | 脚本 | 始终 | 二期 | 校验操作 |

### 7.4 文件菜单

| 菜单项 | 命令 | 来源 | 条件 | 阶段 | 说明 |
|--------|------|------|------|------|------|
| 下载到本地 | `rc_download` | DLL | 始终 | 一期 | 下载文件到本地 |
| 复制远程路径 | `rc_copy_path` | DLL | 始终 | 一期 | 复制 `remote:path` 格式 |
| 复制文件ID | `rc_copy_id` | DLL | 有 ID | 一期 | 复制文件 ID |
| 刷新 | `rc_refresh` | DLL | 始终 | 一期 | 清除缓存并刷新 |
| 分享链接 | `rc_share` | 脚本 | 支持 publiclink | 二期 | 生成分享链接 |
| 复制分享链接 | `rc_copy_link` | DLL | 支持 publiclink | 二期 | 直接复制链接 |
| 版本历史 | `rc_versions` | 脚本 | 支持 versions | 二期 | 查看版本历史 |

### 7.5 文件夹菜单

| 菜单项 | 命令 | 来源 | 条件 | 阶段 | 说明 |
|--------|------|------|------|------|------|
| 新建文件夹 | `rc_mkdir` | DLL | 始终 | 一期 | 在此文件夹内创建 |
| 复制远程路径 | `rc_copy_path` | DLL | 始终 | 一期 | 复制 `remote:path` 格式 |
| 刷新 | `rc_refresh` | DLL | 始终 | 一期 | 清除缓存并刷新 |
| 同步到... | `rc_sync` | 脚本 | 始终 | 二期 | 同步操作 |
| 校验文件 | `rc_check` | 脚本 | 始终 | 二期 | 校验操作 |

### 7.6 菜单动态生成逻辑

```cpp
extern "C" __declspec(dllexport) BOOL VFS_GetContextMenuW(
    HANDLE hVFSData, LPVFSFUNCDATA lpFuncData, 
    LPWSTR lpszFiles, LPVFSCONTEXTMENUDATAW lpMenuData) 
{
    RclonePathInfo pathInfo;
    ParseRclonePath(lpszFiles, pathInfo);
    
    RcloneBackendFeatures features;
    if (!pathInfo.isRoot) {
        features = RcloneFeatures::GetFeatures(pathInfo.fs);
    }
    
    // 根据路径上下文和能力动态构建菜单项
    std::vector<VFSCUSTOMMENUITEMW> items;
    
    if (pathInfo.isRoot) {
        // 根目录菜单
        AddMenuItem(items, L"存储信息", L"rc_about", ...);
        AddMenuItem(items, L"刷新远程列表", L"rc_refresh", ...);
        AddMenuItem(items, L"在浏览器中管理", L"rc_open_browser", ...);
    } else {
        // 文件/文件夹菜单
        if (isSelectedDir) {
            AddMenuItem(items, L"新建文件夹", L"rc_mkdir", ...);
        }
        if (features.supportsAbout) {
            AddMenuItem(items, L"存储信息", L"rc_about", ...);
        }
        AddMenuItem(items, L"刷新", L"rc_refresh", ...);
        // ... 根据能力动态添加
    }
    
    // 设置菜单数据
    lpMenuData->lpCustomItems = items.data();
    lpMenuData->iNumCustomItems = (int)items.size();
    // ...
}
```

---

## 8. 能力检测体系设计

### 8.1 能力数据结构

```cpp
struct RcloneBackendFeatures {
    bool supportsAbout = false;       // operations/about
    bool supportsPublicLink = false;  // operations/publiclink
    bool supportsTrash = false;       // backend/command (trash)
    bool supportsVersions = false;    // backend/command (revisions)
    bool supportsHash = false;        // 哈希计算
    std::vector<std::string> hashTypes;  // 支持的哈希算法 (md5, sha1, etc.)
    std::wstring remoteType;          // 存储类型 (drive, s3, etc.)
};
```

### 8.2 检测策略

**三层检测机制：**

1. **已知类型查表**（最快，零 API 调用）

```cpp
static const std::unordered_map<std::wstring, RcloneBackendFeatures> knownFeatures = {
    { L"drive", { true, true, true, true, true, {"md5", "sha1"}, L"drive" } },
    { L"onedrive", { true, true, true, false, true, {"sha1"}, L"onedrive" } },
    { L"dropbox", { true, true, false, true, true, {"sha256"}, L"dropbox" } },
    { L"s3", { false, false, false, false, true, {"md5"}, L"s3" } },
    { L"ftp", { false, false, false, false, false, {}, L"ftp" } },
    { L"webdav", { false, false, false, false, false, {}, L"webdav" } },
    // ... 更多类型
};
```

2. **API 探测**（首次访问未知类型时）

```cpp
RcloneBackendFeatures DetectFeatures(const std::wstring& fs) {
    RcloneBackendFeatures features;
    
    // 测试 about 支持
    std::string aboutRes = SendHttpPost("/operations/about", "{\"fs\":\"" + WideToUtf8(fs) + "\"}");
    features.supportsAbout = !aboutRes.empty() && aboutRes.find("\"error\"") == std::string::npos;
    
    // 测试 publiclink 支持
    // (不实际创建链接，仅通过类型查表或尝试 backend/features)
    
    return features;
}
```

3. **运行时缓存**（检测后缓存到 Feature Cache）

```cpp
class RcloneFeatures {
public:
    static RcloneBackendFeatures GetFeatures(const std::wstring& fs);
    static void Invalidate(const std::wstring& fs);
    static void InvalidateAll();
    
private:
    static std::unordered_map<std::wstring, RcloneBackendFeatures> s_featureCache;
    static std::mutex s_featureMutex;
};
```

### 8.3 能力检测时机

| 时机 | 操作 | 说明 |
|------|------|------|
| 首次访问远程 | 查表 → 探测 → 缓存 | 延迟检测，不预加载所有远程 |
| 手动刷新 | 清除 Feature Cache | 重新检测 |
| 配置变更 | 清除相关 Feature Cache | 远程配置可能改变能力 |

---

## 9. 缓存策略设计

### 9.1 多层缓存架构

```
┌─────────────────────────────────────────────┐
│  L1: 内存缓存（最快，进程内）                  │
│  ┌─────────────┐  ┌──────────────┐          │
│  │ Stat Cache  │  │ Dir Cache    │          │
│  │ (单文件信息) │  │ (目录列表)    │          │
│  │ TTL: 30s    │  │ TTL: 60s     │          │
│  └─────────────┘  └──────────────┘          │
│  ┌─────────────┐  ┌──────────────┐          │
│  │ About Cache │  │ Feature Cache│          │
│  │ (配额信息)   │  │ (能力检测)    │          │
│  │ TTL: 300s   │  │ TTL: 永久*    │          │
│  └─────────────┘  └──────────────┘          │
│  ┌─────────────┐                             │
│  │ Remote Cache│                             │
│  │ (远程列表)   │                             │
│  │ TTL: 120s   │                             │
│  └─────────────┘                             │
├─────────────────────────────────────────────┤
│  L2: JSON 文件缓存（跨进程，DLL↔脚本通信）     │
│  ┌─────────────────────────────────┐        │
│  │ %TEMP%\RcloneVFS\cache\         │        │
│  │   dir_{hash}.json               │        │
│  │   stat_{hash}.json              │        │
│  │   about_{remote}.json           │        │
│  │   features_{remote}.json        │        │
│  └─────────────────────────────────┘        │
├─────────────────────────────────────────────┤
│  L3: rclone 内置缓存（--cache-dir）           │
│  └── 由 rclone 自身管理，插件不控制           │
└─────────────────────────────────────────────┘
```

### 9.2 缓存条目定义

```cpp
struct CacheEntry {
    std::vector<RcloneFileInfo> dirEntries;  // Dir Cache 专用
    RcloneFileInfo statInfo;                  // Stat Cache 专用
    RcloneAboutInfo aboutInfo;                // About Cache 专用
    RcloneBackendFeatures features;           // Feature Cache 专用
    std::vector<RcloneRemoteInfo> remotes;    // Remote Cache 专用
    ULONGLONG timestamp;                      // 缓存时间戳
};

class RcloneCache {
public:
    // Stat 缓存
    bool GetStat(const std::wstring& fs, const std::wstring& remote, RcloneFileInfo& out);
    void SetStat(const std::wstring& fs, const std::wstring& remote, const RcloneFileInfo& info);
    void InvalidateStat(const std::wstring& fs, const std::wstring& remote);
    
    // Dir 缓存
    bool GetDir(const std::wstring& fs, const std::wstring& remote, std::vector<RcloneFileInfo>& out);
    void SetDir(const std::wstring& fs, const std::wstring& remote, const std::vector<RcloneFileInfo>& entries);
    void InvalidateDir(const std::wstring& fs, const std::wstring& remote);
    
    // About 缓存
    bool GetAbout(const std::wstring& fs, RcloneAboutInfo& out);
    void SetAbout(const std::wstring& fs, const RcloneAboutInfo& info);
    void InvalidateAbout(const std::wstring& fs);
    
    // Feature 缓存
    bool GetFeatures(const std::wstring& fs, RcloneBackendFeatures& out);
    void SetFeatures(const std::wstring& fs, const RcloneBackendFeatures& features);
    void InvalidateFeatures(const std::wstring& fs);
    
    // Remote 缓存
    bool GetRemotes(std::vector<RcloneRemoteInfo>& out);
    void SetRemotes(const std::vector<RcloneRemoteInfo>& remotes);
    void InvalidateRemotes();
    
    // 批量失效
    void InvalidateAll();
    void InvalidateForPath(const std::wstring& fs, const std::wstring& remote);
    
    // 预加载
    void PreloadAbout(const std::wstring& fs);
    void PreloadChildren(const std::wstring& fs, const std::wstring& remote);
    
private:
    std::unordered_map<std::wstring, CacheEntry> m_statCache;
    std::unordered_map<std::wstring, CacheEntry> m_dirCache;
    std::unordered_map<std::wstring, CacheEntry> m_aboutCache;
    std::unordered_map<std::wstring, CacheEntry> m_featureCache;
    CacheEntry m_remoteCache;
    std::mutex m_cacheMutex;
    
    static constexpr ULONGLONG STAT_TTL = 30000;    // 30 秒
    static constexpr ULONGLONG DIR_TTL = 60000;     // 60 秒
    static constexpr ULONGLONG ABOUT_TTL = 300000;  // 5 分钟
    static constexpr ULONGLONG REMOTE_TTL = 120000; // 2 分钟
};
```

### 9.3 缓存策略细节

| 缓存类型 | Key 格式 | TTL | 失效触发 | 说明 |
|----------|----------|-----|----------|------|
| Stat | `{fs}\|{remote}` | 30s | 写操作后立即失效 | 单文件信息查询 |
| Dir | `{fs}\|{remote}` | 60s | 写操作后立即失效 | 目录列表缓存 |
| About | `{fs}` | 300s (5min) | 手动刷新 | 存储配额信息，变化慢 |
| Feature | `{fs}` | 进程生命周期 | 重启 / 手动刷新 | 后端能力检测，很少变化 |
| RemoteList | `__remotes__` | 120s (2min) | config 变更后失效 | 远程列表缓存 |

### 9.4 预加载策略

```
用户导航到 rclone://gdrive/Documents/
  → 主请求：ListDirectory("gdrive:", "Documents")     ← 同步，阻塞
  → 预加载：About("gdrive:")                          ← 后台（如果未缓存且支持）
  → 预加载：Stat("gdrive:", "")                       ← 后台（如果未缓存）
  → 预加载：ListDirectory("gdrive:", "")              ← 后台（如果未缓存）
```

预加载规则：
1. 首次访问远程时，后台预加载 About 信息和 Feature 检测
2. 目录列表返回后，后台预加载子目录的 Stat 信息（仅第一层，最多 10 个子目录）
3. 预加载请求使用低优先级线程，不阻塞主请求
4. 预加载结果写入缓存，后续导航直接命中

### 9.5 缓存失效策略

**写操作触发的失效：**

```
上传/删除/移动/重命名/新建文件夹 → 失效相关缓存：
  1. 失效当前目录的 Dir Cache
  2. 失效相关文件的 Stat Cache
  3. 失效父目录的 Dir Cache（因为大小/时间可能变化）
  4. 如果涉及配额变化，失效 About Cache
```

**手动刷新触发的失效：**

```
右键"刷新"或 F5 → 失效当前路径相关缓存：
  1. 失效当前目录的 Dir Cache
  2. 失效当前目录下所有文件的 Stat Cache
  3. 失效 About Cache（如果在远程根目录）
```

**全量刷新：**

```
清除所有内存缓存 + 删除 JSON 文件缓存
```

---

## 10. DLL-脚本通信设计

### 10.1 通信方式

| 方向 | 方式 | 说明 |
|------|------|------|
| DLL → 脚本 | JSON 缓存文件 | DLL 写入原始数据到 JSON 文件，脚本读取 |
| 脚本 → DLL | 删缓存 + Go REFRESH | 脚本执行操作后删除缓存文件，发送 `Go REFRESH` 通知 DLL 刷新 |

### 10.2 JSON 缓存文件格式

```
%TEMP%\RcloneVFS\
├── cache\
│   ├── dir_{hash}.json        ← 目录列表缓存（DLL 写，脚本读）
│   ├── stat_{hash}.json       ← 文件信息缓存（DLL 写，脚本读）
│   ├── about_{remote}.json    ← 存储配额缓存（DLL 写，脚本读）
│   └── features_{remote}.json ← 能力检测缓存（DLL 写，脚本读）
├── config\
│   └── settings.ini           ← 插件配置（DLL 和脚本共读）
└── temp\                       ← 临时下载文件（智能编辑用）
```

### 10.3 JSON 缓存文件内容示例

**dir_{hash}.json：**

```json
{
  "fs": "gdrive:",
  "remote": "Documents",
  "timestamp": 1715400000000,
  "entries": [
    {
      "name": "report.pdf",
      "isDir": false,
      "size": 1048576,
      "modTime": "2026-05-10T08:30:00Z",
      "mimeType": "application/pdf",
      "id": "1a2b3c4d5e6f"
    },
    {
      "name": "Projects",
      "isDir": true,
      "size": 0,
      "modTime": "2026-05-09T14:20:00Z",
      "mimeType": "inode/directory",
      "id": "7g8h9i0j1k2l"
    }
  ]
}
```

**about_{remote}.json：**

```json
{
  "fs": "gdrive:",
  "timestamp": 1715400000000,
  "total": 18253611008,
  "used": 7993453766,
  "free": 1411001220,
  "trashed": 104857602,
  "other": 8849156022
}
```

**features_{remote}.json：**

```json
{
  "fs": "gdrive:",
  "timestamp": 1715400000000,
  "remoteType": "drive",
  "supportsAbout": true,
  "supportsPublicLink": true,
  "supportsTrash": true,
  "supportsVersions": true,
  "supportsHash": true,
  "hashTypes": ["md5", "sha1"]
}
```

### 10.4 脚本通知 DLL 刷新

```javascript
// RcloneVFS.js 中脚本执行操作后
function OnRcloneShare(data) {
    // 执行分享操作...
    
    // 删除缓存文件
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var cacheDir = fso.GetSpecialFolder(2) + "\\RcloneVFS\\cache\\";
    if (fso.FolderExists(cacheDir)) {
        fso.DeleteFile(cacheDir + "dir_*.json");
        fso.DeleteFile(cacheDir + "stat_*.json");
    }
    
    // 通知 DOpus 刷新
    DOpus.Output("RcloneVFS: Cache invalidated, refreshing...");
    var cmd = DOpus.Create.Command();
    cmd.RunCommand("Go REFRESH");
}
```

---

## 11. 存储配额功能设计

### 11.1 数据来源

通过 `operations/about` RC API 获取：

```json
{
  "total": 18253611008,
  "used": 7993453766,
  "free": 1411001220,
  "trashed": 104857602,
  "other": 8849156022
}
```

### 11.2 数据结构

```cpp
struct RcloneAboutInfo {
    uint64_t total = 0;     // 总配额（字节）
    uint64_t used = 0;      // 已用空间（字节）
    uint64_t free = 0;      // 可用空间（字节）
    uint64_t trashed = 0;   // 回收站占用（字节）
    uint64_t other = 0;     // 其他占用（字节）
    uint64_t objects = 0;   // 对象数量
    bool hasTotal = false;
    bool hasUsed = false;
    bool hasFree = false;
    bool hasTrashed = false;
    bool hasObjects = false;
};
```

### 11.3 API 调用

```cpp
bool RcloneClient::About(const std::wstring& fs, RcloneAboutInfo& outInfo) {
    json req;
    req["fs"] = WideToUtf8(fs);
    std::string res = SendHttpPost("/operations/about", req.dump());
    if (res.empty()) return false;
    
    try {
        auto j = json::parse(res);
        if (j.contains("total") && j["total"].is_number()) {
            outInfo.total = j["total"].get<uint64_t>();
            outInfo.hasTotal = true;
        }
        if (j.contains("used") && j["used"].is_number()) {
            outInfo.used = j["used"].get<uint64_t>();
            outInfo.hasUsed = true;
        }
        if (j.contains("free") && j["free"].is_number()) {
            outInfo.free = j["free"].get<uint64_t>();
            outInfo.hasFree = true;
        }
        if (j.contains("trashed") && j["trashed"].is_number()) {
            outInfo.trashed = j["trashed"].get<uint64_t>();
            outInfo.hasTrashed = true;
        }
        if (j.contains("objects") && j["objects"].is_number()) {
            outInfo.objects = j["objects"].get<uint64_t>();
            outInfo.hasObjects = true;
        }
        return true;
    } catch (...) { return false; }
}
```

### 11.4 展示方式

1. **自定义列**：根目录列集中的配额相关列
2. **右键菜单**：远程上右键 → "存储信息" → 弹出详细配额面板
3. **优雅降级**：不支持 about 的后端，列显示 "-"

### 11.5 性能考虑

- About 信息变化缓慢，TTL 设为 5 分钟
- 首次访问远程时后台预加载
- 批量获取：根目录显示多个远程时，并行请求 About（最多 5 个并发）

---

## 12. 守护进程管理设计

### 12.1 现有问题

| 问题 | 说明 |
|------|------|
| 无健康检查 | daemon 可能崩溃但状态仍为 running |
| 无重连机制 | API 调用失败后不尝试重启 |
| 密码生成 | 基于计算机名，换机器后不一致 |
| 优雅关闭 | 直接 TerminateProcess，不发送 shutdown |

### 12.2 改进方案

| 改进 | 说明 |
|------|------|
| 心跳检测 | 定期 `rc/noop` 检测 daemon 存活，崩溃后自动重启 |
| 重连机制 | API 调用失败时尝试重启 daemon 并重试一次 |
| 优雅关闭 | `VFS_Uninit` 时先发送 `core/shutdown` 再 TerminateProcess |
| 端口检测 | 启动前检测端口是否被占用，被占用则尝试复用现有 daemon |
| 启动等待 | 改进启动等待逻辑，使用指数退避 |

### 12.3 心跳检测实现

```cpp
class RcloneDaemon {
public:
    static bool EnsureRunning();
    static void StartHeartbeat();
    static void StopHeartbeat();
    
private:
    static std::thread s_heartbeatThread;
    static std::atomic<bool> s_heartbeatRunning;
    static constexpr int HEARTBEAT_INTERVAL = 30;  // 30 秒
    
    static void HeartbeatLoop() {
        while (s_heartbeatRunning.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
            if (s_isDaemonRunning.load()) {
                std::string res = SendHttpPost("/rc/noop", "{}");
                if (res.empty()) {
                    // Daemon 可能崩溃，尝试重启
                    s_isDaemonRunning.store(false);
                    EnsureRunning();
                }
            }
        }
    }
};
```

### 12.4 重连机制

```cpp
std::string RcloneClient::SendHttpPostWithRetry(const std::string& path, const std::string& jsonPayload) {
    std::string res = SendHttpPost(path, jsonPayload);
    if (!res.empty()) return res;
    
    // 第一次失败，尝试重启 daemon
    DebugLog("RC API call failed, attempting daemon restart...\n");
    s_isDaemonRunning.store(false);
    if (EnsureDaemonStarted()) {
        res = SendHttpPost(path, jsonPayload);
    }
    
    return res;
}
```

---

## 13. 数据结构定义

### 13.1 核心数据结构

```cpp
// 路径信息
struct RclonePathInfo {
    std::wstring remote;       // 远程名称（不含冒号）
    std::wstring remotePath;   // 远程路径
    std::wstring fs;           // 完整 fs 标识（含冒号）
    bool isRoot;               // 是否为 rclone:// 根目录
    bool isRemoteRoot;         // 是否为远程根目录
};

// 文件信息
struct RcloneFileInfo {
    std::wstring name;
    bool isDir;
    uint64_t size;
    FILETIME modTime;
    std::wstring mimeType;
    std::wstring id;
    std::wstring hash;         // 新增：文件哈希
    std::string hashType;      // 新增：哈希类型
};

// 远程信息
struct RcloneRemoteInfo {
    std::wstring name;
    std::wstring type;
};

// 存储配额信息
struct RcloneAboutInfo {
    uint64_t total = 0;
    uint64_t used = 0;
    uint64_t free = 0;
    uint64_t trashed = 0;
    uint64_t other = 0;
    uint64_t objects = 0;
    bool hasTotal = false;
    bool hasUsed = false;
    bool hasFree = false;
    bool hasTrashed = false;
    bool hasObjects = false;
};

// 后端能力
struct RcloneBackendFeatures {
    bool supportsAbout = false;
    bool supportsPublicLink = false;
    bool supportsTrash = false;
    bool supportsVersions = false;
    bool supportsHash = false;
    std::vector<std::string> hashTypes;
    std::wstring remoteType;
};

// 缓存条目
struct CacheEntry {
    std::vector<RcloneFileInfo> dirEntries;
    RcloneFileInfo statInfo;
    RcloneAboutInfo aboutInfo;
    RcloneBackendFeatures features;
    std::vector<RcloneRemoteInfo> remotes;
    ULONGLONG timestamp;
};

// VFS 文件上下文
struct VFS_FILE_CONTEXT {
    bool isWrite;
    std::wstring targetFs;
    std::wstring targetRemote;
    HINTERNET hHttpRequest;
    HANDLE hReadPipe;
    HANDLE hProcess;
    uint64_t seekPos;
    uint64_t streamPos;
    uint64_t fileSize;
};
```

### 13.2 列 ID 枚举

```cpp
enum {
    // 根目录列集
    COL_RC_REMOTE = 1,
    COL_RC_STYPE,         // 存储类型
    COL_RC_QUOTA_TOTAL,   // 总配额
    COL_RC_QUOTA_USED,    // 已用空间
    COL_RC_QUOTA_FREE,    // 可用空间
    COL_RC_QUOTA_USAGE,   // 使用率（脚本创造数据）
    COL_RC_TRASHED_SIZE,  // 回收站占用
    COL_RC_STATUS,        // 状态
    
    // 文件目录列集
    COL_RC_REMOTE2 = 101, // 远程存储（文件上下文）
    COL_RC_ITYPE,         // 项目类型
    COL_RC_FSIZE,         // 大小
    COL_RC_MTIME,         // 修改时间
    COL_RC_MIME,          // MIME类型
    COL_RC_FID,           // 文件ID
    COL_RC_RPATH,         // 远程路径
    COL_RC_HASH,          // 哈希
    COL_RC_SHARED,        // 分享状态（脚本创造数据，二期）
};

#define NUM_ROOT_COLUMNS 8
#define NUM_FILE_COLUMNS 9
```

---

## 14. 一期/二期/三期划分

### 14.1 一期：核心重构

| 功能 | 涉及模块 | 优先级 |
|------|----------|--------|
| 架构重构（模块化拆分） | 全部 | P0 |
| 智能缓存（多层缓存+预加载） | RcloneCache | P0 |
| 上下文感知自定义列 | ColumnManager | P0 |
| 动态适配右键菜单 | MenuManager | P0 |
| 存储配额功能 | RcloneClient + ColumnManager | P1 |
| 能力检测基础 | RcloneFeatures | P1 |
| 守护进程优化 | RcloneClient | P1 |
| 路径解析重构 | PathParser | P1 |

### 14.2 二期：云存储特性

| 功能 | 涉及模块 | 优先级 |
|------|----------|--------|
| 分享链接管理 | 脚本 + RcloneClient | P0 |
| 回收站/版本管理 | 脚本 + RcloneClient | P1 |
| 同步/校验操作 | 脚本 + RcloneClient | P1 |

### 14.3 三期：高级功能

| 功能 | 涉及模块 | 优先级 |
|------|----------|--------|
| 配置管理（图形化） | 脚本 + RcloneClient | P0 |
| 带宽控制 | 脚本 + RcloneClient | P1 |
| 任务管理 | 脚本 + RcloneClient | P2 |
| rclone CLI 集成 | 脚本 | P2 |

---

## 15. rclone RC API 能力映射

### 15.1 一期 API

| API | 用途 | 状态 |
|-----|------|------|
| `operations/list` | 目录浏览 | 已实现 |
| `operations/stat` | 文件信息 | 已实现 |
| `operations/copyfile` | 复制文件 | 已实现 |
| `operations/movefile` | 移动文件 | 已实现 |
| `operations/deletefile` | 删除文件 | 已实现 |
| `operations/mkdir` | 创建目录 | 已实现 |
| `operations/rmdir` | 删除目录 | 已实现 |
| `operations/purge` | 递归删除 | 待新增 |
| `operations/about` | 存储配额 | 待新增 |
| `operations/uploadfile` | 上传文件 | 已实现 |
| `config/listremotes` | 远程列表 | 已实现 |
| `config/dump` | 远程配置（含类型） | 已实现 |
| `rc/noop` | 心跳检测 | 待新增 |
| `core/shutdown` | 优雅关闭 | 待新增 |

### 15.2 二期 API

| API | 用途 | 状态 |
|-----|------|------|
| `operations/publiclink` | 分享链接 | 待新增 |
| `operations/check` | 校验文件 | 待新增 |
| `sync/copy` | 同步复制 | 待新增 |
| `sync/move` | 同步移动 | 待新增 |
| `backend/command` | 后端特定命令 | 待新增 |

### 15.3 三期 API

| API | 用途 | 状态 |
|-----|------|------|
| `config/create` | 创建远程配置 | 待新增 |
| `config/delete` | 删除远程配置 | 待新增 |
| `config/update` | 更新远程配置 | 待新增 |
| `config/providers` | 列出可用存储类型 | 待新增 |
| `core/command` | 执行 rclone 命令 | 待新增 |
| `core/bwlimit` | 带宽限制 | 待新增 |
| `job/status` | 任务状态查询 | 待新增 |
| `job/stop` | 停止任务 | 待新增 |
| `cache/expire` | 清除 rclone 缓存 | 待新增 |

---

## 16. 持久化存储设计

### 16.1 存储路径规范

插件所有持久化数据统一存储到以下目录：

```
%AppData%\GPSoftware\Directory Opus\User Data\VFSPlugin\RcloneVFS\
├── cache\                    ← L2 缓存文件
│   ├── stat_*.json           ← 文件信息缓存
│   ├── dir_*.json            ← 目录列表缓存
│   ├── about_*.json          ← 存储配额缓存
│   ├── features_*.json       ← 能力检测缓存
│   └── remotes.json          ← 远程列表缓存
├── config\                   ← 配置文件
│   └── settings.json         ← 插件配置
├── logs\                     ← 日志文件
│   ├── rclonevfs.log         ← 当前日志
│   └── rclonevfs-YYYY-MM-DD.log ← 历史日志（按日期滚动）
└── quota\                    ← 配额数据持久化
    └── quota.json            ← 所有远程的配额汇总
```

**路径获取方式：**
- 通过 DOpus API `GetConfigPath(OPUSPATH_STATEDATA)` 获取基础路径
- 拼接 `VFSPlugin\RcloneVFS\` 作为插件专属目录

### 16.2 配置文件设计

**settings.json 格式：**

```json
{
  "version": 1,
  "rclonePath": "C:\\Program Files\\rclone\\rclone.exe",
  "configPath": "",
  "autoStartDaemon": true,
  "rcPort": 8657,
  "connTimeout": 30,
  "cacheEnabled": true,
  "cacheTTL": 300,
  "dirCacheTime": 60,
  "logEnabled": true,
  "logLevel": "INFO",
  "logMaxSize": 10,
  "logMaxFiles": 5,
  "autoSync": true,
  "syncInterval": 2,
  "maxIdle": 300,
  "maxConnections": 4,
  "transfers": 4,
  "bandwidthLimit": "",
  "showHidden": false,
  "caseSensitive": false,
  "confirmDelete": true
}
```

### 16.3 日志系统设计

**日志级别：**

| 级别 | 说明 | 使用场景 |
|------|------|----------|
| ERROR | 严重错误，影响功能 | API调用失败、守护进程崩溃、文件操作失败 |
| WARN | 警告，可能影响性能 | 缓存过期、重试操作、非关键错误 |
| INFO | 关键操作日志 | 启动守护进程、连接远程、配置加载 |
| DEBUG | 详细调试信息 | API调用详情、缓存命中/未命中、菜单构建 |

**日志滚动策略：**
- 单文件最大大小：10MB（可配置）
- 保留历史文件数：5个（可配置）
- 文件名格式：`rclonevfs-YYYY-MM-DD.log`

**日志写入 API：**

```cpp
void Log(LogLevel level, const wchar_t* format, ...);
void LogError(const wchar_t* format, ...);
void LogWarn(const wchar_t* format, ...);
void LogInfo(const wchar_t* format, ...);
void LogDebug(const wchar_t* format, ...);
```

### 16.4 配额数据持久化设计

**quota.json 格式：**

```json
{
  "version": 1,
  "lastUpdate": "2026-05-14T08:00:00Z",
  "remotes": {
    "gdrive:": {
      "total": 18253611008,
      "used": 7993453766,
      "free": 1411001220,
      "trashed": 104857602,
      "source": "about_api",
      "updatedAt": "2026-05-14T08:00:00Z",
      "supportsAbout": true
    },
    "myftp:": {
      "used": 1234567890,
      "source": "recursive_calc",
      "updatedAt": "2026-05-14T07:30:00Z",
      "supportsAbout": false,
      "calcStatus": "completed"
    }
  }
}
```

### 16.5 配额计算策略

| 策略 | 适用场景 | 优先级 | 优点 | 缺点 |
|------|----------|--------|------|------|
| **About API** | 云存储（drive, onedrive, dropbox等） | 1 | 准确、快速、实时 | 不是所有后端都支持 |
| **专用API** | 需要更详细配额信息时 | 2 | 可获取共享空间、团队盘等 | 需要额外OAuth认证 |
| **递归计算** | 不支持About的后端（local, ftp, sftp等） | 3 | 通用 | 性能差，大目录耗时长 |
| **缓存值** | 离线时 | 4 | 快速 | 可能过时 |

**计算流程：**

```
获取已用空间
    ↓
检查持久化缓存（quota.json）
    ↓
存在且未过期（TTL=5分钟）→ 直接返回
    ↓
不存在或已过期 → 检查后端特性（supportsAbout）
    ↓
支持About → 调用About API → 更新缓存 → 返回
    ↓
不支持About → 后台异步递归计算 → 返回缓存值（可能为空或过时）
    ↓
计算完成后更新缓存并刷新显示
```

### 16.6 缓存清理策略

| 缓存类型 | TTL | 清理时机 |
|----------|-----|----------|
| Stat | 30秒 | 写操作后立即失效 |
| Dir | 60秒 | 写操作后立即失效 |
| About | 5分钟 | 手动刷新时失效 |
| Feature | 进程生命周期 | 重启/手动刷新时失效 |
| RemoteList | 2分钟 | config变更后失效 |

**启动时清理：**
- 检查所有缓存文件的时间戳
- 删除过期的缓存文件
- 保留最近7天的日志文件

---

## 附录 A：重构后文件结构

```
RcloneVFS/
├── headers/
│   ├── vfs plugins.h          ← DOpus VFS API 定义
│   └── plugin support.h       ← 插件支持函数
├── src/
│   ├── RcloneVFS.cpp          ← VFS 插件主入口
│   ├── RcloneClient.cpp       ← rclone RC API 通信层
│   ├── RcloneClient.h         ← rclone RC API 接口定义
│   ├── RcloneCache.cpp        ← 缓存管理（新增）
│   ├── RcloneCache.h          ← 缓存接口定义（新增）
│   ├── RcloneFeatures.cpp     ← 能力检测（新增）
│   ├── RcloneFeatures.h       ← 能力检测接口（新增）
│   ├── PathParser.cpp         ← 路径解析（新增）
│   ├── PathParser.h           ← 路径解析接口（新增）
│   ├── ColumnManager.cpp      ← 自定义列管理（新增）
│   ├── ColumnManager.h        ← 列管理接口（新增）
│   ├── MenuManager.cpp        ← 右键菜单管理（新增）
│   ├── MenuManager.h          ← 菜单管理接口（新增）
│   ├── DaemonManager.cpp      ← 守护进程管理（新增）
│   ├── DaemonManager.h        ← 守护进程接口（新增）
│   └── DataStructs.h          ← 数据结构定义（新增）
├── Utils.h                    ← 工具函数
├── json.hpp                   ← JSON 解析
├── RcloneVFS.def              ← DLL 导出定义
├── resource.h                 ← 资源 ID
├── resource.rc                ← 资源文件
├── build.bat                  ← 构建脚本
└── README.md                  ← 说明文档
```

## 附录 B：脚本文件结构

```
RcloneVFS.js                   ← DOpus 脚本插件
├── OnRcloneColumn             ← 创造数据列回调
├── OnRcloneShare              ← 分享链接命令（二期）
├── OnRcloneTrash              ← 回收站浏览命令（二期）
├── OnRcloneSync               ← 同步命令（二期）
├── OnRcloneCheck              ← 校验命令（二期）
├── OnRcloneVersions           ← 版本历史命令（二期）
├── OnRcloneConfig             ← 配置管理命令（三期）
├── OnRcloneAddRemote          ← 添加远程命令（三期）
└── Helper functions           ← 辅助函数
```

## 附录 C：与 GitHubVFS 设计对比

| 维度 | GitHubVFS | RcloneVFS | 说明 |
|------|-----------|-----------|------|
| 路径复杂度 | 高（虚拟目录+搜索+@ref） | 低（简单路径映射） | rclone 是通用文件系统 |
| 上下文层级 | 多层（根/仓库/Issue/PR/Code...） | 两层（根/文件目录） | rclone 场景更简单 |
| 能力差异 | 无（GitHub API 统一） | 大（50+ 云服务商） | rclone 需要动态适配 |
| 搜索功能 | 核心功能（6 种搜索） | 无 | 云存储搜索由服务商提供 |
| 虚拟文件 | 多（signal/info.txt 等） | 无 | rclone 只处理真实文件 |
| 智能编辑 | 无 | 核心功能 | 云存储文件编辑后自动上传 |
| 分享功能 | 无 | 重要功能 | 云存储核心需求 |
| 同步功能 | 无 | 重要功能 | rclone 核心能力 |
