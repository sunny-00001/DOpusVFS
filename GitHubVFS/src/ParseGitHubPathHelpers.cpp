// ============================================================================
// ParseGitHubPathHelpers.cpp — 【旧版参考文件，未参与编译】
// ============================================================================
// ⚠ 此文件为重构前的参考快照，CMakeLists.txt 未将其加入编译。
// 当前生效的辅助函数定义在 GitHubVFS.cpp 内部（同名 static 函数）。
// 如需修改路径解析逻辑，请修改 GitHubVFS.cpp 中的对应函数。
// ============================================================================

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
        
        // 解析 owner=xxx 参数
        size_t ownerPos = queryString.find(L"owner=");
        if (ownerPos != std::wstring::npos) {
            size_t valueStart = ownerPos + 6; // "owner=" 长度
            size_t valueEnd = queryString.find(L'&', valueStart);
            if (valueEnd == std::wstring::npos) valueEnd = queryString.length();
            info.ownerFilter = queryString.substr(valueStart, valueEnd - valueStart);
            
            // URL 解码（处理 %20 等编码）
            size_t pos = 0;
            while ((pos = info.ownerFilter.find(L'%', pos)) != std::wstring::npos && pos + 2 < info.ownerFilter.length()) {
                std::wstring hex = info.ownerFilter.substr(pos + 1, 2);
                wchar_t ch = (wchar_t)std::wcstol(hex.c_str(), nullptr, 16);
                info.ownerFilter.replace(pos, 3, 1, ch);
                pos++;
            }
        }
    }
}

// 辅助函数：连接路径段
static std::wstring JoinPathSegments(const std::vector<std::wstring>& segs, int startIdx) {
    if (startIdx >= (int)segs.size()) return L"";
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

// 辅助函数：扁平化逻辑（消除 Repos/Starred/Subscriptions 的重复代码）
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
            std::wstring candidateOwner = (pos != std::wstring::npos) ? r.fullName.substr(0, pos) : r.fullName;
            
            // 应用 ownerFilter
            if (!info.ownerFilter.empty() && _wcsicmp(candidateOwner.c_str(), info.ownerFilter.c_str()) != 0)
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

// 子函数 1：处理仓库级保留字（消除 seg1 和 seg2 的重复代码）
static void ParseRepoSubdir(
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
}

// 子函数 2：处理 @{ref} 语法（消除 seg1 和 seg2 的重复代码）
static void ParseRefSyntax(
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
}

// 子函数 3：处理搜索路径
static void ParseSearchPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    if (segs.size() == 1) {
        info.context = GITHUB_CTX_SEARCH_CENTER;
        info.isDir = true;
        return;
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
    
    // Search/{Type}/{query}/
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
        if (contentSegs.size() >= 2 && info.context == GITHUB_CTX_SEARCH_REPOS) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            info.context = GITHUB_CTX_REPO;
            info.searchQuery.clear();
            info.searchType.clear();
            // 剩余段作为仓库内路径
            if (contentSegs.size() > 2) {
                info.path = JoinPathSegments(contentSegs, 2);
            }
            return;
        }
        
        // Search/Users/{q}/{login}/ → OWNER
        if (contentSegs.size() >= 1 && info.context == GITHUB_CTX_SEARCH_USERS) {
            info.owner = contentSegs[0];
            info.context = GITHUB_CTX_OWNER;
            info.searchQuery.clear();
            info.searchType.clear();
            return;
        }
        
        // Search/Issues/{q}/{owner}/{repo}/{number} → ISSUE_DETAIL
        if (contentSegs.size() >= 3 && info.context == GITHUB_CTX_SEARCH_ISSUES) {
            info.owner = contentSegs[0];
            info.repo = contentSegs[1];
            int num = 0;
            try { num = std::stoi(contentSegs[2]); } catch (...) {}
            if (num > 0) {
                info.issueNumber = num;
                info.context = GITHUB_CTX_ISSUES;
                info.searchQuery.clear();
                info.searchType.clear();
                return;
            }
        }
    }
}

// 子函数 4：处理 Trending 路径
static void ParseTrendingPath(
    const std::vector<std::wstring>& segs,
    GitHubPathInfo& info) {
    
    info.context = GITHUB_CTX_TRENDING;
    info.isDir = true;
    
    if (segs.size() >= 2) {
        const std::wstring& seg1 = segs[1];
        if (seg1 == L"daily" || seg1 == L"weekly" || seg1 == L"monthly") {
            info.trendingSince = seg1;
        } else {
            // seg1 不是时间范围，当作语言名（默认 daily）
            info.trendingSince = L"daily";
            info.trendingLang = seg1;
        }
    }
    
    if (segs.size() >= 3 && info.trendingLang.empty()) {
        info.trendingLang = segs[2];
    }
}

// 子函数 5：处理根目录保留字
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
        ParseSearchPath(segs, info);
        return info;
    }
    
    // Trending 特殊处理
    if (seg0 == L"Trending") {
        ParseTrendingPath(segs, info);
        return info;
    }
    
    return info;
}

// 子函数 6：处理 owner/repo 路径
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
        ParseRepoSubdir(segs, info, 1);
        return info;
    }
    
    // 检查是否是 @{ref} 语法
    if (seg1.length() > 1 && seg1[0] == L'@') {
        ParseRefSyntax(segs, info, 1);
        return info;
    }
    
    // 否则是 owner/repo
    info.repo = seg1;
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    
    if (segs.size() == 2) return info;
    
    // 检查 repo 下的仓库级保留字
    const std::wstring& seg2 = segs[2];
    if (IsRepoSubdir(seg2)) {
        ParseRepoSubdir(segs, info, 2);
        return info;
    }
    
    // 检查 repo 下的 @{ref} 语法
    if (seg2.length() > 1 && seg2[0] == L'@') {
        ParseRefSyntax(segs, info, 2);
        return info;
    }
    
    // 默认：文件路径
    info.context = GITHUB_CTX_CODE;
    info.path = JoinPathSegments(segs, 2);
    return info;
}
