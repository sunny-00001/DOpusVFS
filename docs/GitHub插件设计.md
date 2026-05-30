# GitHub VFS 插件设计文档

> 本文档完整记录了项目设计阶段的所有讨论细节，包括思考过程、备选方案、取舍理由和创新想法的演进。

---

## 一、项目起源与核心需求

### 1.1 最初的需求表述

用户提出：**"我正在设计一种通过协议路径来操作 GitHub 的方式。路径以 `github://` 开头。比如我想导航到搜索仓库，搜索代码、搜索作者，下载仓库代码。我应该如何处理？"**

核心需求拆解：
- 通过 `github://` 协议路径操作 GitHub
- 导航到搜索仓库
- 搜索代码
- 搜索作者
- 下载仓库代码

### 1.2 核心约束的发现

讨论过程中发现了一个关键约束：**VFS 自定义右键菜单完全不可用**。这意味着所有交互必须通过路径处理来代替右键菜单操作。这个约束深刻影响了整个设计方向——从"路径+右键菜单"的双轨交互，转变为"纯路径驱动"的单轨交互。

用户原话：**"排除右键菜单，因为 vfs 自定义右键菜单完全不可用，所以我才用路径处理代替"**

---

## 二、路径范式设计——从一种到五种再到混合式

### 2.1 第一轮讨论：有哪些路径处理方式

最初提出了五种路径处理范式：

#### 范式一：资源映射式

路径直接映射 GitHub 的资源层级结构，最直观的方式：

```
github://owner/repo/path/to/file
github://microsoft/vscode/src/main.ts
```

**优点**：直观，与 GitHub URL 结构一致
**缺点**：无法表达搜索、星标等非资源操作；路径歧义问题（`explore` 是用户名还是虚拟目录？）

#### 范式二：虚拟目录式

用虚拟目录组织功能入口，不直接对应 GitHub 资源：

```
github://search/repos/keyword
github://my-starred/
github://trending/
```

**优点**：功能入口清晰，无歧义
**缺点**：偏离 GitHub 原生结构，需要记忆虚拟目录名

#### 范式三：命令式路径

路径即命令，导航到该路径即触发操作：

```
github://clone/microsoft/vscode
github://star/microsoft/vscode
github://fork/microsoft/vscode
```

**优点**：操作语义明确
**缺点**：与文件管理器"浏览"的交互模式冲突；命令执行后路径指向什么？

#### 范式四：查询参数式

在路径后附加查询参数来控制行为：

```
github://microsoft/vscode?branch=dev&sort=name
github://search?q=react&type=repos&sort=stars
```

**优点**：灵活，可扩展
**缺点**：VFS 路径解析器可能不支持 `?` 和 `&`；不符合文件路径的直觉

#### 范式五：混合式

结合以上多种方式的优点，不同场景用不同范式：

```
github://explore/repos/react/        # 虚拟目录+搜索即目录
github://go/microsoft/vscode/        # 前缀式直达
github://go/microsoft/vscode/.gh/    # 元数据目录
```

**最终选择**：混合式，因为它最灵活，能适应不同场景。

### 2.2 路径歧义问题的深入讨论

**问题提出**：当用户输入 `github://explore` 时，这是虚拟目录还是名为 "explore" 的 GitHub 用户？当输入 `github://microsoft/vscode` 时，microsoft 是用户还是组织？

**讨论了多种解决方案**：

1. **保留字方案**：将 `explore`、`mine`、`search` 等设为保留字，不允许作为用户名
   - 问题：GitHub 上确实存在名为 "explore" 的用户

2. **前缀区分方案**：用前缀明确路径类型
   - `github://go/microsoft/vscode` — 明确是直达资源
   - `github://explore/` — 明确是虚拟目录
   - 用户评价：**"我觉得 go 挺好的"**

3. **配置切换方案**：严格模式 vs 宽松模式
   - 严格模式：必须使用前缀，无歧义
   - 宽松模式：可直接输入 `owner/repo`，非保留字时视为用户名

**最终决策**：采用前缀区分 + 配置切换的混合方案。默认严格模式（必须用 `explore/mine/go` 前缀），可选宽松模式（直接输入 `owner/repo`）。

### 2.3 角色视角式（补充范式）

在五种基础范式之外，讨论中又提出了按用户角色组织路径的思路：

| 角色 | 路径前缀 | 场景 |
|------|----------|------|
| 探索者 | `github://explore/` | 我要搜索和发现 |
| 所有者 | `github://mine/` | 我要管理自己的东西 |
| 直达者 | `github://go/` | 我知道我要去哪里 |

这个设计让根目录只有三个入口，简洁且无歧义：

```
github://
├── explore/    # 探索
├── mine/       # 我的
└── go/         # 直达
```

### 2.4 五种范式与关键决策点集成到配置对话框

用户提出：**"可以将五种范式和关键决策点集成到配置对话框吗"**

讨论结果：在配置对话框的"路径"标签页中集成以下决策点：

| 决策点 | 配置项 | 选项 | 默认值 |
|--------|--------|------|--------|
| 路径范式 | 路径模式 | 严格模式/宽松模式 | 严格模式 |
| 元数据前缀 | 元数据目录名 | 自定义文本 | `.gh` |
| 引用关键字 | 分支切换前缀 | 自定义文本 | `@` |
| 快捷别名 | 启用别名 | 开/关 | 开 |
| 旧路径兼容 | 启用旧路径 | 开/关 | 开 |

---

## 三、VFS 插件路径处理的拓展讨论

### 3.1 第一轮拓展：VFS 路径处理还能做什么

用户问：**"先说 VFS 插件的路径处理还能做哪些拓展"**

讨论了以下拓展方向：

1. **路径别名/快捷方式**：`~` 代表当前用户，`s/` 代表搜索
2. **路径自动补全**：输入时提示用户名、仓库名
3. **路径模板**：预设常用路径组合
4. **路径历史/书签**：记录常用路径
5. **路径重定向**：旧路径自动跳转到新路径

### 3.2 第二轮拓展：从 VFS 特性和 GitHub 功能特性出发

用户追问：**"还有吗，你从 VFS 插件本身特性和 GitHub 支持功能特性上拓展"**

#### 从 VFS 插件特性拓展：

1. **虚拟文件交互**：利用文件的存在/不存在表示状态（信号文件）
2. **文件内容映射**：虚拟文件内容反映 GitHub 资源状态（镜像文件）
3. **目录即操作**：进入目录触发操作（传送门目录）
4. **文件读取即触发**：复制/下载文件触发操作（动作文件）
5. **自定义列信息**：在详情视图中展示 GitHub 元数据
6. **路径驱动的上下文菜单**：虽然自定义右键菜单不可用，但可以通过脚本按钮调用命令

#### 从 GitHub 功能特性拓展：

1. **Issue/PR 管理**：通过虚拟文件浏览和编辑
2. **分支/标签切换**：通过路径段切换代码版本
3. **星标/关注操作**：通过信号文件切换
4. **Release 下载**：通过动作文件触发
5. **代码搜索结果**：搜索即目录模式
6. **通知浏览**：虚拟目录展示通知
7. **Gist 管理**：虚拟目录管理 Gist
8. **CI/CD 状态**：虚拟文件反映工作流状态
9. **Wiki 浏览**：虚拟目录浏览 Wiki 页面
10. **安全告警**：虚拟文件展示安全漏洞信息

### 3.3 排除右键菜单后的创新方向

用户明确：**"排除右键菜单，因为 vfs 自定义右键菜单完全不可用，所以我才用路径处理代替，你还有什么好的想法"**

这个约束催生了八种创新的路径交互模式（详见第四章）。

---

## 四、八种路径交互模式——详细设计

### 4.1 信号文件（Signal File）

**概念起源**：需要一种方式来表示和切换布尔状态（星标/未星标、关注/未关注），但不能用右键菜单。

**设计思路**：利用文件的存在与否表示状态。文件存在 = "开"，文件不存在 = "关"。双击（打开）文件 = 切换状态。

**详细设计**：

| 信号文件 | 存在含义 | 双击操作 |
|----------|----------|----------|
| `starred.signal` | 已星标 | 取消星标（文件消失） |
| `watching.signal` | 已关注 | 取消关注（文件消失） |
| `pinned.signal` | 已固定 | 取消固定（文件消失） |

**交互流程**：
1. 用户进入 `github://go/owner/repo/.gh/`
2. 看到 `starred.signal` 文件 → 知道已星标
3. 双击 `starred.signal` → 调用 API 取消星标 → 文件消失
4. 再次进入目录 → `starred.signal` 不存在 → 知道未星标
5. 双击空白处创建 `starred.signal` → 调用 API 添加星标 → 文件出现

**讨论细节**：信号文件应该是零字节还是有内容？
- 零字节：更纯粹，状态完全由存在性决定
- 有内容：可以包含额外信息（如星标时间）
- **决策**：零字节为主，保持简洁。额外信息通过镜像文件提供。

### 4.2 管道目录（Pipeline Directory）

**概念起源**：需要一种方式来处理"输入→处理→输出"的工作流。

**设计思路**：虚拟目录，将文件放入（写入）该目录即触发处理，处理结果在目录中显示。

**潜在应用场景**：
- 代码格式化目录：放入源码文件，格式化后的文件出现在目录中
- CI/CD 触发目录：放入配置文件，触发工作流
- 批量重命名目录：放入重命名规则文件，执行批量操作
- 图片压缩目录：放入图片，压缩后出现在目录中

**讨论细节**：管道目录的实现复杂度较高，需要监听文件写入事件。在 VFS 中实现可能需要轮询或钩子机制。因此将管道目录列为 P3 优先级。

### 4.3 镜像文件（Mirror File）

**概念起源**：需要一种方式来查看和修改 GitHub 资源的元数据（描述、主题、主页等），但不能用右键菜单的"属性"。

**设计思路**：虚拟文件，内容实时反映 GitHub 资源状态。读取文件 = 查看 GitHub 数据，修改文件内容 = 更新 GitHub 数据。

**详细设计**：

| 镜像文件 | 读取时显示 | 写入时更新 |
|----------|------------|------------|
| `info.txt` | 仓库描述、语言、星标数、Fork数等 | 修改描述、主页等可写字段 |
| `topics.txt` | 仓库主题标签（每行一个） | 更新主题标签 |
| `homepage.txt` | 仓库主页 URL | 更新主页 URL |

**交互流程示例**（修改仓库描述）：
1. 打开 `info.txt` → 看到当前描述
2. 修改描述内容 → 保存
3. VFS 关闭文件时调用 GitHub API 更新描述
4. 再次打开 `info.txt` → 看到更新后的描述

**讨论细节**：
- `info.txt` 应该是纯文本还是结构化格式（如 JSON/YAML）？
  - 纯文本：更符合文件管理器的使用习惯，普通用户友好
  - JSON/YAML：结构化，但编辑门槛高
  - **决策**：采用键值对格式的纯文本，类似 `.ini` 文件风格
- 只读字段（如星标数）如何处理？
  - 显示但不可修改，写入时忽略只读字段

### 4.4 传送门目录（Portal Directory）

**概念起源**：需要一种方式来切换查看不同分支/标签/commit 的文件，但不能用下拉框或标签页。

**设计思路**：虚拟目录，进入后展示另一个路径视图的内容。目录名即"传送目的地"。

**详细设计**：

| 传送门路径 | 展示内容 |
|------------|----------|
| `.gh/branches/main/` | main 分支的文件列表 |
| `.gh/branches/dev/` | dev 分支的文件列表 |
| `.gh/tags/v1.0/` | v1.0 标签的文件列表 |
| `.gh/at/abc1234/` | commit abc1234 的文件列表 |

**与 `@` 引用关键字的关系**：
- `github://go/owner/repo/@main/` — 在路径中直接切换分支（快捷方式）
- `github://go/owner/repo/.gh/branches/main/` — 通过元数据目录切换（完整路径）
- 两种方式展示相同内容，`@` 是快捷写法

**讨论细节**：传送门目录是否应该显示在文件路径中？
- 是：路径完整，用户知道自己在看哪个分支
- 否：路径更简洁
- **决策**：是，路径应该反映当前查看的分支/标签

### 4.5 动作文件（Action File）

**概念起源**：需要一种方式来触发一次性操作（下载、复制 URL 等），但不能用右键菜单。

**设计思路**：只读虚拟文件，用户对文件执行"读取"操作（复制/下载）时触发实际操作。

**详细设计**：

| 动作文件 | 用户操作 | 触发行为 |
|----------|----------|----------|
| `download.zip` | 复制文件到本地 | 下载仓库 ZIP 压缩包 |
| `download.tar.gz` | 复制文件到本地 | 下载仓库 TAR.GZ 压缩包 |
| `clone-url.txt` | 打开文件/复制内容 | 显示/复制 git clone URL |

**讨论细节**：
- 动作文件应该有文件大小吗？
  - 有：文件管理器可能根据大小决定是否显示进度条
  - 无：更明确表示这是虚拟文件
  - **决策**：下载类文件显示预估大小，URL 类文件显示实际内容长度
- 复制 `download.zip` 到本地时，如何显示进度？
  - VFS 的 `VFS_ReadFile` 可以分块返回数据，DOpus 会显示进度条

### 4.6 列表目录（List Directory）

**概念起源**：需要一种方式来浏览 GitHub 的列表型资源（Issues、PRs、分支等）。

**设计思路**：虚拟目录，每个条目是一个虚拟文件或子目录。

**详细设计**：

| 列表目录 | 条目格式 | 条目类型 |
|----------|----------|----------|
| `issues/` | `#1 标题`, `#2 标题` | 虚拟文件（可编辑） |
| `pulls/` | `#1 标题`, `#2 标题` | 虚拟文件（可编辑） |
| `branches/` | `main`, `dev` | 虚拟目录（传送门） |
| `tags/` | `v1.0`, `v2.0` | 虚拟目录（传送门） |
| `releases/` | `v1.0.0`, `v2.0.0` | 虚拟文件 |

**Issue/PR 的虚拟文件设计**：
- 打开 `#123 标题` 文件 → 显示 Issue/PR 的正文内容
- 修改文件内容 → 更新 Issue/PR 的标题和正文
- 文件属性中显示：状态、作者、标签、评论数

**状态过滤**：
```
.gh/issues/open/      → 只显示打开的 Issues
.gh/issues/closed/    → 只显示关闭的 Issues
.gh/pulls/open/       → 只显示打开的 PRs
.gh/pulls/closed/     → 只显示关闭的 PRs
```

### 4.7 搜索即目录（Search-as-Directory）

**概念起源**：搜索是 GitHub 最常用的功能之一，需要一种自然的方式来表达"搜索"。

**设计思路**：搜索关键词作为路径段，目录内容即搜索结果。导航到路径 = 执行搜索。

**详细设计**：

```
github://explore/repos/              → 搜索入口（显示"输入搜索关键词.txt"提示文件）
github://explore/repos/react/        → 搜索 "react" 的仓库结果
github://explore/code/               → 代码搜索入口
github://explore/code/useState/      → 搜索 "useState" 的代码结果
github://explore/users/              → 用户搜索入口
github://explore/users/torvalds/     → 搜索 "torvalds" 的用户结果
```

**过滤条件设计**：通过路径段附加键值对过滤参数：
```
github://explore/repos/react/language:typescript/sort:stars/
```

**讨论细节**：
- 搜索关键词包含特殊字符（如 `C++`）怎么办？
  - URL 编码：`C%2B%2B`
  - 但 VFS 路径中 `%` 可能有问题
  - **决策**：对特殊字符进行 URL 编码，路径解析时自动解码
- 搜索结果如何翻页？
  - GitHub API 默认每页 30 条
  - 可以在配置中调整每页条目数（最大 100）
  - 暂不实现翻页，通过调整每页条目数来显示更多结果

### 4.8 状态迁移（State Migration）

**概念起源**：需要一种方式来切换同一资源的不同状态视图。

**设计思路**：通过路径段指定状态，导航到不同路径即切换到不同状态视图。

**详细设计**：

```
.gh/issues/open/              → 打开的 Issues
.gh/issues/closed/            → 关闭的 Issues
.gh/compare/main..dev         → 比较 main 和 dev 分支
.gh/compare/v1.0..v2.0        → 比较两个标签
```

**比较功能的路径设计**：
- `..` 作为分隔符连接两个引用
- `compare/main..dev` → 比较分支 main 和 dev
- `compare/abc123..def456` → 比较两个 commit
- 比较结果以虚拟文件形式展示（diff 文本）

---

## 五、元数据目录 `.gh/` 的完整设计

### 5.1 设计动机

GitHub 仓库不仅仅是文件集合，还有 Issues、PRs、分支、标签、Release 等丰富的元数据。这些元数据需要一个统一的入口来访问和操作，同时不干扰正常的文件浏览。

### 5.2 为什么选择隐藏目录形式

**讨论了多种方案**：

1. **并行虚拟目录**：在仓库根目录下同时显示文件和元数据目录
   - 问题：文件列表中混入虚拟条目，干扰正常浏览
2. **独立路径前缀**：`github://meta/owner/repo/issues/`
   - 问题：路径过长，脱离仓库上下文
3. **隐藏目录**：`owner/repo/.gh/`
   - 优点：不干扰正常浏览，需要时进入即可；类似 `.git` 目录的约定
   - **决策**：采用隐藏目录形式

### 5.3 完整目录结构

```
owner/repo/.gh/
├── issues/                      # Issue 列表目录
│   ├── open/                    # 打开的 Issues（状态迁移）
│   ├── closed/                  # 关闭的 Issues（状态迁移）
│   └── 123                      # Issue #123 详情（虚拟文件，可编辑）
├── pulls/                       # PR 列表目录
│   ├── open/                    # 打开的 PRs
│   ├── closed/                  # 关闭的 PRs
│   └── 456                      # PR #456 详情
├── branches/                    # 分支列表目录
│   └── main/                    # 分支视图（传送门目录，进入后显示该分支文件）
├── tags/                        # 标签列表目录
│   └── v1.0/                    # 标签视图（传送门目录）
├── at/                          # 按 commit SHA 查看
│   └── abc1234/                 # 指定 commit 的文件视图
├── releases/                    # 发布版本列表
├── compare/                     # 分支/标签比较
│   └── main..dev                # 比较 main 和 dev
├── starred.signal               # 星标信号文件
├── watching.signal              # 关注信号文件
├── pinned.signal                # 固定信号文件
├── info.txt                     # 仓库信息镜像文件
├── topics.txt                   # 主题标签镜像文件
├── homepage.txt                 # 主页 URL 镜像文件
├── download.zip                 # 下载 ZIP 动作文件
├── download.tar.gz              # 下载 TAR.GZ 动作文件
└── clone-url.txt                # 克隆 URL 动作文件
```

### 5.4 可配置项

| 配置项 | 默认值 | 可选值 | 说明 |
|--------|--------|--------|------|
| 元数据前缀 | `.gh` | `.gh`, `.github`, `.meta`, 自定义 | 适应不同用户习惯 |
| 引用关键字 | `@` | `@`, `#`, 自定义 | 切换分支/标签的前缀 |

**讨论细节**：为什么默认用 `.gh` 而不是 `.github`？
- `.gh` 更短，输入方便
- `.github` 更明确，但与 GitHub Actions 的 `.github` 目录可能混淆
- 可配置，用户自行选择

---

## 六、路径别名系统——详细设计

### 6.1 设计动机

用户经常需要输入常用路径，如搜索仓库、查看自己的仓库等。别名系统提供快捷方式，减少输入。

### 6.2 完整别名表

| 别名 | 展开为 | 说明 | 记忆技巧 |
|------|--------|------|----------|
| `~` | `mine` | 当前用户 | Unix 家目录 |
| `~/repos` | `mine/repos` | 我的仓库 | Unix 路径风格 |
| `~username` | `go/username` | 直达用户 | `~` = 用户 |
| `@username` | `go/username` | 直达用户 | `@` = 提及用户 |
| `s/keyword` | `explore/repos/keyword` | 搜索仓库 | **s**earch |
| `sc/keyword` | `explore/code/keyword` | 搜索代码 | **s**earch **c**ode |
| `su/keyword` | `explore/users/keyword` | 搜索用户 | **s**earch **u**sers |
| `t` | `explore/trending` | 趋势 | **t**rending |
| `t/d` | `explore/trending/daily` | 每日趋势 | **t**rending **d**aily |
| `t/w` | `explore/trending/weekly` | 每周趋势 | **t**rending **w**eekly |
| `t/m` | `explore/trending/monthly` | 每月趋势 | **t**rending **m**onthly |

### 6.3 别名展开规则

1. `~` 开头：展开为 `mine/` 或 `go/`
   - `~` → `mine`
   - `~/xxx` → `mine/xxx`
   - `~username` → `go/username`
   - `~username/repo` → `go/username/repo`

2. `@` 开头：展开为 `go/`
   - `@username` → `go/username`
   - `@username/repo` → `go/username/repo`

3. `s/` 开头：展开为搜索仓库
   - `s/react` → `explore/repos/react`

4. `sc/` 开头：展开为搜索代码
   - `sc/useState` → `explore/code/useState`

5. `su/` 开头：展开为搜索用户
   - `su/torvalds` → `explore/users/torvalds`

6. `t` 相关：展开为趋势
   - `t` → `explore/trending`
   - `t/d` → `explore/trending/daily`
   - `t/w` → `explore/trending/weekly`
   - `t/m` → `explore/trending/monthly`

### 6.4 别名冲突处理

**讨论**：如果 GitHub 上有用户名为 `s` 或 `t` 的用户怎么办？
- 在严格模式下，别名优先，`s/` 和 `t` 不会被视为用户名
- 如需访问这些用户，使用 `go/s` 或 `go/t`
- 别名可通过配置开关禁用

---

## 七、旧路径兼容方案

### 7.1 需要兼容的旧路径

| 旧路径 | 新路径 | 说明 |
|--------|--------|------|
| `github://search/keyword` | `github://explore/repos/keyword` | 搜索仓库 |
| `github://search/keyword/owner/repo` | `github://explore/repos/keyword` 然后进入 `owner/repo` | 搜索后导航 |
| `github://my-repos/` | `github://mine/repos/` | 我的仓库 |
| `github://my-repos/owner/repo/path` | `github://mine/repos/owner/repo/path` | 我的仓库内路径 |
| `github://starred/` | `github://mine/starred/` | 星标仓库 |

### 7.2 兼容实现方式

在路径解析器中，当 `enableOldPaths` 配置开启时：
1. 检测到 `search` 开头 → 标记为 `isLegacyPath`，转换为 `explore/repos/` 路径
2. 检测到 `my-repos` 开头 → 标记为 `isLegacyPath`，转换为 `mine/repos/` 路径
3. 检测到 `starred` → 标记为 `isLegacyPath`，转换为 `mine/starred/` 路径

旧路径标记 `isLegacyPath` 的目的：可以在 UI 中提示用户"此路径格式已过时，建议使用新格式"。

---

## 八、双引擎架构设计

### 8.1 架构图

```
┌─────────────────────────────────────────────┐
│              Directory Opus                  │
│                                             │
│  ┌───────────────┐    ┌──────────────────┐  │
│  │   VFS 引擎     │    │   脚本引擎        │  │
│  │  (显示/交互)    │    │  (命令执行)       │  │
│  │               │    │                  │  │
│  │ · 路径解析     │    │ · git clone      │  │
│  │ · 目录列表     │    │ · git pull       │  │
│  │ · 文件读写     │    │ · git push       │  │
│  │ · 信号文件     │    │ · git checkout   │  │
│  │ · 镜像文件     │    │ · 脚本按钮       │  │
│  │ · 动作文件     │    │ · 批量操作       │  │
│  └───────┬───────┘    └────────┬─────────┘  │
│          │                      │            │
│          └──────────┬───────────┘            │
│                     │                        │
│          ┌──────────▼──────────┐             │
│          │    GitHubClient     │             │
│          │    (API 层)         │             │
│          │                    │             │
│          │ · REST API 调用    │             │
│          │ · 认证管理         │             │
│          │ · 缓存管理         │             │
│          │ · 错误处理         │             │
│          │ · WinHTTP 会话     │             │
│          └────────────────────┘             │
└─────────────────────────────────────────────┘
```

### 8.2 为什么需要双引擎

**讨论过程**：

VFS 插件天生适合"浏览"和"查看"，但不适合"执行命令"。Git 操作（clone、pull、push 等）是命令行操作，与 VFS 的文件浏览模式不匹配。

**解决方案**：
- **VFS 引擎**：负责所有"浏览"和"交互"——路径解析、目录列表、文件读写、信号文件、镜像文件、动作文件
- **脚本引擎**：负责所有"执行"——Git 命令、批量操作、脚本按钮
- **API 层**：共享的 GitHub API 客户端，VFS 引擎和脚本引擎都通过它访问 GitHub

### 8.3 脚本按钮设计

由于右键菜单不可用，通过 DOpus 的脚本按钮来提供命令入口：

```
@disablenosel
github://go/{sourcepath|filepath|..\\..\\}
```

脚本按钮可以调用 VFS 提供的上下文菜单命令（如 `gh_search`、`gh_refresh` 等）。

---

## 九、错误即文件模式（Error-as-File）

### 9.1 设计动机

传统做法是操作失败时弹出错误对话框。但在文件管理器中，弹窗会打断用户的操作流，而且用户无法复制错误信息。

### 9.2 设计方案

当操作失败时，在目录中显示一个错误文件，文件名即错误摘要，文件内容即错误详情：

| 错误文件 | 触发条件 | 文件内容 |
|----------|----------|----------|
| `⚠ 未找到用户或组织.txt` | 用户/组织不存在或无公开仓库 | "xxx 不存在或无公开仓库" |
| `⚠ 仓库不存在或无法访问.txt` | 仓库不存在或无权限 | "owner/repo 无法访问" |
| `⚠ 路径不存在.txt` | 仓库内路径不存在 | "path 在仓库中不存在" |
| `⚠ 无法解析路径.txt` | 路径格式无效 | "xxx 路径无效" |

### 9.3 优势

1. **不打断操作流**：用户继续留在文件管理器中
2. **错误信息可复制**：打开文件即可复制错误文本
3. **错误信息可搜索**：文件名包含关键信息
4. **符合文件管理器习惯**：错误也是一种"内容"

### 9.4 讨论细节

**问题**：错误文件是否应该可以删除？
- 可以删除：用户"确认"了错误，删除后目录为空
- 不可删除：错误文件始终存在，提醒用户
- **决策**：错误文件设为只读属性，不可删除。用户需要修正路径才能消除错误。

---

## 十、配置对话框设计——详细方案

### 10.1 布局参考

参考 rclone 的配置界面布局，采用标签页式设计。rclone 的配置界面特点：
- 标签页分组，信息不拥挤
- 每个标签页内分组框进一步细分
- 底部有全局操作按钮

### 10.2 四个标签页详细设计

#### 认证标签页

```
┌─ 认证设置 ──────────────────────────────┐
│                                         │
│  ┌─ Token 认证 ──────────────────────┐  │
│  │ Token: [________________________] │  │
│  │ 提示: 在 GitHub Settings >        │  │
│  │       Developer settings >        │  │
│  │       Personal access tokens      │  │
│  │       生成 Token                  │  │
│  └───────────────────────────────────┘  │
│                                         │
│  ┌─ OAuth 设备流 ────────────────────┐  │
│  │ [启动 OAuth 认证]                 │  │
│  │ 状态: 未认证                      │  │
│  └───────────────────────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```

**讨论细节**：
- Token 认证最简单，是主要认证方式
- OAuth 设备流更安全，但实现复杂（需要轮询）
- Basic 认证（用户名+密码）已不推荐，GitHub 将逐步弃用
- **决策**：Token 为主，OAuth 为辅，Basic 不实现

#### 连接标签页

```
┌─ 连接设置 ──────────────────────────────┐
│                                         │
│  API URL: [api.github.com___________]   │
│  连接超时: [30___] 秒                    │
│  缓存超时: [60___] 秒                    │
│  每页条目: [100__] 条                    │
│                                         │
│  ☑ 自动刷新  间隔: [60___] 秒            │
│                                         │
│  ┌─ 代理设置 ────────────────────────┐  │
│  │ ☑ 使用代理                        │  │
│  │ 类型: [HTTP ▼]                    │  │
│  │ 主机: [______________]            │  │
│  │ 端口: [7890____]                  │  │
│  └───────────────────────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```

**讨论细节**：
- API URL 支持 GitHub Enterprise Server（GHES），如 `github.mycompany.com/api/v3`
- 代理支持是必要功能，中国用户经常需要代理访问 GitHub
- SOCKS5 代理通过 WinHTTP 的 `socks=` 前缀支持

#### 显示标签页

```
┌─ 显示设置 ──────────────────────────────┐
│                                         │
│  ☑ 显示 Fork 仓库                       │
│  ☑ 显示已归档仓库                        │
│  ☑ 显示私有仓库                          │
│  ☑ 显示描述列                            │
│                                         │
│  ☑ 大文件警告  阈值: [10___] MB          │
│                                         │
│  仓库排序: [更新时间 ▼]                  │
│  提交消息模板: [___________________]     │
│                                         │
└─────────────────────────────────────────┘
```

**讨论细节**：
- 大文件警告：GitHub API 对大文件操作有限制，超过阈值时提醒用户
- 提交消息模板：上传文件时自动使用模板作为 commit message 前缀
- 仓库排序选项：更新时间、名称、星标数、创建时间

#### 路径标签页

```
┌─ 路径设置 ──────────────────────────────┐
│                                         │
│  路径模式: [严格模式 ▼]                  │
│            (严格模式: explore/mine/go)   │
│            (宽松模式: 直接输入 owner/repo)│
│                                         │
│  元数据前缀: [.gh______]                 │
│  引用关键字: [@________]                 │
│                                         │
│  ☑ 启用快捷别名                          │
│    ~ = mine, s/ = 搜索, t = 趋势        │
│                                         │
│  ☑ 启用旧路径兼容                        │
│    search/ → explore/repos/             │
│    my-repos/ → mine/repos/              │
│                                         │
└─────────────────────────────────────────┘
```

**讨论细节**：路径标签页是五种范式和关键决策点的集成点。用户在这里决定：
1. 使用严格模式还是宽松模式（范式选择）
2. 元数据目录叫什么（约定配置）
3. 分支切换用什么符号（约定配置）
4. 是否启用别名（便捷性 vs 一致性）
5. 是否兼容旧路径（向后兼容）

### 10.3 全局操作

| 按钮 | 功能 | 实现方式 |
|------|------|----------|
| 测试连接 | 异步测试 API 连接，显示认证用户名 | 创建后台线程调用 `GitHubClient::TestConnection()` |
| 确定 | 保存配置并关闭 | 调用 `SaveConfigFromTabs()` + `GitHubClient::SaveConfig()` |
| 取消 | 放弃修改 | 直接关闭对话框 |

---

## 十一、自定义列设计

### 11.1 设计动机

DOpus 的详情视图支持自定义列，可以展示比文件名更丰富的信息。对于 GitHub 仓库，星标数、语言、描述等信息非常重要。

### 11.2 八列自定义信息

| 列键 | 列名 | 对齐方式 | 适用范围 | 数据来源 |
|------|------|----------|----------|----------|
| `ghdesc` | 描述 | 左对齐 | 仓库 | `description` 字段，超 80 字符截断 |
| `ghlang` | 语言 | 左对齐 | 仓库/代码 | `language` 字段 |
| `ghstars` | 星标数 | 右对齐/数字 | 仓库 | `stargazers_count` 字段 |
| `ghvis` | 可见性 | 左对齐 | 仓库 | `private` 字段 → "私有"/"公开" |
| `ghbranch` | 默认分支 | 左对齐 | 仓库 | `default_branch` 字段 |
| `ghforks` | Fork数 | 右对齐/数字 | 仓库 | `forks_count` 字段 |
| `ghsize` | 大小 | 右对齐/大小 | 仓库/文件 | `size` 字段，自动格式化 (KB/MB/GB) |
| `ghupdated` | 更新时间 | 左对齐 | 仓库/文件 | `pushed_at`/`updated_at` 字段 |

### 11.3 不同视图的列内容

| 视图 | 描述 | 语言 | 星标数 | 可见性 | 默认分支 | Fork数 | 大小 | 更新时间 |
|------|------|------|--------|--------|----------|--------|------|----------|
| 仓库列表 | 仓库描述 | 主语言 | ⭐数 | 公开/私有 | main | Fork数 | 仓库大小 | 推送时间 |
| 文件列表 | 目录/文件 | - | - | - | - | - | 文件大小 | 更新时间 |
| 代码搜索 | "代码" | 仓库语言 | 仓库星标 | - | - | - | - | - |
| 用户列表 | "用户" | 真实姓名 | 公开仓库数 | - | 位置 | - | - | - |
| Issue列表 | "Issue" | 编号 | 状态 | 作者 | 评论数 | - | - | - |
| PR列表 | "PR" | 编号 | 状态 | 作者 | 评论数 | - | - | - |
| 分支列表 | "分支" | 分支名 | ✓(默认) | SHA前7位 | - | - | - | - |
| Release列表 | "发布" | 标签名 | 发布名 | 预发布/正式 | 草稿 | - | - | - |

---

## 十二、上下文菜单命令设计

### 12.1 设计约束

VFS 自定义右键菜单完全不可用，但 DOpus 仍然会调用 `VFS_GetContextMenuW` 和 `VFS_ContextVerbW`。我们可以提供自定义菜单项，供脚本按钮或 DOpus 内置的上下文菜单机制使用。

### 12.2 命令列表

#### 根目录/搜索目录

| 命令 | 标签 | 功能 | 实现方式 |
|------|------|------|----------|
| `gh_search` | 搜索仓库... | 弹出输入框，跳转搜索结果 | InputBox → 构建路径 → VFSCVRES_CHANGEDIR |
| `gh_search_code` | 搜索代码... | 弹出输入框，搜索代码 | InputBox → 构建路径 → VFSCVRES_CHANGEDIR |
| `gh_refresh` | 刷新缓存 | 清除缓存并刷新 | InvalidateCache → VFSCVRES_CHANGE |

#### 仓库目录

| 命令 | 标签 | 功能 | 实现方式 |
|------|------|------|----------|
| `gh_open_browser` | 在浏览器中打开 | 打开仓库 GitHub 页面 | ShellExecuteW(htmlUrl) |
| `gh_copy_clone_url` | 复制克隆 URL | 复制 git clone 地址 | GetRepoInfo → CopyToClipboard(cloneUrl) |
| `gh_view_issues` | 查看 Issues | 浏览器打开 Issues 页 | ShellExecuteW(issues URL) |
| `gh_view_prs` | 查看 Pull Requests | 浏览器打开 PR 页 | ShellExecuteW(pulls URL) |
| `gh_view_releases` | 查看 Releases | 浏览器打开 Releases 页 | ShellExecuteW(releases URL) |
| `gh_copy_repo_url` | 复制仓库地址 | 复制 HTML URL | GetRepoInfo → CopyToClipboard(htmlUrl) |
| `gh_refresh` | 刷新缓存 | 清除缓存并刷新 | InvalidateCache → VFSCVRES_CHANGE |

#### 文件

| 命令 | 标签 | 功能 | 实现方式 |
|------|------|------|----------|
| `gh_open_browser` | 在浏览器中打开 | 打开文件 GitHub 页面 | ShellExecuteW(blob URL) |
| `gh_copy_raw_url` | 复制原始文件 URL | 复制 raw URL | 构建raw URL → CopyToClipboard |
| `gh_copy_sha` | 复制 SHA | 复制文件 commit SHA | GetFileInfo → CopyToClipboard(sha) |
| `gh_refresh` | 刷新缓存 | 清除缓存并刷新 | InvalidateCache → VFSCVRES_CHANGE |

---

## 十三、完整路径映射表

### 根级路径

```
github://                          → 根目录（显示 explore, mine, go 三个目录）
github://explore/                  → 探索目录（repos, code, users, trending, topics）
github://mine/                     → 我的目录（repos, starred, watching, notifications, gists）
github://go/                       → 直达目录（显示"输入用户名或组织名.txt"提示）
```

### 探索路径

```
github://explore/repos/            → 仓库搜索入口（显示"输入搜索关键词.txt"）
github://explore/repos/keyword/    → 搜索 "keyword" 的仓库结果
github://explore/repos/keyword/lang:typescript/sort:stars/  → 带过滤条件的搜索
github://explore/code/             → 代码搜索入口
github://explore/code/keyword/     → 搜索 "keyword" 的代码结果
github://explore/users/            → 用户搜索入口
github://explore/users/keyword/    → 搜索 "keyword" 的用户结果
github://explore/trending/         → 趋势仓库
github://explore/topics/           → 主题浏览（显示"输入主题关键词.txt"）
```

### 我的路径

```
github://mine/repos/               → 我的仓库列表（预加载缓存）
github://mine/repos/owner/         → 指定用户的仓库
github://mine/repos/owner/repo/    → 仓库文件列表
github://mine/repos/owner/repo/path/ → 仓库内文件路径
github://mine/starred/             → 星标仓库
github://mine/watching/            → 关注仓库
github://mine/notifications/       → 通知（开发中，显示"通知功能开发中.txt"）
github://mine/gists/               → Gists（开发中，显示"Gist功能开发中.txt"）
```

### 直达路径

```
github://go/owner/                 → 用户的仓库列表（先搜用户仓库，再搜组织仓库）
github://go/owner/repo/            → 仓库文件列表（默认分支）
github://go/owner/repo/@branch/    → 指定分支的文件列表
github://go/owner/repo/@branch/path/ → 指定分支的文件路径
github://go/owner/repo/.gh/        → 元数据目录根
github://go/owner/repo/path/       → 仓库内文件路径（默认分支）
```

### 元数据路径

```
github://go/owner/repo/.gh/issues/           → Issue 列表（默认 open）
github://go/owner/repo/.gh/issues/open/      → 打开的 Issues
github://go/owner/repo/.gh/issues/closed/    → 关闭的 Issues
github://go/owner/repo/.gh/issues/123        → Issue #123 详情
github://go/owner/repo/.gh/pulls/            → PR 列表
github://go/owner/repo/.gh/pulls/open/       → 打开的 PRs
github://go/owner/repo/.gh/pulls/closed/     → 关闭的 PRs
github://go/owner/repo/.gh/pulls/456         → PR #456 详情
github://go/owner/repo/.gh/branches/         → 分支列表
github://go/owner/repo/.gh/branches/main/    → main 分支文件（传送门）
github://go/owner/repo/.gh/branches/main/path/ → main 分支内路径
github://go/owner/repo/.gh/tags/             → 标签列表
github://go/owner/repo/.gh/tags/v1.0/        → v1.0 标签文件（传送门）
github://go/owner/repo/.gh/at/               → 按 commit 查看入口
github://go/owner/repo/.gh/at/abc1234/       → 指定 commit 的文件
github://go/owner/repo/.gh/releases/         → 发布版本列表
github://go/owner/repo/.gh/compare/main..dev → 比较 main 和 dev
github://go/owner/repo/.gh/starred.signal    → 星标信号
github://go/owner/repo/.gh/watching.signal   → 关注信号
github://go/owner/repo/.gh/pinned.signal     → 固定信号
github://go/owner/repo/.gh/info.txt          → 仓库信息
github://go/owner/repo/.gh/topics.txt        → 主题标签
github://go/owner/repo/.gh/homepage.txt      → 主页 URL
github://go/owner/repo/.gh/download.zip      → 下载 ZIP
github://go/owner/repo/.gh/download.tar.gz   → 下载 TAR.GZ
github://go/owner/repo/.gh/clone-url.txt     → 克隆 URL
```

### 别名路径

```
github://~/                        → github://mine/
github://~/repos/                  → github://mine/repos/
github://~/starred/                → github://mine/starred/
github://~username/                → github://go/username/
github://~username/repo/           → github://go/username/repo/
github://@username/                → github://go/username/
github://@username/repo/           → github://go/username/repo/
github://s/keyword/                → github://explore/repos/keyword/
github://sc/keyword/               → github://explore/code/keyword/
github://su/keyword/               → github://explore/users/keyword/
github://t/                        → github://explore/trending/
github://t/d/                      → github://explore/trending/daily/
github://t/w/                      → github://explore/trending/weekly/
github://t/m/                      → github://explore/trending/monthly/
```

### 旧路径（兼容模式）

```
github://search/                   → github://explore/repos/
github://search/keyword/           → github://explore/repos/keyword/
github://search/keyword/owner/     → 搜索后进入 owner
github://search/keyword/owner/repo/ → 搜索后进入 repo
github://my-repos/                 → github://mine/repos/
github://my-repos/owner/           → github://mine/repos/owner/
github://my-repos/owner/repo/      → github://mine/repos/owner/repo/
github://starred/                  → github://mine/starred/
```

### 宽松模式路径

```
github://owner/                    → 用户/组织的仓库列表（非保留字时）
github://owner/repo/               → 仓库文件列表
github://owner/repo/path/          → 仓库内文件路径
```

---

## 十四、功能优先级排序——详细分类

### 14.1 分类汇总方法

将所有讨论过的功能按以下维度分类：
- **必要性**：P0 必须实现 → P1 应该实现 → P2 可以实现 → P3 未来扩展
- **类别**：核心路径 | 搜索功能 | 仓库操作 | 文件操作 | 元数据 | 配置 | 交互创新

### 14.2 P0 - 必须实现（核心功能，没有这些插件不可用）

| 功能 | 类别 | 说明 |
|------|------|------|
| `github://` 协议识别 | 核心路径 | VFS 插件注册和前缀识别 |
| 基本路径解析 | 核心路径 | 路径分段、保留字识别 |
| 三大虚拟目录 | 核心路径 | `explore/`, `mine/`, `go/` |
| 仓库搜索 | 搜索功能 | `explore/repos/keyword/` |
| 代码搜索 | 搜索功能 | `explore/code/keyword/` |
| 用户搜索 | 搜索功能 | `explore/users/keyword/` |
| 仓库浏览 | 仓库操作 | 仓库列表、文件目录 |
| 文件读取 | 文件操作 | 下载文件内容 |
| Token 认证 | 配置 | Personal Access Token |
| 配置对话框 | 配置 | 四标签页配置界面 |
| 缓存机制 | 配置 | 目录/仓库/搜索结果缓存 |
| 错误即文件 | 交互创新 | 错误以虚拟文件展示 |
| WinHTTP 会话管理 | 配置 | HTTP 连接、超时、SSL |

### 14.3 P1 - 应该实现（重要功能，没有这些体验不完整）

| 功能 | 类别 | 说明 |
|------|------|------|
| 元数据目录 `.gh/` | 元数据 | 仓库级 GitHub 操作入口 |
| 信号文件 | 交互创新 | 星标/关注切换 |
| 镜像文件 | 交互创新 | 仓库信息读写 |
| 动作文件 | 交互创新 | 下载压缩包、复制克隆 URL |
| Issue 列表 | 元数据 | Issues 浏览和详情 |
| PR 列表 | 元数据 | Pull Requests 浏览和详情 |
| 分支切换 | 元数据 | `@branch` 和 `.gh/branches/` |
| 标签切换 | 元数据 | `.gh/tags/` |
| 文件上传 | 文件操作 | 写操作支持 |
| 文件删除 | 文件操作 | 删除文件 |
| 文件重命名/移动 | 文件操作 | MoveGitHubFile |
| 别名系统 | 核心路径 | `~`, `s/`, `t` 等快捷路径 |
| 自定义列 | 配置 | 仓库/文件详细信息列 |
| 上下文菜单命令 | 交互创新 | 脚本按钮可调用的命令 |
| 仓库属性对话框 | 交互创新 | 右键属性查看仓库详情 |

### 14.4 P2 - 可以实现（增强功能，锦上添花）

| 功能 | 类别 | 说明 |
|------|------|------|
| OAuth 设备流 | 配置 | 一键 OAuth 认证 |
| 旧路径兼容 | 核心路径 | `search/`, `my-repos/` 等旧格式 |
| 宽松路径模式 | 核心路径 | 直接输入 `owner/repo` |
| 代理支持 | 配置 | HTTP/SOCKS5 代理 |
| 大文件警告 | 配置 | 超阈值文件操作警告 |
| 自动刷新 | 配置 | 定时刷新目录内容 |
| 下载归档 | 交互创新 | ZIP/TAR.GZ 下载 |
| Release 列表 | 元数据 | 发布版本浏览 |
| 按 commit 查看 | 元数据 | `.gh/at/sha/` |
| 分支比较 | 元数据 | `.gh/compare/from..to` |
| 搜索过滤条件 | 搜索功能 | `language:xxx/sort:xxx` |

### 14.5 P3 - 未来扩展（创新功能，远期规划）

| 功能 | 类别 | 说明 |
|------|------|------|
| 通知中心 | 元数据 | `mine/notifications/` |
| Gist 支持 | 元数据 | `mine/gists/` |
| 趋势分析 | 搜索功能 | 按时间范围的趋势仓库 |
| 主题浏览 | 搜索功能 | 按主题发现仓库 |
| 贡献者列表 | 元数据 | `.gh/contributors/` |
| CI/CD 状态 | 元数据 | `.gh/workflows/` |
| Wiki 浏览 | 元数据 | `.gh/wiki/` |
| 安全告警 | 元数据 | `.gh/security/` |
| 协作者管理 | 元数据 | `.gh/collaborators/` |
| 标签管理 | 元数据 | `.gh/labels/` |
| 里程碑 | 元数据 | `.gh/milestones/` |
| 管道目录 | 交互创新 | 文件放入即触发处理 |
| 实时 Git 集成 | 配置 | 配置窗口中集成 Git 操作 |

---

## 十五、实时 Git 功能集成方案

### 15.1 用户需求

用户提出：**"从 p0 开始实现，实时 git，全部集成到配置窗口中"**

### 15.2 集成方案

在配置对话框中新增"Git"标签页，或在现有标签页中增加 Git 相关控件：

| 功能 | 控件类型 | 说明 |
|------|----------|------|
| Git 状态概览 | 静态文本 | 显示当前仓库的分支、未提交更改数 |
| 快速 Clone | 按钮+路径框 | 一键克隆仓库到本地指定路径 |
| Pull | 按钮 | 从配置窗口直接拉取 |
| Push | 按钮 | 从配置窗口直接推送 |
| 分支管理 | 下拉框+按钮 | 创建/切换/删除分支 |
| 提交历史 | 列表框 | 查看最近提交记录 |
| Stash 管理 | 按钮 | 暂存/恢复更改 |

### 15.3 技术方案

- 调用系统 `git.exe` 命令行工具
- 通过管道捕获输出并解析
- 异步执行，不阻塞 UI
- 错误处理：git 命令失败时在 UI 中显示错误信息

### 15.4 需要确认的问题

1. Git 功能是集成到配置对话框还是独立的脚本按钮？
2. 是否需要本地仓库路径配置？
3. Clone 目标路径如何确定？
4. 是否需要 Git 凭证管理？

---

## 十六、创新设计讨论记录

### 16.1 用户追问"还有什么创新设计"时的讨论

#### 搜索即目录的深化

搜索不仅仅是输入关键词，还可以：
- **搜索历史**：`explore/history/` 显示最近搜索
- **搜索建议**：输入时自动补全（受 VFS 限制，可能无法实现）
- **搜索模板**：预设常用搜索组合

#### 路径即状态

路径不仅表示位置，还表示状态：
- `github://go/owner/repo/@main/` — 当前查看 main 分支
- `github://go/owner/repo/.gh/issues/open/` — 当前查看打开的 Issues
- 切换路径 = 切换状态

#### 虚拟文件系统即 API 浏览器

VFS 可以作为 GitHub API 的可视化浏览器：
- 每个虚拟文件/目录对应一个 API 端点
- 路径结构反映 API 资源层级
- 文件内容反映 API 响应

#### 时间旅行

通过 commit SHA 查看任意历史版本：
- `github://go/owner/repo/.gh/at/abc1234/` — 查看 commit abc1234 时的文件
- 可以对比不同时间点的文件差异

#### 仓库健康度可视化

通过虚拟文件展示仓库健康指标：
- 活跃度文件：最近 commit 频率
- 贡献者文件：贡献者统计
- Issue 响应时间文件：平均响应时间

### 16.2 用户追问"还有什么新奇想法"时的讨论

#### 路径组合操作

通过路径组合实现复杂操作：
- `github://go/owner/repo/.gh/compare/v1.0..v2.0/` — 比较两个版本
- `github://go/owner/repo/.gh/branches/feature-1/` — 查看特性分支

#### 仓库间导航

从一个仓库跳转到相关仓库：
- Fork 网络：查看 Fork 关系
- 依赖关系：查看依赖的仓库
- 相似仓库：推荐类似项目

#### 批量操作

通过目录选择实现批量操作：
- 选中多个 Issue 文件 → 批量关闭
- 选中多个仓库目录 → 批量星标

---

## 十七、关键设计决策记录

| # | 决策点 | 备选方案 | 最终选择 | 选择理由 |
|---|--------|----------|----------|----------|
| 1 | 路径范式 | 五种范式选一 | 混合式 | 不同场景用不同范式最灵活 |
| 2 | 路径歧义解决 | 保留字/前缀/配置 | `go/` 前缀 + 配置切换 | 用户评价"go挺好的"，简洁明确 |
| 3 | 右键菜单替代 | 自定义菜单/路径交互 | 纯路径交互 | VFS 右键菜单完全不可用 |
| 4 | 元数据目录形式 | 并行目录/独立前缀/隐藏目录 | 隐藏目录 `.gh/` | 不干扰正常浏览，类似 `.git` 约定 |
| 5 | 元数据前缀 | 固定 `.gh`/可配置 | 可配置（默认 `.gh`） | 适应不同用户习惯 |
| 6 | 引用关键字 | 固定 `@`/可配置 | 可配置（默认 `@`） | 避免与路径段冲突 |
| 7 | 错误处理 | 弹窗/文件/状态栏 | 错误即文件 | 不打断操作流，可复制 |
| 8 | 认证方式 | Token/OAuth/Basic | Token 为主，OAuth 辅助 | Token 最简单，OAuth 更安全 |
| 9 | 缓存策略 | 无/定时/手动 | 定时 + 手动刷新 | 平衡实时性和性能 |
| 10 | 配置界面 | 单页/向导/标签页 | 标签页（参考 rclone） | 信息量大时组织清晰 |
| 11 | 信号文件大小 | 零字节/有内容 | 零字节 | 状态完全由存在性决定 |
| 12 | 旧路径兼容 | 不兼容/自动跳转/透明兼容 | 透明兼容（可配置） | 向后兼容，可关闭 |
| 13 | 别名冲突 | 别名优先/用户名优先 | 别名优先（可用 go/ 访问） | 别名使用频率更高 |
| 14 | 错误文件可删除 | 可删除/不可删除 | 不可删除（只读） | 错误应通过修正路径消除 |
| 15 | 搜索关键词特殊字符 | 原样/URL编码 | URL 编码 | VFS 路径中特殊字符需编码 |

---

## 十八、待确认事项

以下问题在设计讨论中提出但尚未最终确认：

1. **Git 功能集成方式**：集成到配置对话框还是独立脚本按钮？
2. **Clone 目标路径**：如何确定本地克隆路径？
3. **Git 凭证管理**：是否需要独立的凭证管理？
4. **搜索翻页**：是否需要实现翻页机制？
5. **管道目录实现**：如何监听文件写入事件？
6. **info.txt 格式**：纯文本键值对还是 JSON？→ **已确认：键值对格式**（见十九.2）
7. **批量操作**：如何通过 VFS 实现多选操作？
8. **仓库间导航**：Fork 网络和依赖关系的展示方式？
9. **搜索历史**：是否持久化搜索历史？
10. **仓库健康度**：健康指标的来源和计算方式？

---

## 十九、P0 迭代需求分析：动作文件 + 操作路径目录

> 日期: 2026-05-10
> 状态: 需求分析完成，待方案设计

### 19.1 背景与问题

当前 `.gh/` 元数据目录下的虚拟文件（信号文件、镜像文件、动作文件）**全部不可操作**。用户在目录中能看到 `starred.signal`、`info.txt`、`download.zip` 等文件，但双击打开会失败——`VFS_CreateFileW` 把它们当作普通 GitHub 仓库文件去下载，必然 404。

**根本原因**：`VFS_CreateFileW` 的逻辑是 `ParseGitHubPath → DownloadFile`，但虚拟文件不存在于 GitHub 仓库中，它们的内容需要由插件动态生成。

**关键约束发现**：VFS 插件系统中，双击打开、编辑、复制等文件操作的自定义行为不可靠。因此交互模型需要调整。

### 19.2 核心决策记录

以下决策在需求分析阶段经讨论确认：

| # | 决策项 | 结论 | 理由 |
|---|--------|------|------|
| 1 | 交互模型 | **混合模式**：自定义列展示信息 + 路径导航触发操作 | VFS 文件操作不可靠，路径导航是最可靠的触发方式 |
| 2 | 信息展示方式 | **自定义列**（columns） | 不依赖打开文件内容，列数据由 DLL 直接填充 |
| 3 | 操作路径格式 | `.gh/{操作名}/` | 与现有 .gh/ 子目录风格一致，简洁 |
| 4 | 操作结果展示 | 目录中显示结果文件 | 用户能看到操作成功/失败，符合 VFS 交互模式 |
| 5 | 下载机制 | `.gh/download-zip/` 内显示虚拟 ZIP 文件，复制到本地即下载 | 最自然的"文件管理器下载"体验 |
| 6 | 动作文件定位 | 复制下载 + 路径触发双模式 | 两种入口都支持，覆盖面广 |
| 7 | 信号文件可见性 | **条件显示**：已星标时显示 starred.signal，未星标时显示占位提示文件 | 符合"文件即状态"理念，占位提示保证可发现性 |
| 8 | 信号文件切换机制 | 双击打开=切换 + 删除/创建=切换，两种都支持 | 覆盖面广 |
| 9 | 镜像文件格式 | 键值对格式（类似 .ini） | 简单直观，普通用户友好 |
| 10 | 镜像文件只读字段 | 显示全部 + 标注 [只读] | 信息完整，用户知道哪些能改 |

### 19.3 本迭代范围

#### 包含 ✅

| # | 功能 | 说明 |
|---|------|------|
| 1 | **操作路径目录** | star/ unstar/ watch/ unwatch/ download-zip/ download-tar/ clone-url/ |
| 2 | **动作文件交互** | download.zip / download.tar.gz / clone-url.txt 的读取和复制 |
| 3 | **自定义列增强** | 动作文件在列中显示提示信息（可下载、大小等） |

#### 不包含 ❌（后续迭代）

| 功能 | 原因 | 计划迭代 |
|------|------|----------|
| 信号文件（starred.signal 等） | 依赖操作路径目录模式先建立 | P1 |
| 镜像文件（info.txt 等） | 依赖自定义列脚本先实现 | P1 |
| Issue/PR 编辑 | 独立功能 | P2 |
| 代码搜索/用户搜索展示 | 独立功能 | P1 |

### 19.4 功能需求详细规格

#### 19.4.1 操作路径目录

**星标操作路径**

| 路径 | 触发操作 | 目录内容 |
|------|----------|----------|
| `.gh/star/` | 调用 `StarRepo(owner, repo)` | 显示 `✓ 已星标.txt` |
| `.gh/unstar/` | 调用 `UnstarRepo(owner, repo)` | 显示 `✓ 已取消星标.txt` |

**关注操作路径**

| 路径 | 触发操作 | 目录内容 |
|------|----------|----------|
| `.gh/watch/` | 调用 `WatchRepo(owner, repo)` | 显示 `✓ 已关注.txt` |
| `.gh/unwatch/` | 调用 `UnwatchRepo(owner, repo)` | 显示 `✓ 已取消关注.txt` |

**下载操作路径**

| 路径 | 目录内容 | 复制行为 |
|------|----------|----------|
| `.gh/download-zip/` | 显示 `{owner}-{repo}.zip` 虚拟文件 | 复制到本地 = 下载 ZIP |
| `.gh/download-tar/` | 显示 `{owner}-{repo}.tar.gz` 虚拟文件 | 复制到本地 = 下载 TAR.GZ |

**克隆 URL 路径**

| 路径 | 目录内容 |
|------|----------|
| `.gh/clone-url/` | 显示 `clone-url.txt`，内容为 git clone URL |

**操作路径在 .gh/ 根目录的展示**

更新后的 `.gh/` 根目录完整结构：

```
owner/repo/.gh/
├── issues/              # 已有：Issue 列表目录
├── pulls/               # 已有：PR 列表目录
├── branches/            # 已有：分支列表目录
├── tags/                # 已有：标签列表目录
├── releases/            # 已有：发布版本列表
├── star/                # 新增：星标操作目录
├── unstar/              # 新增：取消星标操作目录
├── watch/               # 新增：关注操作目录
├── unwatch/             # 新增：取消关注操作目录
├── download-zip/        # 新增：下载 ZIP 目录
├── download-tar/        # 新增：下载 TAR.GZ 目录
├── clone-url/           # 新增：克隆 URL 目录
├── starred.signal       # 已有（暂不改造，P1 处理）
├── watching.signal      # 已有（暂不改造，P1 处理）
├── info.txt             # 已有（暂不改造，P1 处理）
├── download.zip         # 已有（本迭代改造为可读取）
├── clone-url.txt        # 已有（本迭代改造为可读取）
```

#### 19.4.2 动作文件交互

**download.zip / download.tar.gz**

| 操作 | 当前行为 | 改造后行为 |
|------|----------|------------|
| 在 .gh/ 目录中看到 | 显示为普通文件 | 自定义列显示"📦 可下载 ZIP / 预估大小" |
| 复制到本地 | 失败（404） | 调用 `DownloadArchive` 返回实际 ZIP/TAR.GZ 数据 |
| 双击打开 | 失败 | 返回实际数据（如果 VFS 支持） |

**clone-url.txt**

| 操作 | 当前行为 | 改造后行为 |
|------|----------|------------|
| 在 .gh/ 目录中看到 | 显示为普通文件 | 自定义列显示 git clone URL |
| 打开/复制内容 | 失败（404） | 返回 `git clone https://github.com/owner/repo.git` 文本 |

#### 19.4.3 路径解析扩展

`ParseGitHubPath` 需要识别新的 `metaType`：

| 新 metaType | 路径示例 | 含义 |
|-------------|----------|------|
| `GITHUB_META_ACTION_STAR` | `.gh/star/` | 星标操作目录 |
| `GITHUB_META_ACTION_UNSTAR` | `.gh/unstar/` | 取消星标操作目录 |
| `GITHUB_META_ACTION_WATCH` | `.gh/watch/` | 关注操作目录 |
| `GITHUB_META_ACTION_UNWATCH` | `.gh/unwatch/` | 取消关注操作目录 |
| `GITHUB_META_ACTION_DOWNLOAD_ZIP` | `.gh/download-zip/` | 下载 ZIP 目录 |
| `GITHUB_META_ACTION_DOWNLOAD_TAR` | `.gh/download-tar/` | 下载 TAR.GZ 目录 |
| `GITHUB_META_ACTION_CLONE_URL` | `.gh/clone-url/` | 克隆 URL 目录 |

### 19.5 验收标准

| # | 验收条件 | 给定 | 当 | 那么 |
|---|----------|------|-----|------|
| 1 | 星标操作 | 用户导航到 `github://go/owner/repo/.gh/star/` | 目录加载完成 | 调用了 `StarRepo` API，目录显示 `✓ 已星标.txt` |
| 2 | 取消星标 | 用户导航到 `github://go/owner/repo/.gh/unstar/` | 目录加载完成 | 调用了 `UnstarRepo` API，目录显示 `✓ 已取消星标.txt` |
| 3 | 关注操作 | 用户导航到 `github://go/owner/repo/.gh/watch/` | 目录加载完成 | 调用了 `WatchRepo` API，目录显示 `✓ 已关注.txt` |
| 4 | 取消关注 | 用户导航到 `github://go/owner/repo/.gh/unwatch/` | 目录加载完成 | 调用了 `UnwatchRepo` API，目录显示 `✓ 已取消关注.txt` |
| 5 | 下载 ZIP（路径触发） | 用户导航到 `github://go/owner/repo/.gh/download-zip/` | 目录加载完成 | 目录显示 `owner-repo.zip` 虚拟文件 |
| 6 | 下载 ZIP（复制） | 用户将 `.gh/download-zip/owner-repo.zip` 复制到本地 | 复制完成 | 本地文件为有效的 ZIP 压缩包 |
| 7 | 下载 TAR.GZ | 同上但为 tar.gz 格式 | 复制完成 | 本地文件为有效的 TAR.GZ 压缩包 |
| 8 | 克隆 URL（路径触发） | 用户导航到 `.gh/clone-url/` | 目录加载完成 | 目录显示 `clone-url.txt` |
| 9 | 克隆 URL（读取） | 用户打开 `.gh/clone-url/clone-url.txt` | 文件打开 | 内容为 `git clone https://github.com/owner/repo.git` |
| 10 | 动作文件直接读取 | 用户打开 `.gh/download.zip` | 文件打开 | 返回有效的 ZIP 数据 |
| 11 | 动作文件直接读取 | 用户打开 `.gh/clone-url.txt` | 文件打开 | 返回 git clone URL 文本 |
| 12 | .gh/ 根目录 | 用户导航到 `.gh/` | 目录加载完成 | 显示原有条目 + 新增操作目录（star/ unstar/ watch/ unwatch/ download-zip/ download-tar/ clone-url/） |
| 13 | API 失败处理 | `StarRepo` 等操作 API 返回失败 | 目录加载完成 | 显示 `✗ 操作失败.txt`，内容包含错误信息 |

### 19.6 约束条件

| 约束 | 说明 |
|------|------|
| VFS 文件操作限制 | 双击/编辑/复制等操作自定义可能无效，优先依赖路径导航 |
| API 速率限制 | 认证用户 5000次/小时，操作路径每次导航消耗 1 次 |
| 操作幂等性 | 重复进入 `.gh/star/` 不应报错（GitHub API 本身幂等） |
| 线程安全 | `ReadDirectory` 在后台线程调用，API 调用需线程安全 |

### 19.7 后续迭代规划

| 迭代 | 内容 | 依赖 |
|------|------|------|
| P1-a | 信号文件交互（条件显示 + 占位提示 + 路径导航切换） | 本迭代操作路径目录模式 |
| P1-b | 镜像文件交互（自定义列展示 + 路径导航更新） | 自定义列脚本 |
| P1-c | 自定义列脚本 GitHubVFS.js（22 列） | 无 |
| P2 | 代码搜索/用户搜索展示 + Issue/PR 详情查看编辑 | 无 |
| P3 | 通知/Gist/Trending/Topics | 无 |
| P4 | Compare + 扩展元数据 | 无 |
