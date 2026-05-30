# GitHubVFS 路径重构设计：虚拟路径参数化

## 一、背景与问题

### 1.1 当前问题

当前 GitHubVFS 的仓库根目录中，**虚拟入口和真实文件混合显示**：

```
github:///owner/repo/
├── Issues/          ← 虚拟入口（API查询）
├── Pulls/           ← 虚拟入口（API查询）
├── Branches/        ← 虚拟入口（API查询）
├── Tags/            ← 虚拟入口（API查询）
├── Releases/        ← 虚拟入口（API查询）
├── src/             ← 真实文件
├── docs/            ← 真实文件
└── README.md        ← 真实文件
```

**核心矛盾**：

1. **语义错位**：Issues/Pulls/Branches 等是"视图查询"，不是"目录内容"，混在文件列表里语义不对
2. **文件名冲突**：如果仓库恰好有 `Issues/` 文件夹，会被 `IsRepoSubdir()` 过滤掉，用户看不到真实文件
3. **路径层级混乱**：`@{ref}` 分支切换写在路径里，`page:N` 分页也写在路径里，但这些都不是路径层级
4. **owner 冗余**：`owner/repo/` 两级路径中，owner 几乎总是可从缓存自动解析
5. **代码膨胀**：21 个上下文枚举、17 个 Read 函数，大量重复逻辑

### 1.2 设计原则

**路径 = 位置，参数 = 查询/视图/动作**

- 路径只表示"我在哪"（真实文件位置或导航分类）
- 参数表示"我看什么"（视图、分支、搜索、分页、owner消歧等）
- **不做向后兼容**，旧路径格式直接废弃

## 二、改造范围

### 2.1 改造层级划分

| 层级 | 性质 | 处理方式 |
|------|------|---------|
| 根目录 `github:///` | 纯导航枢纽，无真实文件 | **保留目录显示**，不变 |
| 仓库根目录 `repo/` | 真实内容 + 视图查询 | **目录只显示真实文件**，视图用参数+右键菜单 |
| 仓库子目录 `repo/src/` | 纯真实内容 | 只显示真实文件，右键菜单提供视图入口 |

### 2.2 不变的部分

根目录保留目录导航，因为根目录没有真实文件，目录是最佳导航方式：

```
github:///
├── Repos/           ← 保留
├── Starred/         ← 保留
├── Subscriptions/   ← 保留
├── Notifications/   ← 保留
├── Gists/           ← 保留
├── Trending/        ← 保留
└── Search/          ← 保留
```

### 2.3 owner 从路径元素降级为查询参数

**核心改动**：`github:///owner/repo/` → `github:///repo/`

owner 在几乎所有导航场景下都可以从缓存自动解析，不需要用户手动指定：

| 导航入口 | owner 是否已知 | 示例 |
|---------|--------------|------|
| `Repos/` → 点击仓库 | ✅ 已知（就是自己） | `common/` → owner 是当前用户 |
| `Starred/` → 点击仓库 | ✅ 已知（缓存中有） | `vscode/` → 缓存知道是 `microsoft/vscode` |
| `Subscriptions/` → 点击仓库 | ✅ 已知 | 同上 |
| `Search/` → 点击结果 | ✅ 已知（搜索结果带 owner） | 同上 |

仅在同名仓库消歧时需要 `?owner=xxx`：

```
github:///vscode/                    → 自动从缓存解析 owner（默认取当前用户的）
github:///vscode/?owner=microsoft    → 手动指定 owner（消歧）
```

消歧规则：
- 缓存中只有一个匹配 → 直接使用
- 缓存中有多个匹配 → 优先取当前用户的仓库，其次取最近更新的
- `?owner=xxx` 显式指定 → 使用指定的 owner

## 三、改造详情

### 3.1 仓库根目录：虚拟入口 → 参数查询 + 右键菜单

#### 改造前

```
github:///owner/repo/
├── Issues/          ← 虚拟入口
├── Pulls/           ← 虚拟入口
├── Branches/        ← 虚拟入口
├── Tags/            ← 虚拟入口
├── Releases/        ← 虚拟入口
├── src/             ← 真实文件
└── README.md        ← 真实文件
```

#### 改造后

```
github:///repo/
├── src/             ← 只显示真实文件
├── docs/
└── README.md

github:///repo/?view=issues    → Issue列表
github:///repo/?view=pulls     → PR列表
github:///repo/?view=branches  → 分支列表
github:///repo/?view=tags      → 标签列表
github:///repo/?view=releases  → 发布列表
```

#### 右键菜单

仓库目录（含根目录和子目录）的右键菜单增加视图入口：

```
┌─ 在浏览器中打开
├─ 复制克隆 URL
├─ ─────────────
├─ ▶ Issues          → 导航到 ?view=issues
├─ ▶ Pull Requests   → 导航到 ?view=pulls
├─ ▶ Branches        → 导航到 ?view=branches
├─ ▶ Tags            → 导航到 ?view=tags
├─ ▶ Releases        → 导航到 ?view=releases
├─ ─────────────
├─ 刷新
└─ 属性
```

**关键改进**：任何仓库路径下（包括子目录）的右键菜单都提供视图入口，用户不需要返回根目录。

### 3.2 `@{ref}` 分支切换 → `?ref=` 参数

#### 改造前

```
github:///owner/repo/@main/src/          → main分支
github:///owner/repo/@develop/src/       → develop分支
github:///owner/repo/@v1.0/src/          → v1.0标签
```

#### 改造后

```
github:///repo/src/                      → 默认分支
github:///repo/src/?ref=develop          → develop分支
github:///repo/src/?ref=v1.0             → v1.0标签
github:///repo/src/?ref=abc1234          → 指定commit SHA
```

#### 右键菜单

```
├─ 🔄 切换分支...     → 弹出分支列表，选择后导航到 ?ref=xxx
├─ 🏷️ 切换标签...     → 弹出标签列表，选择后导航到 ?ref=xxx
```

#### 分支/标签列表目录

`?view=branches` 和 `?view=tags` 返回的列表中，每个条目点击后自动导航到对应 `?ref=xxx`：

```
?view=branches 返回：
├── main       → 点击导航到 ?ref=main
├── develop    → 点击导航到 ?ref=develop
└── feature/x  → 点击导航到 ?ref=feature/x
```

### 3.3 `page:N` 分页 → `?page=N` 参数

#### 改造前

```
github:///Search/Repos/myquery/page:2
github:///owner/repo/Issues/page:2
```

#### 改造后

```
github:///?search=repos&q=myquery&page=2
github:///repo/?view=issues&page=2
```

### 3.4 搜索 → 右键菜单快捷入口

根目录的 `Search/` 目录保留作为搜索中心，但右键菜单增加快捷搜索入口：

#### 根目录右键菜单

```
┌─ 🔍 搜索仓库...     → 弹出输入框 → ?search=repos&q=xxx
├─ 🔍 搜索代码...     → 弹出输入框 → ?search=code&q=xxx
├─ 🔍 搜索用户...     → 弹出输入框 → ?search=users&q=xxx
├─ 🔍 搜索Issues...   → 弹出输入框 → ?search=issues&q=xxx
├─ 🔍 搜索Commits...  → 弹出输入框 → ?search=commits&q=xxx
├─ ─────────────
├─ 刷新
└─ 配置
```

### 3.5 参数组合

所有参数可以自由组合：

```
github:///repo/?view=issues&page=2              → Issues第2页
github:///repo/src/?ref=develop                  → develop分支的src目录
github:///repo/src/?ref=develop&page=2           → （如需分页）
github:///?view=repos&owner=microsoft            → 过滤owner
github:///?view=trending&since=daily&lang=python → 趋势筛选
github:///?search=repos&q=react&page=2           → 搜索仓库第2页
```

## 四、统一后的路径语义

| 语法 | 含义 | 类型 |
|------|------|------|
| `github:///` | 根目录导航 | 位置 |
| `github:///Repos/` | 仓库列表 | 位置 |
| `github:///repo/` | 仓库根目录（真实文件） | 位置 |
| `github:///repo/src/` | 仓库子目录（真实文件） | 位置 |
| `?view=issues` | 查看Issues | 视图查询 |
| `?view=pulls` | 查看PR | 视图查询 |
| `?view=branches` | 查看分支 | 视图查询 |
| `?view=tags` | 查看标签 | 视图查询 |
| `?view=releases` | 查看发布 | 视图查询 |
| `?ref=develop` | 切换分支/标签/SHA | 视图查询 |
| `?owner=microsoft` | 指定仓库owner（消歧） | 消歧参数 |
| `?search=repos&q=xxx` | 搜索仓库 | 动作查询 |
| `?search=code&q=xxx` | 搜索代码 | 动作查询 |
| `?page=2` | 翻页 | 分页参数 |
| `&since=daily` | 趋势时间范围 | 过滤参数 |
| `&lang=python` | 趋势语言 | 过滤参数 |

## 五、枚举精简

### 5.1 GitHubContext 枚举：21 → 8

```
改造前（21个）：                    改造后（8个）：
GITHUB_CTX_ROOT                    GITHUB_CTX_ROOT
GITHUB_CTX_REPOS                   GITHUB_CTX_REPOS
GITHUB_CTX_REPOS_OWNER             ❌ 删除（owner过滤用参数）
GITHUB_CTX_STARRED                 GITHUB_CTX_STARRED
GITHUB_CTX_STARRED_OWNER           ❌ 删除
GITHUB_CTX_SUBSCRIPTIONS           GITHUB_CTX_SUBSCRIPTIONS
GITHUB_CTX_SUBSCRIPTIONS_OWNER     ❌ 删除
GITHUB_CTX_NOTIFICATIONS           GITHUB_CTX_NOTIFICATIONS
GITHUB_CTX_GISTS                   GITHUB_CTX_GISTS
GITHUB_CTX_TRENDING                GITHUB_CTX_TRENDING
GITHUB_CTX_SEARCH_CENTER           ❌ 删除（搜索入口移到右键菜单）
GITHUB_CTX_SEARCH_REPOS            GITHUB_CTX_SEARCH
GITHUB_CTX_SEARCH_CODE             ❌ 合并（用 ?search=code 区分）
GITHUB_CTX_SEARCH_USERS            ❌ 合并
GITHUB_CTX_SEARCH_ISSUES           ❌ 合并
GITHUB_CTX_SEARCH_COMMITS          ❌ 合并
GITHUB_CTX_SEARCH_TOPICS           ❌ 合并
GITHUB_CTX_SEARCH_OWNER            ❌ 删除
GITHUB_CTX_OWNER                   ❌ 删除（owner降级为参数）
GITHUB_CTX_REPO                    GITHUB_CTX_REPO
GITHUB_CTX_CODE                    ❌ 合并到 REPO（去掉虚拟入口后无区别）
GITHUB_CTX_ISSUES                  ❌ 合并到 VIEW
GITHUB_CTX_PULLS                   ❌ 合并到 VIEW
GITHUB_CTX_BRANCHES                ❌ 合并到 VIEW
GITHUB_CTX_TAGS                    ❌ 合并到 VIEW
GITHUB_CTX_RELEASES                GITHUB_CTX_VIEW
```

### 5.2 精简后的枚举定义

```cpp
enum GitHubContext {
    GITHUB_CTX_ROOT = 0,         // 根目录
    GITHUB_CTX_REPOS,            // Repos/ — 我的仓库列表
    GITHUB_CTX_STARRED,          // Starred/ — 星标仓库列表
    GITHUB_CTX_SUBSCRIPTIONS,    // Subscriptions/ — 关注仓库列表
    GITHUB_CTX_NOTIFICATIONS,    // Notifications/ — 通知
    GITHUB_CTX_GISTS,            // Gists/ — Gist列表
    GITHUB_CTX_TRENDING,         // Trending/ — 趋势
    GITHUB_CTX_SEARCH,           // ?search=xxx — 搜索（类型由参数区分）
    GITHUB_CTX_REPO,             // repo/ — 仓库（含根目录和子目录）
    GITHUB_CTX_VIEW,             // ?view=xxx — 视图（类型由参数区分）
};
```

### 5.3 InternalReadDirectory switch 精简

```cpp
// 改造前：21个case
switch (pathInfo.context) {
    case GITHUB_CTX_ROOT: ...
    case GITHUB_CTX_REPOS: ...
    case GITHUB_CTX_REPOS_OWNER: ...
    case GITHUB_CTX_STARRED: ...
    case GITHUB_CTX_STARRED_OWNER: ...
    case GITHUB_CTX_SUBSCRIPTIONS: ...
    case GITHUB_CTX_SUBSCRIPTIONS_OWNER: ...
    case GITHUB_CTX_NOTIFICATIONS: ...
    case GITHUB_CTX_GISTS: ...
    case GITHUB_CTX_TRENDING: ...
    case GITHUB_CTX_SEARCH_CENTER: ...
    case GITHUB_CTX_SEARCH_REPOS: ...
    case GITHUB_CTX_SEARCH_CODE: ...
    case GITHUB_CTX_SEARCH_USERS: ...
    case GITHUB_CTX_SEARCH_ISSUES: ...
    case GITHUB_CTX_SEARCH_COMMITS: ...
    case GITHUB_CTX_SEARCH_TOPICS: ...
    case GITHUB_CTX_SEARCH_OWNER: ...
    case GITHUB_CTX_OWNER: ...
    case GITHUB_CTX_REPO: ...
    case GITHUB_CTX_CODE: ...
    case GITHUB_CTX_ISSUES: ...
    case GITHUB_CTX_PULLS: ...
    case GITHUB_CTX_BRANCHES: ...
    case GITHUB_CTX_TAGS: ...
    case GITHUB_CTX_RELEASES: ...
}

// 改造后：8个case
switch (pathInfo.context) {
    case GITHUB_CTX_ROOT:          return ReadRootDirectory(lpRDD);
    case GITHUB_CTX_REPOS:         return ReadRepoListDirectory(lpRDD, REPO_SOURCE_OWNED);
    case GITHUB_CTX_STARRED:       return ReadRepoListDirectory(lpRDD, REPO_SOURCE_STARRED);
    case GITHUB_CTX_SUBSCRIPTIONS: return ReadRepoListDirectory(lpRDD, REPO_SOURCE_WATCHED);
    case GITHUB_CTX_NOTIFICATIONS: return ReadNotificationsDirectory(lpRDD);
    case GITHUB_CTX_GISTS:         return ReadGistsDirectory(lpRDD);
    case GITHUB_CTX_TRENDING:      return ReadTrendingDirectory(lpRDD, pathInfo);
    case GITHUB_CTX_SEARCH:        return ReadSearchDirectory(lpRDD, pathInfo);
    case GITHUB_CTX_REPO:          return ReadRepoDirectory(lpRDD, pathInfo);
    case GITHUB_CTX_VIEW:          return ReadViewDirectory(lpRDD, pathInfo);
}
```

## 六、函数合并

### 6.1 仓库列表类：3个函数 → 1个

`Repos/`、`Starred/`、`Subscriptions/` 三个目录做的事情完全一样：**从不同 API 拿仓库列表，渲染成目录项**。

```cpp
// 改造前：3个函数
ReadReposDirectory()
ReadStarredDirectory()
ReadSubscriptionsDirectory()

// 改造后：1个函数
enum RepoSource { REPO_SOURCE_OWNED, REPO_SOURCE_STARRED, REPO_SOURCE_WATCHED };

static int ReadRepoListDirectory(LPVFSREADDIRDATAW lpRDD, RepoSource source) {
    std::vector<GitHubRepoInfo> repos;
    switch (source) {
        case REPO_SOURCE_OWNED:   repos = GetCachedRepos(); break;
        case REPO_SOURCE_STARRED: repos = GetCachedStarred(); break;
        case REPO_SOURCE_WATCHED: repos = GetCachedSubscriptions(); break;
    }
    // ... 统一渲染逻辑 ...
}
```

### 6.2 视图类：5个函数 → 1个

Issues / Pulls / Branches / Tags / Releases 同理：**从不同 API 拿列表，渲染成目录项**。

```cpp
// 改造前：5个函数
ReadIssuesDirectory()
ReadPullsDirectory()
ReadBranchesDirectory()
ReadTagsDirectory()
ReadReleasesDirectory()

// 改造后：1个函数
static int ReadViewDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    const std::wstring& view = pathInfo.query.view;
    if (view == L"issues")   return ReadIssuesView(lpRDD, pathInfo);
    if (view == L"pulls")    return ReadPullsView(lpRDD, pathInfo);
    if (view == L"branches") return ReadBranchesView(lpRDD, pathInfo);
    if (view == L"tags")     return ReadTagsView(lpRDD, pathInfo);
    if (view == L"releases") return ReadReleasesView(lpRDD, pathInfo);
    return AddErrorFile(lpRDD, L"⚠ 未知视图.txt", L"不支持的 view 参数: " + view);
}
```

### 6.3 搜索类：6个函数 → 1个

6 种搜索也是同一个模式：**查询关键词 → 返回结果列表**。

```cpp
// 改造前：6个函数
ReadSearchReposDirectory()
ReadSearchCodeDirectory()
ReadSearchUsersDirectory()
ReadSearchIssuesDirectory()
ReadSearchCommitsDirectory()
ReadSearchTopicsDirectory()

// 改造后：1个函数
static int ReadSearchDirectory(LPVFSREADDIRDATAW lpRDD, const GitHubPathInfo& pathInfo) {
    const std::wstring& type = pathInfo.query.search;
    const std::wstring& q    = pathInfo.query.query;
    if (type == L"repos")   return SearchRepos(lpRDD, q, pathInfo.query.page);
    if (type == L"code")    return SearchCode(lpRDD, q, pathInfo.query.page);
    if (type == L"users")   return SearchUsers(lpRDD, q, pathInfo.query.page);
    if (type == L"issues")  return SearchIssues(lpRDD, q, pathInfo.query.page);
    if (type == L"commits") return SearchCommits(lpRDD, q, pathInfo.query.page);
    if (type == L"topics")  return SearchTopics(lpRDD, q, pathInfo.query.page);
    return AddErrorFile(lpRDD, L"⚠ 未知搜索类型.txt", L"不支持的 search 参数: " + type);
}
```

## 七、结构体精简

### 7.1 GitHubPathInfo：14字段 → 5字段 + Query子结构

```cpp
// 改造前
struct GitHubPathInfo {
    std::wstring owner;         // 从路径解析
    std::wstring repo;
    std::wstring path;
    std::wstring searchQuery;   // 搜索关键词
    std::wstring searchType;    // 搜索类型
    std::wstring ref;           // 分支/标签/SHA
    GitHubRefType refType;      // ref类型
    std::wstring ownerFilter;   // owner过滤
    std::wstring trendingSince; // 趋势时间范围
    std::wstring trendingLang;  // 趋势语言
    std::wstring metaItem;      // 元数据项ID
    GitHubContext context;
    int issueNumber;
    int page;
    bool isDir;
};

// 改造后
struct GitHubPathInfo {
    // 路径字段：从路径解析
    std::wstring repo;          // 仓库名
    std::wstring path;          // 仓库内文件路径
    GitHubContext context;      // 上下文（8种）
    int issueNumber;            // Issue/PR 编号
    bool isDir;

    // 参数字段：从查询字符串解析
    struct Query {
        std::wstring view;      // issues, pulls, branches, tags, releases
        std::wstring ref;       // 分支/标签/SHA
        std::wstring search;    // repos, code, users, issues, commits, topics
        std::wstring query;     // 搜索关键词
        std::wstring owner;     // owner消歧
        std::wstring since;     // daily, weekly, monthly
        std::wstring lang;      // 编程语言
        std::wstring metaItem;  // Gist ID 等
        int page;               // 页码
    } query;
};
```

**关键改进**：路径字段和参数字段泾渭分明，`owner`、`ref`、`page`、`search` 等全部归入 Query 子结构。

### 7.2 可删除的枚举

```cpp
// 删除 GitHubRefType 枚举
// ref 类型不再需要在路径解析阶段确定，运行时根据 API 返回自然区分
enum GitHubRefType {
    GITHUB_REF_DEFAULT = 0,  // ❌ 删除
    GITHUB_REF_BRANCH = 1,   // ❌ 删除
    GITHUB_REF_TAG = 2,      // ❌ 删除
    GITHUB_REF_SHA = 3       // ❌ 删除
};
```

## 八、可删除的代码汇总

### 8.1 可删除的函数

| 函数 | 估计行数 | 删除原因 |
|------|---------|---------|
| `ReadReposOwnerDirectory()` | ~40 | owner 分组层删除 |
| `ReadStarredOwnerDirectory()` | ~40 | 同上 |
| `ReadSubscriptionsOwnerDirectory()` | ~40 | 同上 |
| `ReadOwnerDirectory()` | ~30 | GITHUB_CTX_OWNER 删除 |
| `ReadIssuesDirectory()` | ~60 | 合并到 ReadViewDirectory |
| `ReadPullsDirectory()` | ~60 | 合并到 ReadViewDirectory |
| `ReadBranchesDirectory()` | ~60 | 合并到 ReadViewDirectory |
| `ReadTagsDirectory()` | ~60 | 合并到 ReadViewDirectory |
| `ReadReleasesDirectory()` | ~60 | 合并到 ReadViewDirectory |
| `ReadSearchCenterDirectory()` | ~30 | 搜索中心删除 |
| `ReadSearchOwnerDirectory()` | ~30 | 搜索 owner 删除 |
| `ReadSearchReposDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `ReadSearchCodeDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `ReadSearchUsersDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `ReadSearchIssuesDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `ReadSearchCommitsDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `ReadSearchTopicsDirectory()` | ~50 | 合并到 ReadSearchDirectory |
| `IsRepoSubdir()` | ~15 | 虚拟入口删除，不再需要过滤 |

### 8.2 可删除的逻辑代码

| 代码 | 估计行数 | 删除原因 |
|------|---------|---------|
| `ReadRepoDirectory()` 中5个虚拟入口创建 | ~100 | 虚拟入口移到参数查询 |
| `@{ref}` 路径解析逻辑 | ~80 | 改为 `?ref=` 参数 |
| `page:N` 路径解析逻辑 | ~30 | 改为 `?page=` 参数 |
| `owner/repo` 两级路径解析逻辑 | ~50 | owner 降级为参数 |
| `IsRepoSubdir` 调用点 | ~10 | 不再需要过滤 |
| `GitHubRefType` 枚举及相关逻辑 | ~20 | ref 类型不再预解析 |

### 8.3 删除统计

| 类别 | 行数 |
|------|------|
| 可删除的函数 | ~765 |
| 可删除的逻辑代码 | ~290 |
| **合计可删除** | **~1055** |
| 预计新增代码（3个通用函数+参数解析+右键菜单） | ~350 |
| **净减少** | **~705** |

## 九、URL 参数解析

### 9.1 查询字符串解析

扩展当前的 `?owner=xxx` 为完整的查询字符串解析：

```
?view=issues&ref=develop&page=2&owner=microsoft
```

### 9.2 参数定义

| 参数 | 值域 | 说明 |
|------|------|------|
| `view` | issues, pulls, branches, tags, releases | 视图类型 |
| `ref` | 任意字符串 | 分支名/标签名/commit SHA |
| `search` | repos, code, users, issues, commits, topics | 搜索类型 |
| `query` (或 `q`) | 任意字符串 | 搜索关键词 |
| `owner` | 任意字符串 | owner消歧 |
| `since` | daily, weekly, monthly | 趋势时间范围 |
| `lang` | 任意字符串 | 趋势/搜索语言筛选 |
| `page` | 正整数 | 分页页码 |

### 9.3 解析实现

```cpp
static void ParseQueryString(const std::wstring& qs, GitHubPathInfo::Query& query) {
    // 按 & 分割，每个按 = 分割
    // view=issues → query.view = L"issues"
    // ref=develop → query.ref = L"develop"
    // page=2      → query.page = 2
    // owner=xxx   → query.owner = L"xxx"
    // q=xxx       → query.query = L"xxx"（q 是 query 的简写）
    // search=repos→ query.search = L"repos"
    // since=daily → query.since = L"daily"
    // lang=python → query.lang = L"python"
}
```

## 十、实施计划

### 第一步：结构体与枚举重构

1. 精简 `GitHubContext` 枚举：21 → 8
2. 精简 `GitHubPathInfo` 结构体：新增 Query 子结构
3. 删除 `GitHubRefType` 枚举
4. 编译验证

### 第二步：路径解析重构

1. 重写 `ParseGitHubPath()`：路径只解析 repo/path，参数解析到 Query 子结构
2. 删除 `@{ref}`、`page:N`、`owner/repo` 解析逻辑
3. 新增 `ParseQueryString()` 函数
4. 编译验证

### 第三步：函数合并与删除

1. 合并仓库列表类：3个 → 1个 `ReadRepoListDirectory()`
2. 合并视图类：5个 → 1个 `ReadViewDirectory()`
3. 合并搜索类：6个 → 1个 `ReadSearchDirectory()`
4. 删除所有废弃函数
5. 删除 `IsRepoSubdir()` 及调用
6. 删除仓库根目录5个虚拟入口创建代码
7. 编译验证

### 第四步：右键菜单改造

1. 仓库目录右键菜单增加视图入口（Issues/Pulls/Branches/Tags/Releases）
2. 仓库目录右键菜单增加分支/标签切换
3. 根目录右键菜单增加搜索快捷入口
4. 编译验证

### 第五步：端到端验证

1. 导航到 `github:///Repos/` → 显示仓库列表
2. 点击仓库 → `github:///repo/` → 只显示真实文件
3. 右键 → Issues → `github:///repo/?view=issues` → 显示Issue列表
4. 右键 → 切换分支 → `github:///repo/?ref=develop` → 显示develop分支内容
5. 根目录右键 → 搜索仓库 → 输入关键词 → 显示搜索结果
6. 验证同名仓库消歧：`github:///vscode/` vs `github:///vscode/?owner=microsoft`
