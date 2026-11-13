#include "core/crypto.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "json.hpp"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

using nlohmann::json;

namespace noldoj::crypto {
namespace {

// ---------------- AES-128（仅加密方向，供 CTR 模式使用） ----------------

const unsigned char kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

unsigned char xtime(unsigned char x) { return static_cast<unsigned char>((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00)); }

void expandKey(const Key& key, unsigned char roundKeys[176]) {
    std::memcpy(roundKeys, key.data(), 16);
    unsigned char rcon = 0x01;
    for (int i = 16; i < 176; i += 4) {
        unsigned char t[4] = {roundKeys[i - 4], roundKeys[i - 3], roundKeys[i - 2], roundKeys[i - 1]};
        if (i % 16 == 0) {
            const unsigned char first = t[0];
            t[0] = static_cast<unsigned char>(kSbox[t[1]] ^ rcon);
            t[1] = kSbox[t[2]];
            t[2] = kSbox[t[3]];
            t[3] = kSbox[first];
            rcon = xtime(rcon);
        }
        roundKeys[i + 0] = static_cast<unsigned char>(roundKeys[i - 16] ^ t[0]);
        roundKeys[i + 1] = static_cast<unsigned char>(roundKeys[i - 15] ^ t[1]);
        roundKeys[i + 2] = static_cast<unsigned char>(roundKeys[i - 14] ^ t[2]);
        roundKeys[i + 3] = static_cast<unsigned char>(roundKeys[i - 13] ^ t[3]);
    }
}

void encryptBlock(const unsigned char roundKeys[176], const unsigned char in[16], unsigned char out[16]) {
    unsigned char s[16];
    for (int i = 0; i < 16; ++i) s[i] = static_cast<unsigned char>(in[i] ^ roundKeys[i]);

    for (int round = 1; round <= 10; ++round) {
        for (int i = 0; i < 16; ++i) s[i] = kSbox[s[i]];

        unsigned char t = s[1];
        s[1] = s[5];
        s[5] = s[9];
        s[9] = s[13];
        s[13] = t;
        t = s[2];
        s[2] = s[10];
        s[10] = t;
        t = s[6];
        s[6] = s[14];
        s[14] = t;
        t = s[15];
        s[15] = s[11];
        s[11] = s[7];
        s[7] = s[3];
        s[3] = t;

        if (round != 10) {
            for (int c = 0; c < 4; ++c) {
                unsigned char* col = s + c * 4;
                const unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                const unsigned char all = static_cast<unsigned char>(a0 ^ a1 ^ a2 ^ a3);
                col[0] = static_cast<unsigned char>(a0 ^ all ^ xtime(static_cast<unsigned char>(a0 ^ a1)));
                col[1] = static_cast<unsigned char>(a1 ^ all ^ xtime(static_cast<unsigned char>(a1 ^ a2)));
                col[2] = static_cast<unsigned char>(a2 ^ all ^ xtime(static_cast<unsigned char>(a2 ^ a3)));
                col[3] = static_cast<unsigned char>(a3 ^ all ^ xtime(static_cast<unsigned char>(a3 ^ a0)));
            }
        }
        for (int i = 0; i < 16; ++i) s[i] = static_cast<unsigned char>(s[i] ^ roundKeys[round * 16 + i]);
    }
    std::memcpy(out, s, 16);
}

std::string ctrCrypt(const Key& key, const std::array<unsigned char, 16>& iv, const std::string& input) {
    unsigned char roundKeys[176];
    expandKey(key, roundKeys);

    unsigned char counter[16];
    std::memcpy(counter, iv.data(), 16);

    std::string out(input.size(), '\0');
    unsigned char stream[16];
    for (std::size_t offset = 0; offset < input.size(); offset += 16) {
        encryptBlock(roundKeys, counter, stream);
        const std::size_t block = std::min<std::size_t>(16, input.size() - offset);
        for (std::size_t i = 0; i < block; ++i) {
            out[offset + i] = static_cast<char>(static_cast<unsigned char>(input[offset + i]) ^ stream[i]);
        }
        for (int i = 15; i >= 0; --i) {
            if (++counter[i] != 0) break;
        }
    }
    return out;
}

// ---------------- SHA-256 / HMAC-SHA256 ----------------

const std::uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

std::uint32_t rotr(std::uint32_t value, int bits) { return (value >> bits) | (value << (32 - bits)); }

std::string sha256Raw(const unsigned char* data, std::size_t size) {
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    std::vector<unsigned char> message(data, data + size);
    const std::uint64_t bitLength = static_cast<std::uint64_t>(size) * 8;
    message.push_back(0x80);
    while (message.size() % 64 != 56) message.push_back(0x00);
    for (int i = 7; i >= 0; --i) message.push_back(static_cast<unsigned char>((bitLength >> (i * 8)) & 0xff));

    for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const unsigned char* p = message.data() + chunk + i * 4;
            w[i] = (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
                   (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = hh + s1 + ch + kSha256K[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::string out(32, '\0');
    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = static_cast<char>((h[i] >> 24) & 0xff);
        out[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xff);
        out[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xff);
        out[i * 4 + 3] = static_cast<char>(h[i] & 0xff);
    }
    return out;
}

// ---------------- Base64 ----------------

const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& input) {
    std::string out;
    out.reserve((input.size() + 2) / 3 * 4);
    std::size_t i = 0;
    while (i + 2 < input.size()) {
        const unsigned int value = (static_cast<unsigned char>(input[i]) << 16) |
                                   (static_cast<unsigned char>(input[i + 1]) << 8) |
                                   static_cast<unsigned char>(input[i + 2]);
        out.push_back(kBase64Chars[(value >> 18) & 0x3f]);
        out.push_back(kBase64Chars[(value >> 12) & 0x3f]);
        out.push_back(kBase64Chars[(value >> 6) & 0x3f]);
        out.push_back(kBase64Chars[value & 0x3f]);
        i += 3;
    }
    const std::size_t rest = input.size() - i;
    if (rest == 1) {
        const unsigned int value = static_cast<unsigned char>(input[i]) << 16;
        out.push_back(kBase64Chars[(value >> 18) & 0x3f]);
        out.push_back(kBase64Chars[(value >> 12) & 0x3f]);
        out += "==";
    } else if (rest == 2) {
        const unsigned int value = (static_cast<unsigned char>(input[i]) << 16) |
                                   (static_cast<unsigned char>(input[i + 1]) << 8);
        out.push_back(kBase64Chars[(value >> 18) & 0x3f]);
        out.push_back(kBase64Chars[(value >> 12) & 0x3f]);
        out.push_back(kBase64Chars[(value >> 6) & 0x3f]);
        out.push_back('=');
    }
    return out;
}

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string base64Decode(const std::string& input) {
    std::string out;
    int buffer = 0;
    int bits = 0;
    for (char c : input) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        const int value = base64Value(c);
        if (value < 0) throw std::runtime_error("base64 解码失败");
        buffer = (buffer << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buffer >> bits) & 0xff));
        }
    }
    return out;
}

const char* const kMacLabel = "noldoj-cli/credentials-mac-v1";

std::string macKeyFor(const Key& key) {
    return sha256Raw(key.data(), key.size()) + std::string(kMacLabel);
}

}  // namespace

Key randomKey() {
    Key key{};
#ifdef _WIN32
    const NTSTATUS status = BCryptGenRandom(nullptr, key.data(), static_cast<ULONG>(key.size()),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) throw std::runtime_error("无法从系统随机源生成主密钥");
#else
    const int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd < 0) throw std::runtime_error("无法打开 /dev/urandom 生成主密钥");
    std::size_t filled = 0;
    while (filled < key.size()) {
        const ssize_t got = ::read(fd, key.data() + filled, key.size() - filled);
        if (got <= 0) {
            ::close(fd);
            throw std::runtime_error("读取系统随机源失败");
        }
        filled += static_cast<std::size_t>(got);
    }
    ::close(fd);
#endif
    return key;
}

std::string keyToHex(const Key& key) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(key.size() * 2);
    for (unsigned char byte : key) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0f]);
    }
    return out;
}

bool keyFromHex(const std::string& hex, Key* out) {
    if (hex.size() != 32) return false;
    Key key{};
    for (std::size_t i = 0; i < 16; ++i) {
        int high = -1;
        int low = -1;
        for (int nibble = 0; nibble < 2; ++nibble) {
            const char c = hex[i * 2 + nibble];
            int value = -1;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            if (nibble == 0) high = value;
            else low = value;
        }
        if (high < 0 || low < 0) return false;
        key[i] = static_cast<unsigned char>((high << 4) | low);
    }
    *out = key;
    return true;
}

std::string sha256(const std::string& input) {
    return sha256Raw(reinterpret_cast<const unsigned char*>(input.data()), input.size());
}

std::string hmacSha256(const std::string& key, const std::string& data) {
    constexpr std::size_t kBlock = 64;
    std::string blockKey = key.size() > kBlock ? sha256Raw(reinterpret_cast<const unsigned char*>(key.data()), key.size()) : key;
    blockKey.resize(kBlock, '\0');

    std::string inner(kBlock, '\0');
    std::string outer(kBlock, '\0');
    for (std::size_t i = 0; i < kBlock; ++i) {
        inner[i] = static_cast<char>(static_cast<unsigned char>(blockKey[i]) ^ 0x36);
        outer[i] = static_cast<char>(static_cast<unsigned char>(blockKey[i]) ^ 0x5c);
    }
    const std::string innerHash = sha256Raw(reinterpret_cast<const unsigned char*>((inner + data).data()), inner.size() + data.size());
    const std::string message = outer + innerHash;
    return sha256Raw(reinterpret_cast<const unsigned char*>(message.data()), message.size());
}

std::string aes128Ctr(const Key& key, const std::array<unsigned char, 16>& iv, const std::string& input) {
    return ctrCrypt(key, iv, input);
}

std::string seal(const Key& key, const std::string& plaintext) {
    const Key ivSource = randomKey();
    std::array<unsigned char, 16> iv{};
    std::memcpy(iv.data(), ivSource.data(), 16);

    const std::string cipher = ctrCrypt(key, iv, plaintext);
    const std::string mac = hmacSha256(macKeyFor(key), std::string(iv.begin(), iv.end()) + cipher);

    json envelope;
    envelope["version"] = 1;
    envelope["alg"] = "aes-128-ctr+hmac-sha256";
    envelope["iv"] = base64Encode(std::string(iv.begin(), iv.end()));
    envelope["ct"] = base64Encode(cipher);
    envelope["mac"] = base64Encode(mac);
    return envelope.dump(2);
}

std::string unseal(const Key& key, const std::string& envelopeText) {
    json envelope;
    try {
        envelope = json::parse(envelopeText);
    } catch (const std::exception&) {
        throw std::runtime_error("凭证文件已损坏（不是合法的加密信封）");
    }
    if (!envelope.contains("iv") || !envelope.contains("ct") || !envelope.contains("mac")) {
        throw std::runtime_error("凭证文件不是受支持的加密信封");
    }

    const std::string ivText = base64Decode(envelope.value("iv", std::string()));
    if (ivText.size() != 16) throw std::runtime_error("凭证文件的初始向量无效");
    std::array<unsigned char, 16> iv{};
    std::memcpy(iv.data(), ivText.data(), 16);

    const std::string cipher = base64Decode(envelope.value("ct", std::string()));
    const std::string expected = base64Decode(envelope.value("mac", std::string()));
    const std::string actual = hmacSha256(macKeyFor(key), ivText + cipher);

    unsigned char diff = static_cast<unsigned char>(expected.size() == actual.size() ? 0 : 1);
    for (std::size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
        diff = static_cast<unsigned char>(diff | (static_cast<unsigned char>(expected[i]) ^ static_cast<unsigned char>(actual[i])));
    }
    if (diff != 0) {
        throw std::runtime_error("凭证解密失败：主密钥不匹配或文件被修改");
    }
    return ctrCrypt(key, iv, cipher);
}

bool isEnvelope(const std::string& text) {
    try {
        const json parsed = json::parse(text);
        return parsed.is_object() && parsed.contains("ct") && parsed.contains("mac") && parsed.contains("iv");
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace noldoj::crypto
