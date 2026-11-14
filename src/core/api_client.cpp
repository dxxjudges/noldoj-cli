#include "core/api_client.hpp"

#include <cctype>
#include <iostream>

#include "core/auth.hpp"
#include "core/url.hpp"

namespace noldoj {

ApiClient::ApiClient(IHttpTransport& transport, SiteConfig site, TokenProvider* provider, bool verbose)
    : transport_(transport), site_(std::move(site)), provider_(provider), verbose_(verbose) {}

ApiResponse ApiClient::parseResponse(const HttpResponse& response) const {
    ApiResponse parsed;
    parsed.status = response.status;
    parsed.rawBody = response.body;
    parsed.setCookies = response.setCookies;
    parsed.rawHeaders = response.rawHeaders;
    if (!response.body.empty()) {
        try {
            parsed.data = nlohmann::json::parse(response.body);
            parsed.isJson = true;
        } catch (const std::exception&) {
            parsed.isJson = false;
        }
    }
    return parsed;
}

ApiResponse ApiClient::call(const ApiCall& call) {
    HttpRequest request;
    request.method = call.method;
    request.url = joinUrl(site_.base_url, call.path) + buildQuery(call.query);
    request.timeoutMs = site_.timeout_ms;
    request.body = call.body;

    // 自定义头先入列，后面的默认头仅在未出现同名头时补充，保证 -H 可以覆盖默认值。
    for (const auto& [name, value] : call.extraHeaders) {
        request.headers.emplace_back(name, value);
    }
    const auto hasHeader = [&request](const std::string& wanted) {
        for (const auto& [name, _] : request.headers) {
            if (name.size() != wanted.size()) continue;
            bool same = true;
            for (std::size_t i = 0; i < name.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(name[i])) !=
                    std::tolower(static_cast<unsigned char>(wanted[i]))) {
                    same = false;
                    break;
                }
            }
            if (same) return true;
        }
        return false;
    };

    if (!call.contentType.empty() && !hasHeader("Content-Type")) {
        request.headers.emplace_back("Content-Type", call.contentType);
    }
    if (!hasHeader("Accept")) {
        request.headers.emplace_back("Accept", "application/json, text/csv, application/zip, */*");
    }

    if (call.attachAuth && provider_) {
        if (const auto bearer = provider_->bearer()) {
            request.headers.emplace_back("Authorization", *bearer);
        }
    }

    if (verbose_) {
        std::cerr << "> " << request.method << " " << request.url << "\n";
    }

    const HttpResponse response = transport_.send(request);
    ApiResponse parsed = parseResponse(response);

    if (verbose_) {
        std::cerr << "< HTTP " << parsed.status << "\n";
    }
    return parsed;
}

}  // namespace noldoj
