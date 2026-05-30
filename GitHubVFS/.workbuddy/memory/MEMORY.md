# GitHubVFS 项目记忆

## 项目概况
- Directory Opus VFS 插件，用 C++ 编写，浏览 GitHub 仓库
- 构建系统：compile.bat + MSVC 2022 BuildTools (cl.exe)
- 关键源文件：`src/GitHubClient.cpp`（API客户端）、`src/GitHubVFS.cpp`（VFS入口）、`src/GitHubClient.h`
- 链接库需要：Advapi32 Crypt32 Shell32 Comctl32 Winhttp User32 Gdi32 Comdlg32

## 2026-05-16 批量修复
修复了 12 个 bug，全部编译通过并部署：
1. **UrlEncode 路径分隔符**：添加 `UrlEncodePathSegments()` 方法，仅编码各段但保留 `/`
2. **同名仓库歧义**：Starred/Subscriptions/Repos 中同名仓库使用 `ownerFilter` 过滤
3. **UTF-16 代理对**：`ParseJsonString` 支持 `\uD800-\uDBFF` + `\uDC00-\uDFFF` 代理对
4. **缓存竞态**：确认原 `g_reposLoading` 已在锁内设置无竞态，`PreloadReposThread` 加异常保护
5. **cFileName 截断**：添加 `SafeCopyFileName()` 函数，截断时加 `...` 提示
6. **Iso8601 往返**：添加 `ParseJsonFileTime()` 直接从 JSON 解析时间，跳过 wstring↔string 转换
7. **DetectRefType 冗余**：合并两个重复条件为一个
8. **死代码清理**：删除未使用的 `IsReservedWord()` 和 `IsRepoReservedWord()`
9. **@ref 检测**：`seg1[0] == L'@'` 替代 `substr(0,1) == L"@"`
10. **LogToFile 溢出**：动态构建日志行，限制单行 4096 字符，前向声明消除 C4717 警告
11. **Search page:N 冲突**：分离 `page:N` 和重定向逻辑，用 `contentSegs` 过滤分页段
12. **ParseJsonFileTime 提取**：同 #6，替换 20 处 `Iso8601ToFileTime(WideToUtf8(ParseJsonString(...)))`

## 2026-05-16 ParseGitHubPath 重构
将 542 行 ParseGitHubPath 拆分为 80 行主函数 + 11 个辅助函数，消除代码重复：
- ExtractPathWithoutPrefix, ParseQueryString, JoinPathSegments, GetOwnerContextType
- ResolveFlatRepo（消除 Repos/Starred/Subscriptions 3处扁平化重复）
- ParseRepoSubdir, ParseRefSyntax（消除 seg1/seg2 2处重复）
- ParseSearchPath, ParseTrendingPath, ParseRootPath, ParseOwnerRepoPath

第二轮审查修复 13 个 Bug：
1. 前向声明位置错误（移到 GitHubPathInfo 定义之后）
2. 多余闭合括号
3. 辅助函数无边界检查（ResolveFlatRepo/ParseRepoSubdir/ParseRefSyntax 添加防御）
4. metaItem 从未被设置（Gists/Notifications 子路径信息丢失）
5. Gists/Notifications 子路径完全未处理
6. page:N 在非搜索路径中不生效
7. int vs size_t 类型安全（offset 参数改为 size_t）
8. ParseQueryString URL 解码不完整（重写查询字符串解析）
9. Search/Code、Search/Commits 重定向缺失
10. Releases/{tagName}/ 子路径未处理
11. Code 子路径 isDir 语义
12. ParsePageSegment 用 compare() 替代 substr()
13. GetOwnerContextType 改用 unordered_map

## 编译注意
- `compile.bat` 直接调用 cl.exe，需要 vcvarsall.bat 环境
- 需要链接 `Comdlg32.lib`（GetSaveFileNameW），已添加到 compile.bat
- C4717 递归警告需用前向声明消除
