#include "net/curl_transport.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include <curl/curl.h>

#include "net/transport.hpp"

namespace noldoj {
namespace {

struct ResponseState {
    std::string body;
    std::string headers;
};

// 分段上传的读取状态：内存片段直接拷贝，文件片段按需打开并流式读取，
// 避免把最大 200MB 的测试数据 ZIP 一次性读入内存。
struct UploadState {
    const RequestBody* body = nullptr;
    std::size_t segment = 0;
    std::size_t offset = 0;
    std::ifstream file;
    std::string error;
};

size_t writeBody(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<ResponseState*>(userdata);
    state->body.append(data, size * nmemb);
    return size * nmemb;
}

size_t writeHeader(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<ResponseState*>(userdata);
    state->headers.append(data, size * nmemb);
    return size * nmemb;
}

size_t readUpload(char* buffer, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<UploadState*>(userdata);
    const std::size_t want = size * nmemb;
    std::size_t written = 0;

    while (written < want && state->segment < state->body->segments.size()) {
        const BodySegment& segment = state->body->segments[state->segment];

        if (segment.kind == BodySegment::Kind::Memory) {
            const std::size_t available = segment.data.size() - state->offset;
            const std::size_t chunk = (std::min)(available, want - written);
            if (chunk > 0) {
                std::memcpy(buffer + written, segment.data.data() + state->offset, chunk);
                written += chunk;
                state->offset += chunk;
            }
            if (state->offset >= segment.data.size()) {
                ++state->segment;
                state->offset = 0;
            }
            continue;
        }

        if (!state->file.is_open()) {
            state->file.open(segment.path, std::ios::binary);
            if (!state->file) {
                state->error = "无法读取文件: " + segment.path;
                return 0;
            }
        }

        const std::size_t remaining = segment.length > state->offset ? segment.length - state->offset : 0;
        const std::size_t chunk = (std::min)(remaining, want - written);
        if (chunk == 0) {
            ++state->segment;
            state->offset = 0;
            state->file.close();
            continue;
        }

        state->file.read(buffer + written, static_cast<std::streamsize>(chunk));
        const std::size_t got = static_cast<std::size_t>(state->file.gcount());
        if (got == 0) {
            state->error = "文件读取中断: " + segment.path;
            return 0;
        }
        written += got;
        state->offset += got;
        if (state->offset >= segment.length) {
            ++state->segment;
            state->offset = 0;
            state->file.close();
        }
    }
    return written;
}

// curl_global_init 只应执行一次。
void ensureCurlGlobal() {
    static const bool initialized = []() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw std::runtime_error("初始化 libcurl 失败");
        }
        return true;
    }();
    (void)initialized;
}

struct CurlHandle {
    CURL* handle = nullptr;
    ~CurlHandle() {
        if (handle != nullptr) curl_easy_cleanup(handle);
    }
};

struct HeaderList {
    curl_slist* list = nullptr;
    ~HeaderList() {
        if (list != nullptr) curl_slist_free_all(list);
    }

    void append(const std::string& line) {
        curl_slist* next = curl_slist_append(list, line.c_str());
        if (next == nullptr) throw std::runtime_error("构造请求头失败: " + line);
        list = next;
    }
};

}  // namespace

HttpResponse CurlTransport::send(const HttpRequest& request) {
    ensureCurlGlobal();

    CurlHandle easy;
    easy.handle = curl_easy_init();
    if (easy.handle == nullptr) throw std::runtime_error("创建 libcurl 句柄失败");

    HeaderList headerList;
    for (const auto& [name, value] : request.headers) {
        headerList.append(name + ": " + value);
    }

    ResponseState response;
    UploadState upload;
    upload.body = &request.body;

    CURL* handle = easy.handle;
    curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS,
                     static_cast<long>(request.timeoutMs > 0 ? request.timeoutMs : 30000));
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &response);
    if (headerList.list != nullptr) {
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerList.list);
    }

    const std::size_t totalBytes = request.body.totalBytes();
    if (totalBytes > 0) {
        const bool singleMemory = request.body.segments.size() == 1 &&
                                  request.body.segments.front().kind == BodySegment::Kind::Memory;
        if (singleMemory) {
            const std::string& data = request.body.segments.front().data;
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(data.size()));
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data.c_str());
        } else {
            // POST + READFUNCTION 配合 CUSTOMREQUEST 支持 PUT/PATCH 等任意带体方法。
            curl_easy_setopt(handle, CURLOPT_POST, 1L);
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(totalBytes));
            curl_easy_setopt(handle, CURLOPT_READFUNCTION, readUpload);
            curl_easy_setopt(handle, CURLOPT_READDATA, &upload);
        }
    }

    if (request.method == "HEAD") {
        curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
    } else if (request.method != "GET") {
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    }

    const CURLcode code = curl_easy_perform(handle);

    long statusCode = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);

    if (code != CURLE_OK) {
        if (!upload.error.empty()) throw std::runtime_error(upload.error);
        throw std::runtime_error("HTTP 请求失败: " + std::string(curl_easy_strerror(code)));
    }

    HttpResponse result;
    result.status = static_cast<int>(statusCode);
    result.rawHeaders = std::move(response.headers);
    result.body = std::move(response.body);
    return result;
}

std::unique_ptr<IHttpTransport> makeHttpTransport() {
    return std::make_unique<CurlTransport>();
}

}  // namespace noldoj
