#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/auth.hpp"
#include "core/config.hpp"
#include "core/types.hpp"
#include "json.hpp"

namespace noldoj {

struct ApiResponse {
    int status = 0;
    bool isJson = false;
    nlohmann::json data;
    std::string rawBody;
    std::vector<std::pair<std::string, std::string>> setCookies;
    std::string rawHeaders;
};

struct ApiCall {
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> query;
    RequestBody body;
    std::string contentType;  // 为空表示无请求体
    bool attachAuth = false;
    // 追加的自定义请求头（如 -H）。同名头优先使用这里指定的值。
    std::vector<std::pair<std::string, std::string>> extraHeaders;
};

// 负责：拼装 URL、附加认证头。
class ApiClient {
public:
    ApiClient(IHttpTransport& transport, SiteConfig site, TokenProvider* provider, bool verbose);

    ApiResponse call(const ApiCall& call);

    const SiteConfig& site() const { return site_; }

private:
    ApiResponse parseResponse(const HttpResponse& response) const;

    IHttpTransport& transport_;
    SiteConfig site_;
    TokenProvider* provider_;
    bool verbose_;
};

}  // namespace noldoj
