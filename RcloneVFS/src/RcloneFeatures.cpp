#include "RcloneFeatures.h"
#include "RcloneCache.h"
#include "RcloneClient.h"
#include "Utils.h"
#include "json.hpp"

using json = nlohmann::json;

RcloneBackendFeatures RcloneFeatures::Get(const std::wstring& fs, const std::wstring& remoteType) {
    RcloneBackendFeatures cached;
    if (RcloneCache::GetFeatures(fs, cached)) {
        return cached;
    }

    RcloneBackendFeatures features = LookupKnownType(remoteType);

    if (features.remoteType.empty()) {
        features.remoteType = remoteType;
    }

    if (features.supportsAbout == false && !remoteType.empty()) {
        features = DetectViaApi(fs);
        features.remoteType = remoteType;
    }

    RcloneCache::SetFeatures(fs, features);
    return features;
}

void RcloneFeatures::Invalidate(const std::wstring& fs) {
    RcloneCache::InvalidateFeatures(fs);
}

void RcloneFeatures::InvalidateAll() {
    RcloneCache::InvalidateAll();
}

RcloneBackendFeatures RcloneFeatures::LookupKnownType(const std::wstring& remoteType) {
    if (remoteType == L"drive") {
        return { true, true, true, true, true, {"md5", "sha1"}, L"drive" };
    }
    if (remoteType == L"onedrive") {
        return { true, true, true, false, true, {"sha1", "sha256"}, L"onedrive" };
    }
    if (remoteType == L"dropbox") {
        return { true, true, false, true, true, {"sha256"}, L"dropbox" };
    }
    if (remoteType == L"s3") {
        return { false, false, false, false, true, {"md5"}, L"s3" };
    }
    if (remoteType == L"swift") {
        return { false, false, false, false, true, {"md5"}, L"swift" };
    }
    if (remoteType == L"azureblob") {
        return { false, false, false, false, true, {"md5"}, L"azureblob" };
    }
    if (remoteType == L"pcloud") {
        return { true, true, false, false, true, {"md5", "sha1"}, L"pcloud" };
    }
    if (remoteType == L"mega") {
        return { true, false, false, false, false, {}, L"mega" };
    }
    if (remoteType == L"box") {
        return { true, true, true, false, true, {"sha1"}, L"box" };
    }
    if (remoteType == L"ftp") {
        return { false, false, false, false, false, {}, L"ftp" };
    }
    if (remoteType == L"sftp") {
        return { false, false, false, false, true, {"md5"}, L"sftp" };
    }
    if (remoteType == L"webdav") {
        return { false, false, false, false, false, {}, L"webdav" };
    }
    if (remoteType == L"local") {
        return { false, false, false, false, true, {"md5"}, L"local" };
    }
    if (remoteType == L"crypt") {
        return { false, false, false, false, false, {}, L"crypt" };
    }
    if (remoteType == L"union") {
        return { false, false, false, false, false, {}, L"union" };
    }
    if (remoteType == L"cache") {
        return { false, false, false, false, false, {}, L"cache" };
    }
    if (remoteType == L"chunker") {
        return { false, false, false, false, false, {}, L"chunker" };
    }
    if (remoteType == L"compress") {
        return { false, false, false, false, false, {}, L"compress" };
    }
    if (remoteType == L"alias") {
        return { false, false, false, false, false, {}, L"alias" };
    }
    if (remoteType == L"googlecloudstorage") {
        return { false, false, false, false, true, {"md5"}, L"googlecloudstorage" };
    }
    if (remoteType == L"b2") {
        return { true, false, false, false, true, {"sha1"}, L"b2" };
    }
    if (remoteType == L"jottacloud") {
        return { true, true, true, true, true, {"md5", "sha256"}, L"jottacloud" };
    }
    if (remoteType == L"storj") {
        return { false, false, false, false, false, {}, L"storj" };
    }
    if (remoteType == L"yandex") {
        return { true, true, false, false, true, {"md5", "sha256"}, L"yandex" };
    }

    return { false, false, false, false, false, {}, remoteType };
}

RcloneBackendFeatures RcloneFeatures::DetectViaApi(const std::wstring& fs) {
    RcloneBackendFeatures features;

    // 1. Probe About
    std::string aboutRes = RcloneClient::SendHttpPost("/operations/about",
        "{\"fs\":\"" + WideToUtf8(fs) + "\"}");
    if (!aboutRes.empty() && aboutRes.find("\"error\"") == std::string::npos) {
        features.supportsAbout = true;
    }

    // 2. Look up features from config/providers (has "Features" per provider)
    std::wstring remoteName = fs;
    while (!remoteName.empty() && remoteName.back() == L':') remoteName.pop_back();
    // First get the remote's type via config/get
    std::string configRes = RcloneClient::SendHttpPostFull("/config/get",
        "{\"name\":\"" + WideToUtf8(remoteName) + "\"}");
    std::string remoteType;
    if (!configRes.empty()) {
        try {
            auto j = json::parse(configRes);
            if (j.contains("type") && j["type"].is_string()) {
                remoteType = j["type"].get<std::string>();
            }
        } catch (...) {}
    }

    // Then look up the provider's Features from config/providers
    if (!remoteType.empty()) {
        std::string provRes = RcloneClient::SendHttpPostFull("/config/providers", "{}");
        if (!provRes.empty()) {
            try {
                auto pj = json::parse(provRes);
                if (pj.contains("providers") && pj["providers"].is_array()) {
                    for (const auto& p : pj["providers"]) {
                        if (p.contains("Name") && p["Name"].is_string() &&
                            p["Name"].get<std::string>() == remoteType) {
                            // Found matching provider — read Features
                            if (p.contains("Features") && p["Features"].is_object()) {
                                auto& feat = p["Features"];
                                features.supportsAbout = feat.value("CanAbout", features.supportsAbout);
                                features.supportsPublicLink = feat.value("CanShare", features.supportsPublicLink);
                                features.supportsTrash = feat.value("CanTrash", features.supportsTrash);
                                features.supportsVersions = feat.value("CanVersion", features.supportsVersions);
                            }
                            break;
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // 3. Probe PublicLink as fallback
    if (!features.supportsPublicLink) {
        std::string linkRes = RcloneClient::SendHttpPost("/operations/publiclink",
            "{\"fs\":\"" + WideToUtf8(fs) + "\",\"remote\":\"\"}");
        if (!linkRes.empty() && linkRes.find("\"error\"") == std::string::npos && linkRes.find("\"url\"") != std::string::npos) {
            features.supportsPublicLink = true;
        }
    }

    // 4. Probe Versions as fallback
    if (!features.supportsVersions && !remoteType.empty()) {
        std::string path = "/backend/" + remoteType + ":versions";
        std::string verRes = RcloneClient::SendHttpPostFull(path,
            "{\"fs\":\"" + WideToUtf8(fs) + "\",\"arg\":[],\"opt\":{}}");
        if (!verRes.empty() && verRes.find("\"error\"") == std::string::npos) {
            features.supportsVersions = true;
        }
    }

    // Note: Trash cannot be reliably probed via API; rely on providers or known mapping

    return features;
}
