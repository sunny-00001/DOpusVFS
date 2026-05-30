# RcloneVFS 改进进度文档

## 文档信息

| 项目 | 说明 |
|------|------|
| 文档版本 | v1.0 |
| 创建日期 | 2026年5月 |
| 最后更新 | 2026年5月 |
| 状态 | 进行中 |

---

## 改进概述

本文档记录 RcloneVFS 插件的持续改进进度，涵盖持久化存储、配额管理、日志系统等核心功能的实现情况。

---

## 完成的改进

### 1. 持久化存储设计

**目标**：将配置、缓存、日志统一存储到 `%AppData%\GPSoftware\Directory Opus\User Data\VFSPlugin\RcloneVFS\`

**实现状态**：✅ 已完成

**详细内容**：

| 存储类型 | 路径 | 实现文件 | 状态 |
|----------|------|----------|------|
| 配置文件 | `config/settings.json` | ConfigManager.cpp | ✅ |
| 缓存文件 | `cache/*.json` | RcloneCache.cpp | ✅ |
| 日志文件 | `logs/rclonevfs.log` | Logger.cpp | ✅ |
| 配额数据 | `quota/quota.json` | QuotaManager.cpp | ✅ |

### 2. 配置管理器改进

**目标**：从注册表存储迁移到 JSON 文件存储

**实现状态**：✅ 已完成

**变更内容**：
- 配置文件格式：注册表 → JSON
- 存储路径：`HKCU\Software\RcloneVFS` → `%AppData%\GPSoftware\Directory Opus\User Data\VFSPlugin\RcloneVFS\config\settings.json`
- 向后兼容：保留从注册表读取的能力，便于升级

### 3. 日志系统

**目标**：实现多级日志记录、日志滚动、日志清理

**实现状态**：✅ 已完成

**特性**：
- **日志级别**：ERROR、WARN、INFO、DEBUG
- **日志滚动**：按文件大小（默认10MB）和数量（默认5个）滚动
- **日志清理**：自动清理指定天数前的旧日志
- **线程安全**：使用互斥锁保证多线程环境下的日志写入安全

### 4. 配额管理系统

**目标**：实现配额数据的持久化存储和多种计算策略

**实现状态**：✅ 已完成

**计算策略**：

| 策略 | 优先级 | 适用场景 | 实现方式 |
|------|--------|----------|----------|
| About API | 1 | 支持About的云存储（Google Drive、OneDrive等） | 直接调用 rclone RC API |
| 递归计算 | 2 | 不支持About的后端（local、ftp、sftp等） | 递归遍历文件累加大小 |
| 缓存值 | 3 | 离线或计算进行中 | 返回持久化的缓存数据 |

**缓存策略**：
- TTL：5分钟（可配置）
- 持久化：存储到 `quota.json`
- 异步更新：递归计算在后台线程执行，不阻塞UI

---

## 正在进行的改进

### 5. 批量操作功能（VFS_BatchOperationW）

**目标**：实现基于 rclone 的复制/移动功能，支持远程到远程、本地到远程、远程到本地操作

**实现状态**：✅ 已完成

**实现内容**：

| 功能 | 状态 | 说明 |
|------|------|------|
| VFS_BatchOperationW | ✅ | 批量复制/移动入口函数 |
| Remote → Remote | ✅ | 使用 rclone sync/copy/move API |
| Local → Remote | ✅ | 使用 rclone operations/copyfile API |
| Remote → Local | ✅ | 使用 rclone operations/copyfile API |
| 进度显示 | ⏳ | 待实现（解析 rclone --progress 输出） |
| 错误详情 | ✅ | 显示 rclone 返回的错误信息 |

**新增文件**：
- `BatchOperationManager.h` - 批量操作管理器头文件
- `BatchOperationManager.cpp` - 批量操作管理器实现

**修改文件**：
- `RcloneVFS.cpp` - 添加 VFS_BatchOperationW 函数
- `RcloneVFS.def` - 导出 VFS_BatchOperationW
- `build.bat` - 添加 BatchOperationManager.cpp 到编译列表

**操作场景映射**：

| 场景 | 源类型 | 目标类型 | rclone API |
|------|--------|----------|------------|
| 远程到远程 | RCLONE_VIRTUAL | RCLONE_VIRTUAL | /sync/copy 或 /sync/move |
| 本地到远程 | LOCAL_FILESYSTEM | RCLONE_VIRTUAL | /operations/copyfile |
| 远程到本地 | RCLONE_VIRTUAL | LOCAL_FILESYSTEM | /operations/copyfile |
| 本地到本地 | LOCAL_FILESYSTEM | LOCAL_FILESYSTEM | 返回 DODEFAULT（由 DOpus 处理） |

### 6. 性能优化

**目标**：优化目录列表加载速度和内存使用

**实现状态**：🔄 进行中

**计划优化点**：
- [ ] 目录列表增量更新
- [ ] 智能预加载机制
- [ ] 内存缓存优化策略

---

## 待实现的改进

### 7. 高级功能

| 功能 | 优先级 | 状态 | 描述 |
|------|--------|------|------|
| 增量同步 | 高 | ⏳ 待实现 | 支持双向增量同步 |
| 批量操作进度显示 | 中 | ⏳ 待实现 | 解析 rclone --progress 输出，显示进度条 |
| 版本管理 | 中 | ✅ 已完成 | 支持云存储版本历史 |
| 共享管理 | 低 | ✅ 已完成 | 管理共享链接权限 |

---

## 技术债务清理

| 项目 | 状态 | 描述 |
|------|------|------|
| 代码重构 | ⏳ 待进行 | 将RcloneVFS.cpp拆分为更小的模块 |
| 单元测试 | ⏳ 待进行 | 添加核心功能的单元测试 |
| 文档完善 | ⏳ 待进行 | 完善API文档和使用说明 |

---

## 编译验证

**最近编译时间**：2026年5月
**编译状态**：✅ 成功
**输出文件**：`D:\VFS\RcloneVFS.dll`（1,571,840 字节）
**部署状态**：✅ 已部署到 Directory Opus

---

## 问题追踪

| 问题ID | 标题 | 状态 | 严重程度 |
|--------|------|------|----------|
| #1 | 根目录加载缓慢 | ✅ 已修复 | 高 |
| #2 | 右键属性菜单灰色 | ✅ 已修复 | 高 |
| #3 | 配额信息不显示 | ✅ 已修复 | 高 |
| #4 | 配置文件分散 | ✅ 已修复 | 中 |
| #5 | 复制粘贴在虚拟路径失效 | ✅ 已修复 | 高 |

---

## 下一步计划

1. **短期**：完善配额管理的错误处理和重试机制
2. **中期**：实现增量同步功能
3. **长期**：添加完整的单元测试套件

---

## 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v1.0 | 2026-05 | 初始版本，完成持久化存储和配额管理 |
| v1.1 | 2026-05 | 添加 VFS_BatchOperationW，支持复制粘贴功能 |