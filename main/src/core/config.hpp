#pragma once

#include <map>
#include <string>
#include <vector>

namespace noldoj {

struct SiteConfig {
    std::string name;                       // 站点键名（配置中的名字或 ad-hoc URL）
    std::string base_url;                   // 例如 http://localhost:3000/api/v1
    std::string default_language = "cpp";
    int timeout_ms = 30000;
};

struct AppConfig {
    std::string default_site;
    std::map<std::string, SiteConfig> sites;
};

// 单个站点的凭证。CLI 仅使用私人令牌（长期有效的 access token，无需刷新）。
struct CredentialEntry {
    std::string personal_token;

    bool empty() const { return personal_token.empty(); }
};

struct CredentialStore {
    std::map<std::string, CredentialEntry> sites;  // key = 站点名 / 规范化后的 URL
};

// 默认配置目录：%USERPROFILE%\.noldoj-cli
std::string defaultConfigDir();

// createIfMissing 为真且文件不存在时写入默认配置；解析失败时抛出异常。
AppConfig loadConfig(const std::string& path, bool createIfMissing, std::string* createdHint);
void saveConfig(const std::string& path, const AppConfig& config);

CredentialStore loadCredentials(const std::string& path);
void saveCredentials(const std::string& path, const CredentialStore& store);

// 依据 --site 参数（站点名或完整 URL）解析出实际使用的站点。
// 空参数时使用配置中的 default_site；未配置的站点名报错；URL 形式允许直接使用。
SiteConfig resolveSite(const AppConfig& config, const std::string& siteArg);

// 规范化 base_url：去掉尾部斜杠；若路径为空或仅有 "/" 则补上 /api/v1。
std::string normalizeBaseUrl(const std::string& url);

// 判断字符串是否是 http(s) 地址。
bool looksLikeUrl(const std::string& value);

}  // namespace noldoj
