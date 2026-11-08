#pragma once

#include "core/types.hpp"

namespace noldoj {

// 非 Windows 平台的 HTTP(S) 客户端实现（libcurl），
// 覆盖 Linux x86/x64/arm64 与 Android（Termux）arm64。
class CurlTransport final : public IHttpTransport {
public:
    HttpResponse send(const HttpRequest& request) override;
};

}  // namespace noldoj
