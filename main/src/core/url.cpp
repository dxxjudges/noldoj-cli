#include "core/url.hpp"

#include <stdexcept>

namespace noldoj {

std::string percentEncode(const std::string& value) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string buildQuery(const std::vector<std::pair<std::string, std::string>>& query) {
    if (query.empty()) return {};
    std::string out = "?";
    bool first = true;
    for (const auto& [key, value] : query) {
        if (!first) out.push_back('&');
        first = false;
        out += percentEncode(key);
        out.push_back('=');
        out += percentEncode(value);
    }
    return out;
}

std::string joinUrl(const std::string& baseUrl, const std::string& path) {
    std::string base = baseUrl;
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (path.empty()) return base;
    if (path.front() == '/') return base + path;
    return base + "/" + path;
}

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl parsed;

    const std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) throw std::runtime_error("非法的服务端地址: " + url);

    const std::string scheme = url.substr(0, schemeEnd);
    parsed.secure = (scheme == "https");
    parsed.port = parsed.secure ? 443 : 80;

    const std::size_t authorityStart = schemeEnd + 3;
    const std::size_t pathStart = url.find('/', authorityStart);
    std::string authority = url.substr(authorityStart, pathStart == std::string::npos
                                                          ? std::string::npos
                                                          : pathStart - authorityStart);
    parsed.target = pathStart == std::string::npos ? "/" : url.substr(pathStart);
    if (parsed.target.empty()) parsed.target = "/";

    const std::size_t portSep = authority.rfind(':');
    if (portSep != std::string::npos && portSep + 1 < authority.size()) {
        const std::string maybePort = authority.substr(portSep + 1);
        bool allDigits = true;
        for (char c : maybePort) {
            if (c < '0' || c > '9') {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            parsed.host = authority.substr(0, portSep);
            parsed.port = std::stoi(maybePort);
            return parsed;
        }
    }
    parsed.host = authority;
    return parsed;
}

}  // namespace noldoj
