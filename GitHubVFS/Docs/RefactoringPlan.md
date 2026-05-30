# GitHubVFS 路径处理代码重构方案

## 一、重构目标

### 1.1 主要问题
1. **ParseGitHubPath 函数过长**（427-969行，共542行）
2. **代码重复严重**
   - 扁平化逻辑在 3 个地方重复（Repos/Starred/Subscriptions）
   - 仓库级保留字逻辑在 2 个地方重复（seg1 和 seg2）
   - @{ref} 语法在 2 个地方处理（seg1 和 seg2）
3. **上下文类型过多**（18+ 种）
4. **嵌套 if-else 过深**

### 1.2 重构目标
- 将 ParseGitHubPath 拆分为多个子函数（每个 < 100 行）
- 消除代码重复
- 减少上下文类型数量
- 提高代码可读性和可维护性
- 保持功能和 API 兼容性

---

## 二、重构方案详解

### 方案 A：拆分 ParseGitHubPath 函数（推荐，低风险）

#### A.1 拆分策略

**原函数**：ParseGitHubPath（542行）
**拆分后**：
1. `ParseRootPath()` - 处理根目录保留字（Repos/Starred/...）
2. `ParseOwnerRepoPath()` - 处理 owner/repo 路径
3. `ParseRepoSubdir()` - 处理仓库级保留字（Code/Issues/...）
4. `ParseSearchPath()` - 处理搜索路径（Search/Repos/...）
5. `ParseTrendingPath()` - 处理趋势路径（Trending/...）
6. `ResolveFlatRepo()` - 扁平化逻辑（消除重复）

#### A.2 重构后的函数签名

```cpp
// 主函数（简化版，约 80 行）
static GitHubPathInfo ParseGitHubPath(LPCWSTR pszPath) {
    GitHubPathInfo info;
    
    // 1. 基础检查和初始化（20 行）
    if (!pszPath || !IsGitHubVfsPath(pszPath)) return info;
    std::wstring rest = ExtractPathWithoutPrefix(pszPath);
    ParseQueryString(rest, info);  // 解析 ?owner=xxx
    
    // 2. 空路径检查（10 行）
    if (rest.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    // 3. 分割路径（5 行）
    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    // 4. 路由到子函数（45 行）
    const std::wstring& seg0 = segs[0];
    
    if (IsRootReservedWord(seg0)) {
        return ParseRootPath(segs, info);  // 方案 A.3
    } else {
        return ParseOwnerRepoPath(segs, info);  // 方案 A.4
    }
}

// 子函数 1：处理根目录保留字（约 150 行，原 474-751 行）
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
    
    // 扁平化逻辑：统一处理 Repos/Starred/Subscriptions
    if (seg0 == L"Repos" || seg0 == L"Starred" || seg0 == L"Subscriptions") {
        if (segs.size() >= 2) {
            // 调用统一的函数处理扁平化
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
        return ParseSearchPath(segs, info);  // 方案 A.5
    }
    
    // Trending 特殊处理
    if (seg0 == L"Trending") {
        return ParseTrendingPath(segs, info);  // 方案 A.6
    }
    
    return info;
}

// 子函数 2：扁平化逻辑（约 60 行，消除重复）
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

// 子函数 3：处理 owner/repo 路径（约 120 行）
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
        return ParseRepoSubdir(segs, info, 1);  // 方案 A.7
    }
    
    // 检查是否是 @{ref} 语法
    if (seg1.length() > 1 && seg1[0] == L'@') {
        return ParseRefSyntax(segs, info, 1);  // 方案 A.8
    }
    
    // 否则是 owner/repo
    info.repo = seg1;
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    
    if (segs.size() == 2) return info;
    
    // 检查 repo 下的仓库级保留字
    const std::wstring& seg2 = segs[2];
    if (IsRepoSubdir(seg2)) {
        return ParseRepoSubdir(segs, info, 2);  // 方案 A.7
    }
    
    // 检查 repo 下的 @{ref} 语法
    if (seg2.length() > 1 && seg2[0] == L'@') {
        return ParseRefSyntax(segs, info, 2);  // 方案 A.8
    }
    
    // 默认：文件路径
    info.context = GITHUB_CTX_CODE;
    info.path = JoinPathSegments(segs, 2);
    return info;
}

// 子函数 4：处理仓库级保留字（约 100 行，消除重复）
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

// 子函数 5：处理 @{ref} 语法（约 30 行，消除重复）
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

// 子函数 6：处理搜索路径（约 100 行）
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
        // ... 其他重定向规则
    }
    
    return info;
}

// 子函数 7：处理 Trending 路径（约 40 行）
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

// 辅助函数：获取 owner 上下文类型
static GitHubContext GetOwnerContextType(const std::wstring& rootWord) {
    if (rootWord == L"Repos") return GITHUB_CTX_REPOS_OWNER;
    if (rootWord == L"Starred") return GITHUB_CTX_STARRED_OWNER;
    if (rootWord == L"Subscriptions") return GITHUB_CTX_SUBSCRIPTIONS_OWNER;
    return GITHUB_CTX_ROOT;
}
```

#### A.3 重构收益
1. **函数长度**：从 542 行 → 8 个子函数（每个 30-150 行）
2. **代码重复**：消除 3 处扁平化逻辑重复，2 处仓库级保留字重复
3. **可维护性**：每个函数职责单一，易于理解和测试
4. **性能**：使用查找表，O(1) 查找代替 O(n) if-else
5. **风险**：低风险，保持 API 兼容

---

### 方案 B：减少上下文类型（中风险）

#### B.1 合并策略

**可合并的上下文类型**：
1. `GITHUB_CTX_REPOS` + `GITHUB_CTX_REPOS_OWNER` → `GITHUB_CTX_REPOS`
2. `GITHUB_CTX_STARRED` + `GITHUB_CTX_STARRED_OWNER` → `GITHUB_CTX_STARRED`
3. `GITHUB_CTX_SUBSCRIPTIONS` + `GITHUB_CTX_SUBSCRIPTIONS_OWNER` → `GITHUB_CTX_SUBSCRIPTIONS`

**合并方法**：
- 使用 `info.owner` 是否为空来区分显示所有 owner 还是指定 owner
- 如果 `info.owner.empty()` → 显示所有 owner 的仓库
- 如果 `!info.owner.empty()` → 显示指定 owner 的仓库

#### B.2 代码修改示例

**修改前**（InternalReadDirectory 中的 case）：
```cpp
case GITHUB_CTX_REPOS:
    // 显示所有 owner 的仓库
    return ReadReposAll(info, ...);

case GITHUB_CTX_REPOS_OWNER:
    // 显示指定 owner 的仓库
    return ReadReposByOwner(info, ...);
```

**修改后**：
```cpp
case GITHUB_CTX_REPOS:
    if (info.owner.empty()) {
        // 显示所有 owner 的仓库
        return ReadReposAll(info, ...);
    } else {
        // 显示指定 owner 的仓库
        return ReadReposByOwner(info, ...);
    }
```

#### B.3 重构收益
1. **上下文类型**：从 18+ 种 → 15 种（减少 3 种）
2. **switch case 数量**：减少 3 个 case 分支
3. **代码行数**：减少约 50-100 行
4. **风险**：中风险，需要仔细测试所有相关功能

---

### 方案 C：简化路径语法（高风险，激进）

#### C.1 移除扁平化逻辑

**当前**：`github:///Repos/{repoName}/` → 自动查找 owner
**简化后**：统一使用 `github:///{owner}/{repo}/`

**优点**：
- 大幅简化路径解析逻辑
- 消除 ResolveFlatRepo 函数
- 路径结构更一致

**缺点**：
- 用户体验下降（需要多一层目录导航）
- 需要修改大量现有代码和用户习惯

#### C.2 统一使用查询参数

**当前**：`github:///Search/Repos/{query}/`
**简化后**：`github:///Search?type=repos&q={query}`

**优点**：
- 减少路径模式数量
- 更灵活的搜索参数

**缺点**：
- 需要修改搜索相关代码
- 可能影响用户体验

#### C.3 风险
- **高风险**：需要仔细评估用户体验影响
- **兼容性**：需要保持向后兼容或提供迁移方案

---

## 三、重构实施步骤

### 3.1 第一阶段：拆分 ParseGitHubPath（高优先级，低风险）
1. **创建子函数**：实现 ParseRootPath, ParseOwnerRepoPath 等
2. **单元测试**：为每个子函数编写测试用例
3. **集成测试**：确保重构后功能不变
4. **代码审查**：仔细检查每个子函数的逻辑

### 3.2 第二阶段：消除代码重复（中优先级）
1. **扁平化逻辑**：实现 ResolveFlatRepo 函数
2. **仓库级保留字**：实现 ParseRepoSubdir 函数
3. **@{ref} 语法**：实现 ParseRefSyntax 函数
4. **测试**：确保重定向逻辑正确

### 3.3 第三阶段：减少上下文类型（中优先级，中风险）
1. **合并上下文**：REPOS + REPOS_OWNER 等
2. **修改 switch case**：更新 InternalReadDirectory
3. **充分测试**：测试所有相关功能

### 3.4 第四阶段：简化路径语法（低优先级，高风险）
1. **用户调研**：评估用户体验影响
2. **设计新路径结构**：制定详细的迁移方案
3. **向后兼容**：提供旧路径的兼容层
4. **逐步推进**：分阶段实施，充分测试

---

## 四、重构后代码结构对比

### 4.1 重构前
```
ParseGitHubPath (542 行)
├─ 检查前缀 (20 行)
├─ 解析 URL 参数 (30 行)
├─ 去除首尾 / (10 行)
├─ 检查空路径 (15 行)
├─ 分割路径 (5 行)
├─ 根目录保留字处理 (270 行)
│   ├─ Repos (90 行)
│   ├─ Starred (90 行)
│   ├─ Subscriptions (90 行)
│   ├─ Notifications (5 行)
│   ├─ Gists (10 行)
│   ├─ Trending (20 行)
│   └─ Search (60 行)
└─ owner/repo 处理 (200 行)
    ├─ 仓库级保留字 (120 行)
    ├─ @{ref} 语法 (40 行)
    └─ 文件路径 (40 行)
```

### 4.2 重构后
```
ParseGitHubPath (80 行)
├─ 基础检查和初始化 (20 行)
├─ 空路径检查 (10 行)
├─ 分割路径 (5 行)
└─ 路由到子函数 (45 行)

ParseRootPath (150 行)
├─ 查找表匹配 (10 行)
├─ 扁平化处理 (20 行，调用 ResolveFlatRepo)
├─ Search 处理 (调用 ParseSearchPath)
└─ Trending 处理 (调用 ParseTrendingPath)

ParseOwnerRepoPath (120 行)
├─ owner 处理 (10 行)
├─ 仓库级保留字 (调用 ParseRepoSubdir)
├─ @{ref} 语法 (调用 ParseRefSyntax)
└─ repo 处理 (40 行)

ResolveFlatRepo (60 行)  // 消除重复
ParseRepoSubdir (100 行)  // 消除重复
ParseRefSyntax (30 行)    // 消除重复
ParseSearchPath (100 行)
ParseTrendingPath (40 行)
```

---

## 五、测试计划

### 5.1 单元测试
- 为每个子函数编写测试用例
- 覆盖所有路径模式和边界情况
- 使用 Google Test 框架

### 5.2 集成测试
- 测试所有路径模式
- 测试重定向逻辑
- 测试搜索功能
- 测试扁平化逻辑

### 5.3 回归测试
- 确保重构后功能不变
- 与重构前的输出对比
- 性能测试（确保没有性能下降）

---

## 六、风险评估

### 6.1 低风险（方案 A）
- 拆分 ParseGitHubPath 函数
- 消除代码重复
- 使用查找表

### 6.2 中风险（方案 B）
- 减少上下文类型
- 需要仔细测试所有相关功能
- 可能影响用户体验

### 6.3 高风险（方案 C）
- 简化路径语法
- 需要充分评估用户体验影响
- 需要提供迁移方案

---

## 七、总结

### 7.1 推荐实施顺序
1. **第一阶段**：拆分 ParseGitHubPath（方案 A）→ 高优先级，低风险
2. **第二阶段**：消除代码重复（方案 A）→ 中优先级
3. **第三阶段**：减少上下文类型（方案 B）→ 中优先级，中风险
4. **第四阶段**：简化路径语法（方案 C）→ 低优先级，高风险

### 7.2 预期收益
1. **代码可读性**：显著提高（函数长度从 542 行 → 80 行）
2. **可维护性**：显著提高（每个函数职责单一）
3. **代码质量**：消除重复代码，使用查找表
4. **性能**：查找表 O(1) 代替 if-else O(n)
5. **测试性**：每个子函数可独立测试

### 7.3 注意事项
1. **保持兼容**：确保重构后 API 和功能不变
2. **充分测试**：每个阶段都要充分测试
3. **逐步推进**：不要一次性重构所有代码
4. **代码审查**：每个修改都要经过代码审查

---

**生成时间**：2026-05-16
**文件位置**：D:\VFS\GitHubVFS\Docs\RefactoringPlan.md
