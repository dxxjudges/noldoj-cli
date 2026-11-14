#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "core/config.hpp"

namespace noldoj {

// 令牌提供者抽象。当前仅支持私人令牌（长期有效，无需刷新）：
//   StoredTokenProvider —— 从站点凭证文件读取 personal_token
//   StaticTokenProvider —— 命令行 --access-token 临时覆盖
class TokenProvider {
public:
    virtual ~TokenProvider() = default;

    virtual std::string mode() const = 0;
    virtual std::optional<std::string> rawToken() const = 0;
    virtual std::optional<std::string> bearer() const = 0;
    virtual void persist() {}
};

std::unique_ptr<TokenProvider> makeStoredTokenProvider(CredentialEntry& entry,
                                                        std::function<void()> persistCallback);
std::unique_ptr<TokenProvider> makeStaticTokenProvider(std::string token);

}  // namespace noldoj
