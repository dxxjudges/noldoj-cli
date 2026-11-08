#pragma once

#include "core/types.hpp"

namespace noldoj {

// Windows 原生 HTTP(S) 客户端实现（WinHTTP / Schannel），无第三方依赖。
class WinHttpTransport final : public IHttpTransport {
public:
    HttpResponse send(const HttpRequest& request) override;
};

}  // namespace noldoj
