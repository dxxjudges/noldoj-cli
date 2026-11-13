#include "core/keyring.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace noldoj {
namespace {

const char* const kTarget = "noldoj-cli/master-key";
const char* const kService = "noldoj-cli";
const char* const kAccount = "master-key";

std::string trimNewline(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ')) value.pop_back();
    return value;
}

std::string masterKeyPath(const std::string& configDir) {
    return (fs::path(configDir) / "master.key").string();
}

#ifdef _WIN32

bool keyringRead(std::string* value) {
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(L"noldoj-cli/master-key", CRED_TYPE_GENERIC, 0, &credential)) return false;
    *value = std::string(reinterpret_cast<const char*>(credential->CredentialBlob),
                         static_cast<std::size_t>(credential->CredentialBlobSize));
    CredFree(credential);
    return true;
}

bool keyringWrite(const std::string& value) {
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(L"noldoj-cli/master-key");
    credential.Comment = const_cast<LPWSTR>(L"NoldOJ CLI 凭据加密主密钥");
    credential.CredentialBlobSize = static_cast<DWORD>(value.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.data()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"noldoj-cli");
    return CredWriteW(&credential, 0) != FALSE;
}

#else

// 执行命令并读取标准输出；返回进程退出码。
int runCommand(const std::string& command, std::string* output) {
    FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr) return -1;
    std::string text;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) text += buffer;
    const int status = ::pclose(pipe);
    if (output != nullptr) *output = trimNewline(text);
    return status;
}

bool keyringRead(std::string* value) {
#if defined(__APPLE__)
    const std::string command =
        std::string("security find-generic-password -s ") + kService + " -a " + kAccount + " -w 2>/dev/null </dev/null";
#else
    const std::string command = std::string("secret-tool lookup service ") + kService + " account " + kAccount +
                                " 2>/dev/null </dev/null";
#endif
    std::string text;
    const int status = runCommand(command, &text);
    if (status != 0 || text.empty()) return false;
    *value = text;
    return true;
}

bool keyringWrite(const std::string& value) {
#if defined(__APPLE__)
    const std::string command = std::string("security add-generic-password -s ") + kService + " -a " + kAccount +
                                " -w '" + value + "' -U 2>/dev/null </dev/null";
#else
    const std::string command = std::string("printf '%s' '") + value + "' | secret-tool store --label='" +
                                kService + " master key' service " + kService + " account " + kAccount +
                                " 2>/dev/null";
#endif
    std::string ignored;
    const int status = runCommand(command, &ignored);
    return status == 0;
}

#endif

void restrictPermissions(const std::string& path) {
#ifndef _WIN32
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#else
    (void)path;
#endif
}

bool fileRead(const std::string& path, std::string* value) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    text = trimNewline(text);
    if (text.empty()) return false;
    *value = text;
    return true;
}

bool fileWrite(const std::string& path, const std::string& value) {
    const fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << value << "\n";
    out.close();
    restrictPermissions(path);
    return true;
}

// 依次尝试：系统凭据库 → 回退文件。
bool readStoredHex(const std::string& configDir, std::string* hex, std::string* source) {
    std::string value;
    if (keyringRead(&value)) {
        *hex = value;
        *source = systemKeyringName();
        return true;
    }
    const std::string path = masterKeyPath(configDir);
    if (fileRead(path, &value)) {
        *hex = value;
        *source = "file:" + path;
        return true;
    }
    return false;
}

}  // namespace

std::string systemKeyringName() {
#if defined(_WIN32)
    return "windows-credential-manager";
#elif defined(__APPLE__)
    return "macos-keychain";
#elif defined(__linux__)
    return "linux-secret-service";
#endif
    return "file";
}

bool tryLoadMasterKey(const std::string& configDir, MasterKeyInfo* out) {
    std::string hex;
    std::string source;
    if (!readStoredHex(configDir, &hex, &source)) return false;

    crypto::Key key{};
    if (!crypto::keyFromHex(hex, &key)) return false;

    if (out != nullptr) {
        out->key = key;
        out->source = source;
    }
    return true;
}

MasterKeyInfo loadOrCreateMasterKey(const std::string& configDir) {
    MasterKeyInfo info;
    if (tryLoadMasterKey(configDir, &info)) return info;

    const crypto::Key key = crypto::randomKey();
    const std::string hex = crypto::keyToHex(key);
    info.key = key;

    if (keyringWrite(hex)) {
        info.source = systemKeyringName();
        return info;
    }

    const std::string path = masterKeyPath(configDir);
    if (!fileWrite(path, hex)) {
        throw std::runtime_error("无法保存主密钥：系统凭据库不可用，且写入 " + path + " 失败");
    }
    info.source = "file:" + path;
    return info;
}

}  // namespace noldoj
