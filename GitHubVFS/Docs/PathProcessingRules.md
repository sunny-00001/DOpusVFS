# GitHubVFS 路径处理规则详解

## 一、路径前缀和基础规则

### 1.1 路径前缀
- 所有路径以 `github:///` 开头
- 实际路径部分在 `github:///` 之后

### 1.2 URL 参数解析
- 支持 `?owner=xxx` 参数进行过滤
- 参数中的 `%20` 等编码会自动解码
- 示例：`github:///Repos?owner=microsoft`

### 1.3 分页语法
- 支持 `page:N` 格式进行分页
- 示例：`github:///Search/Repos/myrepo/page:2`

---

## 二、根目录保留字（大写开头）

### 2.1 Repos/ - 我的仓库
**路径模式**：
- `github:///Repos/` - 显示所有仓库列表（按 owner 分组）
- `github:///Repos/{owner}/` - 显示指定 owner 的仓库
- `github:///Repos/{repoName}/` - **扁平化模式**，直接显示仓库内容
- `github:///Repos/{repoName}/{path}` - 仓库内路径

**处理逻辑**：
1. 如果只有 `Repos/` → 显示所有仓库（按 owner 分组）
2. 如果 `Repos/{owner}/` → 显示该 owner 的所有仓库
3. 如果 `Repos/{repoName}/` → 从缓存中查找真实 owner，重定向到仓库上下文
4. 支持 `?owner=xxx` 参数过滤

**扁平化逻辑**：
- 当访问 `Repos/{repoName}/` 时，插件会自动从缓存中查找该仓库的真实 owner
- 如果找到，直接当作 `GITHUB_CTX_REPO` 处理
- 如果找不到，回退到 `GITHUB_CTX_REPOS_OWNER`（显示 owner 列表）

### 2.2 Starred/ - 星标仓库
**路径模式**：
- `github:///Starred/` - 显示所有星标仓库（按 owner 分组）
- `github:///Starred/{owner}/` - 显示指定 owner 的星标仓库
- `github:///Starred/{repoName}/` - **扁平化模式**，直接显示仓库内容
- `github:///Starred/{repoName}/{path}` - 仓库内路径

**处理逻辑**：与 Repos/ 类似，但数据来源是星标仓库列表

### 2.3 Subscriptions/ - 关注仓库
**路径模式**：
- `github:///Subscriptions/` - 显示所有关注的仓库（按 owner 分组）
- `github:///Subscriptions/{owner}/` - 显示指定 owner 的关注仓库
- `github:///Subscriptions/{repoName}/` - **扁平化模式**，直接显示仓库内容
- `github:///Subscriptions/{repoName}/{path}` - 仓库内路径

**处理逻辑**：与 Repos/ 类似，但数据来源是关注的仓库列表

### 2.4 Notifications/ - 通知
**路径模式**：
- `github:///Notifications/` - 显示通知列表

**处理逻辑**：
- 直接设置为 `GITHUB_CTX_NOTIFICATIONS`
- 显示用户的 GitHub 通知

### 2.5 Gists/ - Gist
**路径模式**：
- `github:///Gists/` - 显示所有 Gist 列表
- `github:///Gists/{gist_id}/` - 显示指定 Gist 的内容

**处理逻辑**：
- 如果只有 `Gists/` → 显示所有 Gist
- 如果 `Gists/{gist_id}/` → 显示该 Gist 的文件列表

### 2.6 Trending/ - 趋势
**路径模式**：
- `github:///Trending/` - 显示今日所有语言的趋势仓库
- `github:///Trending/{when}/` - 显示指定时间范围的趋势（daily/weekly/monthly）
- `github:///Trending/{lang}/` - 显示指定语言的今日趋势
- `github:///Trending/{when}/{lang}/` - 显示指定时间和语言的趋势

**参数**：
- `when`: `daily`, `weekly`, `monthly`
- `lang`: 编程语言名称（如 `python`, `javascript`, `go`）

**处理逻辑**：
1. 解析第一段 `seg1`
2. 如果 `seg1` 是时间范围（daily/weekly/monthly）→ 设置 `trendingSince`
3. 如果 `seg1` 不是时间范围 → 当作语言名，默认时间范围为 daily
4. 如果还有第三段 `seg2` 且 `trendingLang` 为空 → 设置为语言名

### 2.7 Search/ - 搜索中心
**路径模式**：
- `github:///Search/` - 显示搜索中心（包含所有搜索类型入口）
- `github:///Search/{type}/{query}/` - 执行指定类型的搜索

**搜索类型（type）**：
1. `Repos` - 仓库搜索
2. `Code` - 代码搜索
3. `Users` - 用户搜索
4. `Issues` - Issue 搜索
5. `Commits` - 提交搜索
6. `Topics` - 主题搜索
7. `Owner` - Owner 列表（用于过滤跳转）

#### 2.7.1 Search/Repos/ - 仓库搜索
**路径模式**：
- `github:///Search/Repos/{query}/` - 搜索仓库
- `github:///Search/Repos/{query}/{owner}/{repo}/` - **重定向到仓库上下文**
- `github:///Search/Repos/{query}/page:2` - 分页

**特殊逻辑**：
- 如果搜索结果中有 ` seg3/{owner}/{repo}/` → 自动重定向到 `GITHUB_CTX_REPO`
- 这意味着从搜索结果点击仓库，会直接进入仓库上下文

#### 2.7.2 Search/Code/ - 代码搜索
**路径模式**：
- `github:///Search/Code/{query}/` - 搜索代码

#### 2.7.3 Search/Users/ - 用户搜索
**路径模式**：
- `github:///Search/Users/{query}/` - 搜索用户
- `github:///Search/Users/{query}/{login}/` - **重定向到用户上下文**

**特殊逻辑**：
- 如果搜索结果中有 `{login}/` → 自动重定向到 `GITHUB_CTX_OWNER`

#### 2.7.4 Search/Issues/ - Issue 搜索
**路径模式**：
- `github:///Search/Issues/{query}/` - 搜索 Issue
- `github:///Search/Issues/{query}/{owner}/{repo}/{number}/` - **重定向到 Issue 详情**

**特殊逻辑**：
- 如果搜索结果中有 `{owner}/{repo}/{number}/` → 自动重定向到 `GITHUB_CTX_ISSUES` 并显示该 Issue 详情

#### 2.7.5 Search/Commits/ - 提交搜索
**路径模式**：
- `github:///Search/Commits/{query}/` - 搜索提交

#### 2.7.6 Search/Topics/ - 主题搜索
**路径模式**：
- `github:///Search/Topics/{query}/` - 搜索主题

#### 2.7.7 Search/Owner/ - Owner 列表
**路径模式**：
- `github:///Search/Owner/` - 显示所有 owner 列表
- `github:///Search/Owner/{username}/` - **重定向到该用户的仓库搜索**

**特殊逻辑**：
- `Search/Owner/{username}/` → 自动重定向到 `Search/Repos?owner={username}`
- 显示该 owner 的所有仓库

---

## 三、owner/repo 子树（小写开头）

### 3.1 owner 上下文
**路径模式**：
- `github:///{owner}/` - 显示该 owner 的所有仓库

**处理逻辑**：
- 设置为 `GITHUB_CTX_OWNER`
- 显示该用户的全部仓库

### 3.2 仓库上下文
**路径模式**：
- `github:///{owner}/{repo}/` - 显示仓库根目录（默认是 Code）
- `github:///{owner}/{repo}/{path}` - 仓库内文件路径

**处理逻辑**：
- 设置为 `GITHUB_CTX_REPO`
- 默认显示仓库根目录的文件列表
- `{path}` 可以是多层路径

---

## 四、仓库级保留字

### 4.1 Code/ - 代码
**路径模式**：
- `github:///{owner}/{repo}/Code/` - 显示代码根目录
- `github:///{owner}/{repo}/Code/{path}` - 代码文件路径
- `github:///Repos/{repoName}/Code/` - 扁平化模式下的代码目录
- `github:///Repos/{repoName}/Code/{path}` - 扁平化模式下的代码文件

**处理逻辑**：
- 设置为 `GITHUB_CTX_CODE`
- 显示指定路径下的文件和目录

### 4.2 Issues/ - 问题
**路径模式**：
- `github:///{owner}/{repo}/Issues/` - 显示 Issue 列表
- `github:///{owner}/{repo}/Issues/{number}/` - 显示指定 Issue 详情
- `github:///{owner}/{repo}/Issues/page:2` - 分页

**处理逻辑**：
- 设置为 `GITHUB_CTX_ISSUES`
- 如果有 `{number}` → 设置 `issueNumber`，显示该 Issue 详情
- 如果有 `page:N` → 设置 `page`，显示指定页

### 4.3 Pulls/ - 拉取请求
**路径模式**：
- `github:///{owner}/{repo}/Pulls/` - 显示 PR 列表
- `github:///{owner}/{repo}/Pulls/{number}/` - 显示指定 PR 详情
- `github:///{owner}/{repo}/Pulls/page:2` - 分页

**处理逻辑**：
- 设置为 `GITHUB_CTX_PULLS`
- 处理逻辑与 Issues 类似

### 4.4 Branches/ - 分支
**路径模式**：
- `github:///{owner}/{repo}/Branches/` - 显示分支列表
- `github:///{owner}/{repo}/Branches/{ref}/` - 显示指定分支的代码
- `github:///{owner}/{repo}/Branches/{ref}/{path}` - 指定分支下的文件路径

**处理逻辑**：
- 设置为 `GITHUB_CTX_BRANCHES`
- 设置 `ref = {ref}`
- 设置 `refType = GITHUB_REF_BRANCH`
- 显示该分支下的文件

### 4.5 Tags/ - 标签
**路径模式**：
- `github:///{owner}/{repo}/Tags/` - 显示标签列表
- `github:///{owner}/{repo}/Tags/{ref}/` - 显示指定标签的代码
- `github:///{owner}/{repo}/Tags/{ref}/{path}` - 指定标签下的文件路径

**处理逻辑**：
- 设置为 `GITHUB_CTX_TAGS`
- 设置 `ref = {ref}`
- 设置 `refType = GITHUB_REF_TAG`
- 显示该标签下的文件

### 4.6 Releases/ - 发布
**路径模式**：
- `github:///{owner}/{repo}/Releases/` - 显示发布列表

**处理逻辑**：
- 设置为 `GITHUB_CTX_RELEASES`
- 显示该仓库的所有 Release

---

## 五、特殊语法

### 5.1 @{ref} 语法 - 指定分支/标签/SHA
**路径模式**：
- `github:///{owner}/{repo}/@{ref}/` - 使用指定 ref 显示代码
- `github:///{owner}/{repo}/@{ref}/{path}` - 指定 ref 下的文件路径

**示例**：
- `github:///microsoft/vscode/@main/` - 显示 main 分支的代码
- `github:///microsoft/vscode/@v1.80.0/` - 显示 v1.80.0 标签的代码
- `github:///microsoft/vscode/@abc123def/` - 显示指定 commit SHA 的代码

**处理逻辑**：
1. 检测 `seg1` 或 `seg2` 是否以 `@` 开头
2. 提取 `@` 之后的部分作为 `ref`
3. 调用 `DetectRefType(ref)` 判断 ref 类型（分支/标签/SHA）
4. 设置为 `GITHUB_CTX_CODE`
5. 后续路径作为文件路径

**DetectRefType 逻辑**：
- 如果 `ref` 是 40 位十六进制 → `GITHUB_REF_COMMIT`（SHA）
- 如果 `ref` 包含 `/` → `GITHUB_REF_BRANCH`（可能是分支路径，如 `feature/xxx`）
- 其他 → `GITHUB_REF_BRANCH`（默认当作分支）

### 5.2 page:N 语法 - 分页
**路径模式**：
- 可以在任何列表页面使用 `page:N`
- 示例：`github:///{owner}/{repo}/Issues/page:2`

**处理逻辑**：
- 解析路径段，如果某个段匹配 `page:(\d+)`
- 提取数字 N，设置 `info.page = N`
- 显示第 N 页的结果

### 5.3 ?owner=xxx 语法 - URL 参数过滤
**路径模式**：
- `github:///Repos?owner=microsoft` - 只显示 microsoft 的仓库
- `github:///Starred?owner=microsoft` - 只显示 microsoft 的星标仓库

**处理逻辑**：
1. 在路径中查找 `?` 位置
2. 提取 `?` 之后的查询字符串
3. 解析 `owner=xxx` 参数
4. 设置 `info.ownerFilter = xxx`
5. 在后续处理中，只显示匹配该 owner 的仓库

**URL 解码**：
- 查询字符串中的 `%20` 等编码会自动解码为空格等字符

---

## 六、路径解析的完整流程

### 6.1 ParseGitHubPath 函数流程

```
输入：pszPath (如 L"github:///microsoft/vscode/Code/src/main.ts")

1. 检查路径前缀
   - 如果不是 github:/// 开头 → 返回空 info

2. 去除路径前缀，获取 rest
   - rest = L"microsoft/vscode/Code/src/main.ts"

3. 解析 URL 参数（如果有 ?）
   - 提取 ? 之后的查询字符串
   - 解析 owner=xxx 参数
   - 对参数值进行 URL 解码

4. 去除首尾的 /
   - rest = L"microsoft/vscode/Code/src/main.ts"

5. 如果 rest 为空 → 根目录上下文
   - context = GITHUB_CTX_ROOT

6. 分割路径为段数组 segs
   - segs = [L"microsoft", L"vscode", L"Code", L"src", L"main.ts"]

7. 检查第一段 seg0
   - 如果是根目录保留字（Repos/Starred/...）→ 进入根目录保留字处理
   - 如果是普通字符串 → 当作 owner，进入 owner/repo 处理

8. 根目录保留字处理
   - 根据保留字类型（Repos/Starred/...）设置对应 context
   - 如果是扁平化模式（segs.size() >= 2）
     - 从缓存中查找真实 owner
     - 如果找到 → 重定向到 GITHUB_CTX_REPO
     - 如果找不到 → 显示 owner 列表

9. owner/repo 处理
   - seg0 = owner
   - 设置 context = GITHUB_CTX_OWNER
   - 如果有 seg1
     - 如果 seg1 是仓库级保留字（Code/Issues/...）→ 进入仓库级保留字处理
     - 如果 seg1 以 @ 开头 → @{ref} 语法处理
     - 否则 → seg1 = repo，进入仓库上下文

10. 仓库级保留字处理
    - 根据保留字类型（Code/Issues/Branches/...）设置对应 context
    - 解析后续路径段作为文件路径或参数

11. @{ref} 语法处理
    - 提取 ref
    - 检测 ref 类型（分支/标签/SHA）
    - 设置 context = GITHUB_CTX_CODE
    - 解析后续路径段作为文件路径

12. 仓库上下文
    - context = GITHUB_CTX_REPO
    - 解析后续路径段作为文件路径

13. 返回 GitHubPathInfo 结构体
```

### 6.2 GitHubPathInfo 结构体字段

```cpp
struct GitHubPathInfo {
    GitHubContext context;      // 上下文类型（18种）
    std::wstring owner;         // owner 名称
    std::wstring repo;          // repo 名称
    std::wstring ref;           // 分支/标签/SHA
    GitHubRefType refType;      // ref 类型（分支/标签/SHA）
    std::wstring path;          // 文件路径
    std::wstring searchType;    // 搜索类型（Repos/Code/...）
    std::wstring searchQuery;   // 搜索查询字符串
    std::wstring ownerFilter;   // owner 过滤参数
    std::wstring trendingSince; // 趋势时间范围（daily/weekly/monthly）
    std::wstring trendingLang;  // 趋势编程语言
    std::wstring metaItem;      // 元数据包（如 gist_id）
    int issueNumber;             // Issue 或 PR 编号
    int page;                   // 分页页码
    bool isDir;                 // 是否是目录
};
```

---

## 七、上下文类型（GitHubContext 枚举）

### 7.1 全部 18 种上下文类型

1. **GITHUB_CTX_ROOT** - 根目录
2. **GITHUB_CTX_REPOS** - 我的仓库列表（按 owner 分组）
3. **GITHUB_CTX_REPOS_OWNER** - 我的仓库（指定 owner）
4. **GITHUB_CTX_STARRED** - 星标仓库列表（按 owner 分组）
5. **GITHUB_CTX_STARRED_OWNER** - 星标仓库（指定 owner）
6. **GITHUB_CTX_SUBSCRIPTIONS** - 关注仓库列表（按 owner 分组）
7. **GITHUB_CTX_SUBSCRIPTIONS_OWNER** - 关注仓库（指定 owner）
8. **GITHUB_CTX_NOTIFICATIONS** - 通知
9. **GITHUB_CTX_GISTS** - Gist 列表
10. **GITHUB_CTX_TRENDING** - 趋势
11. **GITHUB_CTX_SEARCH_CENTER** - 搜索中心
12. **GITHUB_CTX_SEARCH_REPOS** - 搜索仓库
13. **GITHUB_CTX_SEARCH_CODE** - 搜索代码
14. **GITHUB_CTX_SEARCH_USERS** - 搜索用户
15. **GITHUB_CTX_SEARCH_ISSUES** - 搜索 Issue
16. **GITHUB_CTX_SEARCH_COMMITS** - 搜索提交
17. **GITHUB_CTX_SEARCH_TOPICS** - 搜索主题
18. **GITHUB_CTX_SEARCH_OWNER** - 搜索 Owner

（注：还有 REPO、OWNER、CODE、ISSUES、PULLS、BRANCHES、TAGS、RELEASES 等上下文类型，实际总数超过 18 种）

---

## 八、重定向规则

### 8.1 扁平化重定向
- `Repos/{repoName}/` → `REPO` (如果找到真实 owner)
- `Starred/{repoName}/` → `REPO` (如果找到真实 owner)
- `Subscriptions/{repoName}/` → `REPO` (如果找到真实 owner)

### 8.2 搜索结果重定向
- `Search/Repos/{q}/{owner}/{repo}/` → `REPO`
- `Search/Users/{q}/{login}/` → `OWNER`
- `Search/Issues/{q}/{owner}/{repo}/{number}/` → `ISSUE_DETAIL`
- `Search/Owner/{username}/` → `Search/Repos?owner={username}`

---

## 九、辅助函数

### 9.1 IsRootReservedWord(seg)
- 判断段是否是根目录保留字
- 保留字：`Repos`, `Starred`, `Subscriptions`, `Notifications`, `Gists`, `Trending`, `Search`
- 判断规则：首字母大写

### 9.2 IsRepoSubdir(seg)
- 判断段是否是仓库级保留字
- 保留字：`Code`, `Issues`, `Pulls`, `Branches`, `Tags`, `Releases`
- 判断规则：首字母大写

### 9.3 ParsePageSegment(seg)
- 解析 `page:N` 语法
- 如果段匹配 `page:(\d+)` → 返回数字 N
- 否则 → 返回 0

### 9.4 DetectRefType(ref)
- 判断 @{ref} 中的 ref 类型
- 如果是 40 位十六进制 → `GITHUB_REF_COMMIT`
- 如果包含 `/` → `GITHUB_REF_BRANCH`
- 其他 → `GITHUB_REF_BRANCH`

### 9.5 SplitPath(path)
- 将路径按 `/` 分割为段数组
- 自动处理连续的 `/`
- 返回 `std::vector<std::wstring>`

---

## 十、典型路径示例

### 10.1 根目录
- `github:///` → 根目录

### 10.2 我的仓库
- `github:///Repos/` → 我的仓库列表
- `github:///Repos/microsoft/` → microsoft 的仓库
- `github:///Repos/vscode/` → 直接访问 vscode 仓库（扁平化）
- `github:///Repos/vscode/src/` → vscode 仓库的 src 目录（扁平化）

### 10.3 星标仓库
- `github:///Starred/` → 星标仓库列表
- `github:///Starred/vscode/` → 直接访问 vscode 仓库（扁平化）

### 10.4 趋势
- `github:///Trending/` → 今日所有语言趋势
- `github:///Trending/daily/` → 今日趋势
- `github:///Trending/weekly/python/` → Python 本周趋势

### 10.5 搜索
- `github:///Search/` → 搜索中心
- `github:///Search/Repos/vscode/` → 搜索仓库
- `github:///Search/Code/function/` → 搜索代码
- `github:///Search/Users/microsoft/` → 搜索用户

### 10.6 仓库访问
- `github:///microsoft/vscode/` → vscode 仓库根目录
- `github:///microsoft/vscode/README.md` → vscode 仓库的 README.md
- `github:///microsoft/vscode/@main/` → vscode 的 main 分支
- `github:///microsoft/vscode/@main/README.md` → main 分支的 README.md
- `github:///microsoft/vscode/Code/` → 代码目录
- `github:///microsoft/vscode/Issues/` → Issue 列表
- `github:///microsoft/vscode/Issues/123/` → Issue #123 详情
- `github:///microsoft/vscode/Branches/main/` → main 分支
- `github:///microsoft/vscode/Tags/v1.80.0/` → v1.80.0 标签

---

## 十一、复杂度来源总结

### 11.1 路径模式过多
- 根目录保留字：7 种
- 搜索类型：7 种
- 仓库级保留字：6 种
- 特殊语法：@{ref}, ?owner=xxx, page:N
- 扁平化模式：3 种（Repos/Starred/Subscriptions）

### 11.2 上下文类型过多
- 根目录相关：11 种
- 仓库相关：7 种
- 总计：18+ 种

### 11.3 重定向逻辑复杂
- 扁平化重定向：3 种
- 搜索结果重定向：4 种

### 11.4 代码重复
- 扁平化逻辑在 3 个地方重复（Repos/Starred/Subscriptions）
- 仓库级保留字逻辑在 2 个地方重复（owner/repo/ 和 owner/repo/Code/）
- @{ref} 语法在 2 个地方处理（seg1 和 seg2）

---

## 十二、简化建议

### 12.1 减少上下文类型
- 合并 REPOS_OWNER 和 REPOS
- 合并 STARRED_OWNER 和 STARRED
- 合并 SUBSCRIPTIONS_OWNER 和 SUBSCRIPTIONS

### 12.2 重构 ParseGitHubPath
- 拆分为多个子函数
- 使用查找表代替 if-else 链

### 12.3 统一路径语法
- 使用查询参数代替路径段
- 例如：`github:///Search?type=repos&q={query}`

### 12.4 移除扁平化逻辑
- 统一使用 `owner/repo/` 格式
- 简化路径解析

---

**生成时间**：2026-05-16
**文件位置**：D:\VFS\GitHubVFS\Docs\PathProcessingRules.md
