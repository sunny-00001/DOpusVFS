#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "DataStructs.h"
#include "vfs_plugins.h"

struct MenuContext {
    bool isRoot;
    bool isRemoteRoot;
    bool isDir;
    bool isSelectedDir;
    std::wstring fs;
    std::wstring remote;
    std::wstring remoteType;
    RcloneBackendFeatures features;
    bool hasId;
};

class MenuManager {
public:
    static bool BuildContextMenu(const MenuContext& ctx, LPVFSCONTEXTMENUDATAW lpMenuData);
    static int HandleVerb(const MenuContext& ctx, LPVFSFUNCDATA lpFuncData, LPVFSCONTEXTVERBDATAW lpVerbData);

private:
    static std::vector<VFSCONTEXTMENUITEMW> BuildMenuItems(const MenuContext& ctx);
};
