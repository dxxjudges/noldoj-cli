#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace noldoj {

// 请求体片段：支持内存片段与磁盘文件片段，避免大文件（测试数据 ZIP 最大 200MB）全部读入内存。
struct BodySegment {
    enum class Kind { Memory, File };

    Kind kind = Kind::Memory;
    std::string data;   // kind == Memory
    std::string path;   // kind == File
    std::size_t length = 0;
};

struct RequestBody {
    std::vector<BodySegment> segments;

    bool empty() const { return segments.empty(); }

    std::size_t totalBytes() const {
        std::size_t total = 0;
        for (const auto& segment : segments) total += segment.length;
        return total;
    }

    static RequestBody fromMemory(std::string data) {
        RequestBody body;
        BodySegment segment;
        segment.kind = BodySegment::Kind::Memory;
        segment.data = std::move(data);
        segment.length = segment.data.size();
        body.segments.push_back(std::move(segment));
        return body;
    }
};

struct HttpRequest {
    std::string method;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    RequestBody body;
    long timeoutMs = 30000;
};

struct HttpResponse {
    int status = 0;
    std::string rawHeaders;                                          // 原始响应头（CRLF 分隔）
    std::vector<std::pair<std::string, std::string>> setCookies;     // 解析后的 Set-Cookie
    std::string body;
};

// 网络传输接口：核心层只依赖该抽象，便于替换实现（WinHTTP / libcurl）。
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

}  // namespace noldoj
