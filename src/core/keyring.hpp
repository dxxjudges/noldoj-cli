#pragma once

#include <string>

#include "core/crypto.hpp"

namespace noldoj {

struct MasterKeyInfo {
    crypto::Key key{};
    // 存放位置描述，例如 "windows-credential-manager"、"macos-keychain"、"file:/path/master.key"
    std::string source;
};

// 读取主密钥；不存在时返回 false（不会创建）。
bool tryLoadMasterKey(const std::string& configDir, MasterKeyInfo* out);

// 读取主密钥；不存在时生成 256 位随机密钥并写入系统凭据库，
// 凭据库不可用时回退到配置目录下权限为 0600 的 master.key 文件。
MasterKeyInfo loadOrCreateMasterKey(const std::string& configDir);

// 当前平台系统凭据库的名称；为空表示该平台没有可用的凭据库。
std::string systemKeyringName();

}  // namespace noldoj
