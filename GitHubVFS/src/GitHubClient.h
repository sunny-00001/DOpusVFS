#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

enum GitHubAuthMode {
    GITHUB_AUTH_TOKEN = 0,
    GITHUB_AUTH_BASIC = 1,
    GITHUB_AUTH_OAUTH = 2
};

enum GitHubProxyType {
    GITHUB_PROXY_NONE = 0,
    GITHUB_PROXY_HTTP = 1,
    GITHUB_PROXY_SOCKS5 = 2
};

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
    int64_t stargazersCount;
    int64_t forksCount;
    int64_t openIssuesCount;
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

enum VirtualFileType {
    VIRTUAL_FILE_NONE = 0,
    VIRTUAL_FILE_DOWNLOAD_ZIP,
    VIRTUAL_FILE_DOWNLOAD_TAR,
    VIRTUAL_FILE_CLONE_URL,
    VIRTUAL_FILE_RESULT_TEXT
};

struct GitHubContentInfo {
    std::vector<BYTE> data;
    size_t readPos;
    bool isWrite;
    std::wstring owner;
    std::wstring repo;
    std::wstring path;
    std::wstring sha;
    std::wstring uploadBranch;
    VirtualFileType virtualType;
    std::wstring tempFilePath;
    HANDLE hTempFile;
    FILETIME originalModTime;
    bool modified;
};

struct GitHubSearchResult {
    std::vector<GitHubRepoInfo> repos;
    int totalCount;
    bool incomplete;
};

struct GitHubCodeItem {
    std::wstring name;
    std::wstring path;
    std::wstring htmlUrl;
    std::wstring owner;
    std::wstring repo;
    std::wstring repoFullName;
    std::wstring repoHtmlUrl;
    std::wstring repoLanguage;
    int repoStars;
};

struct GitHubCodeSearchResult {
    std::vector<GitHubCodeItem> items;
    int totalCount;
    bool incomplete;
};

struct GitHubUserInfo {
    std::wstring login;
    std::wstring name;
    std::wstring avatarUrl;
    std::wstring htmlUrl;
    std::wstring bio;
    std::wstring company;
    std::wstring location;
    std::wstring email;
    std::wstring blog;
    int publicRepos;
    int followers;
    int following;
};

struct GitHubUserSearchResult {
    std::vector<GitHubUserInfo> users;
    int totalCount;
    bool incomplete;
};

struct GitHubIssueInfo {
    int number;
    std::wstring title;
    std::wstring body;
    std::wstring state;
    std::wstring htmlUrl;
    std::wstring author;
    std::wstring owner;       // v3.1: 所属仓库 owner
    std::wstring repo;        // v3.1: 所属仓库名
    std::vector<std::wstring> labels;
    int comments;
    FILETIME createdAt;
    FILETIME updatedAt;
    FILETIME closedAt;
    bool isPullRequest;
};

struct GitHubIssueSearchResult {
    std::vector<GitHubIssueInfo> items;
    int totalCount;
    bool incomplete;
};

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

struct GitHubBranchInfo {
    std::wstring name;
    std::wstring sha;
    bool isDefault;
};

struct GitHubReleaseInfo {
    std::wstring tagName;
    std::wstring name;
    std::wstring body;
    std::wstring htmlUrl;
    std::wstring tarballUrl;
    std::wstring zipballUrl;
    bool isPrerelease;
    bool isDraft;
    FILETIME publishedAt;
    FILETIME createdAt;
};

enum GitHubPathMode {
    GITHUB_PATH_STRICT = 0,
    GITHUB_PATH_LOOSE = 1
};

// 新枚举（重构后使用）
enum GitHubContext {
    GITHUB_CTX_ROOT = 0,
    GITHUB_CTX_REPO,
    GITHUB_CTX_VIEW,
    GITHUB_CTX_SEARCH
};

// 旧枚举（兼容层，逐步删除）
enum GitHubContextLegacy {
    GITHUB_CTX_REPOS_OLD = 100,
    GITHUB_CTX_REPOS_OWNER_OLD = 101,
    GITHUB_CTX_STARRED_OLD = 102,
    GITHUB_CTX_STARRED_OWNER_OLD = 103,
    GITHUB_CTX_SUBSCRIPTIONS_OLD = 104,
    GITHUB_CTX_SUBSCRIPTIONS_OWNER_OLD = 105,
    GITHUB_CTX_NOTIFICATIONS_OLD = 106,
    GITHUB_CTX_GISTS_OLD = 107,
    GITHUB_CTX_TRENDING_OLD = 108,
    GITHUB_CTX_SEARCH_CENTER_OLD = 109,
    GITHUB_CTX_SEARCH_REPOS_OLD = 110,
    GITHUB_CTX_SEARCH_CODE_OLD = 111,
    GITHUB_CTX_SEARCH_USERS_OLD = 112,
    GITHUB_CTX_SEARCH_ISSUES_OLD = 113,
    GITHUB_CTX_SEARCH_COMMITS_OLD = 114,
    GITHUB_CTX_SEARCH_TOPICS_OLD = 115,
    GITHUB_CTX_SEARCH_OWNER_OLD = 116,
    GITHUB_CTX_OWNER_OLD = 117,
    GITHUB_CTX_CODE_OLD = 118,
    GITHUB_CTX_ISSUES_OLD = 119,
    GITHUB_CTX_PULLS_OLD = 120,
    GITHUB_CTX_BRANCHES_OLD = 121,
    GITHUB_CTX_TAGS_OLD = 122,
    GITHUB_CTX_RELEASES_OLD = 123
};

// 兼容宏：旧枚举名称映射到旧枚举值
#define GITHUB_CTX_REPOS GITHUB_CTX_REPOS_OLD
#define GITHUB_CTX_REPOS_OWNER GITHUB_CTX_REPOS_OWNER_OLD
#define GITHUB_CTX_STARRED GITHUB_CTX_STARRED_OLD
#define GITHUB_CTX_STARRED_OWNER GITHUB_CTX_STARRED_OWNER_OLD
#define GITHUB_CTX_SUBSCRIPTIONS GITHUB_CTX_SUBSCRIPTIONS_OLD
#define GITHUB_CTX_SUBSCRIPTIONS_OWNER GITHUB_CTX_SUBSCRIPTIONS_OWNER_OLD
#define GITHUB_CTX_NOTIFICATIONS GITHUB_CTX_NOTIFICATIONS_OLD
#define GITHUB_CTX_GISTS GITHUB_CTX_GISTS_OLD
#define GITHUB_CTX_TRENDING GITHUB_CTX_TRENDING_OLD
#define GITHUB_CTX_SEARCH_CENTER GITHUB_CTX_SEARCH_CENTER_OLD
#define GITHUB_CTX_SEARCH_REPOS GITHUB_CTX_SEARCH_REPOS_OLD
#define GITHUB_CTX_SEARCH_CODE GITHUB_CTX_SEARCH_CODE_OLD
#define GITHUB_CTX_SEARCH_USERS GITHUB_CTX_SEARCH_USERS_OLD
#define GITHUB_CTX_SEARCH_ISSUES GITHUB_CTX_SEARCH_ISSUES_OLD
#define GITHUB_CTX_SEARCH_COMMITS GITHUB_CTX_SEARCH_COMMITS_OLD
#define GITHUB_CTX_SEARCH_TOPICS GITHUB_CTX_SEARCH_TOPICS_OLD
#define GITHUB_CTX_SEARCH_OWNER GITHUB_CTX_SEARCH_OWNER_OLD
#define GITHUB_CTX_OWNER GITHUB_CTX_OWNER_OLD
#define GITHUB_CTX_CODE GITHUB_CTX_CODE_OLD
#define GITHUB_CTX_ISSUES GITHUB_CTX_ISSUES_OLD
#define GITHUB_CTX_PULLS GITHUB_CTX_PULLS_OLD
#define GITHUB_CTX_BRANCHES GITHUB_CTX_BRANCHES_OLD
#define GITHUB_CTX_TAGS GITHUB_CTX_TAGS_OLD
#define GITHUB_CTX_RELEASES GITHUB_CTX_RELEASES_OLD

// 旧 ref 类型枚举（兼容层）
enum GitHubRefType {
    GITHUB_REF_DEFAULT = 0,
    GITHUB_REF_BRANCH = 1,
    GITHUB_REF_TAG = 2,
    GITHUB_REF_SHA = 3
};

enum GitHubRepoSort {
    GITHUB_SORT_UPDATED = 0,
    GITHUB_SORT_NAME = 1,
    GITHUB_SORT_STARS = 2,
    GITHUB_SORT_CREATED = 3
};

struct GitHubConfig {
    GitHubAuthMode authMode;
    std::wstring token;
    std::wstring username;
    std::wstring password;
    std::wstring apiUrl;
    int cacheTimeout;
    int connTimeout;
    int itemsPerPage;
    bool showForks;
    bool showArchived;
    bool showPrivate;
    bool showDescription;
    std::wstring defaultBranchName;
    GitHubProxyType proxyType;
    std::wstring proxyHost;
    int proxyPort;
    std::wstring currentUser;
    GitHubRepoSort repoSort;
    std::wstring commitMessageTemplate;
    bool autoRefresh;
    int refreshInterval;
    bool largeFileWarn;
    int largeFileSizeMB;
    GitHubPathMode pathMode;
    std::wstring metaPrefix;
    std::wstring refKeyword;
    bool enableAliases;
    bool enableOldPaths;
    std::wstring defaultOwner;
    std::vector<std::wstring> searchHistory;
};

class GitHubClient {
public:
    static void Init();
    static void Cleanup();

    static bool LoadConfig();
    static bool SaveConfig();
    static GitHubConfig& GetConfig();
    static std::wstring GetConfigFilePath();

    static std::vector<GitHubRepoInfo> ListUserRepos(int page = 1);
    static std::vector<GitHubRepoInfo> ListUserRepos(const std::wstring& owner, int page = 1);
    static std::vector<GitHubRepoInfo> ListOrgRepos(const std::wstring& org, int page = 1);
    static bool GetRepoInfo(const std::wstring& owner, const std::wstring& repo, GitHubRepoInfo& outInfo);

    static GitHubSearchResult SearchRepos(const std::wstring& query, int page = 1, const std::wstring& sort = L"stars");
    static GitHubSearchResult SearchUserRepos(const std::wstring& username, int page = 1);
    static GitHubSearchResult SearchOrgRepos(const std::wstring& org, int page = 1);
    static GitHubCodeSearchResult SearchCode(const std::wstring& query, int page = 1);
    static GitHubUserSearchResult SearchUsers(const std::wstring& query, int page = 1);
    static GitHubIssueSearchResult SearchIssues(const std::wstring& query, int page = 1);
    static GitHubCommitSearchResult SearchCommits(const std::wstring& query, int page = 1);
    static GitHubTopicSearchResult SearchTopics(const std::wstring& query, int page = 1);

    static std::vector<GitHubIssueInfo> ListIssues(const std::wstring& owner, const std::wstring& repo,
                                                    const std::wstring& state = L"open", int page = 1);
    static bool GetIssue(const std::wstring& owner, const std::wstring& repo, int number, GitHubIssueInfo& outInfo);
    static bool CreateIssue(const std::wstring& owner, const std::wstring& repo,
                            const std::wstring& title, const std::wstring& body, int& outNumber);
    static bool UpdateIssue(const std::wstring& owner, const std::wstring& repo, int number,
                            const std::wstring& title, const std::wstring& body, const std::wstring& state);
    static bool CloseIssue(const std::wstring& owner, const std::wstring& repo, int number);

    static std::vector<GitHubIssueInfo> ListPullRequests(const std::wstring& owner, const std::wstring& repo,
                                                         const std::wstring& state = L"open", int page = 1);
    static bool GetPullRequest(const std::wstring& owner, const std::wstring& repo, int number, GitHubIssueInfo& outInfo);

    static std::vector<GitHubBranchInfo> ListBranches(const std::wstring& owner, const std::wstring& repo);
    static std::vector<GitHubBranchInfo> ListTags(const std::wstring& owner, const std::wstring& repo);

    static std::vector<GitHubReleaseInfo> ListReleases(const std::wstring& owner, const std::wstring& repo);

    static bool StarRepo(const std::wstring& owner, const std::wstring& repo);
    static bool UnstarRepo(const std::wstring& owner, const std::wstring& repo);
    static bool IsStarred(const std::wstring& owner, const std::wstring& repo);
    static bool WatchRepo(const std::wstring& owner, const std::wstring& repo);
    static bool UnwatchRepo(const std::wstring& owner, const std::wstring& repo);

    static std::vector<GitHubRepoInfo> ListStarredRepos(int page = 1);
    static std::vector<GitHubRepoInfo> ListWatchedRepos(int page = 1);

    static std::vector<GitHubNotificationInfo> ListNotifications(bool all = false, int page = 1);
    static bool MarkNotificationsRead();
    static std::vector<GitHubGistInfo> ListGists(const std::wstring& username = L"", int page = 1);

    static bool DownloadArchive(const std::wstring& owner, const std::wstring& repo,
                                const std::wstring& ref, const std::wstring& format,
                                std::vector<BYTE>& outData);

    static std::vector<GitHubFileInfo> ListDirectory(const std::wstring& owner, const std::wstring& repo,
                                                      const std::wstring& path, const std::wstring& ref = L"");
    static bool GetFileInfo(const std::wstring& owner, const std::wstring& repo,
                            const std::wstring& path, GitHubFileInfo& outInfo, const std::wstring& ref = L"");

    static bool DownloadFile(const std::wstring& owner, const std::wstring& repo,
                             const std::wstring& path, std::vector<BYTE>& outData, const std::wstring& ref = L"");
    static bool DownloadFileByUrl(const std::wstring& owner, const std::wstring& repo,
                                   const std::wstring& path, std::vector<BYTE>& outData, const std::wstring& ref = L"");
    static bool UploadFile(const std::wstring& owner, const std::wstring& repo,
                           const std::wstring& path, const std::vector<BYTE>& data,
                           const std::wstring& message, const std::wstring& branch = L"");
    static bool DeleteGitHubFile(const std::wstring& owner, const std::wstring& repo,
                           const std::wstring& path, const std::wstring& message, const std::wstring& sha);
    static bool CreateGitHubDir(const std::wstring& owner, const std::wstring& repo,
                                const std::wstring& path, const std::wstring& message, const std::wstring& branch = L"");
    static bool MoveGitHubFile(const std::wstring& owner, const std::wstring& repo,
                         const std::wstring& oldPath, const std::wstring& newPath,
                         const std::wstring& message, const std::wstring& sha, const std::wstring& branch = L"");

    static std::wstring GetDefaultBranch(const std::wstring& owner, const std::wstring& repo);

    static bool TestConnection(std::wstring& outUsername);
    static bool StartOAuthDeviceFlow(std::wstring& outDeviceCode, std::wstring& outUserCode, std::wstring& outVerifyUrl);
    static bool PollOAuthToken(const std::wstring& deviceCode, std::wstring& outToken);

    static std::string WideToUtf8(const std::wstring& wide);
    static std::wstring Utf8ToWide(const std::string& utf8);
    static std::wstring UrlEncode(const std::wstring& value);
    static std::string UrlEncodeA(const std::string& value);
    static std::wstring UrlEncodePathSegments(const std::wstring& path);
    static std::string UrlEncodePathSegmentsA(const std::string& path);
    static std::string JsonEscape(const std::string& s);
    static std::wstring Base64EncodeW(const std::vector<BYTE>& data);
    static std::string Base64EncodeA(const std::vector<BYTE>& data);
    static std::vector<BYTE> Base64DecodeA(const std::string& encoded);

    static void InvalidateCache();
    static void ResetSession();

    static FILETIME Iso8601ToFileTime(const std::string& iso8601);
    static FILETIME ParseJsonFileTime(const std::string& json, const std::string& key);
    static std::wstring ParseJsonString(const std::string& json, const std::string& key);
    static int ParseJsonInt(const std::string& json, const std::string& key);
    static int64_t ParseJsonInt64(const std::string& json, const std::string& key);
    static bool ParseJsonBool(const std::string& json, const std::string& key);

    template<typename T>
    static std::vector<T> ParseJsonArray(const std::string& json, std::function<T(const std::string&)> parser) {
        std::vector<T> result;
        size_t arrStart = json.find('[');
        if (arrStart == std::string::npos) return result;
        size_t pos = arrStart + 1;
        while (pos < json.length()) {
            size_t objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = FindJsonObjectEnd(json, objStart);
            if (objEnd == std::string::npos) break;
            result.push_back(parser(json.substr(objStart, objEnd - objStart)));
            pos = objEnd;
        }
        return result;
    }

    template<typename T>
    static std::vector<T> ParseJsonArrayAfterKey(const std::string& json, const std::string& key,
                                                   std::function<T(const std::string&)> parser) {
        std::string itemsKey = "\"" + key + "\"";
        size_t itemsPos = json.find(itemsKey);
        if (itemsPos == std::string::npos) return {};
        size_t arrStart = json.find('[', itemsPos);
        if (arrStart == std::string::npos) return {};
        size_t pos = arrStart + 1;
        std::vector<T> result;
        while (pos < json.length()) {
            size_t objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = FindJsonObjectEnd(json, objStart);
            if (objEnd == std::string::npos) break;
            result.push_back(parser(json.substr(objStart, objEnd - objStart)));
            pos = objEnd;
        }
        return result;
    }

    static size_t FindJsonObjectEnd(const std::string& s, size_t objStart);

private:
    static HINTERNET s_hSession;
    static HINTERNET s_hConnect;
    static GitHubConfig s_config;
    static std::mutex s_cacheMutex;
    static std::mutex s_configMutex;
    static std::mutex s_sessionMutex;

    struct DirCacheEntry {
        std::vector<GitHubFileInfo> files;
        ULONGLONG timestamp;
    };
    struct RepoCacheEntry {
        GitHubRepoInfo info;
        ULONGLONG timestamp;
    };
    struct SearchCacheEntry {
        GitHubSearchResult repoResult;
        GitHubCodeSearchResult codeResult;
        ULONGLONG timestamp;
    };
    static std::unordered_map<std::wstring, DirCacheEntry> s_dirCache;
    static std::unordered_map<std::wstring, RepoCacheEntry> s_repoCache;
    static std::unordered_map<std::wstring, SearchCacheEntry> s_searchCache;

    static std::string SendHttpGet(const std::wstring& path);
    static std::string SendHttpGetImpl(const std::wstring& path);
    static std::vector<BYTE> SendHttpGetBinary(const std::wstring& path);
    static std::string SendHttpDelete(const std::wstring& path);
    static std::string SendHttpPut(const std::wstring& path, const std::string& body);
    static std::string SendHttpPatch(const std::wstring& path, const std::string& body);
    static std::string SendHttpPost(const std::wstring& host, const std::wstring& path, const std::string& body,
                                     const std::wstring& extraHeaders = L"");

    struct HttpRequestOpts {
        bool hasBody = false;
        bool addContentType = false;
        bool addAcceptV3 = true;
        bool handle204AsSpace = false;
        bool lenientErrors = false;
    };
    static std::string SendHttpRequest(const std::wstring& method, const std::wstring& path,
                                        const std::string& body, const HttpRequestOpts& opts);

    static bool EnsureSession();
    static std::wstring GetAuthHeader();
};
