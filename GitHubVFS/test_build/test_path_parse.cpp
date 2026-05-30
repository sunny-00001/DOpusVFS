#include <iostream>
#include <string>
#include <cassert>

#define GITHUB_VFS_PREFIX L"github://"
#define GITHUB_VFS_PREFIX_LEN 9

enum GitHubContext {
    GITHUB_CTX_ROOT = 0,
    GITHUB_CTX_REPO,
    GITHUB_CTX_VIEW,
    GITHUB_CTX_SEARCH
};

struct GitHubPathInfo {
    std::wstring repo;
    std::wstring path;
    GitHubContext context;
    int issueNumber;
    bool isDir;

    struct Query {
        std::wstring view;
        std::wstring ref;
        std::wstring search;
        std::wstring query;
        std::wstring owner;
        std::wstring state;
        std::wstring since;
        std::wstring lang;
        std::wstring from;
        int page;

        Query() : page(1) {}
    } query;

    std::wstring resolvedOwner;

    GitHubPathInfo() {
        issueNumber = 0;
        context = GITHUB_CTX_ROOT;
        isDir = false;
    }
};

bool IsGitHubVfsPath(LPCWSTR pszPath) {
    return pszPath && wcsncmp(pszPath, GITHUB_VFS_PREFIX, GITHUB_VFS_PREFIX_LEN) == 0;
}

std::wstring ExtractPathWithoutPrefix(LPCWSTR pszPath) {
    std::wstring rest = pszPath + GITHUB_VFS_PREFIX_LEN;
    while (!rest.empty() && rest.front() == L'/') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == L'/') rest.pop_back();
    return rest;
}

void ParseQueryString(std::wstring& rest, GitHubPathInfo& info) {
    size_t queryPos = rest.find(L'?');
    if (queryPos == std::wstring::npos) return;

    std::wstring queryString = rest.substr(queryPos + 1);
    rest = rest.substr(0, queryPos);

    size_t start = 0;
    size_t pos = 0;
    while ((pos = queryString.find(L'&', start)) != std::wstring::npos || start < queryString.length()) {
        std::wstring param;
        if (pos != std::wstring::npos) {
            param = queryString.substr(start, pos - start);
            start = pos + 1;
        } else {
            param = queryString.substr(start);
            start = queryString.length();
        }

        if (param.length() > 5 && param.compare(0, 5, L"view=") == 0) {
            info.query.view = param.substr(5);
        }
        else if (param.length() > 4 && param.compare(0, 4, L"ref=") == 0) {
            info.query.ref = param.substr(4);
        }
        else if (param.length() > 7 && param.compare(0, 7, L"search=") == 0) {
            info.query.search = param.substr(7);
        }
        else if (param.length() > 6 && param.compare(0, 6, L"query=") == 0) {
            info.query.query = param.substr(6);
        }
        else if (param.length() > 2 && param.compare(0, 2, L"q=") == 0) {
            info.query.query = param.substr(2);
        }
        else if (param.length() > 6 && param.compare(0, 6, L"owner=") == 0) {
            info.query.owner = param.substr(6);
        }
        else if (param.length() > 5 && param.compare(0, 5, L"page=") == 0) {
            try { info.query.page = std::stoi(param.substr(5)); } catch (...) {}
        }
    }
}

std::vector<std::wstring> SplitPath(const std::wstring& path) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = path.find(L'/', start)) != std::wstring::npos) {
        if (pos > start) parts.push_back(path.substr(start, pos - start));
        start = pos + 1;
    }
    if (start < path.length()) parts.push_back(path.substr(start));
    return parts;
}

std::wstring JoinPathSegments(const std::vector<std::wstring>& segs, size_t startIdx) {
    if (startIdx >= segs.size()) return L"";
    std::wstring result = segs[startIdx];
    for (size_t i = startIdx + 1; i < segs.size(); i++) {
        result += L"/" + segs[i];
    }
    return result;
}

GitHubPathInfo ParseGitHubPathNew(LPCWSTR pszPath) {
    GitHubPathInfo info;
    
    if (!pszPath || !IsGitHubVfsPath(pszPath)) return info;
    
    std::wstring rest = ExtractPathWithoutPrefix(pszPath);
    ParseQueryString(rest, info);
    
    if (rest.empty()) {
        if (!info.query.search.empty()) {
            info.context = GITHUB_CTX_SEARCH;
            info.isDir = true;
        } else if (!info.query.view.empty()) {
            info.context = GITHUB_CTX_VIEW;
            info.isDir = true;
        } else {
            info.context = GITHUB_CTX_ROOT;
            info.isDir = true;
        }
        return info;
    }
    
    std::vector<std::wstring> segs = SplitPath(rest);
    if (segs.empty()) {
        info.context = GITHUB_CTX_ROOT;
        info.isDir = true;
        return info;
    }
    
    for (const auto& seg : segs) {
        if (seg == L".." || seg == L".") {
            return info;
        }
    }
    
    info.repo = segs[0];
    info.context = GITHUB_CTX_REPO;
    info.isDir = true;
    
    if (segs.size() > 1) {
        info.path = JoinPathSegments(segs, 1);
    }
    
    if (!info.query.view.empty()) {
        info.context = GITHUB_CTX_VIEW;
    }
    
    return info;
}

void PrintPathInfo(const GitHubPathInfo& info, const std::wstring& testPath) {
    std::wcout << L"测试路径: " << testPath << std::endl;
    std::wcout << L"  context: ";
    switch (info.context) {
        case GITHUB_CTX_ROOT: std::wcout << L"ROOT"; break;
        case GITHUB_CTX_REPO: std::wcout << L"REPO"; break;
        case GITHUB_CTX_VIEW: std::wcout << L"VIEW"; break;
        case GITHUB_CTX_SEARCH: std::wcout << L"SEARCH"; break;
    }
    std::wcout << std::endl;
    std::wcout << L"  repo: " << info.repo << std::endl;
    std::wcout << L"  path: " << info.path << std::endl;
    std::wcout << L"  isDir: " << (info.isDir ? L"true" : L"false") << std::endl;
    std::wcout << L"  query.view: " << info.query.view << std::endl;
    std::wcout << L"  query.search: " << info.query.search << std::endl;
    std::wcout << L"  query.query: " << info.query.query << std::endl;
    std::wcout << L"  query.ref: " << info.query.ref << std::endl;
    std::wcout << L"  query.owner: " << info.query.owner << std::endl;
    std::wcout << L"  query.page: " << info.query.page << std::endl;
    std::wcout << std::endl;
}

int main() {
    std::wcout << L"=== GitHubVFS 路径解析测试 ===" << std::endl << std::endl;

    std::vector<std::wstring> testPaths = {
        L"github://",
        L"github:///",
        L"github:///react/",
        L"github:///react/src/",
        L"github:///react/src/index.tsx",
        L"github:///react/?view=issues",
        L"github:///react/?view=issues&page=2",
        L"github:///react/?view=pulls&state=closed",
        L"github:///react/src/?ref=develop",
        L"github:///?search=repos&q=react",
        L"github:///?search=repos&q=react&page=2",
        L"github:///?search=code&q=function",
        L"github:///?view=starred",
        L"github:///?view=trending&since=daily&lang=python",
        L"github:///vscode/?owner=microsoft",
    };

    for (const auto& path : testPaths) {
        GitHubPathInfo info = ParseGitHubPathNew(path.c_str());
        PrintPathInfo(info, path);
    }

    std::wcout << L"=== 测试完成 ===" << std::endl;
    return 0;
}