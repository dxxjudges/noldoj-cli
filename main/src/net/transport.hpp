#pragma once

#include <memory>

#include "core/types.hpp"

namespace noldoj {

// 按平台创建 HTTP(S) 传输实现：
//   Windows（x86/x64/arm64）—— WinHTTP / Schannel，无第三方依赖；
//   Linux（x86/x64/arm64）、Android（Termux arm64）—— libcurl。
std::unique_ptr<IHttpTransport> makeHttpTransport();

}  // namespace noldoj
