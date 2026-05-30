# EnvVFS - Directory Opus 配置指南

本文档说明如何在 Directory Opus 中配置 EnvVFS 插件，实现完整的环境变量管理功能。

## 方案概述

**核心思路**：VFS 插件只负责列出条目，所有交互功能通过 Directory Opus 原生功能实现。

- ✅ **VFS 插件**：列出环境变量，提供自定义列
- ✅ **文件夹格式**：为不同类型的环境变量分配图标
- ✅ **自定义菜单**：通过路径匹配实现右键菜单
- ✅ **按钮栏**：创建专用工具栏，支持快捷键

---

## 一、文件夹格式配置（图标分配）

### 1. 打开文件夹格式设置

```
设置 → 文件夹格式 → 新建
```

### 2. 配置路径规则

**名称**：`环境变量`

**路径**：`env://`

**图标设置**：

现在环境变量文件都有 `.env` 后缀，可以更容易地配置图标：

#### 方案 A：使用文件扩展名匹配（推荐）

在"文件名模式"中添加规则：

| 模式 | 图标 | 说明 |
|------|------|------|
| `*.env` | `shell32.dll,14` | 环境变量文件 |

#### 方案 B：使用通配符匹配

在"文件名模式"中添加规则：

| 模式 | 图标 | 说明 |
|------|------|------|
| `*` | `shell32.dll,14` | 默认图标（系统变量） |

#### 方案 C：使用脚本动态分配（高级）

在文件夹格式中使用脚本：

```javascript
// OnGetCustomIcon 事件
function OnGetCustomIcon(GetCustomIconData) {
    var path = GetCustomIconData.item.path;
    
    // 根据来源分配不同图标
    if (path.indexOf("env://") === 0) {
        // 可以根据自定义列的值来分配图标
        // 这里需要访问自定义列数据
        return "shell32.dll,14"; // 默认图标
    }
}
```

### 3. 应用配置

点击"确定"保存文件夹格式。

---

## 二、自定义右键菜单配置

### 1. 打开文件类型设置

```
设置 → 文件类型 → 所有文件和文件夹 → 上下文菜单
```

### 2. 添加环境变量专用菜单

点击"新建" → "Opus 函数"，添加以下菜单项：

**注意**：现在环境变量文件都有 `.env` 后缀，例如 `PATH.env`、`JAVA_HOME.env` 等。

#### 菜单项 1：编辑环境变量

**标签**：`编辑环境变量`

**条件**：`{filepath} 匹配 "env://*.env"`

**命令**：
```opusscript
@ifpath env://*.env
{filepath$}
// TODO: 实现编辑逻辑
// 可以调用外部脚本或程序
```

#### 菜单项 2：复制变量值

**标签**：`复制变量值`

**条件**：`{filepath} 匹配 "env://*.env"`

**命令**：
```opusscript
@ifpath env://*.env
Clipboard SET {file$}
// 或者使用脚本获取自定义列的值
```

#### 菜单项 3：复制变量名

**标签**：`复制变量名`

**条件**：`{filepath} 匹配 "env://*.env"`

**命令**：
```opusscript
@ifpath env://*.env
// 移除 .env 后缀
@set varname={file$|nopath|noterm|ext=}
Clipboard SET {$varname}
```

#### 菜单项 4：删除环境变量

**标签**：`删除环境变量`

**条件**：`{filepath} 匹配 "env://*.env"`

**命令**：
```opusscript
@ifpath env://*.env
// 移除 .env 后缀
@set varname={file$|nopath|noterm|ext=}
@confirm 确定要删除环境变量 "{$varname}" 吗？
Delete {filepath$}
```

#### 菜单项 5：打开系统环境变量设置

**标签**：`打开系统环境变量设置`

**条件**：`{filepath} 匹配 "env://*.env"`

**命令**：
```opusscript
@ifpath env://*.env
rundll32.exe sysdm.cpl,EditEnvironmentVariables
```

### 3. 使用脚本实现高级功能（可选）

创建脚本文件 `EnvVFS_Menu.js`：

```javascript
// EnvVFS_Menu.js
// Directory Opus 脚本 - 环境变量管理

function OnInit(initData) {
    initData.name = "EnvVFS Menu";
    initData.version = "1.0";
    initData.copyright = "2026";
    initData.url = "";
    initData.desc = "环境变量管理菜单";
}

function OnAddCommands(addCommands) {
    var cmd = addCommands.AddCommand();
    cmd.name = "EnvVFSEdit";
    cmd.desc = "编辑环境变量";
    cmd.label = "编辑环境变量";
    cmd.method = "OnEnvVFSEdit";
    
    cmd = addCommands.AddCommand();
    cmd.name = "EnvVFSCopyValue";
    cmd.desc = "复制环境变量值";
    cmd.label = "复制变量值";
    cmd.method = "OnEnvVFSCopyValue";
}

function OnEnvVFSEdit(cmdData) {
    var path = cmdData.func.sourcetab.selected(0).path;
    if (path.indexOf("env://") === 0) {
        var varName = path.substring(6);
        // 实现编辑逻辑
        DOpus.Output("编辑环境变量: " + varName);
    }
}

function OnEnvVFSCopyValue(cmdData) {
    var path = cmdData.func.sourcetab.selected(0).path;
    if (path.indexOf("env://") === 0) {
        var varName = path.substring(6);
        // 从自定义列获取值
        var value = GetEnvValue(varName);
        DOpus.clipboard.SetText(value);
    }
}

function GetEnvValue(varName) {
    // 这里需要实现从 VFS 插件获取环境变量值的逻辑
    // 可以通过读取虚拟文件内容实现
    return "";
}
```

---

## 三、专用按钮栏配置

### 1. 创建新工具栏

```
设置 → 工具栏 → 新建工具栏
```

**名称**：`环境变量工具`

### 2. 添加按钮

**注意**：现在环境变量文件都有 `.env` 后缀。

#### 按钮 1：刷新列表

**标签**：`刷新`

**图标**：`refresh.png` 或系统图标

**命令**：
```opusscript
@ifpath env://*
Refresh
```

**快捷键**：`F5`

#### 按钮 2：编辑变量

**标签**：`编辑`

**图标**：`edit.png`

**命令**：
```opusscript
@ifpath env://*.env
EnvVFSEdit
```

**快捷键**：`Ctrl+E`

#### 按钮 3：复制值

**标签**：`复制值`

**图标**：`copy.png`

**命令**：
```opusscript
@ifpath env://*.env
EnvVFSCopyValue
```

**快捷键**：`Ctrl+C`

#### 按钮 4：删除变量

**标签**：`删除`

**图标**：`delete.png`

**命令**：
```opusscript
@ifpath env://*.env
@set varname={file$|nopath|noterm|ext=}
@confirm 确定要删除环境变量 "{$varname}" 吗？
Delete
```

**快捷键**：`Delete`

#### 按钮 5：系统设置

**标签**：`系统设置`

**图标**：`settings.png`

**命令**：
```opusscript
rundll32.exe sysdm.cpl,EditEnvironmentVariables
```

**快捷键**：`Ctrl+Shift+E`

### 3. 导出按钮栏配置

可以将按钮栏配置导出为 `.dop` 文件，方便分享和备份。

---

## 四、自定义列配置

### 1. 启用自定义列

EnvVFS 插件已经提供了以下自定义列：

- **值** (`env_value`) - 环境变量的值
- **类型** (`env_type`) - 变量类型（REG_SZ, REG_EXPAND_SZ 等）
- **来源** (`env_source`) - 变量来源（系统、用户、临时）
- **系统** (`env_system`) - 是否为系统变量

### 2. 在文件夹格式中启用自定义列

```
设置 → 文件夹格式 → 环境变量 → 列
```

添加以下列：

1. 名称（默认）
2. 值（自定义列）
3. 类型（自定义列）
4. 来源（自定义列）
5. 系统（自定义列）

---

## 五、高级配置

### 1. 使用脚本扩展功能

创建 JScript 脚本文件，实现更复杂的功能：

```javascript
// EnvVFS_Extended.js
// 高级环境变量管理功能

function OnGetEnvValue(varName) {
    // 通过 WScript.Shell 获取环境变量值
    var shell = new ActiveXObject("WScript.Shell");
    return shell.ExpandEnvironmentStrings("%" + varName + "%");
}

function OnSetEnvValue(varName, value) {
    // 设置环境变量（需要管理员权限）
    var shell = new ActiveXObject("WScript.Shell");
    var env = shell.Environment("User"); // 或 "System"
    env(varName) = value;
}
```

### 2. 集成外部工具

可以将外部工具集成到按钮栏：

```opusscript
// 使用 Notepad++ 编辑
"C:\Program Files\Notepad++\notepad++.exe" {filepath$}

// 使用 VS Code 编辑
"code" {filepath$}
```

---

## 六、故障排除

### 问题 1：菜单不显示

**原因**：路径匹配规则不正确

**解决**：检查 `{filepath}` 是否正确匹配 `env://` 路径

### 问题 2：自定义列不显示

**原因**：文件夹格式未启用自定义列

**解决**：在文件夹格式中添加自定义列

### 问题 3：按钮栏不显示

**原因**：工具栏未启用

**解决**：在工具栏列表中勾选"环境变量工具"

---

## 七、配置备份

### 导出配置

1. **文件夹格式**：设置 → 文件夹格式 → 导出
2. **文件类型**：设置 → 文件类型 → 导出
3. **工具栏**：设置 → 工具栏 → 导出

### 导入配置

在需要时可以导入之前导出的配置文件。

---

## 八、总结

通过 Directory Opus 的原生功能，我们实现了：

- ✅ 图标自动分配
- ✅ 自定义右键菜单
- ✅ 专用按钮栏
- ✅ 自定义列显示
- ✅ 快捷键支持

**优势**：
- 不依赖 VFS 插件的不稳定功能
- 配置灵活，易于修改
- 充分利用 Directory Opus 的强大功能
- 用户体验更好

**下一步**：
1. 根据实际需求调整菜单项
2. 创建更多自定义按钮
3. 编写脚本实现复杂功能
4. 分享配置给其他用户

---

**配置文件位置**：
- 文件夹格式：`%APPDATA%\GPSoftware\Directory Opus\FolderFormats\`
- 文件类型：`%APPDATA%\GPSoftware\Directory Opus\FileTypes\`
- 工具栏：`%APPDATA%\GPSoftware\Directory Opus\Toolbars\`

**参考文档**：
- [Directory Opus 官方手册](https://docs.dopus.com)
- [Directory Opus 脚本编程](https://resource.dopus.com/c/scripting/10)
