#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "net/winhttp_transport.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/url.hpp"
#include "net/transport.hpp"

namespace noldoj {
namespace {

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), size);
    return wide;
}

std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), size, nullptr,
                        nullptr);
    return utf8;
}

std::string trim(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void throwWinError(const std::string& action) {
    throw std::runtime_error("WinHTTP 调用失败（" + action + "），错误码: " + std::to_string(GetLastError()));
}

struct HandleCloser {
    HINTERNET handle = nullptr;
    ~HandleCloser() {
        if (handle) WinHttpCloseHandle(handle);
    }
};

// 从原始响应头中解析 Set-Cookie 的 "name=value" 部分。
void collectCookies(const std::string& rawHeaders, std::vector<std::pair<std::string, std::string>>& out) {
    std::size_t pos = 0;
    while (pos < rawHeaders.size()) {
        std::size_t lineEnd = rawHeaders.find("\r\n", pos);
        if (lineEnd == std::string::npos) lineEnd = rawHeaders.size();
        std::string line = rawHeaders.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (lower(trim(line.substr(0, colon))) != "set-cookie") continue;

        std::string cookie = trim(line.substr(colon + 1));
        const std::size_t semi = cookie.find(';');
        if (semi != std::string::npos) cookie = cookie.substr(0, semi);
        const std::size_t eq = cookie.find('=');
        if (eq == std::string::npos) continue;
        out.emplace_back(trim(cookie.substr(0, eq)), trim(cookie.substr(eq + 1)));
    }
}

bool writeSegment(HINTERNET request, const BodySegment& segment, std::string& error) {
    if (segment.kind == BodySegment::Kind::Memory) {
        if (segment.data.empty()) return true;
        DWORD written = 0;
        if (!WinHttpWriteData(request, segment.data.data(), static_cast<DWORD>(segment.data.size()), &written)) {
            error = "上传请求体失败";
            return false;
        }
        return true;
    }

    std::ifstream in(segment.path, std::ios::binary);
    if (!in) {
        error = "无法读取文件: " + segment.path;
        return false;
    }
    char buffer[1024 * 1024];
    std::size_t remaining = segment.length;
    while (remaining > 0) {
        const std::size_t want = (std::min)(remaining, sizeof(buffer));
        in.read(buffer, static_cast<std::streamsize>(want));
        const std::size_t got = static_cast<std::size_t>(in.gcount());
        if (got == 0) {
            error = "文件读取中断: " + segment.path;
            return false;
        }
        DWORD written = 0;
        if (!WinHttpWriteData(request, buffer, static_cast<DWORD>(got), &written)) {
            error = "上传请求体失败";
            return false;
        }
        remaining -= got;
    }
    return true;
}

}  // namespace

HttpResponse WinHttpTransport::send(const HttpRequest& request) {
    const ParsedUrl parsed = parseUrl(request.url);

    HandleCloser session;
    session.handle = WinHttpOpen(L"NoldOJ-CLI/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) throwWinError("WinHttpOpen");

    const int timeout = request.timeoutMs > 0 ? static_cast<int>(request.timeoutMs) : 30000;
    WinHttpSetTimeouts(session.handle, timeout, timeout, timeout, timeout);

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    HandleCloser connection;
    connection.handle = WinHttpConnect(session.handle, toWide(parsed.host).c_str(),
                                       static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!connection.handle) throwWinError("WinHttpConnect");

    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HandleCloser requestHandle;
    requestHandle.handle = WinHttpOpenRequest(connection.handle, toWide(request.method).c_str(),
                                              toWide(parsed.target).c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!requestHandle.handle) throwWinError("WinHttpOpenRequest");

    const std::size_t totalBytes = request.body.totalBytes();

    // 统一附加请求头（含 Content-Length），避免分块传输导致的服务端解析差异。
    std::wstring headerBlock;
    for (const auto& [name, value] : request.headers) {
        headerBlock += toWide(name) + L": " + toWide(value) + L"\r\n";
    }
    headerBlock += L"Content-Length: " + std::to_wstring(totalBytes) + L"\r\n";

    if (!headerBlock.empty()) {
        WinHttpAddRequestHeaders(requestHandle.handle, headerBlock.c_str(),
                                 static_cast<DWORD>(headerBlock.size()),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    const bool hasBody = totalBytes > 0;
    if (!WinHttpSendRequest(requestHandle.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                            static_cast<DWORD_PTR>(totalBytes), 0)) {
        throwWinError("WinHttpSendRequest");
    }

    if (hasBody) {
        for (const auto& segment : request.body.segments) {
            std::string error;
            if (!writeSegment(requestHandle.handle, segment, error)) {
                throw std::runtime_error(error);
            }
        }
    }

    if (!WinHttpReceiveResponse(requestHandle.handle, nullptr)) throwWinError("WinHttpReceiveResponse");

    HttpResponse response;
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(requestHandle.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        response.status = static_cast<int>(statusCode);
    }

    DWORD headerSize = 0;
    DWORD headerIndex = 0;
    WinHttpQueryHeaders(requestHandle.handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
                        &headerSize, &headerIndex);
    if (headerSize > 0) {
        std::wstring raw(headerSize / sizeof(wchar_t), L'\0');
        headerIndex = 0;
        if (WinHttpQueryHeaders(requestHandle.handle, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &headerSize, &headerIndex)) {
            response.rawHeaders = toUtf8(raw);
            collectCookies(response.rawHeaders, response.setCookies);
        }
    }

    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(requestHandle.handle, &available)) throwWinError("WinHttpQueryDataAvailable");
        if (available == 0) break;

        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(requestHandle.handle, chunk.data(), available, &read)) {
            throwWinError("WinHttpReadData");
        }
        response.body.append(chunk.data(), read);
    } while (available > 0);

    return response;
}

std::unique_ptr<IHttpTransport> makeHttpTransport() {
    return std::make_unique<WinHttpTransport>();
}

}  // namespace noldoj
