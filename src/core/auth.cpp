#include "core/auth.hpp"

namespace noldoj {
namespace {

const std::string kBearerPrefix = "Bearer ";

class PersonalTokenProvider final : public TokenProvider {
public:
    PersonalTokenProvider(CredentialEntry& entry, std::function<void()> persist)
        : entry_(entry), persist_(std::move(persist)) {}

    std::string mode() const override { return "personal_token"; }

    std::optional<std::string> rawToken() const override {
        if (entry_.personal_token.empty()) return std::nullopt;
        return entry_.personal_token;
    }

    std::optional<std::string> bearer() const override {
        if (entry_.personal_token.empty()) return std::nullopt;
        return kBearerPrefix + entry_.personal_token;
    }

    void persist() override { persist_(); }

private:
    CredentialEntry& entry_;
    std::function<void()> persist_;
};

class StaticTokenProvider final : public TokenProvider {
public:
    explicit StaticTokenProvider(std::string token) : token_(std::move(token)) {}

    std::string mode() const override { return "personal_token"; }
    std::optional<std::string> rawToken() const override {
        if (token_.empty()) return std::nullopt;
        return token_;
    }
    std::optional<std::string> bearer() const override {
        if (token_.empty()) return std::nullopt;
        return kBearerPrefix + token_;
    }

private:
    std::string token_;
};

}  // namespace

std::unique_ptr<TokenProvider> makeStoredTokenProvider(CredentialEntry& entry,
                                                        std::function<void()> persistCallback) {
    return std::make_unique<PersonalTokenProvider>(entry, std::move(persistCallback));
}

std::unique_ptr<TokenProvider> makeStaticTokenProvider(std::string token) {
    return std::make_unique<StaticTokenProvider>(std::move(token));
}

}  // namespace noldoj
