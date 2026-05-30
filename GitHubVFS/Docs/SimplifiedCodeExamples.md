# GitHubVFS 路径处理简化代码示例

本文档提供具体的代码重构示例，展示如何将复杂的 ParseGitHubPath 函数简化为多个子函数。

---

## 一、重构前：复杂的 ParseGitHubPath 函数

### 1.1 当前代码结构（427-969 行，共 542 行）

```cpp
// 当前实现：单个函数处理所有路径模式（542 行）
static GitHubPathInfo ParseGitHubPath(LPCWSTR pszPath) {
    GitHubPathInfo info;
    if (!pszPath || !IsGitHubVfsPath(pszPath)) return info;

    std::wstring rest = pszPath + GITHUB_VFS_PREFIX_LEN;
    while (!rest.empty() && rest.front() == L'/') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == L'/') rest.pop_back();

    // 解析 URL 参数 ?owner=xxx
    size_t queryPos = rest.find(L'?');
    if (queryPos != std::wstring::npos) {
        std::wstring queryString = rest.substr(queryPos + 1);
        rest = rest.substr(0, queryPos);
        // ... 30 行 URL 解码逻辑 ...
    }

    if (rest.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }

    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }

    const std::wstring& seg0 = segs[0];

    // ═══ 根目录保留字（大写开头）═══
    if (IsRootReservedWord(seg0)) {
        // Repos/ — 我的仓库（扁平化）
        if (seg0 == L"Repos") {
            if (segs.size() >= 2) {
                std::wstring repoName = segs[1];
                std::wstring realOwner;
                
                std::vector<GitHubRepoInfo> repos = GetCachedRepos();
                for (const auto& r : repos) {
                    if (_wcsicmp(r.name.c_str(), repoName.c_str()) == 0) {
                        // ... 20 行查找逻辑 ...
                    }
                }
                
                if (!realOwner.empty()) {
                    info.owner = realOwner;
                    info.repo = repoName;
                    info.context = GITHUB_CTX_REPO;
                    // ... 10 行路径解析 ...
                    return info;
                }
                info.owner = segs[1];
                info.context = GITHUB_CTX_REPOS_OWNER;
                return info;
            }
            info.context = GITHUB_CTX_REPOS;
            return info;
        }

        // Starred/ — 我的星标（扁平化）【重复代码】
        if (seg0 == L"Starred") {
            if (segs.size() >= 2) {
                std::wstring repoName = segs[1];
                std::wstring realOwner;
                
                std::vector<GitHubRepoInfo> starred = GetCachedStarredRepos();
                for (const auto& r : starred) {
                    if (_wcsicmp(r.name.c_str(), repoName.c_str()) == 0) {
                        // ... 20 行查找逻辑（与 Repos 重复）...
                    }
                }
                // ... 后续逻辑与 Repos 相同 ...
            }
        }

        // Subscriptions/ — 我的关注（扁平化）【重复代码】
        // ... 与 Repos/Starred 相同的逻辑 ...

        // Search/ — 搜索中心
        if (seg0 == L"Search") {
            // ... 60 行搜索处理逻辑 ...
        }

        // ... 其他根目录保留字处理 ...
    }

    // ═══ owner/repo 子树 ═══
    info.owner = seg0;
    info.context = GITHUB_CTX_OWNER;
    info.isDir = true;
    if (segs.size() == 1) return info;

    const std::wstring& seg1 = segs[1];

    // 仓库级保留字（在 seg1 位置）
    if (IsRepoSubdir(seg1)) {
        if (seg1 == L"Code") {
            // ... 10 行处理 ...
        }
        if (seg1 == L"Issues") {
            // ... 15 行处理 ...
        }
        // ... 其他仓库级保留字 ...
    }

    // @{ref} 语法（在 seg1 位置）
    if (seg1.length() > 1 && seg1[0] == L'@') {
        info.ref = seg1.substr(1);
        info.refType = DetectRefType(info.ref);
        // ... 10 行处理 ...
    }

    // 其他 → owner/repo 仓库根目录
    info.repo = seg1;
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    if (segs.size() == 2) return info;

    const std::wstring& seg2 = segs[2];

    // 仓库内的仓库级保留字（在 seg2 位置）【重复代码】
    if (IsRepoSubdir(seg2)) {
        // ... 与 seg1 位置相同的逻辑 ...
    }

    // seg2 以 @ 开头 → @{ref} 语法（在 seg2 位置）【重复代码】
    if (seg2.length() > 1 && seg2[0] == L'@') {
        // ... 与 seg1 位置相同的逻辑 ...
    }

    // 默认 → 仓库内文件路径
    info.context = GITHUB_CTX_CODE;
    info.path = seg2;
    for (size_t i = 3; i < segs.size(); i++)
        info.path += L"/" + segs[i];
    return info;
}
```

**问题总结**：
1. 函数过长（542 行）
2. 扁平化逻辑重复 3 次（Repos/Starred/Subscriptions）
3. 仓库级保留字逻辑重复 2 次（seg1 和 seg2）
4. @{ref} 语法重复 2 次（seg1 和 seg2）
5. 深层嵌套的 if-else

---

## 二、重构后：简化的子函数

### 2.1 主函数（简化版，约 80 行）

```cpp
// 重构后：主函数简化为路由逻辑（80 行）
static GitHubPathInfo ParseGitHubPath(LPCWSTR pszPath) {
    GitHubPathInfo info;
    
    // 1. 基础检查和初始化
    if (!pszPath || !IsGitHubVfsPath(pszPath)) return info;
    
    // 2. 提取路径和解析参数
    std::wstring rest = ExtractPathWithoutPrefix(pszPath);
    ParseQueryString(rest, info);
    
    // 3. 空路径检查
    if (rest.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    // 4. 分割路径
    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    // 5. 路由到子函数
    const std::wstring& seg0 = segs[0];
    
    if (IsRootReservedWord(seg0)) {
        return ParseRootPath(segs, info);
    } else {
        return ParseOwnerRepoPath(segs, info);
    }
}

// 辅助函数：提取路径（不含前缀）
static std::wstring ExtractPathWithoutPrefix(LPCWSTR pszPath) {
    std::wstring rest = pszPath + GITHUB_VFS_PREFIX_LEN;
    while (!rest.empty() && rest.front() == L'/') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == L'/') rest.pop_back();
    return rest;
}

// 辅助函数：解析查询字符串
static void ParseQueryString(std::wstring& rest, GitHubPathInfo& info) {
    size_t queryPos = rest.find(L'?');
    if (queryPos != std::wstring::npos) {
        std::wstring queryString = rest.substr(queryPos + 1);
        rest = rest.substr(0, queryPos);
        
        size_t ownerPos = queryString.find(L"owner=");
        if (ownerPos != std::wstring::npos) {
            size_t valueStart = ownerPos + 6;
            size_t valueEnd = queryString.find(L'&', valueStart);
            if (valueEnd == std::wstring::npos) valueEnd = queryString.length();
            info.ownerFilter = queryString.substr(valueStart, valueEnd - valueStart);
            // URL 解码...
        }
    }
}
```

### 2.2 子函数 1：处理根目录保留字（约 150 行）

```cpp
// 子函数：处理根目录保留字
static GitHubPathInfo ParseRootPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    const std::wstring& seg0 = segs[0];
    
    // 使用查找表代替 if-else 链
    static const std::unordered_map<std::wstring, GitHubContext> rootContextMap = {
        {L"Repos", GITHUB_CTX_REPOS},
        {L"Starred", GITHUB_CTX_STARRED},
        {L"Subscriptions", GITHUB_CTX_SUBSCRIPTIONS},
        {L"Notifications", GITHUB_CTX_NOTIFICATIONS},
        {L"Gists", GITHUB_CTX_GISTS},
        {L"Trending", GITHUB_CTX_TRENDING},
        {L"Search", GITHUB_CTX_SEARCH_CENTER}
    };
    
    auto it = rootContextMap.find(seg0);
    if (it != rootContextMap.end()) {
        info.context = it->second;
        info.isDir = true;
    }
    
    // 扁平化逻辑：统一处理（消除重复）
    if (seg0 == L"Repos" || seg0 == L"Starred" || seg0 == L"Subscriptions") {
        if (segs.size() >= 2) {
            if (ResolveFlatRepo(segs, info)) {
                return info;  // 成功重定向到 REPO 上下文
            }
            // 失败则显示 owner 列表
            info.context = GetOwnerContextType(seg0);
            info.owner = segs[1];
            return info;
        }
    }
    
    // Search 特殊处理
    if (seg0 == L"Search") {
        return ParseSearchPath(segs, info);
    }
    
    // Trending 特殊处理
    if (seg0 == L"Trending") {
        return ParseTrendingPath(segs, info);
    }
    
    return info;
}

// 辅助函数：获取 owner 上下文类型
static GitHubContext GetOwnerContextType(const std::wstring& rootWord) {
    if (rootWord == L"Repos") return GITHUB_CTX_REPOS_OWNER;
    if (rootWord == L"Starred") return GITHUB_CTX_STARRED_OWNER;
    if (rootWord == L"Subscriptions") return GITHUB_CTX_SUBSCRIPTIONS_OWNER;
    return GITHUB_CTX_ROOT;
}
```

### 2.3 子函数 2：扁平化逻辑（约 60 行，消除重复）

```cpp
// 子函数：扁平化逻辑（消除 Repos/Starred/Subscriptions 的重复代码）
static bool ResolveFlatRepo(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    std::wstring repoName = segs[1];
    std::wstring realOwner;
    
    // 根据 segs[0] 决定从哪个缓存查找
    std::vector<GitHubRepoInfo> repos;
    if (segs[0] == L"Repos") {
        repos = GetCachedRepos();
    } else if (segs[0] == L"Starred") {
        repos = GetCachedStarredRepos();
    } else if (segs[0] == L"Subscriptions") {
        repos = GetCachedWatchedRepos();
    }
    
    // 查找真实 owner
    for (const auto& r : repos) {
        if (_wcsicmp(r.name.c_str(), repoName.c_str()) == 0) {
            size_t pos = r.fullName.find(L'/');
            std::wstring candidateOwner = (pos != std::wstring::npos) 
                ? r.fullName.substr(0, pos) : r.fullName;
            
            // 应用 ownerFilter
            if (!info.ownerFilter.empty() && 
                _wcsicmp(candidateOwner.c_str(), info.ownerFilter.c_str()) != 0)
                continue;
                
            realOwner = candidateOwner;
            break;
        }
    }
    
    if (!realOwner.empty()) {
        info.owner = realOwner;
        info.repo = repoName;
        info.context = GITHUB_CTX_REPO;
        info.isDir = true;
        if (segs.size() > 2) {
            info.path = JoinPathSegments(segs, 2);
        }
        return true;  // 成功重定向
    }
    
    return false;  // 失败，需要显示 owner 列表
}

// 辅助函数：连接路径段
static std::wstring JoinPathSegments(
    const std::vector<std::wstring>& segs,
    int startIdx) {
    if (startIdx >= segs.size()) return L"";
    std::wstring result = segs[startIdx];
    for (size_t i = startIdx + 1; i < segs.size(); i++) {
        result += L"/" + segs[i];
    }
    return result;
}
```

### 2.4 子函数 3：处理 owner/repo 路径（约 120 行）

```cpp
// 子函数：处理 owner/repo 路径
static GitHubPathInfo ParseOwnerRepoPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    info.owner = segs[0];
    info.context = GITHUB_CTX_OWNER;
    info.isDir = true;
    
    if (segs.size() == 1) return info;
    
    const std::wstring& seg1 = segs[1];
    
    // 检查是否是仓库级保留字
    if (IsRepoSubdir(seg1)) {
        return ParseRepoSubdir(segs, info, 1);
    }
    
    // 检查是否是 @{ref} 语法
    if (seg1.length() > 1 && seg1[0] == L'@') {
        return ParseRefSyntax(segs, info, 1);
    }
    
    // 否则是 owner/repo
    info.repo = seg1;
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    
    if (segs.size() == 2) return info;
    
    // 检查 repo 下的仓库级保留字
    const std::wstring& seg2 = segs[2];
    if (IsRepoSubdir(seg2)) {
        return ParseRepoSubdir(segs, info, 2);
    }
    
    // 检查 repo 下的 @{ref} 语法
    if (seg2.length() > 1 && seg2[0] == L'@') {
        return ParseRefSyntax(segs, info, 2);
    }
    
    // 默认：文件路径
    info.context = GITHUB_CTX_CODE;
    info.path = JoinPathSegments(segs, 2);
    return info;
}
```

### 2.5 子函数 4：处理仓库级保留字（约 100 行，消除重复）

```cpp
// 子函数：处理仓库级保留字（消除 seg1 和 seg2 的重复代码）
static GitHubPathInfo ParseRepoSubdir(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info,
    int offset) {  // offset = 1 或 2
    
    const std::wstring& subdir = segs[offset];
    
    // 使用查找表
    static const std::unordered_map<std::wstring, GitHubContext> repoSubdirMap = {
        {L"Code", GITHUB_CTX_CODE},
        {L"Issues", GITHUB_CTX_ISSUES},
        {L"Pulls", GITHUB_CTX_PULLS},
        {L"Branches", GITHUB_CTX_BRANCHES},
        {L"Tags", GITHUB_CTX_TAGS},
        {L"Releases", GITHUB_CTX_RELEASES}
    };
    
    auto it = repoSubdirMap.find(subdir);
    if (it != repoSubdirMap.end()) {
        info.context = it->second;
        info.isDir = true;
    }
    
    // 特殊处理：需要解析后续路径的上下文
    if (subdir == L"Branches" && segs.size() >= offset + 2) {
        info.ref = segs[offset + 1];
        info.refType = GITHUB_REF_BRANCH;
        if (segs.size() > offset + 2) {
            info.path = JoinPathSegments(segs, offset + 2);
        }
    }
    else if (subdir == L"Tags" && segs.size() >= offset + 2) {
        info.ref = segs[offset + 1];
        info.refType = GITHUB_REF_TAG;
        if (segs.size() > offset + 2) {
            info.path = JoinPathSegments(segs, offset + 2);
        }
    }
    else if ((subdir == L"Issues" || subdir == L"Pulls") && segs.size() >= offset + 2) {
        int pg = ParsePageSegment(segs[offset + 1]);
        if (pg > 0) {
            info.page = pg;
        } else {
            int num = 0;
            try { num = std::stoi(segs[offset + 1]); } catch (...) {}
            if (num > 0) info.issueNumber = num;
        }
    }
    else {
        // Code 或其他：剩余段作为文件路径
        if (segs.size() > offset + 1) {
            info.path = JoinPathSegments(segs, offset + 1);
        }
    }
    
    return info;
}
```

### 2.6 子函数 5：处理 @{ref} 语法（约 30 行，消除重复）

```cpp
// 子函数：处理 @{ref} 语法（消除 seg1 和 seg2 的重复代码）
static GitHubPathInfo ParseRefSyntax(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info,
    int offset) {  // offset = 1 或 2
    
    info.ref = segs[offset].substr(1);  // 去除 @
    info.refType = DetectRefType(info.ref);
    info.context = GITHUB_CTX_CODE;
    info.isDir = true;
    
    if (segs.size() > offset + 1) {
        info.path = JoinPathSegments(segs, offset + 1);
    }
    
    return info;
}
```

### 2.7 子函数 6：处理搜索路径（约 100 行）

```cpp
// 子函数：处理搜索路径
static GitHubPathInfo ParseSearchPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    if (segs.size() == 1) {
        info.context = GITHUB_CTX_SEARCH_CENTER;
        info.isDir = true;
        return info;
    }
    
    const std::wstring& seg1 = segs[1];
    
    // 使用查找表
    static const std::unordered_map<std::wstring, GitHubContext> searchTypeMap = {
        {L"Repos", GITHUB_CTX_SEARCH_REPOS},
        {L"Code", GITHUB_CTX_SEARCH_CODE},
        {L"Users", GITHUB_CTX_SEARCH_USERS},
        {L"Issues", GITHUB_CTX_SEARCH_ISSUES},
        {L"Commits", GITHUB_CTX_SEARCH_COMMITS},
        {L"Topics", GITHUB_CTX_SEARCH_TOPICS},
        {L"Owner", GITHUB_CTX_SEARCH_OWNER}
    };
    
    auto it = searchTypeMap.find(seg1);
    if (it != searchTypeMap.end()) {
        info.context = it->second;
        info.searchType = seg1;
        info.isDir = true;
    }
    
    // 搜索结果重定向逻辑
    if (segs.size() >= 3) {
        info.searchQuery = segs[2];
        
        // 解析 contentSegs（排除 page:N）
        std::vector<std::wstring> contentSegs;
        for (size_t i = 3; i < segs.size(); i++) {
            int pg = ParsePageSegment(segs[i]);
            if (pg > 0) {
                info.page = pg;
            } else {
                contentSegs.push_back(segs[i]);
            }
        }
        
        // 重定向规则
        if (info.context == GITHUB_CTX_SEARCH_REPOS && contentSegs.size() >= 2) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            info.context = GITHUB_CTX_REPO;
            info.searchQuery.clear();
            info.searchType.clear();
            if (contentSegs.size() > 2) {
                info.path = JoinPathSegments(contentSegs, 2);
            }
            return info;
        }
        
        // ... 其他重定向规则 ...
    }
    
    return info;
}
```

### 2.8 子函数 7：处理 Trending 路径（约 40 行）

```cpp
// 子函数：处理 Trending 路径
static GitHubPathInfo ParseTrendingPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    info.context = GITHUB_CTX_TRENDING;
    info.isDir = true;
    
    if (segs.size() >= 2) {
        const std::wstring& seg1 = segs[1];
        if (seg1 == L"daily" || seg1 == L"weekly" || seg1 == L"monthly") {
            info.trendingSince = seg1;
        } else {
            info.trendingSince = L"daily";
            info.trendingLang = seg1;
        }
    }
    
    if (segs.size() >= 3 && info.trendingLang.empty()) {
        info.trendingLang = segs[2];
    }
    
    return info;
}
```

---

## 三、重构收益对比

### 3.1 代码行数对比

| 项目 | 重构前 | 重构后 | 减少 |
|------|---------|--------|------|
| ParseGitHubPath | 542 行 | 80 行 | -462 行 |
| 扁平化逻辑 | 270 行（3 处重复） | 60 行（1 处） | -210 行 |
| 仓库级保留字 | 240 行（2 处重复） | 100 行（1 处） | -140 行 |
| @{ref} 语法 | 80 行（2 处重复） | 30 行（1 处） | -50 行 |
| **总计** | **542 行** | **8 个子函数（共约 540 行）** | **逻辑更清晰** |

**注意**：总行数差不多，但：
1. 每个函数更短（最长 150 行，多数 < 100 行）
2. 消除了代码重复
3. 提高了可读性和可维护性

### 3.2 复杂度对比

| 指标 | 重构前 | 重构后 |
|------|---------|--------|
| 最大函数长度 | 542 行 | 150 行 |
| 嵌套深度 | 最多 7 层 | 最多 3 层 |
| 代码重复 | 严重（3 处扁平化，2 处保留字） | 无重复 |
| 查找效率 | O(n) if-else | O(1) 查找表 |
| 可测试性 | 难（单函数过大） | 易（每个子函数可独立测试） |

### 3.3 可维护性对比

| 方面 | 重构前 | 重构后 |
|------|---------|--------|
| 理解难度 | 高（需要阅读 542 行） | 低（每个子函数 < 150 行） |
| 修改风险 | 高（修改可能影响其他逻辑） | 低（每个函数职责单一） |
| 调试难度 | 高（需要定位到 542 行中的某处） | 低（直接定位到子函数） |
| 测试覆盖 | 难（需要测试所有路径） | 易（每个子函数可独立测试） |

---

## 四、实施建议

### 4.1 分阶段实施

**第一阶段**（高优先级，低风险）：
1. 创建子函数框架
2. 实现 ParseRootPath 和 ParseOwnerRepoPath
3. 编写单元测试
4. 集成测试

**第二阶段**（中优先级）：
1. 实现 ResolveFlatRepo（消除扁平化重复）
2. 实现 ParseRepoSubdir（消除仓库级保留字重复）
3. 实现 ParseRefSyntax（消除 @{ref} 语法重复）
4. 测试重定向逻辑

**第三阶段**（低优先级，可选）：
1. 减少上下文类型（合并 REPOS 和 REPOS_OWNER 等）
2. 简化路径语法（移除扁平化，统一使用查询参数）

### 4.2 测试策略

1. **单元测试**：为每个子函数编写测试用例
2. **集成测试**：测试所有路径模式
3. **回归测试**：确保重构后功能不变
4. **性能测试**：确保没有性能下降

### 4.3 风险控制

1. **保持兼容**：确保重构后 API 和功能不变
2. **代码审查**：每个修改都要经过审查
3. **逐步推进**：不要一次性重构所有代码
4. **充分测试**：每个阶段都要充分测试

---

## 五、总结

通过将 542 行的 ParseGitHubPath 函数拆分为 8 个短小精悍的子函数，我们可以：
1. **提高代码可读性**（每个函数 < 150 行）
2. **消除代码重复**（扁平化、仓库级保留字、@{ref} 语法）
3. **提高可维护性**（每个函数职责单一）
4. **提高可测试性**（每个子函数可独立测试）
5. **保持功能和 API 兼容性**

建议在充分测试的前提下，逐步推进重构工作。

---

**生成时间**：2026-05-16
**文件位置**：D:\VFS\GitHubVFS\Docs\SimplifiedCodeExamples.md
