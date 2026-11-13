#pragma once

#include <array>
#include <string>

namespace noldoj::crypto {

// 128 位主密钥。
using Key = std::array<unsigned char, 16>;

// 从系统密码学随机源生成主密钥（Windows: BCryptGenRandom；POSIX: /dev/urandom）。
Key randomKey();

// 把主密钥编码为 32 位小写十六进制字符串，便于存入凭据库。
std::string keyToHex(const Key& key);
bool keyFromHex(const std::string& hex, Key* out);

// 使用 AES-128-CTR 加密 + HMAC-SHA256 认证，返回可直接落盘的 JSON 信封：
//   {"version":1,"alg":"aes-128-ctr+hmac-sha256","iv":"...","ct":"...","mac":"..."}
std::string seal(const Key& key, const std::string& plaintext);

// 解析信封并校验 MAC，失败时抛出异常（口令不匹配或文件被篡改）。
std::string unseal(const Key& key, const std::string& envelope);

// 是否为 seal() 生成的信封。
bool isEnvelope(const std::string& text);

// 以下函数用于自检向量，不作为对外接口使用。
std::string aes128Ctr(const Key& key, const std::array<unsigned char, 16>& iv, const std::string& input);
std::string sha256(const std::string& input);
std::string hmacSha256(const std::string& key, const std::string& data);

}  // namespace noldoj::crypto
