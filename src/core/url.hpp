#pragma once

#include <string>
#include <utility>
#include <vector>

namespace noldoj {

// RFC3986 非保留字符保留，其余百分号编码。
std::string percentEncode(const std::string& value);

// 生成 "?k=v&..."，参数为空时返回空串。
std::string buildQuery(const std::vector<std::pair<std::string, std::string>>& query);

// 拼接 base_url 与 API 路径（自动处理斜杠）。
std::string joinUrl(const std::string& baseUrl, const std::string& path);

struct ParsedUrl {
    bool secure = false;
    std::string host;
    int port = 80;
    std::string target;  // path + query
};

ParsedUrl parseUrl(const std::string& url);

}  // namespace noldoj
