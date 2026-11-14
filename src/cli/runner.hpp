#pragma once

#include <memory>
#include <string>

#include "cli/registry.hpp"
#include "core/api_client.hpp"
#include "core/auth.hpp"
#include "core/config.hpp"
#include "net/transport.hpp"

namespace noldoj::cli {

struct Runtime {
    GlobalOptions opts;
    AppConfig config;
    CredentialStore credentials;
    SiteConfig site;

    std::string configPath;
    std::string credentialsPath;

    std::unique_ptr<IHttpTransport> transport;
    std::unique_ptr<TokenProvider> tokens;
    std::unique_ptr<ApiClient> api;
    CredentialEntry* credential = nullptr;

    void persistCredentials();

    // 便捷调用：JSON 请求
    ApiResponse requestJson(const std::string& method, const std::string& path,
                            const std::vector<std::pair<std::string, std::string>>& query,
                            const nlohmann::json* body, bool attachAuth);
};

int run(int argc, char** argv);

}  // namespace noldoj::cli
