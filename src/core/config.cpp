#include "core/config.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "core/crypto.hpp"
#include "core/keyring.hpp"
#include "json.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

namespace noldoj {
namespace {

#ifdef _WIN32
std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                        nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}
#endif

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("无法读取文件: " + path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::string& content) {
    const fs::path filePath(path);
    if (filePath.has_parent_path()) fs::create_directories(filePath.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("无法写入文件: " + path);
    out << content;
}

json defaultConfigJson() {
    json sites = json::object();
    json local;
    local["base_url"] = "http://localhost:3000/api/v1";
    local["default_language"] = "cpp";
    local["timeout_ms"] = 30000;
    sites["local"] = local;
    json config;
    config["default_site"] = "local";
    config["sites"] = sites;
    return config;
}

}  // namespace

std::string defaultConfigDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buffer) == S_OK) {
        return toUtf8(std::wstring(buffer)) + "\\.noldoj-cli";
    }
    return ".noldoj-cli";
#else
    // Linux / Android(Termux) 下使用 $HOME，避免依赖平台专属 API。
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        home = std::getenv("USERPROFILE");
    }
    const std::string base = (home != nullptr && *home != '\0') ? home : ".";
    return (fs::path(base) / ".noldoj-cli").string();
#endif
}

AppConfig loadConfig(const std::string& path, bool createIfMissing, std::string* createdHint) {
    AppConfig config;
    if (!fs::exists(path)) {
        if (!createIfMissing) throw std::runtime_error("配置文件不存在: " + path);
        writeFile(path, defaultConfigJson().dump(2));
        if (createdHint) *createdHint = path;
    }

    json raw;
    try {
        raw = json::parse(readFile(path));
    } catch (const std::exception& e) {
        throw std::runtime_error("配置文件解析失败 (" + path + "): " + e.what());
    }

    config.default_site = raw.value("default_site", std::string());
    if (raw.contains("sites") && raw["sites"].is_object()) {
        for (auto it = raw["sites"].begin(); it != raw["sites"].end(); ++it) {
            SiteConfig site;
            site.name = it.key();
            site.base_url = it.value().value("base_url", std::string());
            site.default_language = it.value().value("default_language", std::string("cpp"));
            site.timeout_ms = it.value().value("timeout_ms", 30000);
            if (!site.base_url.empty()) config.sites[it.key()] = site;
        }
    }
    return config;
}

void saveConfig(const std::string& path, const AppConfig& config) {
    json raw;
    raw["default_site"] = config.default_site;
    json sites = json::object();
    for (const auto& [name, site] : config.sites) {
        json entry;
        entry["base_url"] = site.base_url;
        entry["default_language"] = site.default_language;
        entry["timeout_ms"] = site.timeout_ms;
        sites[name] = entry;
    }
    raw["sites"] = sites;
    writeFile(path, raw.dump(2));
}

CredentialStore loadCredentials(const std::string& path) {
    CredentialStore store;
    if (!fs::exists(path)) return store;

    const std::string content = readFile(path);
    const std::string configDir = fs::path(path).parent_path().string();

    json raw;
    bool plaintext = false;
    if (crypto::isEnvelope(content)) {
        MasterKeyInfo info;
        if (!tryLoadMasterKey(configDir, &info)) {
            throw std::runtime_error(
                "找不到用于解密凭证的主密钥（系统凭据库与本机 master.key 均无记录）。\n"
                "请重新执行 `noldoj site add ... --token <pat>` 写入凭据，或删除 " + path + " 后重新配置。");
        }
        raw = json::parse(crypto::unseal(info.key, content));
    } else {
        try {
            raw = json::parse(content);
        } catch (const std::exception& e) {
            throw std::runtime_error("凭证文件解析失败 (" + path + "): " + e.what());
        }
        plaintext = true;
    }

    if (!raw.contains("sites") || !raw["sites"].is_object()) return store;

    for (auto it = raw["sites"].begin(); it != raw["sites"].end(); ++it) {
        CredentialEntry entry;
        entry.personal_token = it.value().value("personal_token", std::string());
        store.sites[it.key()] = entry;
    }

    // 旧版本以明文保存，读取后立即迁移为加密存储。
    if (plaintext) saveCredentials(path, store);
    return store;
}

void saveCredentials(const std::string& path, const CredentialStore& store) {
    json raw;
    json sites = json::object();
    for (const auto& [name, entry] : store.sites) {
        json item;
        item["personal_token"] = entry.personal_token;
        sites[name] = item;
    }
    raw["sites"] = sites;

    const MasterKeyInfo info = loadOrCreateMasterKey(fs::path(path).parent_path().string());
    writeFile(path, crypto::seal(info.key, raw.dump(2)));
    // 加密后仅本人可读写。
#ifndef _WIN32
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
}

std::string masterKeySource(const std::string& configDir) {
    MasterKeyInfo info;
    if (!tryLoadMasterKey(configDir, &info)) return {};
    return info.source;
}

std::string normalizeBaseUrl(const std::string& url) {
    std::string base = url;
    while (!base.empty() && base.back() == '/') base.pop_back();

    const std::size_t schemeEnd = base.find("://");
    if (schemeEnd == std::string::npos) return base;
    const std::size_t pathStart = base.find('/', schemeEnd + 3);
    if (pathStart == std::string::npos || pathStart + 1 >= base.size()) {
        return base + "/api/v1";
    }
    return base;
}

bool looksLikeUrl(const std::string& value) {
    return value.find("://") != std::string::npos ||
           (value.size() >= 4 && (value.compare(0, 4, "http") == 0));
}

SiteConfig resolveSite(const AppConfig& config, const std::string& siteArg) {
    if (siteArg.empty()) {
        if (config.default_site.empty()) {
            throw std::runtime_error("未指定站点，且配置中没有 default_site。请使用 --site 或 `noldoj site use <name>`。");
        }
        auto it = config.sites.find(config.default_site);
        if (it == config.sites.end()) {
            throw std::runtime_error("默认站点 `" + config.default_site + "` 未在配置中定义。");
        }
        SiteConfig site = it->second;
        site.name = it->first;
        return site;
    }

    auto it = config.sites.find(siteArg);
    if (it != config.sites.end()) {
        SiteConfig site = it->second;
        site.name = it->first;
        return site;
    }

    if (looksLikeUrl(siteArg)) {
        SiteConfig site;
        site.base_url = normalizeBaseUrl(siteArg);
        site.name = site.base_url;
        return site;
    }

    std::string known;
    for (const auto& [name, _] : config.sites) {
        if (!known.empty()) known += ", ";
        known += name;
    }
    throw std::runtime_error("未知站点 `" + siteArg + "`。已配置站点: " + (known.empty() ? "（无）" : known));
}

}  // namespace noldoj
