# GitHub VFS 插件 — 完整设计文档

> 版本: 3.1  
> 日期: 2026-05-12  
> 状态: 需求分析完成 / 方案设计完成（v3.1 路径体系重构）  
> 基于: 多轮需求讨论的完整结论  
> 整合: 自定义列方案.md + P0-技术方案-动作文件与操作路径目录.md + 重构设计文档.md

---

## 目录

1. [项目概述](#1-项目概述)
2. [设计原则](#2-设计原则)
3. [架构决策记录](#3-架构决策记录)
4. [当前代码状态](#4-当前代码状态)
5. [路径体系设计](#5-路径体系设计)
6. [自定义列体系设计](#6-自定义列体系设计)
7. [右键菜单体系设计](#7-右键菜单体系设计)
8. [搜索体系设计](#8-搜索体系设计)
9. [交互设计](#9-交互设计)
10. [DLL-脚本通信设计](#10-dll-脚本通信设计)
11. [数据获取架构](#11-数据获取架构)
12. [分页设计](#12-分页设计)
13. [缓存与监控设计](#13-缓存与监控设计)
14. [数据结构定义](#14-数据结构定义)
15. [保留字清单](#15-保留字清单)
16. [一期/二期划分](#16-一期二期划分)
17. [实施计划](#17-实施计划)
18. [ParseGitHubPath 重构方案](#18-parsegithubpath-重构方案)
19. [ReadDirectory 改动方案](#19-readdirectory-改动方案)
20. [完整改动清单与实施顺序](#20-完整改动清单与实施顺序)

---

## 1. 项目概述

### 1.1 目标

为 Directory Opus 开发 `github://` 虚拟文件系统插件，实现：

- 简洁直觉的路径体系
- 上下文感知的自定义列（原始数据 + 创造数据）
- 完整的右键菜单操作
- 6 种 GitHub 搜索类型
- DLL + 脚本混合架构

### 1.2 核心设计理念

| 原则 | 说明 |
|------|------|
| 列显示 + 菜单编辑 | 只读数据用自定义列展示，可写数据用右键菜单编辑后列自动刷新，少用文件打开 |
| 路径即上下文 | 路径自动决定列集、菜单、行为，无需额外判断 |
| 双击 = 最直觉动作 | 双击永远是"进去看看"，不需要思考 |
| 搜索结果 = 指针 | 搜索结果双击跳转到实际路径，不嵌套在搜索路径下 |
| 扁平化 | 去掉不必要的中转层，减少点击次数 |

### 1.3 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                     Directory Opus UI                        │
│              (列显示 / 右键菜单 / 地址栏导航)                  │
├─────────────────────────────┬───────────────────────────────┤
│    GitHubVFS.dll (核心)      │    GitHubVFS.js (脚本)        │
│  ┌───────────────────────┐  │  ┌─────────────────────────┐  │
│  │ 路径解析 ParsePath    │  │  │ 创造数据列计算          │  │
│  │ 目录枚举 ReadDir      │  │  │ 高级命令执行            │  │
│  │ 原始数据列填充        │  │  │ 搜索对话框              │  │
│  │ 右键菜单(核心操作)    │  │  │ gh CLI fallback         │  │
│  │ API 通信              │  │  └─────────────────────────┘  │
│  │ 缓存写入              │  │                               │
│  └───────────────────────┘  │                               │
├─────────────────────────────┴───────────────────────────────┤
│                    通信层                                     │
│     JSON 缓存文件 │ INI 配置文件                    │
├─────────────────────────────────────────────────────────────┤
│                    GitHub API (api.github.com)                │
│              Token 认证 / WinHTTP 通信                        │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 设计原则

### 2.1 列显示 + 菜单编辑

```
只读数据 → 自定义列显示 → 无需打开任何文件
可写数据 → 自定义列显示 + 右键菜单编辑 → 编辑后列自动刷新

文件只用于两种场景：
1. 真实内容（代码文件、Release 资产、Gist 文件）
2. 下载到本地使用的东西
```

**被移除的文件方式**：

| 原文件 | 替代方案 |
|--------|----------|
| `info.txt` 看描述 | 自定义列"描述" + 右键"编辑描述" |
| `topics.txt` 看标签 | 自定义列"主题" + 右键"编辑主题" |
| `homepage.txt` 看主页 | 右键"在浏览器中打开主页" + 右键"编辑主页" |
| `starred.signal` 查状态 | 右键菜单 CHECKED 状态（DLL 直接查询） |
| `watching.signal` 查状态 | 右键菜单 CHECKED 状态（DLL 直接查询，Subscriptions/ 下） |
| `download.zip` | 右键"下载 ZIP" |
| `download.tar.gz` | 右键"下载 TAR" |
| `clone-url.txt` | 右键"复制克隆 URL" |
| `✓ 已星标.txt` | 右键菜单 CHECKED 状态 |
| `✗ 操作失败.txt` | Toast 通知 |

### 2.2 路径即上下文

```
路径自动决定一切：

github://repos/react/           → 仓库搜索列集 + 搜索菜单
github://facebook/react/        → 仓库列集 + 仓库菜单
github://facebook/react/Issues/ → Issue列集 + Issue菜单
github://facebook/react/Code/   → 文件列集 + 文件菜单
```

### 2.3 双击 = 最直觉动作

| 项类型 | 双击行为 |
|--------|----------|
| 仓库文件夹 | 进入仓库 |
| Code/ 中文件 | 下载并打开（关联程序） |
| Issue | 在浏览器中打开完整内容 |
| PR | 在浏览器中打开完整内容 |
| Branch | 进入该分支的代码目录 |
| Release | 进入资产目录 |
| Notification | 跳转到对应 Issue/PR |
| Gist | 进入 Gist 文件列表 |
| 用户 | 进入该用户的仓库列表 |

---

## 3. 架构决策记录

### 3.1 DLL vs 脚本职责划分

| 决策项 | 方案 | 理由 |
|--------|------|------|
| DLL 职责 | 路径解析、目录枚举、自定义列（原始数据）、右键菜单（核心操作）、API 通信、缓存写入 | 性能关键路径，需要直接与 DOpus VFS 接口交互 |
| 脚本职责 | 创造数据列、高级命令、搜索对话框 | 灵活迭代、无需重编译、JScript 可直接调用 gh CLI |
| 数据源 | DLL 为唯一与 GitHub API 通信的角色 | 避免重复请求、保证一致性 |
| 认证 | Token 认证（Personal Access Token / OAuth） | GitHub API 要求 |
| 通信 | JSON 缓存文件 + INI 配置文件 | 进程间解耦，脚本通过删缓存+刷新通知DLL |

### 3.2 DLL vs 脚本判断标准

**DLL 实现的条件（满足任一）**：
1. 需要 VFS 接口回调（路径解析、目录枚举、文件操作）
2. 需要高性能（大量数据填充列）
3. 需要直接 WinHTTP 通信
4. 操作频率高、需要即时响应

**脚本实现的条件（满足任一）**：
1. 需要频繁修改/迭代（创造数据算法、菜单文案）
2. 需要调用外部命令（gh CLI）
3. 需要弹对话框（搜索、配置）
4. 操作频率低、可容忍延迟

### 3.3 自定义列实现策略

| 来源 | 数量 | 说明 |
|------|------|------|
| DLL | 41 列 | 原始数据，同步填充，VFS_GetCustomColumnsW 接口 |
| 脚本 | 6 列 | 创造数据，异步填充，OnGitHubColumn 回调 |

DLL 填充原始数据列（星标数、语言、描述等），脚本读取 DLL 写入的 JSON 缓存计算创造数据列（健康度、类别、可视化条等）。

### 3.4 右键菜单实现策略

| 来源 | 命令 | 说明 |
|------|------|------|
| DLL | gh_open_browser, gh_copy_clone_url, gh_copy_repo_url, gh_copy_raw_url, gh_copy_sha, gh_copy_permalink, gh_download_zip, gh_download_tar, gh_view_issues, gh_view_prs, gh_view_releases, gh_view_contributors, gh_view_commits, gh_view_blame, gh_view_history, gh_refresh, gh_api_usage, gh_search_repos, gh_search_code, gh_search_users, gh_search_issues, gh_search_commits, gh_search_topics | 核心操作，VFS_GetContextMenuW 接口 |
| 脚本 | GitHubStar, GitHubWatch, GitHubFork, GitHubClone, GitHubNewRepo, GitHubNewIssue, GitHubNewGist, GitHubCloseIssue, GitHubMergePR, GitHubAddLabel, GitHubAddComment, GitHubEditDesc, GitHubEditTopics, GitHubEditHomepage, GitHubEditGist, GitHubDeleteGist, GitHubMarkRead, GitHubConfigAuth, GitHubConfigProxy, GitHubCompare, GitHubSetDefault, GitHubDownload | 高级命令，执行后删除缓存文件 + Go REFRESH 通知 DLL |

---

## 4. 当前代码状态

### 4.1 当前路径体系（旧，待重构）

当前代码使用 `explore/`、`mine/`、`go/`、`.gh/` 等前缀：

```
github://
├── explore/                    ← 探索根目录
│   ├── repos/{query}/          ← 仓库搜索
│   ├── code/{query}/           ← 代码搜索
│   ├── users/{query}/          ← 用户搜索
│   ├── trending/               ← 趋势
│   └── topics/                 ← 主题
├── mine/                       ← 我的
│   ├── repos/                  ← 我的仓库
│   ├── starred/                ← 星标
│   ├── Subscriptions/           ← 关注
│   ├── notifications/          ← 通知
│   └── gists/                  ← Gist
├── search/{query}/             ← 旧搜索路径
├── go/{owner}/{repo}/          ← 别名直达
└── {owner}/{repo}/
    └── .gh/                    ← 元数据前缀
        ├── info.txt
        ├── topics.txt
        ├── homepage.txt
        ├── issues/open/
        ├── issues/closed/
        ├── pulls/open/
        ├── pulls/closed/
        ├── branches/{name}/
        ├── tags/{name}/
        ├── releases/{version}/
        ├── star/
        ├── unstar/
        ├── watch/
        ├── unwatch/
        ├── download-zip/
        ├── download-tar/
        └── clone-url/
```

### 4.2 当前 GitHubPathInfo 结构（已重构，使用 GitHubContext 枚举）

> **v3.1 已完成**：22 个布尔字段已替换为 `GitHubContext` 枚举。

```cpp
struct GitHubPathInfo {
    std::wstring owner;
    std::wstring repo;
    std::wstring path;
    std::wstring searchQuery;
    std::wstring searchType;       // 搜索类型：Repos/Code/Users/Issues/Commits/Topics
    std::wstring ref;
    GitHubRefType refType;
    int issueNumber;
    int page;
    std::wstring trendingSince;    // Trending 时间范围：daily/weekly/monthly
    std::wstring trendingLang;     // Trending 语言筛选
    std::wstring metaItem;         // 通用元数据项 ID（如 gist_id）
    GitHubContext context;         // 上下文枚举（替代 22 个布尔字段）
    bool isDir;

    GitHubPathInfo() {
        refType = GITHUB_REF_DEFAULT;
        issueNumber = 0;
        page = 1;
        context = GITHUB_CTX_ROOT;
        isDir = false;
    }
};
```

**重构效果**：
- 布尔字段：22 → 1（`isDir`）
- 代码可读性：大幅提升
- 路径判断：`switch (context)` 替代多重 `if-else`

### 4.3 当前 GitHubContext 枚举（已重构，替代 GitHubMetaType）

> **v3.1 已完成**：`GitHubMetaType` 枚举（30个值）已替换为 `GitHubContext` 枚举（23个值）。

```cpp
enum GitHubContext {
    GITHUB_CTX_ROOT = 0,

    // 根目录保留字（大写开头）
    GITHUB_CTX_REPOS,            // Repos/ — 我的仓库
    GITHUB_CTX_STARRED,          // Starred/ — 我的星标
    GITHUB_CTX_SUBSCRIPTIONS,    // Subscriptions/ — 我的关注
    GITHUB_CTX_NOTIFICATIONS,    // Notifications/ — 我的通知
    GITHUB_CTX_GISTS,            // Gists/ — 我的 Gist
    GITHUB_CTX_TRENDING,         // Trending/ — 趋势

    // 搜索中心
    GITHUB_CTX_SEARCH_CENTER,    // Search/ — 搜索中心页（6种搜索入口）
    GITHUB_CTX_SEARCH_REPOS,     // Search/Repos/{query}/
    GITHUB_CTX_SEARCH_CODE,      // Search/Code/{query}/
    GITHUB_CTX_SEARCH_USERS,     // Search/Users/{query}/
    GITHUB_CTX_SEARCH_ISSUES,    // Search/Issues/{query}/
    GITHUB_CTX_SEARCH_COMMITS,   // Search/Commits/{query}/
    GITHUB_CTX_SEARCH_TOPICS,    // Search/Topics/{query}/

    // 用户/仓库
    GITHUB_CTX_OWNER,            // {owner}/ — 用户仓库列表
    GITHUB_CTX_REPO,             // {owner}/{repo}/ — 仓库根目录
    GITHUB_CTX_CODE,             // {owner}/{repo}/Code/ — 纯代码
    GITHUB_CTX_ISSUES,           // {owner}/{repo}/Issues/
    GITHUB_CTX_PULLS,            // {owner}/{repo}/Pulls/
    GITHUB_CTX_BRANCHES,         // {owner}/{repo}/Branches/
    GITHUB_CTX_TAGS,             // {owner}/{repo}/Tags/
    GITHUB_CTX_RELEASES          // {owner}/{repo}/Releases/
};
```

**重构效果**：
- 枚举值：30 → 23
- 语义清晰：每个值对应一个明确的路径上下文
- 扩展性好：新增路径类型只需添加枚举值

### 4.4 当前自定义列（旧，8列）

| 列 Key | 标签 | 类型 | 说明 |
|--------|------|------|------|
| `ghdesc` | 描述 | 文本 | 仓库描述（截断80字） |
| `ghlang` | 语言 | 文本 | 主要编程语言 |
| `ghstars` | 星标数 | 数字 | stargazers count |
| `ghvis` | 可见性 | 文本 | 公开/私有 |
| `ghbranch` | 默认分支 | 文本 | default branch |
| `ghforks` | Fork数 | 数字 | forks count |
| `ghsize` | 大小 | 尺寸 | 仓库磁盘占用 |
| `ghupdated` | 更新时间 | 文本 | 最后推送时间 |

### 4.5 当前右键菜单（旧，5项）

```
根目录：搜索仓库... / 搜索代码... / 搜索用户...
仓库上：在浏览器中打开 / 复制克隆 URL / ── / 下载 ZIP / 下载 TAR
```

### 4.6 当前 API 方法（已实现）

| 方法 | 状态 |
|------|------|
| ListUserRepos | ✅ |
| ListOrgRepos | ✅ |
| GetRepoInfo | ✅ |
| SearchRepos | ✅ |
| SearchCode | ✅ |
| SearchUsers | ✅ |
| ListIssues | ✅ |
| GetIssue | ✅ |
| CreateIssue | ✅ |
| UpdateIssue | ✅ |
| CloseIssue | ✅ |
| ListPullRequests | ✅ |
| GetPullRequest | ✅ |
| ListBranches | ✅ |
| ListTags | ✅ |
| ListReleases | ✅ |
| StarRepo / UnstarRepo / IsStarred | ✅ |
| WatchRepo / UnwatchRepo | ✅ |
| ListStarredRepos | ✅ |
| ListWatchedRepos | ✅ |
| DownloadArchive | ✅ |
| ListDirectory | ✅ |
| GetFileInfo | ✅ |
| DownloadFile / UploadFile | ✅ |
| DeleteGitHubFile | ✅ |
| CreateGitHubDir | ✅ |
| MoveGitHubFile | ✅ |
| GetDefaultBranch | ✅ |
| TestConnection | ✅ |
| StartOAuthDeviceFlow / PollOAuthToken | ✅ |

### 4.7 待新增 API 方法

| 方法 | 说明 |
|------|------|
| SearchIssues | Issue & PR 搜索 |
| SearchCommits | 提交搜索 |
| SearchTopics | 主题搜索 |
| ListNotifications | 通知列表 |
| ListGists | Gist 列表 |
| ListTrending | 趋势仓库 |

### 4.8 Trending API 实现方案

> **重要**：GitHub 官方不提供 Trending API，需要使用第三方方案。

#### 方案选择

| 方案 | 优点 | 缺点 | 推荐 |
|------|------|------|------|
| 第三方 API（如 [github-trending-api](https://github.com/huchenme/github-trending-api)） | 稳定、有缓存、支持语言筛选 | 依赖第三方服务 | ⭐ 推荐 |
| 自建爬虫 | 完全控制、无依赖 | 需维护、可能被反爬 | 备选 |
| GitHub GraphQL（如有） | 官方支持 | 目前不支持 Trending | 不可行 |

#### 推荐方案：使用第三方 API

**API 端点**：`https://api.gitterapp.com/repositories`

**请求参数**：
```
since=daily|weekly|monthly
language=javascript|python|rust|...
```

**响应格式**：
```json
[
  {
    "author": "facebook",
    "name": "react",
    "avatar": "https://...",
    "url": "https://github.com/facebook/react",
    "description": "A JavaScript library...",
    "language": "JavaScript",
    "languageColor": "#f1e05a",
    "stars": 223000,
    "forks": 45600,
    "starsSince": 123,
    "builtBy": [...]
  }
]
```

**实现要点**：
1. 缓存结果（Trending 更新频率低，可缓存 1 小时）
2. 网络失败时显示错误提示
3. 支持语言列表从 GitHub Popular Languages 获取

#### 备选方案：自建爬虫

如果第三方 API 不可用，可自行爬取 GitHub Trending 页面：

```cpp
// 伪代码
std::vector<GitHubRepoInfo> ListTrending(const std::wstring& since, const std::wstring& lang) {
    std::wstring url = L"https://github.com/trending";
    if (!lang.empty()) url += L"/" + lang;
    url += L"?since=" + since;
    
    std::string html = SendHttpGet(url);  // 爬取 HTML
    return ParseTrendingHtml(html);        // 解析 HTML
}
```

**注意事项**：
- 添加 User-Agent 头
- 限制请求频率（避免被封）
- 处理 HTML 结构变化

---

## 5. 路径体系设计

> **v3.1 更新**：采纳 6 项改进方案——Search 命名空间、@{ref} 自动检测、Trending 语言筛选、虚拟入口统计列、Repos 内搜索入口、移除 IsOwnerSubdir。

### 5.1 最终路径树

```
github://
│
│  ═══ 根目录显示 ═══
├── Repos/                       ← 我的仓库（内含搜索入口）
├── Starred/                     ← 我的星标
├── Subscriptions/                ← 我的关注
├── Notifications/               ← 我的通知
├── Gists/                       ← 我的 Gist
│   └── {gist_id}/               ← Gist 文件
├── Trending/                    ← 趋势（支持时间+语言筛选）
│   ├── {daily|weekly|monthly}/  ← 时间范围
│   └── {daily|weekly|monthly}/{language}/  ← 时间+语言
├── Search/                      ← 搜索中心（6种搜索类型入口）
│   ├── Repos/                   ← 双击进入仓库搜索
│   ├── Code/                    ← 双击进入代码搜索
│   ├── Users/                   ← 双击进入用户搜索
│   ├── Issues/                  ← 双击进入 Issue 搜索
│   ├── Commits/                 ← 双击进入提交搜索
│   └── Topics/                  ← 双击进入主题搜索
│
│  ═══ 搜索路径（地址栏直达，根目录通过 Search/ 中心进入）═══
├── Search/Repos/{query}/        ← 搜索仓库
├── Search/Code/{query}/         ← 搜索代码
├── Search/Users/{query}/        ← 搜索用户
├── Search/Issues/{query}/       ← 搜索 Issue & PR
├── Search/Commits/{query}/      ← 搜索提交
├── Search/Topics/{query}/       ← 搜索主题
│
│  ═══ 用户/仓库路径 ═══
└── {owner}/                     ← 用户公开仓库
    └── {repo}/                  ← 仓库根目录 = 代码 + 虚拟入口
        ├── {path}               ← 真实文件（默认分支）
        ├── Code/{path}          ← 纯代码（无虚拟目录）
        ├── @{ref}/{path}/       ← 切换分支/标签/SHA（自动检测类型）
        ├── Issues/              ← 虚拟入口（显示 "523 open"）
        │   ├── {number}
        │   └── ▶ 第2页.../
        ├── Pulls/               ← 虚拟入口（显示 "12 open"）
        │   └── {number}
        ├── Branches/            ← 虚拟入口（显示 "5"）
        │   └── {branch}/{path}/
        ├── Tags/                ← 虚拟入口（显示 "30"）
        │   └── {tag}/{path}/
        └── Releases/            ← 虚拟入口（显示 "10"）
            └── {version}/       ← Release 资产
```

### 5.2 路径简化对比

| 指标 | 当前（旧） | 重构后 | 变化 |
|------|-----------|--------|------|
| `.gh/` 子项 | 23 | 0 | -100%（移除 .gh/） |
| `GitHubMetaType` 枚举 | 30 | 11 | -63% |
| `GitHubPathInfo` 布尔字段 | 22 | 1 | -95% |
| 路径层级最大深度 | 7 | 5 | -29% |
| 根目录中转层 | 2（explore/mine） | 0 | -100% |
| 根级保留字 | 11（小写） | 7（大写开头） | -36% |
| 搜索保留字 | 6（根级平铺） | 1（Search 命名空间） | -83% |

### 5.3 关键路径变更

| 操作 | 当前路径 | 重构后路径 | 省点击 |
|------|----------|-----------|--------|
| 看自己的仓库 | `github://mine/repos/` | `github://Repos/` | 1次 |
| 搜索仓库 | `github://explore/repos/{q}/` | `github://Search/Repos/{q}/` | 0次（Search 中心直达） |
| 看仓库代码 | `github://o/r/` → 找 `.gh/` | `github://o/r/` 直接看到 | 1次 |
| 看星标仓库 | `github://mine/starred/` | `github://Starred/` | 1次 |
| 看仓库Issues | `github://o/r/.gh/issues/open/` | `github://o/r/Issues/` | 2次 |
| 切换分支 | `github://o/r/.gh/branches/main/` | `github://o/r/@main/` | 2次 |
| 搜索后进入仓库 | 嵌套在搜索路径下 | 双击跳转到 `github://o/r/` | 2次 |
| 搜索代码 | `github://code/{q}/` | `github://Search/Code/{q}/` | 0次 |
| 搜索用户 | `github://users/{q}/` | `github://Search/Users/{q}/` | 0次 |

### 5.4 仓库根目录 = Code 内容 + 虚拟入口

```
双击仓库 → 直接看到代码文件 + 虚拟目录入口

github://facebook/react/
├── src/                  ← 真实目录       类型列: 📁 目录
├── package.json          ← 真实文件       类型列: 📄 文件
├── README.md             ← 真实文件       类型列: 📄 文件
├── Issues/               ← 虚拟入口       类型列: 📋 523 open
├── Pulls/                ← 虚拟入口       类型列: 🔀 12 open
├── Branches/             ← 虚拟入口       类型列: 🌿 5
├── Tags/                 ← 虚拟入口       类型列: 🏷 30
└── Releases/             ← 虚拟入口       类型列: 📦 10

想看纯代码？导航到 github://facebook/react/Code/
→ 只有真实文件，没有虚拟目录
```

**虚拟入口与真实目录同名时的处理**：

```
虚拟入口优先，真实内容通过 Code/ 访问：

  github://facebook/react/Issues/    → 虚拟入口（Issue 列表）
  github://facebook/react/Code/Issues/ → 真实目录（如果仓库有同名目录）

枚举仓库根目录时的规则：
  1. 先添加虚拟入口：Issues/, Pulls/, Branches/, Tags/, Releases/
  2. 遍历 API 返回的真实文件列表
  3. 真实文件名命中 IsRepoSubdir() → 跳过（已被虚拟入口占据）
  4. 其他真实文件 → 正常添加
```

**虚拟入口统计信息的数据来源**：

| 虚拟入口 | 统计数据 | 来源 | 额外 API 请求 |
|----------|----------|------|---------------|
| Issues/ | open 数量 | repoInfo.openIssuesCount | 无（已有） |
| Pulls/ | open 数量 | GET /repos/o/r/pulls?per_page=1 | 1次（可缓存） |
| Branches/ | 数量 | ListBranches 缓存 | 无（已缓存） |
| Tags/ | 数量 | ListTags 缓存 | 无（已缓存） |
| Releases/ | 数量 | ListReleases 缓存 | 无（已缓存） |

### 5.5 `@{ref}` 语法（自动检测类型）

```
github://facebook/react/@main/src/           ← main 分支
github://facebook/react/@develop/src/        ← develop 分支
github://facebook/react/@v18.3.1/src/        ← v18.3.1 标签
github://facebook/react/@abc1234def5678.../  ← 完整 commit SHA（40位）
github://facebook/react/@abc1234/            ← 短 SHA（7位）

自动检测规则（按优先级）：
  1. 40位十六进制 → GITHUB_REF_SHA
  2. 7位十六进制 → GITHUB_REF_SHA（短格式）
  3. 已知分支列表中匹配 → GITHUB_REF_BRANCH
  4. 已知标签列表中匹配 → GITHUB_REF_TAG
  5. 默认 → GITHUB_REF_BRANCH（最常见）

注：GitHub API 的 GetContent 接受 branch/tag/SHA 都能工作，
    refType 主要影响 UI 显示和路径重构，不影响 API 调用。
    分支/标签精确匹配可在 ReadRepoDirectory 时通过缓存修正。

与 Branches/ 的关系：
  Branches/ = "我想看看有哪些分支"（探索模式）
  @main/    = "我知道分支名是 main"（直达模式）
两种都有价值，保留
```

### 5.6 搜索结果双击直达

```
在 github://Search/Repos/react/ 中搜索结果：

facebook/react/     → 双击 → github://facebook/react/    ← 直接跳转
vuejs/vue/          → 双击 → github://vuejs/vue/
angular/angular/    → 双击 → github://angular/angular/

实现：ParseGitHubPath 识别 Search/{type}/{query}/{owner}/{repo}/ 路径
     自动重定向为 {owner}/{repo}/ 路径
```

#### 各搜索类型的重定向规则

| 搜索类型 | 搜索结果路径 | 双击跳转目标 | 说明 |
|----------|-------------|-------------|------|
| Repos | `Search/Repos/{q}/{owner}/{repo}/` | `github://{owner}/{repo}/` | 进入仓库根目录 |
| Users | `Search/Users/{q}/{login}/` | `github://{login}/` | 进入用户仓库列表 |
| Issues | `Search/Issues/{q}/{owner}/{repo}/{number}` | `github://{owner}/{repo}/Issues/{number}` | 进入 Issue 详情 |
| Code | `Search/Code/{q}/{owner}/{repo}/{path}` | `github://{owner}/{repo}/@{sha}/{path}` | 进入文件（带 SHA） |
| Commits | `Search/Commits/{q}/{owner}/{repo}/{sha}` | `github://{owner}/{repo}/@{sha}/` | 进入该提交的代码 |
| Topics | `Search/Topics/{q}/{topic}/` | `github://Search/Repos/topic:{topic}/` | 搜索该主题的仓库 |

#### Code 搜索重定向详解

代码搜索结果需要特殊处理，因为：
1. 结果包含 `owner`、`repo`、`path`、`sha` 字段
2. 用户可能想看文件内容或仓库上下文

**实现逻辑**：
```cpp
// 代码搜索结果双击
if (codeResult.path != "") {
    // 跳转到文件所在位置
    std::wstring target = L"github://" + codeResult.owner + L"/" + codeResult.repo + 
                          L"/@" + codeResult.sha + L"/" + codeResult.path;
    NavigateTo(target);
}
```

**备选行为**（右键菜单）：
- "在仓库中打开" → `github://{owner}/{repo}/`
- "查看文件历史" → 右键菜单 → "查看历史"

#### Commits 搜索重定向详解

提交搜索结果包含 `owner`、`repo`、`sha` 字段。

**实现逻辑**：
```cpp
// 提交搜索结果双击
std::wstring target = L"github://" + commitResult.owner + L"/" + commitResult.repo + 
                      L"/@" + commitResult.sha + L"/";
NavigateTo(target);
```

**显示效果**：
- 进入该提交的代码树
- 列显示该提交的文件列表

#### Topics 搜索重定向详解

主题搜索结果包含 `name`、`description`、`repoCount` 字段。

**实现逻辑**：
```cpp
// 主题搜索结果双击
std::wstring target = L"github://Search/Repos/topic:" + topicResult.name + L"/";
NavigateTo(target);
```

**显示效果**：
- 自动搜索使用该主题的仓库
- 用户可进一步筛选

### 5.7 Repos/ 内搜索入口

```
github://Repos/
├── 🔍 搜索仓库.../        ← 虚拟目录，双击弹出搜索对话框
├── facebook/react/         ← 我的仓库1
├── vuejs/vue/              ← 我的仓库2
└── ...

双击 🔍 搜索仓库.../ → 弹出搜索对话框 → 输入 react → 跳转 Search/Repos/react
```

### 5.8 Trending/ 语言筛选

```
github://Trending/                         ← 默认（daily + all languages）
github://Trending/daily/                   ← 今日趋势
github://Trending/daily/javascript/        ← 今日 JavaScript 趋势
github://Trending/weekly/python/           ← 本周 Python 趋势
github://Trending/monthly/rust/            ← 本月 Rust 趋势

解析规则：
  seg1 匹配 daily|weekly|monthly → 时间范围
  seg1 是其他值 → 当作语言名（默认 daily）
  seg2 存在 → 语言名

即：
  Trending/javascript       → daily + javascript
  Trending/daily/javascript → daily + javascript
  Trending/weekly           → weekly + all
```

### 5.9 移除的路径

| 移除项 | 原因 | 替代方案 |
|--------|------|----------|
| `.gh/` 前缀 | 虚拟目录直接放在仓库根目录 | 虚拟入口优先，真实内容通过 Code/ 访问 |
| `explore/` | 不必要的中转层 | 扁平化到根目录 |
| `mine/` | 不必要的中转层 | 扁平化到根目录 |
| `go/` 前缀 | 不直观 | `@{ref}` 语法替代 |
| 别名系统（`~`, `@`, `s/`, `sc/`, `t/`） | 增加记忆负担 | 扁平化路径 |
| 旧路径兼容（`search/`, `my-repos/`） | 不再需要 | 新路径体系 |
| `issues/open/`, `issues/closed/` | 自定义列可区分状态 | 合并为 `Issues/`，列显示状态 |
| `pulls/open/`, `pulls/closed/` | 同上 | 合并为 `Pulls/`，列显示状态 |
| `.gh/star/`, `.gh/unstar/` | 动作不应是路径 | 右键菜单 |
| `.gh/watch/`, `.gh/unwatch/` | 同上 | 右键菜单 |
| `.gh/download-zip/`, `.gh/download-tar/` | 同上 | 右键菜单 |
| `.gh/clone-url/` | 同上 | 右键菜单 |
| `*.signal` | 不直观 | 右键菜单 CHECKED 状态（DLL 直接查询） |
| `info.txt`, `topics.txt`, `homepage.txt` | 列显示更好 | 自定义列 + 右键编辑 |
| `compare/` | 极低频 + 结果不适合文件管理器 | 右键菜单"比较..." |
| `IsOwnerSubdir` 路径 | 语义矛盾（`torvalds/starred/` 显示当前用户的星标） | 移除，`{owner}/` 只显示仓库列表 |
| 小写搜索保留字（`repos/`, `code/`, `users/`, `issues/`） | 与 Search 命名空间冲突 | 统一为 `Search/{Type}/` |
| `repos/{query}` 双用途 | 同一词两种含义（我的仓库 vs 搜索） | `Repos/` = 我的仓库，`Search/Repos/{q}` = 搜索 |

---

## 6. 自定义列体系设计

### 6.1 设计原则

- DLL 填充原始数据列，脚本填充创造数据列
- 按上下文自动切换列集
- 所有数据都在列中展示，没有需要"打开文件"才能看到的信息
- 每个创造数据列返回 `{value, sort, group}` 三元组

### 6.2 六大设计范式

#### 6.2.1 聚合列 — 一列顶多列

把多个维度的信息压缩到一列中，用紧凑的符号组合展示。

**设计原理**：用户通常不需要同时看 20 列，一个精心设计的聚合列可以覆盖 80% 的浏览场景。用户想深入某个维度时再展开独立列。

**示例输出**：
```
🟢 📦 Rust  ★12.5k  🍴830  MIT  2天前
🟡 📦 Go    ★3.2k   🍴210  Apache  3周前
🔴 📦 JS    ★89     🍴5    无许可  2年前
```

**字段映射**：
```
健康度  类别   语言    星标    Fork   许可证  活跃度
```

#### 6.2.2 状态机列 — 列值随条件变化

列不是静态的文本，而是有优先级的状态机，根据条件自动切换显示。

**状态优先级**（高→低）：

| 优先级 | 状态 | 条件 | 显示 |
|--------|------|------|------|
| 1 | 已归档 | `isArchived === true` | 📦 已归档 |
| 2 | 活跃 | 最后推送 ≤ 7天 | 🟢 活跃 |
| 3 | 维护中 | 最后推送 ≤ 30天 | 🟡 维护中 |
| 4 | 低活跃 | 最后推送 ≤ 90天 | 🟠 低活跃 |
| 5 | 休眠 | 最后推送 ≤ 365天 | 🔴 休眠 |
| 6 | 已死 | 最后推送 > 365天 | ⚫ 已死 |

#### 6.2.3 对比列 — 相对值比绝对值更有意义

绝对数字（12500 星）缺乏上下文，相对排名和百分位才是决策依据。

**示例输出**：
```
🥇 #1   react        (Top 1%)
🥈 #2   webpack      (Top 3%)
🥉 #3   babel        (Top 5%)
#4      eslint       (Top 8%)
#15     my-utils     (Top 75%)
#20     test-proj    (Bottom 10%)
```

#### 6.2.4 信号灯列 — 决策辅助

不只是展示数据，而是替用户做判断，用红绿灯直接给出结论。

**评判规则**：

| 判定 | 条件 | 显示 |
|------|------|------|
| 不推荐 | `isArchived === true` | ❌ 不推荐（已归档） |
| Fork | `isFork === true` | 🔀 Fork（非原创） |
| 新项目 | 创建 < 30天 | 🆕 新项目（数据不足） |
| 推荐 | 健康度 ≥ 70 | ✅ 推荐（健康度 N） |
| 谨慎 | 健康度 40-69 | ⚠ 谨慎（健康度 N） |
| 不推荐 | 健康度 < 40 | ❌ 不推荐（健康度 N） |

#### 6.2.5 路径感知列组 — 同一列名，不同路径下含义不同

列名不变，但行为随路径自动切换，实现"一列多用"。

**示例**：

| 列名 | repo-list 上下文 | repo-files 上下文 | search 上下文 |
|------|-----------------|-------------------|---------------|
| `GhMeta` | `⭐12.5k 🍴830 Rust` | `📁 目录 / 📄 .rs 2.3KB` | `📦 仓库 / 📄 代码` |

#### 6.2.6 渐进式列 — 同一列支持简/繁两种模式

利用 DOpus 的 `value` 和 `group` 字段，一列同时提供概览和详情。

**示例**：
```
正常视图:  🟢 85
分组视图:  🟢 活跃(30) + 社区(22) + 热度(20) + 维护(13)
```

### 6.3 各上下文列定义

#### 根目录

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 固定仓库 / 最近访问 / 功能入口 |
| ghtype | 类型 | 固定/最近/入口 |

#### repos/（我的仓库 / 仓库搜索）

| 列 Key | 标签 | 对齐 | 来源 | 说明 |
|--------|------|------|------|------|
| ghname | 名称 | 左 | DLL | 仓库名 |
| ghdesc | 描述 | 左 | DLL | 仓库描述 |
| ghlang | 语言 | 左 | DLL | 主要语言 |
| ghstars | 星标 | 右 | DLL | stargazers_count |
| ghforks | Fork | 右 | DLL | forks_count |
| ghsize | 大小 | 右 | DLL | 仓库大小 |
| ghupdated | 更新时间 | 左 | DLL | pushed_at |
| ghlicense | 协议 | 左 | DLL | license.spdx_id |
| ghtopics | 主题 | 左 | DLL | topics 数组 |
| ghstatus | 状态 | 左 | DLL | ⭐已星标 👁已关注 🔒私有 📦归档 🍴Fork |
| ghhealth | 健康度 | 右 | 脚本 | 0-100 评分 |
| ghcategory | 类别 | 左 | 脚本 | 框架/库/工具/应用/文档/配置 |
| ghstarbar | 星标可视化 | 左 | 脚本 | ████████░░ 223k |
| ghactivity | 活跃度 | 左 | 脚本 | ████████░░ 热 |
| ghratio | S/F比 | 右 | 脚本 | Star/Fork 比值 |
| ghrecent | 最近活跃 | 左 | 脚本 | "2小时前"/"3天前" |
| ghreadme | README | 左 | DLL | 截取前200字符 |

#### Starred/、Subscriptions/、Trending/

同 repos/ 列集。

#### notifications/

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 来源 | owner/repo #number |
| ghtype | 类型 | Issue / PR / Commit |
| ghreason | 原因 | 新评论/已合并/提及你/... |
| ghtitle | 标题 | 通知标题 |
| ghtime | 时间 | 相对时间 |

#### gists/

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | Gist ID |
| ghdesc | 描述 | Gist 描述 |
| ghpublic | 公开 | ✅/❌ |
| ghfiles | 文件数 | 文件数量 |
| ghupdated | 更新时间 | updated_at |

#### code/{query}/（代码搜索）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 文件名 | 匹配的文件名 |
| ghpath | 路径 | 文件路径 |
| ghrepo | 所属仓库 | owner/repo |
| ghrepolang | 仓库语言 | 仓库主要语言 |
| ghrepostars | 仓库星标 | 仓库星标数 |

#### users/{query}/（用户搜索）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 用户名 |
| ghbio | 简介 | bio |
| ghcompany | 公司 | company |
| ghlocation | 位置 | location |
| ghfollowers | 粉丝 | followers |
| ghrepos | 仓库数 | public_repos |

#### issues/{query}/（Issue & PR 搜索）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghnumber | 编号 | #123 |
| ghtitle | 标题 | Issue 标题 |
| ghstate | 状态 | open / closed |
| ghauthor | 作者 | 创建者 |
| ghlabels | 标签 | 逗号分隔 |
| ghcomments | 评论数 | 评论数量 |
| ghrepo | 所属仓库 | owner/repo |
| ghtype | 类型 | Issue / PR |

#### commits/{query}/（提交搜索）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghmessage | 提交信息 | commit message |
| ghauthor | 作者 | 提交者 |
| ghdate | 日期 | 提交日期 |
| ghrepo | 所属仓库 | owner/repo |
| ghsha | SHA | 提交 SHA |

#### topics/{query}/（主题搜索）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 主题名 |
| ghdesc | 描述 | 主题描述 |
| ghcount | 仓库数 | 使用该主题的仓库数 |

#### {owner}/（用户主页）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 仓库名 |
| ghtype | 类型 | 仓库 |
| ghdesc | 描述 | 仓库描述 |
| ghlang | 语言 | 主要语言 |
| ghstars | 星标 | stargazers_count |
| ghforks | Fork | forks_count |
| ghsize | 大小 | 仓库大小 |
| ghupdated | 更新时间 | pushed_at |

路径栏信息栏显示用户信息：
`👤 Linus Torvalds | 🏢 Linux Foundation | 📍 Portland | 👥 190k followers`

#### {owner}/{repo}/（仓库根目录 = Code + 虚拟入口）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 文件/目录名 |
| ghtype | 类型 | 文件/目录/Issue入口/PR入口/... |
| ghsize | 大小 | 文件大小 |
| ghsha | SHA | 文件 SHA |
| ghupdated | 更新时间 | 最后提交时间 |

虚拟目录额外列：

| 虚拟目录 | 额外列 |
|----------|--------|
| Issues/ | 数量（523 open） |
| Pulls/ | 数量（12 open） |
| Branches/ | 数量（5） |
| Tags/ | 数量（30） |
| Releases/ | 数量（10） |

#### Issues/（仓库 Issue 列表）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghnumber | 编号 | #123 |
| ghtitle | 标题 | Issue 标题 |
| ghstate | 状态 | open / closed（可分组/筛选） |
| ghauthor | 作者 | 创建者 |
| ghlabels | 标签 | 逗号分隔 |
| ghcomments | 评论数 | 评论数量 |
| ghcreated | 创建时间 | created_at |
| ghupdated | 更新时间 | updated_at |

#### Pulls/（仓库 PR 列表）

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghnumber | 编号 | #123 |
| ghtitle | 标题 | PR 标题 |
| ghstate | 状态 | open / closed / merged |
| ghauthor | 作者 | 创建者 |
| ghbranch | 分支 | head → base |
| ghlabels | 标签 | 逗号分隔 |
| ghcomments | 评论数 | 评论数量 |
| ghcreated | 创建时间 | created_at |
| ghupdated | 更新时间 | updated_at |

#### Branches/

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 分支名 |
| ghdefault | 默认 | ✓ / - |
| ghsha | 提交SHA | 前7位 |
| ghcommitter | 提交者 | 最后提交者 |
| ghdate | 提交时间 | 最后提交时间 |

#### Tags/

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghname | 名称 | 标签名 |
| ghsha | 提交SHA | 前7位 |

#### Releases/

| 列 Key | 标签 | 说明 |
|--------|------|------|
| ghversion | 版本 | tag_name |
| ghname | 名称 | release name |
| ghtype | 类型 | 正式 / 预发布 / 草稿 |
| ghassets | 资产数 | 下载资产数量 |
| ghdate | 发布时间 | published_at |

### 6.4 创造数据列算法

#### 健康度评分（GhHealth）

```
输入：stargazers_count, forks_count, open_issues_count, pushed_at, created_at, is_archived

算法：
  baseScore = 0
  baseScore += min(stars / 100, 30)          // 星标贡献最多30分
  baseScore += min(forks / 50, 20)            // Fork贡献最多20分
  baseScore += min(daysSincePush < 30 ? 20 : daysSincePush < 90 ? 10 : 0, 20)  // 活跃度20分
  baseScore += min(1 - openIssues / max(stars, 1), 1) * 15  // Issue响应率15分
  baseScore += min((stars / max(daysSinceCreate, 1)) * 365, 15)  // 增长率15分
  if (is_archived) baseScore *= 0.3           // 归档仓库大幅降分

输出：
  value: "72"
  sort: 72
  group: "良好 (60-79)"
```

#### 项目类别（GhCategory）

```
输入：language, topics, description, stargazers_count, forks_count

规则（优先级从高到低）：
  topics 包含 "framework" → "框架"
  topics 包含 "library" 或 "lib" → "库"
  topics 包含 "cli" 或 "command-line" → "工具"
  topics 包含 "app" 或 "application" → "应用"
  topics 包含 "documentation" 或 "docs" → "文档"
  topics 包含 "config" 或 "dotfiles" → "配置"
  language + description 关键词推断
  默认 → "其他"

输出：
  value: "框架"
  sort: 1（框架）> 2（库）> 3（工具）> 4（应用）> 5（文档）> 6（配置）> 7（其他）
  group: value
```

#### 星标可视化条（GhStarBar）

```
输入：stargazers_count

算法：
  barLen = min(floor(log10(max(stars, 1)) * 2.5), 10)
  bar = "█" × barLen + "░" × (10 - barLen)

输出：
  value: "████████░░ 223k"
  sort: stargazers_count
  group: stars >= 10000 ? "1万+" : stars >= 1000 ? "1千+" : stars >= 100 ? "100+" : "<100"
```

#### 活跃度热条（GhActivity）

```
输入：pushed_at, created_at, stargazers_count

算法：
  daysSincePush = daysSince(pushed_at)
  if (daysSincePush < 7) heat = 10
  else if (daysSincePush < 30) heat = 8
  else if (daysSincePush < 90) heat = 6
  else if (daysSincePush < 180) heat = 4
  else if (daysSincePush < 365) heat = 2
  else heat = 1

  bar = "█" × heat + "░" × (10 - heat)
  label = heat >= 8 ? "热" : heat >= 6 ? "温" : heat >= 4 ? "凉" : "冷"

输出：
  value: "████████░░ 热"
  sort: heat
  group: label
```

#### Star/Fork 比（GhRatio）

```
输入：stargazers_count, forks_count

算法：
  ratio = stars / max(forks, 1)

输出：
  value: "18.6"
  sort: ratio
  group: ratio > 20 ? "极高" : ratio > 10 ? "较高" : ratio > 5 ? "中等" : "较低"
```

#### 最近活跃（GhRecent）

```
输入：pushed_at

算法：
  days = daysSince(pushed_at)
  if (days < 1) label = "今天"
  else if (days < 2) label = "昨天"
  else if (days < 7) label = floor(days) + "天前"
  else if (days < 30) label = floor(days/7) + "周前"
  else if (days < 365) label = floor(days/30) + "月前"
  else label = floor(days/365) + "年前"

输出：
  value: "2小时前"
  sort: -daysSincePush
  group: days < 7 ? "本周" : days < 30 ? "本月" : days < 365 ? "今年" : "更早"
```

### 6.5 DLL 列 vs 脚本列汇总

| 来源 | 数量 | 说明 |
|------|------|------|
| DLL | 41 列 | 原始数据，同步填充 |
| 脚本 | 6 列 | 创造数据，异步填充 |

---

## 7. 右键菜单体系设计

### 7.1 设计原则

- 路径即上下文，不同区域显示不同菜单
- 可写数据通过右键菜单编辑
- 复制操作集中在"复制"子菜单
- 操作后列自动刷新

### 7.2 各上下文菜单

#### 根目录

```
右键（空白处）：
├── 🔍 搜索仓库...
├── 🔍 搜索代码...
├── 🔍 搜索用户...
├── 🔍 搜索 Issues...
├── 🔍 搜索 Commits...
├── 🔍 搜索 Topics...
├── ────────────────
├── ⚙ 配置认证...
├── ⚙ 配置代理...
└── 📊 API 用量
```

#### 仓库列表（Repos/、Starred/、Subscriptions/）

```
右键（仓库上）：
├── ⭐ Star / 取消 Star          ← CHECKED 状态
├── 👁 Watch / 取消 Watch         ← CHECKED 状态
├── 🍴 Fork
├── ────────────────
├── 📋 复制
│   ├── 克隆 URL
│   ├── 仓库地址
│   └── owner/repo
├── ────────────────
├── 📥 克隆到本地...
├── 📥 下载 ZIP
├── 📥 下载 TAR
├── ────────────────
├── 🌐 在浏览器中打开
└── 🔄 刷新缓存
```

#### 仓库根目录

```
右键（空白处）：
├── ⭐ Star / 取消 Star
├── 👁 Watch / 取消 Watch
├── 🍴 Fork
├── ────────────────
├── 📋 复制
│   ├── 克隆 URL
│   ├── 仓库地址
│   └── owner/repo
├── ────────────────
├── 📥 克隆到本地...
├── 📥 下载 ZIP
├── 📥 下载 TAR
├── ────────────────
├── ✏ 编辑描述
├── ✏ 编辑主题
├── ✏ 编辑主页
├── ────────────────
├── 🔍 搜索此仓库...
├── ⚡ 快速跳转
│   ├── 📋 Issues (523)
│   ├── 🔀 Pulls (12)
│   ├── 🌿 Branches (5)
│   ├── 🏷 Tags (30)
│   ├── 📦 Releases (10)
│   └── ⬆ 返回仓库根目录
├── ────────────────
├── 🌐 在浏览器中打开
├── 🌐 查看 Issues
├── 🌐 查看 PRs
├── 🌐 查看 Releases
├── 🌐 查看 Contributors
├── 🌐 查看 Commits
├── ────────────────
└── 🔄 刷新缓存
```

#### Code/ 中文件

```
右键（文件上）：
├── 📋 复制
│   ├── 原始文件 URL
│   ├── SHA
│   ├── Permalink
│   └── 文件路径
├── ────────────────
├── 🌐 查看 Blame
├── 🌐 查看历史
├── 🌐 在浏览器中打开
└── 📥 下载
```

#### Issues/

```
右键（Issue上）：
├── 🌐 在浏览器中打开
├── ✏ 关闭 / 重新打开
├── 🏷 添加标签
├── 💬 添加评论
└── 📋 复制标题

右键（空白处）：
├── ✏ 新建 Issue...
└── 🔍 搜索此仓库 Issues...
```

#### Pulls/

```
右键（PR上）：
├── 🌐 在浏览器中打开
├── ✏ 关闭 / 重新打开
├── 🔀 合并 PR
├── 🏷 添加标签
├── 💬 添加评论
└── 📋 复制标题
```

#### Branches/

```
右键（分支上）：
├── 🌐 在浏览器中打开
├── 📋 复制分支名
├── 🔀 比较...
└── ⭐ 设为默认分支
```

#### Tags/

```
右键（标签上）：
├── 🌐 在浏览器中打开
├── 📋 复制标签名
└── 🔀 比较...
```

#### Releases/

```
右键（Release上）：
├── 🌐 在浏览器中打开
├── 📥 下载所有资产
└── 📋 复制版本号
```

#### Notifications/

```
右键（通知上）：
├── ✅ 标记已读
├── 🌐 在浏览器中打开
└── 🔍 查看仓库

右键（空白处）：
└── ✅ 全部标记已读
```

#### Gists/

```
右键（Gist上）：
├── ✏ 编辑 Gist
├── 🗑 删除 Gist
├── 🌐 在浏览器中打开
└── 📋 复制 Gist URL
```

### 7.3 命令清单

| 来源 | 命令 ID | 标签 |
|------|---------|------|
| DLL | gh_open_browser | 在浏览器中打开 |
| DLL | gh_copy_clone_url | 复制克隆 URL |
| DLL | gh_copy_repo_url | 复制仓库地址 |
| DLL | gh_copy_raw_url | 复制原始文件 URL |
| DLL | gh_copy_sha | 复制 SHA |
| DLL | gh_copy_permalink | 复制 Permalink |
| DLL | gh_download_zip | 下载 ZIP |
| DLL | gh_download_tar | 下载 TAR |
| DLL | gh_view_issues | 查看 Issues（浏览器） |
| DLL | gh_view_prs | 查看 PRs（浏览器） |
| DLL | gh_view_releases | 查看 Releases（浏览器） |
| DLL | gh_view_contributors | 查看 Contributors（浏览器） |
| DLL | gh_view_commits | 查看 Commits（浏览器） |
| DLL | gh_view_blame | 查看 Blame（浏览器） |
| DLL | gh_view_history | 查看历史（浏览器） |
| DLL | gh_refresh | 刷新缓存 |
| DLL | gh_api_usage | API 用量 |
| DLL | gh_search_repos | 搜索仓库（对话框） |
| DLL | gh_search_code | 搜索代码（对话框） |
| DLL | gh_search_users | 搜索用户（对话框） |
| DLL | gh_search_issues | 搜索 Issues（对话框） |
| DLL | gh_search_commits | 搜索 Commits（对话框） |
| DLL | gh_search_topics | 搜索 Topics（对话框） |
| 脚本 | GitHubStar | Star/Unstar |
| 脚本 | GitHubWatch | Watch/Unwatch |
| 脚本 | GitHubFork | Fork |
| 脚本 | GitHubClone | 克隆到本地 |
| 脚本 | GitHubNewRepo | 新建仓库 |
| 脚本 | GitHubNewIssue | 新建 Issue |
| 脚本 | GitHubNewGist | 新建 Gist |
| 脚本 | GitHubCloseIssue | 关闭/重新打开 Issue |
| 脚本 | GitHubMergePR | 合并 PR |
| 脚本 | GitHubAddLabel | 添加标签 |
| 脚本 | GitHubAddComment | 添加评论 |
| 脚本 | GitHubEditDesc | 编辑描述 |
| 脚本 | GitHubEditTopics | 编辑主题 |
| 脚本 | GitHubEditHomepage | 编辑主页 |
| 脚本 | GitHubEditGist | 编辑 Gist |
| 脚本 | GitHubDeleteGist | 删除 Gist |
| 脚本 | GitHubMarkRead | 标记通知已读 |
| 脚本 | GitHubConfigAuth | 配置认证 |
| 脚本 | GitHubConfigProxy | 配置代理 |
| 脚本 | GitHubCompare | 比较引用 |
| 脚本 | GitHubSetDefault | 设为默认分支 |
| 脚本 | GitHubDownload | 下载文件/资产 |

### 7.4 搜索对话框设计

```
┌──────────────────────────────────────┐
│  搜索 GitHub 仓库                     │
│                                      │
│  关键词：[react               ]       │
│                                      │
│  排序：  ○ 最佳匹配  ● 星标数        │
│          ○ Fork数    ○ 最近更新      │
│                                      │
│  语言：  [全部          ▼]           │
│  星标：  [>              ]          │
│  Fork：  ○ 包含  ● 仅原始仓库        │
│                                      │
│  路径预览：                           │
│  github://repos/react+language:...   │
│                                      │
│         [取消]      [搜索]            │
└──────────────────────────────────────┘

点击搜索 → 跳转 github://repos/react+language:javascript+stars:>1000/
```

用户可以直接在地址栏输入限定符，也可以通过对话框 GUI 构建。路径预览让高级用户学会语法。

---

## 8. 搜索体系设计

### 8.1 搜索类型

| # | 类型 | 路径 | API 端点 | 已实现 |
|---|------|------|----------|--------|
| 1 | 仓库 | `Search/Repos/{query}/` | `/search/repositories` | ✅ |
| 2 | 代码 | `Search/Code/{query}/` | `/search/code` | ✅ |
| 3 | 用户 | `Search/Users/{query}/` | `/search/users` | ✅ |
| 4 | Issues & PR | `Search/Issues/{query}/` | `/search/issues` | ❌ 新增 |
| 5 | Commits | `Search/Commits/{query}/` | `/search/commits` | ❌ 新增 |
| 6 | Topics | `Search/Topics/{query}/` | `/search/topics` | ❌ 新增 |

搜索入口统一在 `Search/` 命名空间下，根目录显示 `Search/` 中心页（6 种搜索类型入口），地址栏可直接输入 `Search/{Type}/{query}/`。

### 8.2 搜索限定符支持

```
路径中支持 GitHub 高级搜索语法：

github://Search/Repos/react+language:javascript+stars:>1000/
github://Search/Code/useEffect+repo:facebook/react/
github://Search/Issues/memory+leak+label:bug+state:open/
github://Search/Commits/fix+security+author:torvalds/
github://Search/Users/tom+location:"San+Francisco"+followers:>100/

+ 号分隔限定符（URL 中空格用 + 代替）
```

### 8.3 仓库内上下文搜索

```
在仓库任意位置右键 → 搜索此仓库... → 自动限定 repo:owner/repo

输入关键词 → 跳转 github://Search/Code/关键词+repo:facebook/react/
```

### 8.4 搜索结果双击直达

| 搜索类型 | 双击项 | 跳转到 |
|----------|--------|--------|
| 仓库搜索 | `facebook/react` | `github://facebook/react/` |
| 代码搜索 | `useEffect` in `facebook/react` | `github://facebook/react/Code/src/ReactHooks.js` |
| 用户搜索 | `torvalds` | `github://torvalds/` |
| Issue搜索 | `#1234` in `facebook/react` | `github://facebook/react/Issues/1234` |
| 提交搜索 | `abc1234` in `facebook/react` | 浏览器打开 |
| 主题搜索 | `machine-learning` | `github://Trending/` 筛选该主题 |

### 8.5 搜索中心页

```
github://Search/
├── 📦 Repos/          ← 双击弹出仓库搜索对话框
├── 📄 Code/           ← 双击弹出代码搜索对话框
├── 👤 Users/          ← 双击弹出用户搜索对话框
├── 📋 Issues/         ← 双击弹出 Issue 搜索对话框
├── 📝 Commits/        ← 双击弹出提交搜索对话框
└── 🏷 Topics/         ← 双击弹出主题搜索对话框
```

---

## 9. 交互设计

### 9.1 批量操作

```
DOpus 原生支持多选，VFS 插件右键菜单自动对多选生效

选中 5 个仓库 → 右键：
├── ⭐ 批量星标（5）
├── 👁 批量关注（5）
├── 📋 复制仓库地址（5）
└── 📥 批量克隆...（5）

VFS_ContextVerbW 处理 lpszFiles 中的多个路径
```

### 9.2 仓库内快速跳转

```
在仓库任意子目录右键 → ⚡ 快速跳转：
├── 📋 Issues (523)
├── 🔀 Pulls (12)
├── 🌿 Branches (5)
├── 🏷 Tags (30)
├── 📦 Releases (10)
└── ⬆ 返回仓库根目录

点击直接跳转，不需要回根目录
```

### 9.3 复制子菜单

```
右键 → 📋 复制
├── 克隆 URL (https://github.com/facebook/react.git)
├── 仓库地址 (https://github.com/facebook/react)
├── owner/repo (facebook/react)
├── 原始文件 URL
├── SHA
├── Permalink
└── 文件路径

菜单项直接显示值，点击即复制
```

### 9.4 F5 刷新

```
F5 → DOpus 重新枚举目录 → DLL 清除缓存 → 重新请求 API

同时：
  进入目录时自动检查缓存新鲜度
  过期 → 自动刷新
  新鲜 → 用缓存
```

### 9.5 地址栏即搜索

```
输入 github://repos/react/     → 仓库搜索
输入 github://code/useEffect/  → 代码搜索

保留字判断优先级：
  1. 精确匹配保留字 → 功能入口
  2. 精确匹配已知用户（缓存中）→ 用户主页
  3. 其他 → 自动搜索仓库（二期考虑）
```

---

## 10. DLL-脚本通信设计

### 10.1 通信通道

| 通道 | 方向 | 路径 | 格式 | 用途 |
|------|------|------|------|------|
| JSON 缓存文件 | DLL → 脚本 | `%APPDATA%\GitHubVFS\cache\{owner}_{repo}.json` | JSON | 仓库/文件数据 |
| INI 配置文件 | 双向 | `%APPDATA%\GitHubVFS\config.ini` | INI | 认证/代理/偏好 |
| 缓存失效 | 脚本 → DLL | 删除 JSON 缓存文件 + DOpus `Go REFRESH` | - | 通知 DLL 重新获取 |

### 10.2 为什么不需要信号文件

信号文件在早期设计中用于脚本→DLL 通信，但在当前架构下已无必要：

| 原信号用途 | 替代方案 | 理由 |
|------------|----------|------|
| `star_{owner}_{repo}.sig` | DLL 直接在 VFS_ContextVerbW 中调用 StarRepo API | DLL 已有 Star/Unstar/Watch/Unwatch API，右键菜单可直接触发 |
| `unstar_{owner}_{repo}.sig` | 同上 | 同上 |
| `watch_{owner}_{repo}.sig` | 同上 | 同上 |
| `unwatch_{owner}_{repo}.sig` | 同上 | 同上 |
| `refresh_{owner}_{repo}.sig` | 脚本删除缓存文件 + `Go REFRESH` | DLL 检测到缓存缺失时自动重新获取 |
| `invalidate.sig` | 脚本删除 `cache\*.json` + `Go REFRESH` | 同上 |

**关键变化**：VFS_GetContextMenuW / VFS_ContextVerbW 已可用，DLL 可直接处理所有核心操作（Star/Watch/Copy/Download/Refresh），无需脚本中转。脚本的高级命令（Clone、NewIssue、EditDesc 等）通过 gh CLI 执行后，只需删除缓存文件并触发刷新即可。

**脚本通知 DLL 刷新的流程**：
```
1. 脚本执行操作（如 gh repo clone facebook/react）
2. 脚本删除 %APPDATA%\GitHubVFS\cache\facebook_repos.json
3. 脚本调用 DOpus.Command.RunCommand("Go REFRESH")
4. DLL 在下次 VFS_ReadDirectoryW 时发现缓存缺失，自动重新请求 API
```

### 10.3 JSON 缓存文件格式

```json
{
  "_cached_at": "2026-05-11T10:30:00Z",
  "_cache_type": "repo",
  "id": 10270250,
  "name": "react",
  "full_name": "facebook/react",
  "description": "A JavaScript library for building UIs",
  "language": "JavaScript",
  "default_branch": "main",
  "clone_url": "https://github.com/facebook/react.git",
  "html_url": "https://github.com/facebook/react",
  "private": false,
  "fork": false,
  "archived": false,
  "stargazers_count": 223000,
  "forks_count": 18000,
  "open_issues_count": 523,
  "size": 204800,
  "license": { "spdx_id": "MIT" },
  "topics": ["javascript", "ui", "framework"],
  "updated_at": "2026-05-11T08:00:00Z",
  "created_at": "2013-05-24T00:00:00Z",
  "pushed_at": "2026-05-11T07:30:00Z"
}
```

### 10.4 INI 配置文件

```ini
[auth]
mode=token
token=ghp_xxxxxxxxxxxx
username=
password=

[network]
api_url=api.github.com
proxy_type=0
proxy_host=
proxy_port=0
conn_timeout=30

[cache]
timeout=300
items_per_page=30

[display]
show_forks=true
show_archived=true
show_private=true
show_description=true
repo_sort=0

[paths]
default_branch_name=auto
ref_keyword=@
```

---

## 11. 数据获取架构

### 11.1 架构图

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│   DOpus UI   │     │  GitHubVFS   │     │ GitHubClient │
│  (列/菜单)   │     │    (DLL)     │     │   (DLL内部)  │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                     │
       │ VFS_GetColumnData  │                     │
       │───────────────────>│                     │
       │                    │ GetRepoInfo()       │
       │                    │────────────────────>│
       │                    │                     │ WinHTTP
       │                    │                     │────────> api.github.com
       │                    │                     │<────────
       │                    │<────────────────────│
       │                    │                     │
       │                    │ WriteRepoCache()    │
       │                    │────────────────────>│
       │                    │                     │ → JSON文件
       │<───────────────────│                     │
       │                    │                     │
                                                          ┌──────────────┐
                                                          │GitHubVFS.js  │
                                                          │  (脚本)      │
                                                          └──────┬───────┘
                                                                 │
                                                                 │ LoadRepoCache()
                                                                 │ → 读JSON文件
                                                                 │
                                                                 │ RunGhCommand()
                                                                 │ → gh CLI (fallback)
                                                                 │
                                                                 │ 计算创造数据
                                                                 │ → 填充脚本列
```

### 11.2 数据流规则

```
1. DLL 是唯一与 GitHub API 通信的角色
2. DLL 枚举目录后写入 JSON 缓存
3. 脚本读取 JSON 缓存计算创造数据
4. 脚本不直接调用 GitHub API
5. 脚本可通过 gh CLI 作为 fallback（仅在缓存不可用时）
6. 脚本执行写操作后，删除缓存文件 + Go REFRESH 通知 DLL 刷新
```

### 11.3 认证机制

```
支持的认证方式：
  1. Personal Access Token（推荐）
     - 在 github.com/settings/tokens 创建
     - 权限：repo, read:org, notifications
     - 配置：右键 → 配置认证

  2. OAuth Device Flow（二期）
     - 自动化流程，无需手动创建 Token
     - DLL 实现 StartOAuthDeviceFlow / PollOAuthToken

  3. Basic Auth（不推荐）
     - 用户名 + 密码
     - 仅用于测试

认证请求头：
  Token:    Authorization: token ghp_xxxxxxxxxxxx
  OAuth:    Authorization: Bearer gho_xxxxxxxxxxxx
  Basic:    Authorization: Base64(username:password)
```

---

## 12. 分页设计

### 12.1 问题

```
GitHub API 每页最多返回 100 条
facebook/react 有 1000+ open issues
一次只看到前 100 个
```

### 12.2 方案：虚拟翻页

```
github://facebook/react/Issues/
├── #1 Bug in login
├── #2 Feature request
├── ...（100条）
├── ─────────────────
└── ▶ 第2页.../                    ← 虚拟目录

双击 ▶ 第2页.../ →
github://facebook/react/Issues/page:2/
├── #101 Another bug
├── #102 Fix typo
├── ...（100条）
├── ◀ 第1页/                       ← 返回上一页
└── ▶ 第3页.../                    ← 下一页
```

### 12.3 分页信息列

```
自定义列显示分页信息：
  第1页 / 共12页 (1,180条)

翻页文件属性：
  ◀ 第1页/       → 目录 → 导航到 page:1
  ▶ 第2页.../    → 目录 → 导航到 page:2
```

### 12.4 适用范围

| 区域 | 需要分页 | 每页数量 |
|------|----------|----------|
| repos/ | 是 | 30（默认） |
| Issues/ | 是 | 30 |
| Pulls/ | 是 | 30 |
| 搜索结果 | 是 | 30 |
| Notifications/ | 是 | 30 |
| starred/ | 是 | 30 |
| Branches/ | 通常不需要 | 100 |
| Tags/ | 通常不需要 | 100 |
| Releases/ | 通常不需要 | 100 |

---

## 13. 缓存与监控设计

### 13.1 缓存策略

```
三层缓存：

1. DLL 内存缓存（最快）
   - s_dirCache：目录文件列表
   - s_repoCache：仓库信息
   - s_searchCache：搜索结果
   - 超时：config.cacheTimeout（默认300秒）

2. DLL 磁盘缓存（跨会话）
   - %APPDATA%\GitHubVFS\cache\*.json
   - 写入时机：枚举目录后
   - 读取时机：脚本读取

3. 脚本内存缓存（会话内）
   - 解析后的 JSON 数据
   - 避免重复读取磁盘
```

### 13.2 缓存状态指示

```
自定义列"缓存"显示数据新鲜度：

名称          │ ... │ 缓存
──────────────────────────
facebook/react │ ... │ 5分钟前 ✓
torvalds/linux │ ... │ 2小时前 ⚠

✓ = 新鲜（< 缓存超时）
⚠ = 过期（> 缓存超时）
🔄 = 正在刷新
```

### 13.3 API 用量监控

```
右键（任意位置）→ 📊 API 用量：
  剩余：4,231 / 5,000
  重置时间：47分钟后
  本会话请求：769

自动节流：
  剩余 < 100 → 列显示 ⚠ 警告
  剩余 < 10 → 自动降级为纯缓存模式
  剩余 = 0 → 显示错误提示，等待重置

API 速率限制：
  认证用户：5,000 次/小时
  未认证：60 次/小时
```

### 13.4 速率限制处理

**响应头读取**：

```
X-RateLimit-Limit: 5000        # 每小时限制
X-RateLimit-Remaining: 4231    # 剩余次数
X-RateLimit-Reset: 1715678900  # 重置时间戳（Unix）
```

**实现要点**：

1. **每次 API 响应后更新**：
```cpp
void UpdateRateLimit(const std::map<std::string, std::string>& headers) {
    auto it = headers.find("x-ratelimit-remaining");
    if (it != headers.end()) {
        s_rateLimitRemaining = std::stoi(it->second);
    }
    it = headers.find("x-ratelimit-reset");
    if (it != headers.end()) {
        s_rateLimitReset = std::stoll(it->second);
    }
}
```

2. **请求前检查**：
```cpp
bool CheckRateLimit() {
    if (s_rateLimitRemaining < 10) {
        // 显示警告 Toast
        ShowToast(L"⚠ API 配额即将耗尽");
        return false;
    }
    return true;
}
```

3. **达到限制时的处理**：
```cpp
void OnRateLimitExceeded() {
    time_t now = time(nullptr);
    time_t resetTime = s_rateLimitReset;
    int waitMinutes = (resetTime - now) / 60 + 1;
    
    std::wstring msg = L"API 配额已用尽，" + std::to_wstring(waitMinutes) + L" 分钟后重置";
    ShowErrorFile(msg);
}
```

### 13.5 错误码定义

```cpp
enum GitHubErrorCode {
    GITHUB_ERR_NONE = 0,           // 成功
    
    // 网络错误 (1-99)
    GITHUB_ERR_NETWORK = 1,        // 网络连接失败
    GITHUB_ERR_TIMEOUT = 2,        // 请求超时
    GITHUB_ERR_DNS = 3,            // DNS 解析失败
    GITHUB_ERR_SSL = 4,            // SSL/TLS 错误
    
    // 认证错误 (100-199)
    GITHUB_ERR_AUTH = 100,         // 认证失败（Token 无效）
    GITHUB_ERR_TOKEN_EXPIRED = 101, // Token 过期
    GITHUB_ERR_PERMISSION = 102,   // 权限不足
    
    // API 错误 (200-299)
    GITHUB_ERR_RATE_LIMIT = 200,   // 速率限制
    GITHUB_ERR_NOT_FOUND = 201,    // 资源不存在（404）
    GITHUB_ERR_VALIDATION = 202,   // 参数验证失败（422）
    GITHUB_ERR_SERVER = 203,       // 服务器错误（5xx）
    GITHUB_ERR_SERVICE_UNAVAIL = 204, // 服务不可用（503）
    
    // 数据错误 (300-399)
    GITHUB_ERR_PARSE = 300,        // JSON 解析失败
    GITHUB_ERR_EMPTY = 301,        // 空结果
    GITHUB_ERR_INVALID = 302,      // 数据格式无效
    
    // 用户操作错误 (400-499)
    GITHUB_ERR_CANCELLED = 400,    // 用户取消
    GITHUB_ERR_ABORTED = 401,      // 操作中止
};

struct GitHubError {
    GitHubErrorCode code;
    std::wstring message;
    int httpStatus;
    std::wstring details;
    
    GitHubError() : code(GITHUB_ERR_NONE), httpStatus(0) {}
    
    bool IsSuccess() const { return code == GITHUB_ERR_NONE; }
    bool IsNetworkError() const { return code >= 1 && code < 100; }
    bool IsAuthError() const { return code >= 100 && code < 200; }
    bool IsApiError() const { return code >= 200 && code < 300; }
};
```

**错误显示策略**：

| 错误类型 | 显示方式 | 示例 |
|----------|----------|------|
| 网络错误 | Toast + 重试按钮 | "网络连接失败，点击重试" |
| 认证错误 | 配置对话框 | "Token 无效，请重新配置" |
| 速率限制 | Toast + 倒计时 | "API 配额已用尽，47分钟后重置" |
| 资源不存在 | 虚拟文件 | "⚠ 仓库不存在.txt" |
| 服务器错误 | Toast | "GitHub 服务暂时不可用" |

### 13.6 Gists API 细节

**列表获取**：
```
GET /gists                          ← 当前用户的 Gist 列表
GET /users/{username}/gists         ← 指定用户的公开 Gist
GET /gists/public                   ← 公开 Gist（分页）

响应字段：
  id, description, public, files, created_at, updated_at, html_url
```

**创建 Gist**：
```
POST /gists
{
  "description": "示例 Gist",
  "public": true,
  "files": {
    "hello.py": { "content": "print('Hello')" }
  }
}

返回：完整的 Gist 对象（含 id、html_url）
```

**编辑 Gist**：
```
PATCH /gists/{gist_id}
{
  "description": "更新后的描述",
  "files": {
    "hello.py": { "content": "print('Updated')" },
    "new_file.py": { "content": "# 新文件" }
  }
}
```

**删除 Gist**：
```
DELETE /gists/{gist_id}
返回：204 No Content
```

**获取 Gist 内容**：
```
GET /gists/{gist_id}
响应包含 files 对象，每个文件有 raw_url 或 content 字段
```

### 13.7 Notifications API 细节

**列表获取**：
```
GET /notifications                          ← 当前用户的所有通知
GET /repos/{owner}/{repo}/notifications     ← 指定仓库的通知

参数：
  all=true|false        ← 包含已读通知（默认 false）
  participating=true|false  ← 仅包含用户参与的通知
  since=ISO8601         ← 指定时间之后的通知
  before=ISO8601        ← 指定时间之前的通知

响应字段：
  id, unread, reason, updated_at
  subject: { title, url, type, latest_comment_url }
  repository: { full_name, ... }

reason 可能值：
  "subscribed"     ← 关注了仓库
  "mention"        ← 被 @ 提及
  "author"         ← 是 Issue/PR 作者
  "comment"        ← 评论了 Issue/PR
  "review_requested" ← 被请求 Review
```

**标记已读**：
```
PATCH /notifications/threads/{thread_id}
返回：205 Reset Content

批量标记已读：
PUT /notifications
{
  "last_read_at": "2026-05-13T00:00:00Z"
}
```

**订阅/取消订阅**：
```
PUT /repos/{owner}/{repo}/subscription
{
  "subscribed": true,
  "ignored": false
}

DELETE /repos/{owner}/{repo}/subscription
```

### 13.8 用户主页 API 细节

**获取用户信息**：
```
GET /users/{username}

响应字段：
  login, name, bio, company, location, email, blog
  avatar_url, html_url
  followers, following, public_repos, public_gists
  created_at, updated_at
  type: "User" | "Organization"
```

**获取用户仓库列表**：
```
GET /users/{username}/repos

参数：
  type=owner|member|all      ← 仓库类型
  sort=created|updated|pushed|full_name  ← 排序
  direction=asc|desc
  per_page=1..100
  page=1..

响应：GitHubRepoInfo 数组
```

**获取当前用户信息**（用于根目录显示）：
```
GET /user
返回当前认证用户的完整信息
```

### 13.9 日志系统设计

**日志文件位置**：
```
%APPDATA%\GitHubVFS\logs\githubvfs_{date}.log
```

**日志级别**：
```cpp
enum LogLevel {
    LOG_DEBUG = 0,    // 调试信息（仅开发模式）
    LOG_INFO = 1,     // 常规操作
    LOG_WARN = 2,     // 警告（可恢复的错误）
    LOG_ERROR = 3,    // 错误（影响功能）
    LOG_FATAL = 4     // 致命错误（插件崩溃）
};
```

**日志格式**：
```
[2026-05-13 10:30:45.123] [INFO]  ParseGitHubPath: "github://facebook/react/"
[2026-05-13 10:30:45.456] [DEBUG] API Request: GET /repos/facebook/react
[2026-05-13 10:30:45.789] [WARN]  Rate limit low: 45 remaining
[2026-05-13 10:30:46.012] [ERROR] Network timeout: api.github.com
```

**配置项**：
```ini
[logging]
level=1              ; 0=Debug, 1=Info, 2=Warn, 3=Error
max_files=10         ; 最多保留日志文件数
max_size_mb=5        ; 单文件最大大小
```

### 13.10 配置项完整说明

```ini
[auth]
mode=token           ; 认证模式：token | oauth | none
token=               ; GitHub Personal Access Token
username=            ; 用户名（OAuth 模式）
password=            ; 密码（OAuth 模式，不推荐）

[network]
api_url=api.github.com     ; API 服务器地址
proxy_type=0               ; 代理类型：0=无, 1=HTTP, 2=SOCKS5
proxy_host=                ; 代理服务器地址
proxy_port=0               ; 代理端口
conn_timeout=30            ; 连接超时（秒）
read_timeout=60            ; 读取超时（秒）

[cache]
timeout=300                ; 缓存过期时间（秒）
items_per_page=30          ; 每页显示数量
max_memory_mb=50           ; 内存缓存最大大小

[display]
show_forks=true            ; 显示 Fork 仓库
show_archived=true         ; 显示归档仓库
show_private=true          ; 显示私有仓库
show_description=true      ; 显示描述列
repo_sort=0                ; 排序：0=更新时间, 1=星标, 2=名称

[paths]
default_branch_name=auto   ; 默认分支名：auto | main | master
ref_keyword=@              ; 引用关键字

[logging]
level=1                    ; 日志级别
max_files=10               ; 最大日志文件数
max_size_mb=5              ; 单文件最大大小

[advanced]
offline_mode=false         ; 离线模式
auto_retry=3               ; 网络错误自动重试次数
retry_delay_ms=1000        ; 重试延迟（毫秒）
```

### 13.11 离线模式设计

**触发条件**：
1. 用户手动开启离线模式（配置项 `offline_mode=true`）
2. 网络不可用且缓存存在
3. API 速率限制耗尽

**行为策略**：

| 场景 | 行为 |
|------|------|
| 缓存存在 | 直接返回缓存数据，列显示"📋 缓存"标记 |
| 缓存不存在 | 显示虚拟文件"⚠ 离线模式 - 无缓存数据.txt" |
| 写操作 | 显示 Toast "离线模式下不可执行此操作" |

**缓存优先级**：
```
1. 内存缓存（最新）
2. 磁盘 JSON 缓存
3. 显示错误
```

**UI 指示**：
```
列显示：
  facebook/react/  │ ... │ 📋 缓存(2小时前)

状态栏（DOpus 信息栏）：
  📴 离线模式 | 缓存: 23 项
```

### 13.12 并发请求处理

**问题**：
- DOpus 可能同时枚举多个目录
- 每个 `VFS_ReadDirectoryW` 调用都会触发 API 请求
- 需要避免重复请求和并发冲突

**解决方案**：

```cpp
// 全局请求管理器
class GitHubRequestManager {
private:
    std::mutex m_mutex;
    std::map<std::wstring, RequestState> m_pendingRequests;
    
public:
    // 检查是否有相同请求正在进行
    bool IsPending(const std::wstring& cacheKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pendingRequests.count(cacheKey) > 0;
    }
    
    // 等待正在进行的请求完成
    void WaitForPending(const std::wstring& cacheKey) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_pendingRequests.count(cacheKey)) {
            // 等待条件变量
            m_pendingRequests[cacheKey].cv.wait(lock);
        }
    }
    
    // 标记请求开始
    void MarkStarted(const std::wstring& cacheKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[cacheKey] = RequestState();
    }
    
    // 标记请求完成
    void MarkCompleted(const std::wstring& cacheKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingRequests.count(cacheKey)) {
            m_pendingRequests[cacheKey].cv.notify_all();
            m_pendingRequests.erase(cacheKey);
        }
    }
};
```

**请求合并策略**：
```
场景：两个标签页同时打开 github://facebook/react/

1. 第一个请求开始，标记 cacheKey="facebook_repos" 为 pending
2. 第二个请求检测到 pending，等待
3. 第一个请求完成，写入缓存，通知等待者
4. 第二个请求从缓存读取数据
```

**并发限制**：
```cpp
const int MAX_CONCURRENT_REQUESTS = 5;  // 最大并发请求数

// 使用信号量控制并发
class ConcurrencyLimiter {
    HANDLE m_semaphore;
public:
    ConcurrencyLimiter() {
        m_semaphore = CreateSemaphore(nullptr, MAX_CONCURRENT_REQUESTS, MAX_CONCURRENT_REQUESTS, nullptr);
    }
    
    void Acquire() { WaitForSingleObject(m_semaphore, INFINITE); }
    void Release() { ReleaseSemaphore(m_semaphore, 1, nullptr); }
};
```

### 13.13 API 端点汇总表

| 功能 | 端点 | 方法 | 认证 |
|------|------|------|------|
| **仓库** ||||
| 获取仓库列表 | `/user/repos` | GET | ✓ |
| 获取用户仓库 | `/users/{username}/repos` | GET | 可选 |
| 获取仓库信息 | `/repos/{owner}/{repo}` | GET | 可选 |
| 获取目录内容 | `/repos/{owner}/{repo}/contents/{path}` | GET | 可选 |
| 获取文件内容 | `/repos/{owner}/{repo}/contents/{path}?ref={sha}` | GET | 可选 |
| **星标/关注** ||||
| 获取星标列表 | `/user/starred` | GET | ✓ |
| Star 仓库 | `/user/starred/{owner}/{repo}` | PUT | ✓ |
| Unstar 仓库 | `/user/starred/{owner}/{repo}` | DELETE | ✓ |
| 获取关注列表 | `/user/subscriptions` | GET | ✓ |
| Watch 仓库 | `/repos/{owner}/{repo}/subscription` | PUT | ✓ |
| Unwatch 仓库 | `/repos/{owner}/{repo}/subscription` | DELETE | ✓ |
| **搜索** ||||
| 搜索仓库 | `/search/repositories?q={query}` | GET | 可选 |
| 搜索代码 | `/search/code?q={query}` | GET | ✓ |
| 搜索用户 | `/search/users?q={query}` | GET | 可选 |
| 搜索 Issues | `/search/issues?q={query}` | GET | 可选 |
| 搜索 Commits | `/search/commits?q={query}` | GET | ✓ |
| 搜索 Topics | `/search/topics?q={query}` | GET | 可选 |
| **Issues/PRs** ||||
| 获取 Issues | `/repos/{owner}/{repo}/issues` | GET | 可选 |
| 获取 Issue | `/repos/{owner}/{repo}/issues/{number}` | GET | 可选 |
| 创建 Issue | `/repos/{owner}/{repo}/issues` | POST | ✓ |
| 关闭/重开 Issue | `/repos/{owner}/{repo}/issues/{number}` | PATCH | ✓ |
| 获取 PRs | `/repos/{owner}/{repo}/pulls` | GET | 可选 |
| 合并 PR | `/repos/{owner}/{repo}/pulls/{number}/merge` | PUT | ✓ |
| **分支/标签/发布** ||||
| 获取分支列表 | `/repos/{owner}/{repo}/branches` | GET | 可选 |
| 获取标签列表 | `/repos/{owner}/{repo}/tags` | GET | 可选 |
| 获取发布列表 | `/repos/{owner}/{repo}/releases` | GET | 可选 |
| 下载发布资产 | `/repos/{owner}/{repo}/releases/assets/{id}` | GET | 可选 |
| **Gists** ||||
| 获取 Gist 列表 | `/gists` | GET | ✓ |
| 获取 Gist | `/gists/{gist_id}` | GET | 可选 |
| 创建 Gist | `/gists` | POST | ✓ |
| 编辑 Gist | `/gists/{gist_id}` | PATCH | ✓ |
| 删除 Gist | `/gists/{gist_id}` | DELETE | ✓ |
| **通知** ||||
| 获取通知列表 | `/notifications` | GET | ✓ |
| 标记已读 | `/notifications/threads/{id}` | PATCH | ✓ |
| 批量标记已读 | `/notifications` | PUT | ✓ |
| **用户** ||||
| 获取当前用户 | `/user` | GET | ✓ |
| 获取用户信息 | `/users/{username}` | GET | 可选 |
| **Trending** ||||
| 获取 Trending | 第三方 API 或爬虫 | GET | 无 |

---

## 14. 数据结构定义

### 14.1 GitHubPathInfo（重构后）

```cpp
struct GitHubPathInfo {
    std::wstring owner;
    std::wstring repo;
    std::wstring path;           // 仓库内文件路径
    std::wstring searchQuery;    // 搜索关键词
    std::wstring searchType;     // 搜索类型：Repos/Code/Users/Issues/Commits/Topics
    std::wstring ref;
    GitHubRefType refType;       // DEFAULT / BRANCH / TAG / SHA
    GitHubMetaType metaType;     // 仅用于 Issues/Pulls/Branches/Tags/Releases
    std::wstring metaItem;
    int issueNumber;
    int page;                    // 分页页码
    std::wstring trendingSince;  // Trending 时间范围：daily/weekly/monthly
    std::wstring trendingLang;   // Trending 语言筛选
    std::vector<GitHubPathFilter> filters;

    GitHubContext context;       // ROOT | REPOS | SEARCH_REPOS | ...
    bool isDir;
};
```

22 个布尔 → 1 个枚举 + 1 个布尔 + 2 个搜索/Trending 字段。

### 14.2 GitHubContext 枚举

```cpp
enum GitHubContext {
    GITHUB_CTX_ROOT = 0,

    // 根目录保留字（大写开头）
    GITHUB_CTX_REPOS,            // Repos/ — 我的仓库
    GITHUB_CTX_STARRED,          // Starred/ — 我的星标
    GITHUB_CTX_SUBSCRIPTIONS,    // Subscriptions/ — 我的关注
    GITHUB_CTX_NOTIFICATIONS,    // Notifications/ — 我的通知
    GITHUB_CTX_GISTS,            // Gists/ — 我的 Gist
    GITHUB_CTX_TRENDING,         // Trending/ — 趋势

    // 搜索中心
    GITHUB_CTX_SEARCH_CENTER,    // Search/ — 搜索中心页（6种搜索入口）
    GITHUB_CTX_SEARCH_REPOS,     // Search/Repos/{query}/
    GITHUB_CTX_SEARCH_CODE,      // Search/Code/{query}/
    GITHUB_CTX_SEARCH_USERS,     // Search/Users/{query}/
    GITHUB_CTX_SEARCH_ISSUES,    // Search/Issues/{query}/
    GITHUB_CTX_SEARCH_COMMITS,   // Search/Commits/{query}/
    GITHUB_CTX_SEARCH_TOPICS,    // Search/Topics/{query}/

    // 用户/仓库
    GITHUB_CTX_OWNER,            // {owner}/ — 用户仓库列表
    GITHUB_CTX_REPO,             // {owner}/{repo}/ — 仓库根目录
    GITHUB_CTX_CODE,             // {owner}/{repo}/Code/ — 纯代码
    GITHUB_CTX_ISSUES,           // {owner}/{repo}/Issues/
    GITHUB_CTX_PULLS,            // {owner}/{repo}/Pulls/
    GITHUB_CTX_BRANCHES,         // {owner}/{repo}/Branches/
    GITHUB_CTX_TAGS,             // {owner}/{repo}/Tags/
    GITHUB_CTX_RELEASES          // {owner}/{repo}/Releases/
};
```

变更说明：
- 移除 `GITHUB_CTX_SEARCH_REPOS` 旧含义（原为 `repos/{query}/`），统一到 `Search/` 命名空间
- 新增 `GITHUB_CTX_SEARCH_CENTER`（搜索中心页）
- 新增 `GITHUB_CTX_TRENDING` 替代旧 `GITHUB_CTX_TRENDING`（增加语言筛选支持）
- 所有搜索上下文统一以 `GITHUB_CTX_SEARCH_` 前缀

### 14.3 GitHubMetaType（精简后）

```cpp
enum GitHubMetaType {
    GITHUB_META_NONE = 0,
    GITHUB_META_ISSUES,
    GITHUB_META_ISSUE_DETAIL,
    GITHUB_META_PULLS,
    GITHUB_META_PULL_DETAIL,
    GITHUB_META_BRANCHES,
    GITHUB_META_BRANCH_VIEW,
    GITHUB_META_TAGS,
    GITHUB_META_TAG_VIEW,
    GITHUB_META_RELEASES,
    GITHUB_META_AT_COMMIT
};
```

30 个 → 11 个。

### 14.4 新增数据结构

```cpp
struct GitHubCommitSearchItem {
    std::wstring sha;
    std::wstring message;
    std::wstring author;
    std::wstring date;
    std::wstring owner;
    std::wstring repo;
    std::wstring htmlUrl;
};

struct GitHubCommitSearchResult {
    std::vector<GitHubCommitSearchItem> items;
    int totalCount;
    bool incomplete;
};

struct GitHubTopicInfo {
    std::wstring name;
    std::wstring description;
    int repoCount;
};

struct GitHubTopicSearchResult {
    std::vector<GitHubTopicInfo> items;
    int totalCount;
    bool incomplete;
};

struct GitHubNotificationInfo {
    int id;
    std::wstring reason;
    std::wstring subjectType;    // Issue / PullRequest / Commit
    std::wstring subjectTitle;
    int subjectNumber;
    std::wstring owner;
    std::wstring repo;
    std::wstring updatedAt;
    bool unread;
};

struct GitHubGistInfo {
    std::wstring id;
    std::wstring description;
    bool isPublic;
    int fileCount;
    std::vector<std::wstring> filenames;
    std::wstring updatedAt;
    std::wstring htmlUrl;
};
```

### 14.5 保留的现有数据结构

```cpp
struct GitHubRepoInfo {
    std::wstring name;
    std::wstring fullName;
    std::wstring description;
    std::wstring language;
    std::wstring defaultBranch;
    std::wstring cloneUrl;
    std::wstring htmlUrl;
    bool isPrivate;
    bool isFork;
    bool isArchived;
    int stargazersCount;
    int forksCount;
    int openIssuesCount;
    uint64_t size;
    FILETIME updatedAt;
    FILETIME createdAt;
    FILETIME pushedAt;
};

struct GitHubFileInfo {
    std::wstring name;
    std::wstring path;
    std::wstring sha;
    std::wstring downloadUrl;
    std::wstring htmlUrl;
    bool isDir;
    uint64_t size;
    FILETIME updatedAt;
};

struct GitHubUserInfo {
    std::wstring login;
    std::wstring name;
    std::wstring bio;
    std::wstring company;
    std::wstring location;
    std::wstring email;
    std::wstring avatarUrl;
    std::wstring htmlUrl;
    int followers;
    int following;
    int publicRepos;
    int publicGists;
    bool isSiteAdmin;
    FILETIME createdAt;
};

struct GitHubIssueInfo {
    int number;
    std::wstring title;
    std::wstring body;
    std::wstring state;
    std::wstring author;
    std::wstring authorAvatar;
    std::vector<std::wstring> labels;
    int comments;
    std::wstring owner;
    std::wstring repo;
    FILETIME createdAt;
    FILETIME updatedAt;
    FILETIME closedAt;
};

struct GitHubPullInfo {
    int number;
    std::wstring title;
    std::wstring body;
    std::wstring state;
    std::wstring author;
    std::wstring authorAvatar;
    std::wstring headBranch;
    std::wstring baseBranch;
    std::vector<std::wstring> labels;
    int comments;
    std::wstring owner;
    std::wstring repo;
    FILETIME createdAt;
    FILETIME updatedAt;
    FILETIME closedAt;
    FILETIME mergedAt;
};

struct GitHubBranchInfo {
    std::wstring name;
    std::wstring sha;
    std::wstring committer;
    FILETIME date;
    bool isDefault;
};

struct GitHubTagInfo {
    std::wstring name;
    std::wstring sha;
};

struct GitHubReleaseInfo {
    std::wstring tagName;
    std::wstring name;
    std::wstring body;
    bool isDraft;
    bool isPrerelease;
    std::wstring htmlUrl;
    FILETIME publishedAt;
    int assetCount;
    std::vector<GitHubAssetInfo> assets;
};

struct GitHubAssetInfo {
    std::wstring name;
    uint64_t size;
    std::wstring downloadUrl;
    int downloadCount;
};
```

---

## 15. 保留字清单

> **v3.1 更新**：统一大写开头、Search 命名空间、移除 IsOwnerSubdir。

### 15.1 根目录保留字（显示在根目录）

| 保留字 | 用途 | 子路径 | 冲突风险 |
|--------|------|--------|----------|
| `Repos` | 我的仓库 | 无（搜索入口在目录内） | 零 |
| `Starred` | 我的星标 | 无 | 极低 |
| `Subscriptions` | 我的关注 | 无 | 零（已验证无冲突） |
| `Notifications` | 我的通知 | 无 | 极低 |
| `Gists` | 我的 Gist | `{gist_id}/` | 极低 |
| `Trending` | 趋势 | `{daily\|weekly\|monthly}/{language}/` | 极低 |
| `Search` | 搜索中心 | `Search/{Type}/{query}/` | 极低 |

### 15.2 搜索子类型保留字（Search/ 下的 seg1）

| 保留字 | 用途 | API 端点 |
|--------|------|----------|
| `Repos` | 仓库搜索 | `/search/repositories` |
| `Code` | 代码搜索 | `/search/code` |
| `Users` | 用户搜索 | `/search/users` |
| `Issues` | Issue & PR 搜索 | `/search/issues` |
| `Commits` | 提交搜索 | `/search/commits` |
| `Topics` | 主题搜索 | `/search/topics` |

### 15.3 仓库级保留字

| 保留字 | 用途 | 冲突处理 |
|--------|------|----------|
| `Code` | 纯代码目录 | 虚拟入口优先，真实同名目录通过 Code/ 访问 |
| `Issues` | Issue 列表 | 同上 |
| `Pulls` | PR 列表 | 同上 |
| `Branches` | 分支列表 | 同上 |
| `Tags` | 标签列表 | 同上 |
| `Releases` | Release 列表 | 同上 |

### 15.4 冲突处理

```
保留字与用户名冲突：
  大写开头保留字（Repos/Starred/...）与 GitHub 用户名几乎不冲突
  GitHub 用户名不允许大写开头（实际允许但不常见）
  如果冲突，保留字优先，用户通过右键菜单或搜索中心访问

虚拟目录与真实目录同名：
  虚拟入口优先，真实内容通过 Code/ 访问

  示例：
    仓库中恰好有一个名为 "Issues" 的真实目录：
    github://o/r/Issues/    → 虚拟入口（Issue 列表）
    github://o/r/Code/Issues/ → 真实目录

判断优先级：
  1. 精确匹配保留字（大写开头）→ 功能入口
  2. 其他 → owner/repo 路径
```

### 15.5 已移除的保留字

| 移除项 | 原因 |
|--------|------|
| `repos`（小写） | 统一大写开头为 `Repos` |
| `starred`（小写） | 统一大写开头为 `Starred` |
| `watching`（小写） | 改为 `Subscriptions`（避免与 GitHub 用户 `watching` 冲突） |
| `notifications`（小写） | 统一大写开头为 `Notifications` |
| `gists`（小写） | 统一大写开头为 `Gists` |
| `trending`（小写） | 统一大写开头为 `Trending` |
| `code`（小写，根级搜索） | 移入 Search 命名空间 |
| `users`（小写，根级搜索） | 移入 Search 命名空间 |
| `issues`（小写，根级搜索） | 移入 Search 命名空间 |
| `commits`（小写，根级搜索） | 移入 Search 命名空间 |
| `topics`（小写，根级搜索） | 移入 Search 命名空间 |
| `explore` | 不必要的中转层 |
| `mine` | 不必要的中转层 |
| `go` | `@{ref}` 语法替代 |
| `search` | `Search/` 命名空间替代 |

---

## 16. 一期/二期划分

### 16.1 一期（核心重构）

| 类别 | 内容 |
|------|------|
| 路径体系 | 扁平化根目录、去掉 .gh/、Code/ 分区、@{ref} 语法、去掉 search/ 前缀 |
| 搜索 | 6 种搜索类型、搜索对话框、限定符支持、双击直达 |
| 列显示 | 按上下文切换列集、README 预览列、缓存状态列、状态图标 |
| 交互 | 右键菜单编辑、批量操作、分页、快速跳转 |
| 监控 | API 用量、缓存新鲜度 |
| 新增区域 | Notifications、Gists、用户主页 |
| 通信 | JSON 缓存、INI 配置、缓存失效通知 |
| 脚本 | 创造数据列、高级命令 |

### 16.2 二期（增强功能）

| 类别 | 内容 |
|------|------|
| 文件操作 | 上传文件、新建文件、编辑文件、删除文件、重命名 |
| 拖放 | 从 VFS 拖到本地 = 下载，从本地拖到 VFS = 上传 |
| 颜色编码 | 私有仓库红色、归档灰色、已星标金色 |
| 路径自动补全 | 输入 github:// 后自动补全 |
| OAuth Device Flow | 自动化认证流程 |
| 地址栏智能搜索 | 输入非保留字自动搜索仓库 |
| 趋势详情 | trending/ 支持按语言、时间范围筛选 |
| 高级搜索 | 搜索结果排序、保存搜索条件 |
| 仓库对比 | 比较两个仓库的星标/活跃度/健康度 |
| 通知管理 | 通知筛选、批量操作、Webhook 订阅 |

---

## 17. 实施计划

> 最后更新: 2026-05-11

### 阶段1：需求分析 ✅ 已完成

已完成。所有需求通过多轮讨论确认。

### 阶段2：方案设计 ⚠ 设计完成，待实施

| 任务 | 产出 | 状态 |
|------|------|------|
| 2.1 重构 GitHubPathInfo | GitHubContext 枚举替代布尔字段 | 📝 设计完成 |
| 2.2 定义 JSON 缓存文件格式 | 仓库/文件/搜索 JSON Schema | 📝 设计完成 |
| 2.3 设计脚本模块架构 | GitHubVFS.js 模块划分 | 📝 设计完成 |
| 2.4 定义脚本列的 value/sort/group 规则 | 每个创造数据列的三元组 | 📝 设计完成 |

### 阶段3：代码实现 🔴 未开始

#### 当前代码资产

| 文件 | 行数 | 已实现功能 |
|------|------|-----------|
| GitHubVFS.cpp | 3515 | 旧路径解析(ParseGitHubPath)、目录枚举(VFS_ReadDirectoryW)、基础右键菜单(VFS_GetContextMenuW/VFS_ContextVerbW)、基础自定义列(VFS_GetCustomColumnsW)、文件读写 |
| GitHubClient.cpp | 1916 | WinHTTP 通信、Token 认证、Repo/Issue/PR/Branch/Tag/Release/Star/Watch API、代码搜索、用户搜索、缓存框架 |
| GitHubClient.h | 368 | 基础数据结构(GitHubRepoInfo/FileInfo/IssueInfo/PullInfo/BranchInfo/TagInfo/ReleaseInfo/UserInfo)、API 声明、缓存声明 |

#### 缺失的关键组件

| 组件 | 说明 |
|------|------|
| GitHubContext 枚举 | 替代当前 22 个布尔字段的上下文枚举 |
| GitHubCommitSearchItem/Result | 提交搜索数据结构 |
| GitHubTopicInfo/Result | 主题搜索数据结构 |
| GitHubNotificationInfo | 通知数据结构 |
| GitHubGistInfo | Gist 数据结构 |
| SearchIssues API | Issue 搜索 |
| SearchCommits API | 提交搜索 |
| SearchTopics API | 主题搜索 |
| ListNotifications API | 通知列表 |
| ListGists API | Gist 列表 |
| WriteRepoCache | JSON 缓存写入 |
| 分页支持 | page 参数 + 翻页文件 |
| GitHubVFS.js | 脚本（基础框架+数据读取+创造数据列+高级命令） |

#### 实施步骤

| 步骤 | 内容 | 预估改动 | 依赖 |
|------|------|----------|------|
| 3.1 DLL — 路径解析重构 | 重构 ParseGitHubPath、移除 .gh/、新增保留字识别、GitHubContext 枚举 | ~600行删，~300行改，~200行增 | 无 |
| 3.2 DLL — 右键菜单重构 | 按 GitHubContext 生成菜单、子菜单、搜索对话框 | ~300行增，~150行改 | 3.1 |
| 3.3 DLL — 自定义列扩展 | 按上下文返回列集、新增41列 | ~400行增，~100行改 | 3.1 |
| 3.4 DLL — JSON 缓存写入 | WriteRepoCache/SearchCache/DirCache | ~200行增 | 3.1 |
| 3.5 DLL — 新增搜索 API | SearchIssues/Commits/Topics、ListNotifications/Gists | ~300行增 | 无 |
| 3.6 DLL — 分页支持 | 翻页文件、page 参数 | ~150行增 | 3.1 |
| 3.7 脚本 — 基础框架 | OnInit、列注册、命令注册 | ~300行增 | 3.3, 3.4 |
| 3.8 脚本 — 数据读取层 | LoadCache、RunGh、InvalidateCache | ~250行增 | 3.4, 3.7 |
| 3.9 脚本 — 创造数据列 | 6个算法函数 | ~400行增 | 3.7, 3.8 |
| 3.10 脚本 — 高级命令 | 21个命令实现 | ~600行增 | 3.7, 3.8 |

#### 建议实施顺序

```
3.1 (路径解析) ──┬── 3.2 (右键菜单)
                 ├── 3.3 (自定义列) ──┐
                 ├── 3.4 (缓存写入) ──┼── 3.7 (脚本框架) ──┬── 3.9 (创造数据列)
                 └── 3.6 (分页)       └── 3.8 (数据读取) ──┴── 3.10 (高级命令)
3.5 (搜索API) ───────────────────────── (独立，可并行)
```

### 阶段4：需求验证 🔴 未开始

| 测试项 | 验证方法 | 通过标准 |
|--------|----------|----------|
| 路径解析 | 导航到各种路径 | 新路径正确解析 |
| 右键菜单 | 右键点击各上下文 | 菜单项正确显示，子菜单正常 |
| DLL列 | 导航到各上下文 | 列数据正确填充 |
| 脚本列 | 导航到仓库列表 | 列异步填充，value/sort/group 正确 |
| JSON缓存 | 检查缓存文件 | 文件存在、JSON合法、数据完整 |
| 缓存失效 | 脚本删除缓存 + Go REFRESH | DLL 自动重新获取数据 |
| 搜索 | 6种搜索类型 | 搜索结果正确、双击直达 |
| 分页 | 大列表翻页 | 翻页文件正常、数据正确 |
| 固定/最近 | 根目录显示 | 固定和最近访问正确显示 |

### 阶段5：修复循环 🔴 未开始

- 修复验证中发现的所有问题
- 每个修复后重新验证相关功能
- 确保无回归

### 阶段6：测试通过 🔴 未开始

- 完整回归测试
- 编译 Release 版本
- 确认所有交付物完整

---

## 18. ParseGitHubPath 重构方案

> v3.1 新增章节，细化路径解析的决策树和代码改动。

### 18.1 当前代码问题

> **v3.1 更新**：以下问题已在代码中解决，此表保留作为历史记录。

| # | 问题 | 位置 | 影响 | 状态 |
|---|------|------|------|------|
| 1 | `IsRootKeyword` 使用小写匹配（`repos`/`starred`/...） | L262-266 | 与新大写开头规范不一致 | ✅ 已解决：改用 `IsRootReservedWord` 大写匹配 |
| 2 | `IsOwnerSubdir` 允许 `{owner}/repos/` 走搜索逻辑 | L268-271 | 语义矛盾 | ✅ 已解决：移除 `IsOwnerSubdir` |
| 3 | 搜索路径散落在根级（`code/`、`users/`、`issues/`） | L349-371 | 无 Search 命名空间 | ✅ 已解决：统一到 `Search/` 命名空间 |
| 4 | 缺少 `Search/` 命名空间解析 | 全局 | 无法解析 `Search/Repos/{q}/` | ✅ 已解决：已实现 |
| 5 | 缺少 `Trending/` 语言筛选解析 | 全局 | 无法解析 `Trending/daily/javascript/` | ✅ 已解决：已实现 |
| 6 | `@{ref}` 统一设为 BRANCH，不区分 SHA/Tag | L449-459 | UI 信息不准确 | ✅ 已解决：`DetectRefType()` 自动检测 |
| 7 | `GitHubPathInfo` 缺少 `searchType`/`trendingSince`/`trendingLang` 字段 | L68-82 | 无法承载新路径信息 | ✅ 已解决：已添加字段 |
| 8 | `isGlobal` 字段语义不清 | L81 | 与新保留字体系不匹配 | ✅ 已解决：已移除 |

### 18.2 待解决问题

| # | 问题 | 说明 | 建议 |
|---|------|------|------|
| 1 | 虚拟入口统计信息不完整 | Pulls/Branches/Tags/Releases 显示 "-" | 补充 API 调用获取数量 |
| 2 | JSON 缓存写入功能缺失 | 脚本无法读取 DLL 数据 | 实现 `WriteRepoCache()` |
| 3 | Trending API 无官方支持 | GitHub 无 Trending API | 使用第三方库或爬虫 |
| 4 | DetectRefType 无法区分 Tag/Branch | 如 `v1.0.0` 可能是 Tag 或 Branch | API 验证或右键菜单切换 |
| 5 | 搜索重定向逻辑不完整 | Code/Commits/Topics 重定向未定义 | 补充重定向规则 |

### 18.2 重构后的决策树

```
ParseGitHubPath(pszPath)
│
├── rest 为空 → ROOT, isDir=true
│
├── seg0 命中 IsRootReservedWord()（大写开头）
│   │
│   ├── seg0 == "Repos"          → REPOS, isDir=true
│   │   └── segs.size() >= 2     → searchQuery=seg1, 解析 page
│   │
│   ├── seg0 == "Starred"        → STARRED, isDir=true
│   ├── seg0 == "Subscriptions"  → SUBSCRIPTIONS, isDir=true
│   ├── seg0 == "Notifications"  → NOTIFICATIONS, isDir=true
│   ├── seg0 == "Gists"          → GISTS, isDir=true
│   │   └── segs.size() >= 2     → metaItem=seg1 (gist_id)
│   │
│   ├── seg0 == "Trending"       → TRENDING, isDir=true
│   │   ├── segs.size() >= 2
│   │   │   ├── seg1 ∈ {daily,weekly,monthly} → trendingSince=seg1
│   │   │   └── seg1 为其他值    → trendingSince="daily", trendingLang=seg1
│   │   └── segs.size() >= 3     → trendingLang=seg2
│   │
│   └── seg0 == "Search"         → 进入 Search 子树
│       │
│       ├── segs.size() == 1     → SEARCH_CENTER, isDir=true
│       │
│       └── segs.size() >= 2     → seg1 为搜索类型
│           ├── seg1 == "Repos"   → SEARCH_REPOS, searchType="Repos"
│           ├── seg1 == "Code"    → SEARCH_CODE, searchType="Code"
│           ├── seg1 == "Users"   → SEARCH_USERS, searchType="Users"
│           ├── seg1 == "Issues"  → SEARCH_ISSUES, searchType="Issues"
│           ├── seg1 == "Commits" → SEARCH_COMMITS, searchType="Commits"
│           ├── seg1 == "Topics"  → SEARCH_TOPICS, searchType="Topics"
│           └── seg1 为其他       → SEARCH_CENTER, isDir=true
│           │
│           └── segs.size() >= 3 → searchQuery=seg2, 解析 page
│               │
│               └── 搜索结果重定向（seg3+ 存在时）
│                   ├── Search/Repos/{q}/{owner}/{repo}/...
│                   │   → 重定向为 owner=seg3, repo=seg4, context=REPO
│                   ├── Search/Users/{q}/{login}/
│                   │   → 重定向为 owner=seg3, context=OWNER
│                   └── 其他搜索类型暂不处理重定向
│
├── seg0 未命中保留字 → 进入 owner/repo 子树
│   │
│   ├── owner=seg0, context=OWNER, isDir=true
│   │
│   └── segs.size() >= 2
│       │
│       ├── seg1 命中 IsRepoReservedWord()
│       │   │  （Code/Issues/Pulls/Branches/Tags/Releases）
│       │   │
│       │   ├── seg1 == "Code"     → CODE, isDir=true
│       │   │   └── path = seg2 + "/" + ...（纯代码路径）
│       │   │
│       │   ├── seg1 == "Issues"   → ISSUES, isDir=true
│       │   │   └── seg2 为数字   → issueNumber=seg2
│       │   │
│       │   ├── seg1 == "Pulls"    → PULLS, isDir=true
│       │   │   └── seg2 为数字   → issueNumber=seg2
│       │   │
│       │   ├── seg1 == "Branches" → BRANCHES, isDir=true
│       │   │   └── ref=seg2, refType=BRANCH, path=seg3+...
│       │   │
│       │   ├── seg1 == "Tags"     → TAGS, isDir=true
│       │   │   └── ref=seg2, refType=TAG, path=seg3+...
│       │   │
│       │   └── seg1 == "Releases" → RELEASES, isDir=true
│       │
│       ├── seg1 以 "@" 开头      → @{ref} 语法
│       │   ├── ref = seg1 去掉 "@"
│       │   ├── DetectRefType(ref) → refType
│       │   │   ├── 40位hex → SHA
│       │   │   ├── 7位hex  → SHA
│       │   │   └── 默认    → BRANCH
│       │   ├── context = CODE, isDir=true
│       │   └── path = seg2 + "/" + ...
│       │
│       └── 其他 → repo=seg1, context=REPO, isDir=true
│           │
│           └── segs.size() >= 3
│               ├── seg2 命中 IsRepoReservedWord() → 同上仓库子目录逻辑
│               ├── seg2 以 "@" 开头               → 同上 @{ref} 逻辑
│               └── 其他 → context=CODE, path=seg2+...
```

### 18.3 新增/修改的辅助函数

| 函数 | 变更 | 说明 |
|------|------|------|
| `IsRootReservedWord()` | **替换** `IsRootKeyword()` | 大写开头匹配：Repos/Starred/Subscriptions/Notifications/Gists/Trending/Search |
| `IsSearchType()` | **新增** | 匹配搜索子类型：Repos/Code/Users/Issues/Commits/Topics |
| `IsRepoReservedWord()` | **保留** | 不变：Code/Issues/Pulls/Branches/Tags/Releases |
| `IsOwnerSubdir()` | **删除** | 移除，`{owner}/` 下不再有子目录保留字 |
| `DetectRefType()` | **新增** | 检测 ref 是 SHA(40位)/短SHA(7位)/Branch/Tag |
| `IsHexString()` | **新增** | 判断字符串是否为十六进制 |
| `ParseTrendingSegs()` | **新增** | 解析 Trending 的时间范围和语言参数 |

### 18.4 DetectRefType 实现

```cpp
static bool IsHexString(const std::wstring& s) {
    for (wchar_t c : s) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F')))
            return false;
    }
    return true;
}

static GitHubRefType DetectRefType(const std::wstring& ref) {
    if (ref.length() == 40 && IsHexString(ref))
        return GITHUB_REF_SHA;
    if (ref.length() == 7 && IsHexString(ref))
        return GITHUB_REF_SHA;
    return GITHUB_REF_BRANCH;
}
```

### 18.5 GitHubPathInfo 结构体变更

```cpp
struct GitHubPathInfo {
    std::wstring owner;
    std::wstring repo;
    std::wstring path;
    std::wstring searchQuery;
    std::wstring searchType;       // 新增：Repos/Code/Users/Issues/Commits/Topics
    std::wstring ref;
    GitHubRefType refType;
    int issueNumber;
    int page;
    std::wstring trendingSince;    // 新增：daily/weekly/monthly
    std::wstring trendingLang;     // 新增：语言名
    GitHubContext context;
    bool isDir;
    // 移除：isGlobal（不再需要）

    GitHubPathInfo() {
        refType = GITHUB_REF_DEFAULT;
        issueNumber = 0;
        page = 1;
        context = GITHUB_CTX_ROOT;
        isDir = false;
    }
};
```

---

## 19. ReadDirectory 改动方案

> v3.1 新增章节，细化各上下文的目录枚举逻辑。

### 19.1 ReadRootDirectory（GITHUB_CTX_ROOT）

**当前**：显示 11 个小写保留字目录

**改为**：

```
1. 固定仓库（📌 前缀，从 pinned.json 加载，最多10个）
2. 分隔线
3. 最近访问（🕐 前缀，从 recent.json 加载，最多10个）
4. 分隔线
5. Repos/        ← 大写开头
6. Starred/
7. Subscriptions/
8. Notifications/
9. Gists/
10. Trending/
11. Search/      ← 新增搜索中心入口
```

### 19.2 ReadSearchCenterDirectory（GITHUB_CTX_SEARCH_CENTER）— 新增

```
1. Repos/     ← 双击弹出仓库搜索对话框
2. Code/      ← 双击弹出代码搜索对话框
3. Users/     ← 双击弹出用户搜索对话框
4. Issues/    ← 双击弹出 Issue 搜索对话框
5. Commits/   ← 双击弹出提交搜索对话框
6. Topics/    ← 双击弹出主题搜索对话框
```

每个子目录双击时触发搜索对话框（InputBox），输入关键词后跳转 `Search/{Type}/{query}/`。

### 19.3 ReadReposDirectory（GITHUB_CTX_REPOS）

**当前**：`repos/` 有 query 时搜索，无 query 时显示我的仓库

**改为**：

```
1. 🔍 搜索仓库.../     ← 虚拟目录，双击弹出搜索对话框（新增）
2. 我的仓库列表         ← 从 ListUserRepos() 获取
```

注意：`Repos/` 不再承担搜索功能，搜索统一走 `Search/Repos/{query}/`。

### 19.4 ReadTrendingDirectory（GITHUB_CTX_TRENDING）

**当前**：只支持 `daily/weekly/monthly` 三级

**改为**：

```
trendingSince 为空（Trending/）：
  1. daily/       ← 今日趋势
  2. weekly/      ← 本周趋势
  3. monthly/     ← 本月趋势

trendingSince 有值，trendingLang 为空（Trending/daily/）：
  1. javascript/  ← 语言列表（从 GitHub Popular Languages 获取）
  2. python/
  3. rust/
  ...（可分页）
  4. ▶ 第2页.../

trendingSince 和 trendingLang 都有值（Trending/daily/javascript/）：
  → 调用 GitHub Trending API，返回仓库列表
```

### 19.5 ReadRepoDirectory（GITHUB_CTX_REPO）

**当前**：只显示 API 返回的真实文件

**改为**：混合显示虚拟入口 + 真实文件

```
1. 添加虚拟入口（带统计信息）：
   - Issues/     类型列: "📋 {openIssuesCount} open"
   - Pulls/      类型列: "🔀 {openPullsCount} open"
   - Branches/   类型列: "🌿 {branchCount}"
   - Tags/       类型列: "🏷 {tagCount}"
   - Releases/   类型列: "📦 {releaseCount}"

2. 遍历 API 返回的真实文件列表：
   - 文件名命中 IsRepoReservedWord() → 跳过（已被虚拟入口占据）
   - 其他 → 正常添加
```

### 19.6 ReadSearchResultDirectory（GITHUB_CTX_SEARCH_*）

**当前**：`SEARCH_CODE`/`SEARCH_USERS`/`SEARCH_ISSUES` 各自独立

**改为**：统一通过 `searchType` 字段分发

```
switch (info.context) {
    case GITHUB_CTX_SEARCH_REPOS:   → GitHubClient::SearchRepos(query, page)
    case GITHUB_CTX_SEARCH_CODE:    → GitHubClient::SearchCode(query, page)
    case GITHUB_CTX_SEARCH_USERS:   → GitHubClient::SearchUsers(query, page)
    case GITHUB_CTX_SEARCH_ISSUES:  → GitHubClient::SearchIssues(query, page)   // 新增
    case GITHUB_CTX_SEARCH_COMMITS: → GitHubClient::SearchCommits(query, page)  // 新增
    case GITHUB_CTX_SEARCH_TOPICS:  → GitHubClient::SearchTopics(query, page)   // 新增
}
```

### 19.7 ReadOwnerDirectory（GITHUB_CTX_OWNER）

**当前**：`{owner}/repos/` 走搜索逻辑

**改为**：`{owner}/` 只显示该用户的公开仓库列表

```
1. 调用 GitHubClient::ListUserRepos(owner) 获取用户公开仓库
2. 显示仓库列表（与 Repos/ 格式一致）
3. 不再有子目录保留字（移除 IsOwnerSubdir）
```

---

## 20. 完整改动清单与实施顺序

> v3.1 新增章节，替代旧 §17.3 中的实施步骤。

### 20.1 代码改动清单

| # | 文件 | 改动类型 | 具体内容 | 行数估算 |
|---|------|----------|----------|----------|
| 1 | GitHubVFS.cpp | 删除 | `IsRootKeyword()` 函数 | -5 |
| 2 | GitHubVFS.cpp | 删除 | `IsOwnerSubdir()` 函数 | -4 |
| 3 | GitHubVFS.cpp | 新增 | `IsRootReservedWord()` 函数（大写开头匹配） | +8 |
| 4 | GitHubVFS.cpp | 新增 | `IsSearchType()` 函数 | +6 |
| 5 | GitHubVFS.cpp | 新增 | `IsHexString()` 函数 | +6 |
| 6 | GitHubVFS.cpp | 新增 | `DetectRefType()` 函数 | +8 |
| 7 | GitHubVFS.cpp | 修改 | `GitHubPathInfo` 结构体：新增 searchType/trendingSince/trendingLang，删除 isGlobal | +3/-1 |
| 8 | GitHubVFS.cpp | 重写 | `ParseGitHubPath()` 函数（按 §18.2 决策树） | ~200行重写 |
| 9 | GitHubVFS.cpp | 修改 | `ReadRootDirectory`：大写开头保留字 + Search/ 入口 | ~30行改 |
| 10 | GitHubVFS.cpp | 新增 | `ReadSearchCenterDirectory`：搜索中心页枚举 | ~40行 |
| 11 | GitHubVFS.cpp | 修改 | `ReadReposDirectory`：添加搜索入口虚拟目录 | ~15行 |
| 12 | GitHubVFS.cpp | 修改 | `ReadTrendingDirectory`：支持语言筛选 | ~50行改 |
| 13 | GitHubVFS.cpp | 修改 | `ReadRepoDirectory`：混合虚拟入口+真实文件+统计列 | ~60行改 |
| 14 | GitHubVFS.cpp | 修改 | `ReadSearchResultDirectory`：统一 searchType 分发 + 新增3种搜索 | ~80行改 |
| 15 | GitHubVFS.cpp | 修改 | `ReadOwnerDirectory`：移除 IsOwnerSubdir，只显示仓库列表 | ~20行改 |
| 16 | GitHubVFS.cpp | 修改 | `VFS_ReadDirectoryW`：新增 SEARCH_CENTER/TRENDING/SEARCH_REPOS 等 case | ~30行 |
| 17 | GitHubVFS.cpp | 修改 | `VFS_GetCustomColumnsW`：新增虚拟入口统计列 | ~40行 |
| 18 | GitHubVFS.cpp | 修改 | `VFS_GetContextMenuW`：Search 中心双击搜索对话框 | ~30行 |
| 19 | GitHubVFS.cpp | 修改 | `VFS_ContextVerbW`：搜索对话框跳转逻辑 | ~20行 |
| 20 | GitHubClient.h | 修改 | `GitHubContext` 枚举：新增 SEARCH_CENTER/SEARCH_REPOS 等 | +5/-0 |
| 21 | GitHubClient.h | 新增 | `GitHubCommitSearchItem/Result` 数据结构 | +20 |
| 22 | GitHubClient.h | 新增 | `GitHubTopicInfo/Result` 数据结构 | +15 |
| 23 | GitHubClient.cpp | 新增 | `SearchIssues()` API | +40 |
| 24 | GitHubClient.cpp | 新增 | `SearchCommits()` API | +40 |
| 25 | GitHubClient.cpp | 新增 | `SearchTopics()` API | +40 |
| 26 | GitHubClient.cpp | 新增 | `ListUserRepos(owner)` 重载（指定 owner） | +30 |

### 20.2 实施顺序

```
阶段 A：数据结构 + 路径解析（无 UI 影响，纯逻辑层）
  A1. GitHubClient.h — 更新 GitHubContext 枚举
  A2. GitHubVFS.cpp — 更新 GitHubPathInfo 结构体
  A3. GitHubVFS.cpp — 新增辅助函数（IsRootReservedWord/IsSearchType/IsHexString/DetectRefType）
  A4. GitHubVFS.cpp — 重写 ParseGitHubPath()
  → 验证：单元测试各路径解析结果

阶段 B：目录枚举（UI 可见）
  B1. GitHubVFS.cpp — 修改 ReadRootDirectory
  B2. GitHubVFS.cpp — 新增 ReadSearchCenterDirectory
  B3. GitHubVFS.cpp — 修改 ReadReposDirectory（搜索入口）
  B4. GitHubVFS.cpp — 修改 ReadTrendingDirectory（语言筛选）
  B5. GitHubVFS.cpp — 修改 ReadRepoDirectory（虚拟入口+统计列）
  B6. GitHubVFS.cpp — 修改 ReadOwnerDirectory（移除 IsOwnerSubdir）
  B7. GitHubVFS.cpp — 修改 ReadSearchResultDirectory（统一分发）
  → 验证：导航到各路径，确认目录内容正确

阶段 C：交互 + 列 + 菜单
  C1. GitHubVFS.cpp — 修改 VFS_GetCustomColumnsW（虚拟入口统计列）
  C2. GitHubVFS.cpp — 修改 VFS_GetContextMenuW（搜索对话框）
  C3. GitHubVFS.cpp — 修改 VFS_ContextVerbW（搜索跳转）
  → 验证：右键菜单、列显示、搜索对话框

阶段 D：新增搜索 API（独立，可与 A/B 并行）
  D1. GitHubClient.h — 新增数据结构
  D2. GitHubClient.cpp — 新增 SearchIssues/SearchCommits/SearchTopics
  D3. GitHubClient.cpp — 新增 ListUserRepos(owner) 重载
  → 验证：API 调用返回正确数据
```

### 20.3 验证清单

| # | 验证项 | 操作 | 预期结果 |
|---|--------|------|----------|
| 1 | 根目录 | 导航 `github://` | 显示 7 个大写开头目录 + 固定/最近 |
| 2 | 搜索中心 | 导航 `github://Search/` | 显示 6 个搜索类型子目录 |
| 3 | 仓库搜索 | 导航 `github://Search/Repos/react/` | 显示搜索结果 |
| 4 | 代码搜索 | 导航 `github://Search/Code/useEffect/` | 显示搜索结果 |
| 5 | 用户搜索 | 导航 `github://Search/Users/torvalds/` | 显示搜索结果 |
| 6 | Issue搜索 | 导航 `github://Search/Issues/bug/` | 显示搜索结果 |
| 7 | 提交搜索 | 导航 `github://Search/Commits/fix/` | 显示搜索结果 |
| 8 | 主题搜索 | 导航 `github://Search/Topics/ml/` | 显示搜索结果 |
| 9 | 搜索重定向 | 导航 `github://Search/Repos/react/facebook/react/` | 重定向到 `github://facebook/react/` |
| 10 | 我的仓库 | 导航 `github://Repos/` | 显示搜索入口 + 我的仓库列表 |
| 11 | 仓库根目录 | 导航 `github://facebook/react/` | 显示虚拟入口+真实文件 |
| 12 | 虚拟入口统计 | 查看 Issues/ 的类型列 | 显示 "📋 523 open" |
| 13 | 纯代码 | 导航 `github://facebook/react/Code/` | 只显示真实文件 |
| 14 | @{ref} SHA | 导航 `github://o/r/@abc1234/` | refType=SHA |
| 15 | @{ref} 分支 | 导航 `github://o/r/@main/` | refType=BRANCH |
| 16 | Trending | 导航 `github://Trending/` | 显示 daily/weekly/monthly |
| 17 | Trending+语言 | 导航 `github://Trending/daily/javascript/` | 显示 JS 趋势仓库 |
| 18 | 用户仓库 | 导航 `github://torvalds/` | 只显示 torvalds 的仓库 |
| 19 | 旧路径兼容 | 导航 `github://repos/`（小写） | 不再识别为保留字，当作 owner |
| 20 | 搜索对话框 | 双击 Search/ 下的子目录 | 弹出搜索输入框 |

---

## 附录 A：讨论决策索引

本文档整合了以下多轮讨论的结论：

| 讨论主题 | 关键决策 | 对应章节 |
|----------|----------|----------|
| 自定义列设计 | 六大设计范式（聚合/状态机/对比/信号灯/路径感知/渐进式） | §6.2 |
| DLL vs 脚本职责 | DLL=核心+原始数据，脚本=创造数据+高级命令 | §3.1 |
| 右键菜单归属 | DLL=核心操作，脚本=高级命令，缓存失效通知 DLL | §3.4, §7.3 |
| 路径体系简化 | 去掉 .gh/、explore/、mine/、go/，扁平化 | §5.1-5.7 |
| Issue/PR 合并 | open/closed 合并，自定义列区分状态 | §5.7 |
| Code/ 分区 | 仓库根=Code+虚拟入口，Code/=纯代码 | §5.4 |
| 搜索类型 | 6种搜索（仓库/代码/用户/Issue/提交/主题） | §8.1 |
| 搜索路径 | ~~去掉 Search 前缀~~ → v3.1: Search 命名空间统一搜索 | §5.1, §8.1 |
| 列显示+菜单编辑 | 只读=列，可写=列+右键编辑，少用文件打开 | §2.1 |
| DLL-脚本通信 | JSON缓存+INI配置+缓存失效通知 | §10 |
| 数据获取 | DLL 为唯一数据源，脚本 gh CLI fallback | §11 |
| 认证机制 | Token 认证（推荐），OAuth Device Flow（二期） | §11.3 |
| 分页 | 虚拟翻页目录（▶ 第N页.../） | §12 |
| 固定+最近 | 根目录显示固定仓库和最近访问 | §9.1 |
| 保留字 | 根目录7个（大写开头）+ 仓库级6个 | §15 |
| Search 命名空间 | 6个搜索类型统一在 Search/ 下，根级保留字 11→7 | §5.1, §8.5, §18.2 |
| @{ref} 自动检测 | SHA(40位/7位)自动识别，默认 BRANCH | §5.5, §18.4 |
| Trending 语言筛选 | 支持 {since}/{language}/ 二级筛选 | §5.8, §19.4 |
| 虚拟入口统计列 | Issues/Pulls 等显示 open 数量 | §5.4, §19.5 |
| Repos 内搜索入口 | Repos/ 首项为搜索对话框虚拟目录 | §5.7, §19.3 |
| 移除 IsOwnerSubdir | {owner}/ 下不再有子目录保留字，只显示仓库列表 | §5.9, §19.7 |

## 附录 B：文件清单

| 文件 | 说明 |
|------|------|
| `GitHubVFS完整设计文档.md` | 本文档，整合所有讨论结论 |
| `自定义列方案.md` | 自定义列详细设计（六大范式、算法、数据流） |
| `重构设计文档.md` | 重构设计详细方案 |
| `P0-技术方案-动作文件与操作路径目录.md` | 动作文件技术方案（已被右键菜单替代） |

> **注意**：`P0-技术方案-动作文件与操作路径目录.md` 中描述的动作文件机制（.gh/star/、.gh/download-zip/ 等）已被右键菜单体系替代，不再实施。该文档仅作为历史参考保留。

---

## 21. Owner 扁平化方案（v3.2）

> 版本: 3.2  
> 日期: 2026-05-14  
> 状态: 方案设计完成  
> 目标: 减少目录层级，将 Owner 信息统一通过自定义列展示

### 21.1 需求背景

当前目录结构中，Owner 信息通过目录层级展示，导致：

1. **层级过深**：`github://Search/Repos/{query}/{owner}/{repo}/` 需要 5 层才能到达仓库
2. **操作繁琐**：用户需要多次点击才能找到目标仓库
3. **信息分散**：Owner 信息在路径中，无法在列表中直接比较

### 21.2 需求清单

| 序号 | 需求描述 |
|------|---------|
| 1 | `github://Search/Repos/` 显示所有仓库，包含「所有者」自定义列 |
| 2 | 移除 `github://Search/Repos/{owner}` 路径，不再按 owner 分组 |
| 3 | `github://Search/Owner/` 显示 owner 名称列表（无子目录） |
| 4 | 移除 `github://Search/Owner/{owner}` 路径 |
| 5 | 双击 `github://Search/Owner/{username}` → 跳转到 `github://Search/Repos?owner={username}` |
| 6 | `github://Repos/` 扁平化，直接显示仓库列表（含所有者列） |
| 7 | `github://Subscriptions/` 扁平化，直接显示仓库列表（含所有者列） |
| 8 | `github://Trending/` 子路径：如有 owner 数据则显示，否则不显示 |
| 9 | `github://` 根目录添加「所有者」列 |
| 10 | URL 参数 `?owner=xxx` 支持过滤 |

### 21.3 路径结构变更

#### 变更前

```
github://Search/Repos/{query}/
├── facebook/              ← owner 分组
│   ├── react/            ← 仓库
│   └── jest/
├── vuejs/
│   └── vue/
└── ...

github://Repos/
├── facebook/              ← owner 分组
│   ├── react/
│   └── jest/
└── ...
```

#### 变更后

```
github://Search/Repos/{query}/
├── react/                 ← 仓库（平铺）
├── jest/
├── vue/
└── ...
（所有者列显示：facebook, facebook, vuejs）

github://Search/Owner/
├── facebook/              ← owner 列表（无子目录）
├── vuejs/
└── ...
（双击跳转到 Search/Repos?owner=xxx）

github://Repos/
├── react/                 ← 仓库（平铺）
├── jest/
└── ...
（所有者列显示：facebook, facebook）
```

### 21.4 Owner 列显示规则

| 路径 | Owner 列 | 说明 |
|------|---------|------|
| `github://` (根目录) | ✅ 显示 | 根目录显示固定/最近仓库时包含所有者 |
| `github://Repos/` | ✅ 显示 | 直接显示仓库列表 |
| `github://Subscriptions/` | ✅ 显示 | 直接显示仓库列表 |
| `github://Trending/` | ❌ 不显示 | Trending 列表无 owner 信息 |
| `github://Trending/daily/` | ❌ 不显示 | 日榜无 owner 信息 |
| `github://Trending/weekly/` | ✅ 显示（如有） | 周榜可能包含 owner |
| `github://Trending/monthly/` | ✅ 显示（如有） | 月榜可能包含 owner |
| `github://Search/Repos/` | ✅ 显示 | 显示所有仓库 |
| `github://Search/Repos?owner=xxx` | ✅ 显示（已过滤） | URL 参数过滤 |
| `github://Search/Owner/` | ❌ 不显示 | 显示 owner 名称列表 |

### 21.5 URL 参数过滤机制

#### 参数解析

```cpp
// 在 ParseGitHubPath 中解析 URL 参数
// 示例: github://Search/Repos?owner=facebook

std::wstring ownerFilter;  // 新增字段

// 解析逻辑
size_t queryPos = path.find(L'?');
if (queryPos != std::wstring::npos) {
    std::wstring queryString = path.substr(queryPos + 1);
    // 解析 owner=xxx 参数
    // 支持 URL 编码
}
```

#### 过滤实现

```cpp
// ReadSearchReposDirectory 中
if (!pathInfo.ownerFilter.empty()) {
    // 过滤只显示该 owner 的仓库
    for (const auto& repo : searchResult.repos) {
        std::wstring owner = ExtractOwner(repo.fullName);
        if (_wcsicmp(owner.c_str(), pathInfo.ownerFilter.c_str()) == 0) {
            filtered.push_back(repo);
        }
    }
}
```

### 21.6 旧路径重定向（向后兼容）

| 旧路径 | 重定向目标 |
|-------|-----------|
| `github://Search/Repos/{query}/{owner}/` | `github://Search/Repos/{query}/`（忽略 owner 层级） |
| `github://Search/Owner/{owner}/` | `github://Search/Repos?owner={owner}` |
| `github://Repos/{owner}/` | `github://Repos/`（忽略 owner 层级） |
| `github://Subscriptions/{owner}/` | `github://Subscriptions/`（忽略 owner 层级） |

### 21.7 代码修改清单

| 序号 | 文件 | 修改内容 |
|------|------|---------|
| 1 | GitHubPathInfo 结构 | 添加 `ownerFilter` 字段 |
| 2 | ParseGitHubPath | 添加 URL 参数解析逻辑 |
| 3 | ReadSearchReposDirectory | 支持 ownerFilter 过滤 |
| 4 | ReadSearchOwnerDirectory | 新增函数（显示 owner 列表） |
| 5 | ReadSearchCenterDirectory | 添加 Owner 入口 |
| 6 | ReadReposDirectory | 移除 owner 分组，直接显示仓库 |
| 7 | ReadSubscriptionsDirectory | 移除 owner 分组，直接显示仓库 |
| 8 | ReadTrendingDirectory | 条件显示 Owner 列 |
| 9 | VFS_ContextVerbW | 添加 Owner 双击跳转逻辑 |
| 10 | InternalReadDirectory | 添加 GITHUB_CTX_SEARCH_OWNER case |
| 11 | VFS_GetFileInformationW | 添加对应处理 |

### 21.8 GitHubContext 枚举更新

```cpp
enum GitHubContext {
    // ... 现有值 ...
    
    // 新增
    GITHUB_CTX_SEARCH_OWNER,     // Search/Owner/ — owner 列表
    
    // 移除（不再需要）
    // GITHUB_CTX_REPOS_OWNER,    // Repos/{owner}/ — 已移除
    // GITHUB_CTX_SUBSCRIPTIONS_OWNER, // Subscriptions/{owner}/ — 已移除
};
```

### 21.9 测试计划

| 测试场景 | 预期结果 |
|---------|---------|
| `github://Search/Repos?owner=xxx` | 只显示该 owner 的仓库 |
| `github://Search/Owner/` 双击用户名 | 跳转到 `Search/Repos?owner=xxx` |
| `github://Repos/` | 显示所有仓库，含 Owner 列 |
| `github://Subscriptions/` | 显示所有仓库，含 Owner 列 |
| `github://Trending/daily` | 不显示 Owner 列 |
| `github://Trending/weekly` | 如有 owner 数据则显示 |
| 旧路径 `github://Repos/{owner}/` | 重定向到 `github://Repos/` |
| 旧路径 `github://Search/Owner/{user}/` | 重定向到 `Search/Repos?owner={user}` |

### 21.10 实施顺序

```
阶段 A：数据结构 + 路径解析
  A1. GitHubPathInfo 添加 ownerFilter 字段
  A2. ParseGitHubPath 添加 URL 参数解析
  A3. GitHubContext 枚举更新

阶段 B：目录枚举
  B1. ReadSearchReposDirectory 支持 owner 过滤
  B2. 新增 ReadSearchOwnerDirectory
  B3. ReadSearchCenterDirectory 添加 Owner 入口
  B4. ReadReposDirectory 移除 owner 分组
  B5. ReadSubscriptionsDirectory 移除 owner 分组
  B6. ReadTrendingDirectory 条件显示 Owner 列

阶段 C：交互
  C1. VFS_ContextVerbW 添加 Owner 双击跳转
  C2. 旧路径重定向处理

阶段 D：验证
  D1. 编译验证
  D2. 功能测试
```