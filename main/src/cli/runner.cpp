#include "cli/runner.hpp"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "cli/handlers.hpp"
#include "core/keyring.hpp"
#include "core/url.hpp"
#include "json.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

namespace noldoj::cli {
namespace {

const char* kMaskedHint = "（已隐藏）";

// ---------- 通用小工具 ----------

std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("无法读取文件: " + path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string readStdin() {
    return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
}

void writeBinaryFile(const std::string& path, const std::string& content) {
    const fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("无法写入文件: " + path);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string trim(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string promptLine(const std::string& prompt) {
    std::cerr << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) throw std::runtime_error("输入已中断");
    return trim(line);
}

std::string masked(const std::string& token) {
    if (token.empty()) return kMaskedHint;
    if (token.size() <= 16) return std::string(4, '*');
    return token.substr(0, 8) + "..." + token.substr(token.size() - 6);
}

std::vector<std::string> splitComma(const std::string& value) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = comma == std::string::npos ? value.size() : comma;
        std::string part = trim(value.substr(start, end - start));
        if (!part.empty()) out.push_back(part);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string jsonKey(const std::string& flagName) {
    std::string key = flagName;
    for (char& c : key) {
        if (c == '-') c = '_';
    }
    return key;
}

bool parseBool(const std::string& raw) {
    const std::string value = trim(raw);
    if (value.empty() || value == "true" || value == "1" || value == "yes" || value == "on") return true;
    if (value == "false" || value == "0" || value == "no" || value == "off") return false;
    throw std::runtime_error("无法解析布尔值: " + raw);
}

json flagToJson(const FlagSpec& flag, const std::string& raw) {
    switch (flag.type) {
        case FlagType::String: {
            if (!raw.empty() && raw.front() == '@') return readTextFile(raw.substr(1));
            return raw;
        }
        case FlagType::Int: {
            if (trim(raw) == "null") return nullptr;
            if (raw.empty()) throw std::runtime_error("选项 --" + flag.name + " 需要整数值");
            return std::stoll(raw);
        }
        case FlagType::Float: {
            if (raw.empty()) throw std::runtime_error("选项 --" + flag.name + " 需要数字值");
            return std::stod(raw);
        }
        case FlagType::Bool:
            return parseBool(raw);
        case FlagType::Json: {
            const std::string text = (!raw.empty() && raw.front() == '@') ? readTextFile(raw.substr(1)) : raw;
            try {
                return json::parse(text);
            } catch (const std::exception& e) {
                throw std::runtime_error("选项 --" + flag.name + " 不是合法 JSON: " + e.what());
            }
        }
        case FlagType::IntList: {
            json array = json::array();
            for (const auto& part : splitComma(raw)) array.push_back(std::stoll(part));
            return array;
        }
        case FlagType::StrList: {
            json array = json::array();
            for (const auto& part : splitComma(raw)) array.push_back(part);
            return array;
        }
    }
    return raw;
}

std::string scalarToString(const FlagSpec& flag, const std::string& raw) {
    if (flag.type == FlagType::Bool) return parseBool(raw) ? "true" : "false";
    return raw;
}

bool isTerminalSubmissionStatus(const std::string& status) {
    static const std::set<std::string> kFinal = {
        "accepted", "wrong_answer", "time_limit_exceeded", "memory_limit_exceeded",
        "runtime_error", "compile_error", "system_error"};
    return kFinal.count(status) > 0;
}

bool isTerminalRunStatus(const std::string& status) {
    static const std::set<std::string> kNonFinal = {"pending", "queued", "running", "compiling", ""};
    return kNonFinal.count(status) == 0;
}

// 后端仅允许登录会话（JWT）调用、明确拒绝私人令牌的接口。
// CLI 只持有私人令牌，调用必然 403，因此在本地直接给出明确提示。
bool isSessionOnlyCommand(const std::string& group, const std::string& name) {
    static const std::set<std::string> kSessionOnly = {
        "auth/change-password", "user/sudo-login", "user/reset-password"};
    return kSessionOnly.count(group + "/" + name) > 0;
}

// ---------- 参数解析 ----------

struct Invocation {
    GlobalOptions opts;
    std::vector<std::string> words;
    std::vector<std::pair<std::string, std::string>> commandFlags;
    bool help = false;
    bool version = false;
};

Invocation parseInvocation(int argc, char** argv) {
    Invocation inv;

    auto takeValue = [&](int& index, const std::string& flagName) -> std::string {
        if (index + 1 >= argc) throw std::runtime_error("选项 --" + flagName + " 缺少值");
        return argv[++index];
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (++i; i < argc; ++i) inv.words.push_back(argv[i]);
            break;
        }
        if (arg == "--help" || arg == "-h") {
            inv.help = true;
            continue;
        }
        if (arg == "--version" || arg == "-v") {
            inv.version = true;
            continue;
        }
        if (arg == "-o") {
            inv.opts.output = takeValue(i, "output");
            continue;
        }
        if (arg == "-X") {
            inv.commandFlags.emplace_back("method", takeValue(i, "method"));
            continue;
        }
        if (arg == "-f") {
            inv.commandFlags.emplace_back("raw-field", takeValue(i, "raw-field"));
            continue;
        }
        if (arg == "-F") {
            inv.commandFlags.emplace_back("field", takeValue(i, "field"));
            continue;
        }
        if (arg == "-H") {
            inv.commandFlags.emplace_back("header", takeValue(i, "header"));
            continue;
        }
        if (arg == "-i") {
            inv.opts.includeHeaders = true;
            continue;
        }
        if (arg.rfind("--", 0) == 0) {
            const std::size_t eq = arg.find('=');
            std::string name = eq == std::string::npos ? arg.substr(2) : arg.substr(2, eq - 2);
            std::optional<std::string> inlineValue =
                eq == std::string::npos ? std::nullopt : std::optional<std::string>(arg.substr(eq + 1));

            const auto valueFor = [&](const std::string& flagName) {
                if (inlineValue) return *inlineValue;
                return takeValue(i, flagName);
            };

            if (name == "site") inv.opts.site = valueFor(name);
            else if (name == "access-token") inv.opts.token = valueFor(name);
            else if (name == "config-dir") inv.opts.configDir = valueFor(name);
            else if (name == "body-json") inv.opts.bodyJson = valueFor(name);
            else if (name == "body-json-file") inv.opts.bodyJsonFile = valueFor(name);
            else if (name == "output") inv.opts.output = valueFor(name);
            else if (name == "raw") inv.opts.raw = true;
            else if (name == "verbose") inv.opts.verbose = true;
            else if (name == "include") inv.opts.includeHeaders = true;
            else if (name == "yes" || name == "y") inv.opts.yes = true;
            else if (name == "input") {
                // --input 的值可能是 "-"（标准输入），必须强制消费下一个参数
                inv.commandFlags.emplace_back("input", inlineValue ? *inlineValue : takeValue(i, "input"));
            }
            else {
                std::string value;
                if (inlineValue) {
                    value = *inlineValue;
                } else if (i + 1 < argc && std::string(argv[i + 1]).rfind("-", 0) != 0) {
                    value = argv[++i];
                }
                inv.commandFlags.emplace_back(name, value);
            }
            continue;
        }
        inv.words.push_back(arg);
    }
    return inv;
}

Args parseCommandFlags(const CommandSpec& spec, const Invocation& inv) {
    Args args;
    args.positional = inv.words.size() > 2 ? std::vector<std::string>(inv.words.begin() + 2, inv.words.end())
                                          : std::vector<std::string>();

    std::set<std::string> known;
    for (const auto& flag : spec.flags) known.insert(flag.name);

    for (const auto& [name, value] : inv.commandFlags) {
        if (known.count(name) == 0) {
            std::string available;
            for (const auto& flag : spec.flags) {
                if (!available.empty()) available += ", ";
                available += "--" + flag.name;
            }
            throw std::runtime_error("未知选项 --" + name + "；该命令可用选项: " + (available.empty() ? "（无）" : available));
        }
        args.entries.emplace_back(name, value);
        args.values[name] = value;
        args.present.insert(name);
    }

    for (const auto& flag : spec.flags) {
        if (flag.required && !args.has(flag.name)) {
            throw std::runtime_error("缺少必填选项 --" + flag.name);
        }
    }
    return args;
}

// ---------- 运行时构建 ----------

Runtime buildRuntime(const GlobalOptions& opts, bool needSite) {
    Runtime rt;
    rt.opts = opts;

    const std::string dir = opts.configDir.empty() ? defaultConfigDir() : opts.configDir;
    rt.configPath = dir + "/config.json";
    rt.credentialsPath = dir + "/credentials.json";

    std::string created;
    rt.config = loadConfig(rt.configPath, true, &created);
    if (!created.empty()) {
        std::cerr << "已创建默认配置文件: " << created << "\n";
        // 首次启动即生成 128 位随机主密钥，写入系统凭据库（不可用时回退到本机文件）。
        std::cerr << "已生成凭据加密主密钥（" << loadOrCreateMasterKey(dir).source << "）\n";
    }
    rt.credentials = loadCredentials(rt.credentialsPath);

    if (!needSite) return rt;

    rt.site = resolveSite(rt.config, opts.site);
    rt.credential = &rt.credentials.sites[rt.site.base_url];

    if (!opts.token.empty()) {
        // 与服务端一致：去掉首尾空白，避免 `Bearer <token>\n` 导致鉴权失败
        rt.tokens = makeStaticTokenProvider(trim(opts.token));
    } else {
        rt.tokens = makeStoredTokenProvider(*rt.credential, [&rt]() { rt.persistCredentials(); });
    }
    rt.transport = makeHttpTransport();
    rt.api = std::make_unique<ApiClient>(*rt.transport, rt.site, rt.tokens.get(), opts.verbose);
    return rt;
}

// ---------- 输出 ----------

int printError(const ApiResponse& response) {
    if (response.isJson && response.data.is_object()) {
        const int code = response.data.value("code", 0);
        const std::string reason = response.data.value("reason", std::string());
        const std::string message = response.data.value("message", std::string());
        std::cerr << "错误: HTTP " << response.status;
        if (!reason.empty()) std::cerr << " " << reason;
        if (code != 0) std::cerr << " (" << code << ")";
        if (!message.empty()) std::cerr << ": " << message;
        std::cerr << "\n";
        if (code >= 1 && code <= 6) return code;
        return 1;
    }

    std::cerr << "错误: HTTP " << response.status << "\n";
    if (!response.rawBody.empty()) {
        std::string body = response.rawBody;
        if (body.size() > 500) body = body.substr(0, 500) + "...";
        std::cerr << body << "\n";
    }
    return 1;
}

std::string downloadName(const CommandSpec& spec, const Args& args) {
    std::string pattern = spec.downloadName.empty() ? "response.bin" : spec.downloadName;
    for (const auto& flag : spec.flags) {
        const std::string placeholder = "{" + flag.name + "}";
        if (pattern.find(placeholder) == std::string::npos) continue;
        std::string value = args.get(flag.name);
        if (flag.name == "format" && value.empty()) value = "zip";
        if (flag.name == "filename") value = fs::path(value).filename().string();
        pattern = replaceAll(pattern, placeholder, value.empty() ? flag.name : value);
    }
    return pattern;
}

int render(Runtime& rt, const CommandSpec& spec, const Args& args, const ApiResponse& response) {
    if (response.status < 200 || response.status >= 300) return printError(response);

    const bool wantsFile = spec.download || !rt.opts.output.empty();
    if (wantsFile) {
        std::string path = rt.opts.output;
        if (path.empty()) path = downloadName(spec, args);
        if (path == "-") {
            std::cout << response.rawBody;
            return 0;
        }
        writeBinaryFile(path, response.rawBody);
        std::cerr << "已保存到 " << fs::absolute(path).string() << "（" << response.rawBody.size() << " 字节）\n";
        return 0;
    }

    if (response.isJson) {
        std::cout << (rt.opts.raw ? response.data.dump() : response.data.dump(2)) << "\n";
    } else if (!response.rawBody.empty()) {
        std::cout << response.rawBody << "\n";
    } else {
        std::cout << "（空响应）\n";
    }
    return 0;
}

// ---------- 通用请求执行 ----------

int runGeneric(Runtime& rt, const CommandSpec& spec, const Args& args) {
    std::string path = spec.path;

    for (const auto& flag : spec.flags) {
        if (flag.place != FlagPlace::Path) continue;
        if (!args.has(flag.name)) {
            if (flag.required) throw std::runtime_error("缺少路径参数 --" + flag.name);
            continue;
        }
        path = replaceAll(path, ":" + flag.name, percentEncode(args.get(flag.name)));
    }
    if (path.find(':') != std::string::npos) {
        throw std::runtime_error("命令路径缺少参数: " + path);
    }

    std::vector<std::pair<std::string, std::string>> query;
    for (const auto& flag : spec.flags) {
        if (flag.place != FlagPlace::Query || !args.has(flag.name)) continue;
        query.emplace_back(jsonKey(flag.name), scalarToString(flag, args.get(flag.name)));
    }

    json body = json::object();
    bool hasBody = false;
    if (!rt.opts.bodyJson.empty()) {
        try {
            body = json::parse(rt.opts.bodyJson);
        } catch (const std::exception& e) {
            throw std::runtime_error("--body-json 不是合法 JSON: " + std::string(e.what()));
        }
        if (!body.is_object()) throw std::runtime_error("--body-json 必须是 JSON 对象");
        hasBody = true;
    }
    if (!rt.opts.bodyJsonFile.empty()) {
        const std::string text = readTextFile(rt.opts.bodyJsonFile);
        json extra;
        try {
            extra = json::parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error("--body-json-file 不是合法 JSON: " + std::string(e.what()));
        }
        if (!extra.is_object()) throw std::runtime_error("--body-json-file 必须是 JSON 对象");
        if (!hasBody) body = json::object();
        for (auto it = extra.begin(); it != extra.end(); ++it) body[it.key()] = it.value();
        hasBody = true;
    }
    for (const auto& flag : spec.flags) {
        if (flag.place != FlagPlace::Body || !args.has(flag.name)) continue;
        body[jsonKey(flag.name)] = flagToJson(flag, args.get(flag.name));
        hasBody = true;
    }
    if (!hasBody && (spec.method == "POST" || spec.method == "PUT" || spec.method == "PATCH")) {
        body = json::object();
        hasBody = true;
    }

    const bool attachAuth = spec.auth != AuthMode::None;
    const ApiResponse response = rt.requestJson(spec.method, path, query, hasBody ? &body : nullptr, attachAuth);
    return render(rt, spec, args, response);
}

// ---------- 上传相关 ----------

RequestBody buildMultipart(const std::vector<std::pair<std::string, std::string>>& fields,
                           const std::vector<std::pair<std::string, std::string>>& files,
                           std::string& contentType) {
    const std::string boundary = "----noldoj-cli-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    contentType = "multipart/form-data; boundary=" + boundary;

    RequestBody body;
    auto addText = [&body](const std::string& text) { body.segments.push_back(BodySegment{BodySegment::Kind::Memory, text, "", text.size()}); };

    for (const auto& [name, value] : fields) {
        std::string part = "--" + boundary + "\r\n";
        part += "Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
        part += value + "\r\n";
        addText(part);
        body.segments.back().length = part.size();
        body.segments.back().data = part;
    }

    for (const auto& [field, filePath] : files) {
        if (!fs::exists(filePath)) throw std::runtime_error("文件不存在: " + filePath);
        const std::string filename = fs::path(filePath).filename().string();
        std::string header = "--" + boundary + "\r\n";
        header += "Content-Disposition: form-data; name=\"" + field + "\"; filename=\"" + filename + "\"\r\n";
        header += "Content-Type: application/octet-stream\r\n\r\n";

        BodySegment head;
        head.kind = BodySegment::Kind::Memory;
        head.data = header;
        head.length = header.size();
        body.segments.push_back(head);

        BodySegment filePart;
        filePart.kind = BodySegment::Kind::File;
        filePart.path = filePath;
        filePart.length = static_cast<std::size_t>(fs::file_size(filePath));
        body.segments.push_back(filePart);

        BodySegment tail;
        tail.kind = BodySegment::Kind::Memory;
        tail.data = "\r\n";
        tail.length = 2;
        body.segments.push_back(tail);
    }

    BodySegment end;
    end.kind = BodySegment::Kind::Memory;
    end.data = "--" + boundary + "--\r\n";
    end.length = end.data.size();
    body.segments.push_back(end);
    return body;
}

int sendMultipart(Runtime& rt, const CommandSpec& spec, const Args& args, const std::string& path,
                  const std::vector<std::pair<std::string, std::string>>& files) {
    std::string contentType;
    RequestBody body = buildMultipart({}, files, contentType);

    ApiCall call;
    call.method = spec.method;
    call.path = path;
    call.body = std::move(body);
    call.contentType = contentType;
    call.attachAuth = true;
    const ApiResponse response = rt.api->call(call);
    return render(rt, spec, args, response);
}

// ---------- 轮询 ----------

int pollSubmission(Runtime& rt, long long submissionId, bool briefOutput) {
    std::string lastStatus;
    ApiResponse response;
    for (int attempt = 0; attempt < 240; ++attempt) {
        response = rt.requestJson("GET", "/submissions/" + std::to_string(submissionId), {}, nullptr, true);
        if (response.status < 200 || response.status >= 300) return printError(response);

        const std::string status = response.data.value("status", std::string());
        if (status != lastStatus) {
            std::cerr << "提交 #" << submissionId << " 状态: " << (status.empty() ? "未知" : status) << "\n";
            lastStatus = status;
        }
        if (isTerminalSubmissionStatus(status)) {
            std::cout << (rt.opts.raw ? response.data.dump() : response.data.dump(2)) << "\n";
            return status == "accepted" ? 0 : 0;
        }
        if (!briefOutput) {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    std::cerr << "轮询超时，请稍后用 `noldoj submission get --id " << submissionId << "` 查看结果。\n";
    return 1;
}

int pollIdeRun(Runtime& rt, long long runId) {
    std::string lastStatus;
    ApiResponse response;
    for (int attempt = 0; attempt < 240; ++attempt) {
        response = rt.requestJson("GET", "/ide/run/" + std::to_string(runId), {}, nullptr, false);
        if (response.status < 200 || response.status >= 300) return printError(response);

        const std::string status = response.data.value("status", std::string());
        if (status != lastStatus) {
            std::cerr << "运行 #" << runId << " 状态: " << (status.empty() ? "未知" : status) << "\n";
            lastStatus = status;
        }
        if (isTerminalRunStatus(status)) {
            std::cout << (rt.opts.raw ? response.data.dump() : response.data.dump(2)) << "\n";
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cerr << "轮询超时，请稍后用 `noldoj ide run-status --id " << runId << "` 查看结果。\n";
    return 1;
}

}  // namespace

// =================  handlers  =================

int cmdAuthStatus(Runtime& rt, const Args& args) {
    std::cout << "站点: " << rt.site.name << "\n";
    std::cout << "地址: " << rt.site.base_url << "\n";

    if (!rt.opts.token.empty()) {
        std::cout << "来源: --access-token（临时覆盖，不写入本地）\n";
        std::cout << "令牌: " << masked(rt.opts.token) << "\n";
        return 0;
    }

    const CredentialEntry& entry = *rt.credential;
    if (entry.personal_token.empty()) {
        std::cout << "状态: 未配置私人令牌（仅可访问公开接口）\n";
        std::cout << "提示: 用 `noldoj site add --name <名称> --url <地址> --token <pat>` 配置站点，"
                     "或用 `noldoj auth set-token --token <pat>` 更新当前站点令牌。\n";
        return 0;
    }
    std::cout << "来源: 站点凭证文件\n";
    std::cout << "令牌: " << masked(entry.personal_token) << "（私人令牌，长期有效，无需刷新）\n";
    return 0;
}

int cmdSetToken(Runtime& rt, const Args& args) {
    std::string token = trim(args.get("token"));
    if (!args.get("token-file").empty()) token = trim(readTextFile(args.get("token-file")));
    if (token.empty()) token = promptLine("私人令牌: ");
    if (token.empty()) throw std::runtime_error("私人令牌不能为空");

    rt.credential->personal_token = token;
    rt.persistCredentials();

    std::cout << "已为站点 `" << rt.site.name << "` 保存私人令牌: " << masked(token) << "\n";
    std::cout << "私人令牌视为长期有效的 access token，请求时不会触发刷新。\n";
    return 0;
}

int cmdSudoLogin(Runtime& rt, const Args& args) {
    json body = json::object();
    body["user_id"] = std::stoll(args.get("user-id"));

    ApiResponse response = rt.requestJson("POST", "/users/sudo-login", {}, &body, true);
    if (response.status < 200 || response.status >= 300) return printError(response);

    std::cout << (rt.opts.raw ? response.data.dump() : response.data.dump(2)) << "\n";
    if (response.data.contains("access_token")) {
        std::cout << "提示: 返回的 access_token 为临时令牌，可用 --access-token <token> 直接使用（不会写入本地）。\n";
    }
    return 0;
}

int cmdSubmit(Runtime& rt, const Args& args) {
    json body = json::object();
    body["problem_id"] = std::stoll(args.get("problem-id"));

    std::string language = args.get("language");
    if (language.empty()) language = rt.site.default_language;
    body["language"] = language;

    std::string source;
    const std::string sourceFile = args.get("source-file");
    if (!sourceFile.empty()) {
        source = sourceFile == "-" ? readStdin() : readTextFile(sourceFile);
    } else {
        source = args.get("source-code");
    }
    if (source.empty() && !args.has("answer-data")) {
        throw std::runtime_error("请通过 --source-code 或 --source-file 指定代码");
    }
    if (!source.empty()) body["source_code"] = source;
    if (args.has("answer-data")) body["answer_data"] = args.get("answer-data");
    if (args.has("contest-id")) body["contest_id"] = std::stoll(args.get("contest-id"));

    ApiResponse response = rt.requestJson("POST", "/submissions", {}, &body, true);
    if (response.status < 200 || response.status >= 300) return printError(response);

    const long long submissionId = response.data.value("submission_id", 0LL);
    std::cout << "提交 #" << submissionId << " 已创建。\n";
    if (args.has("watch") && submissionId > 0) return pollSubmission(rt, submissionId, true);
    return 0;
}

int cmdWatchSubmission(Runtime& rt, const Args& args) {
    return pollSubmission(rt, std::stoll(args.get("id")), false);
}

int cmdIdeRun(Runtime& rt, const Args& args) {
    json body = json::object();
    body["language"] = args.get("language");

    std::string source;
    const std::string sourceFile = args.get("source-file");
    if (!sourceFile.empty()) {
        source = sourceFile == "-" ? readStdin() : readTextFile(sourceFile);
    } else {
        source = args.get("source-code");
    }
    if (source.empty()) throw std::runtime_error("请通过 --source-code 或 --source-file 指定代码");
    body["source_code"] = source;

    const std::string stdinFile = args.get("stdin-file");
    if (!stdinFile.empty()) body["stdin"] = stdinFile == "-" ? readStdin() : readTextFile(stdinFile);
    else if (args.has("stdin")) body["stdin"] = args.get("stdin");

    ApiResponse response = rt.requestJson("POST", "/ide/run", {}, &body, true);
    if (response.status < 200 || response.status >= 300) return printError(response);

    const long long runId = response.data.value("run_id", 0LL);
    std::cout << "运行 #" << runId << " 已排队。\n";
    if (args.has("wait") && runId > 0) return pollIdeRun(rt, runId);
    return 0;
}

int cmdImportProblem(Runtime& rt, const Args& args) {
    return sendMultipart(rt, *findCommand("problem", "import"), args, "/problems/import",
                         {{"file", args.get("file")}});
}

int cmdUploadTestdata(Runtime& rt, const Args& args) {
    std::vector<std::pair<std::string, std::string>> files;
    for (const auto& path : args.all("file")) files.emplace_back("files", path);
    if (files.empty()) throw std::runtime_error("请至少指定一个 --file");
    return sendMultipart(rt, *findCommand("problem", "testdata-upload"), args,
                         "/problems/" + args.get("id") + "/testdata", files);
}

int cmdUploadTestdataZip(Runtime& rt, const Args& args) {
    return sendMultipart(rt, *findCommand("problem", "testdata-zip"), args,
                         "/problems/" + args.get("id") + "/testdata-zip", {{"file", args.get("file")}});
}

int cmdUploadFile(Runtime& rt, const Args& args) {
    return sendMultipart(rt, *findCommand("upload", "create"), args, "/uploads", {{"file", args.get("file")}});
}

// ---------- 站点 / 配置 ----------

int cmdSiteList(Runtime& rt, const Args& args) {
    if (rt.config.sites.empty()) {
        std::cout << "（尚未配置任何站点）\n";
        return 0;
    }
    for (const auto& [name, site] : rt.config.sites) {
        const bool isDefault = name == rt.config.default_site;
        std::cout << (isDefault ? "* " : "  ") << name << "  " << site.base_url;
        if (isDefault) std::cout << "  [默认]";

        auto it = rt.credentials.sites.find(site.base_url);
        if (it != rt.credentials.sites.end() && !it->second.personal_token.empty()) {
            std::cout << "  [私人令牌]";
        } else {
            std::cout << "  [未配置令牌]";
        }
        std::cout << "\n";
    }
    return 0;
}

int cmdSiteAdd(Runtime& rt, const Args& args) {
    const std::string name = args.get("name");
    const std::string token = trim(args.get("token"));
    if (token.empty()) throw std::runtime_error("--token 不能为空");

    SiteConfig site;
    site.name = name;
    site.base_url = normalizeBaseUrl(args.get("url"));
    site.default_language = args.get("default-language", "cpp");

    rt.config.sites[name] = site;
    if (rt.config.default_site.empty()) rt.config.default_site = name;
    saveConfig(rt.configPath, rt.config);

    // 令牌按规范化服务端地址单独存储，因此 `--site <name>` 与 `--site <url>` 都能命中同一份令牌。
    rt.credentials.sites[site.base_url].personal_token = token;
    saveCredentials(rt.credentialsPath, rt.credentials);

    std::cout << "已添加站点 `" << name << "`: " << site.base_url << "\n";
    std::cout << "已保存私人令牌: " << masked(token) << "\n";
    if (rt.config.default_site == name) std::cout << "已设为默认站点。\n";
    return 0;
}

int cmdSiteRemove(Runtime& rt, const Args& args) {
    const std::string name = args.get("name");
    auto siteIt = rt.config.sites.find(name);
    if (siteIt == rt.config.sites.end()) throw std::runtime_error("站点不存在: " + name);
    const std::string baseUrl = siteIt->second.base_url;

    if (rt.config.default_site == name) {
        rt.config.default_site = "";
        for (const auto& [otherName, _] : rt.config.sites) {
            if (otherName == name) continue;
            rt.config.default_site = otherName;
            break;
        }
    }
    rt.config.sites.erase(name);
    rt.credentials.sites.erase(baseUrl);
    saveConfig(rt.configPath, rt.config);
    rt.persistCredentials();
    std::cout << "已删除站点 `" << name << "`。\n";
    return 0;
}

int cmdSiteUse(Runtime& rt, const Args& args) {
    const std::string name = args.get("name");
    if (rt.config.sites.count(name) == 0) throw std::runtime_error("站点不存在: " + name);
    rt.config.default_site = name;
    saveConfig(rt.configPath, rt.config);
    std::cout << "默认站点已设置为 `" << name << "`。\n";
    return 0;
}

int cmdSiteShow(Runtime& rt, const Args& args) {
    const SiteConfig site = resolveSite(rt.config, rt.opts.site);
    std::cout << "当前站点: " << site.name << "\n";
    std::cout << "地址: " << site.base_url << "\n";
    std::cout << "默认语言: " << site.default_language << "\n";
    std::cout << "超时: " << site.timeout_ms << " ms\n";
    if (!rt.config.default_site.empty()) std::cout << "配置默认站点: " << rt.config.default_site << "\n";
    return 0;
}

int cmdConfigShow(Runtime& rt, const Args& args) {
    std::cout << readTextFile(rt.configPath) << "\n";
    return 0;
}

int cmdConfigPath(Runtime& rt, const Args& args) {
    std::cout << "配置文件: " << rt.configPath << "\n";
    std::cout << "凭证文件: " << rt.credentialsPath << "（已加密）\n";
    const std::string keySource = masterKeySource(fs::path(rt.configPath).parent_path().string());
    std::cout << "主密钥: " << (keySource.empty() ? "尚未生成（首次保存凭据时创建）" : keySource) << "\n";
    std::cout << "系统凭据库: " << systemKeyringName() << "\n";
    return 0;
}

// ================= Runtime =================

void Runtime::persistCredentials() { saveCredentials(credentialsPath, credentials); }

ApiResponse Runtime::requestJson(const std::string& method, const std::string& path,
                                 const std::vector<std::pair<std::string, std::string>>& query,
                                 const nlohmann::json* body, bool attachAuth) {
    ApiCall call;
    call.method = method;
    call.path = path;
    call.query = query;
    call.attachAuth = attachAuth;
    if (body != nullptr && !body->is_null()) {
        call.body = RequestBody::fromMemory(body->dump());
        call.contentType = "application/json";
    }
    return api->call(call);
}

// ================= api（直接请求任意接口，类似 gh api） =================

namespace {

const std::set<std::string>& httpMethods() {
    static const std::set<std::string> kMethods = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    return kMethods;
}

std::string toUpper(std::string value) {
    for (char& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}

// 把用户输入的路径规范成站点 base_url 之后的相对路径：
// /auth/tokeninfo、auth/tokeninfo、/api/v1/auth/tokeninfo 都映射到 /auth/tokeninfo。
std::string normalizeApiPath(const std::string& raw) {
    std::string path = trim(raw);
    if (path.empty()) throw std::runtime_error("api 需要指定接口路径");
    if (looksLikeUrl(path)) {
        throw std::runtime_error("api 只接受站点相对路径（例如 /auth/tokeninfo）；切换站点请使用 --site");
    }
    while (!path.empty() && path.front() == '/') path.erase(0, 1);
    if (path.rfind("api/v1/", 0) == 0) path.erase(0, 7);
    else if (path == "api/v1") path.clear();
    if (path.empty()) throw std::runtime_error("api 需要指定接口路径");
    return "/" + path;
}

std::pair<std::string, std::string> parseField(const std::string& raw, const std::string& flagName) {
    const std::size_t eq = raw.find('=');
    if (eq == std::string::npos || eq == 0) {
        throw std::runtime_error("选项 " + flagName + " 需要 key=value 形式: " + raw);
    }
    return {trim(raw.substr(0, eq)), raw.substr(eq + 1)};
}

std::pair<std::string, std::string> parseHeader(const std::string& raw) {
    const std::size_t colon = raw.find(':');
    if (colon == std::string::npos || colon == 0) {
        throw std::runtime_error("选项 -H/--header 需要 \"名称: 值\" 形式: " + raw);
    }
    return {trim(raw.substr(0, colon)), trim(raw.substr(colon + 1))};
}

// -F/--field 的值优先按 JSON 解析，解析失败则按普通字符串处理。
json parseTypedValue(const std::string& value) {
    if (value.empty()) return std::string();
    try {
        return json::parse(value);
    } catch (const std::exception&) {
        return value;
    }
}

}  // namespace

int runApiCommand(Runtime& rt, const Invocation& inv) {
    std::string method;
    std::string rawPath;
    std::string input;
    std::vector<std::pair<std::string, std::string>> rawFields;
    std::vector<std::pair<std::string, std::string>> typedFields;
    std::vector<std::pair<std::string, std::string>> headers;

    // words[0] 是 "api"；其后第一个词允许是 HTTP 方法（noldoj api POST /problems）。
    std::vector<std::string> words(inv.words.begin() + 1, inv.words.end());
    std::size_t wordIndex = 0;
    if (words.size() >= 2 && httpMethods().count(toUpper(words[0])) > 0) {
        method = toUpper(words[0]);
        wordIndex = 1;
    }
    if (wordIndex < words.size()) rawPath = words[wordIndex++];
    if (wordIndex < words.size()) {
        throw std::runtime_error("api 只接受一个路径参数，多余参数: " + words[wordIndex]);
    }

    for (const auto& [name, value] : inv.commandFlags) {
        if (name == "method") {
            const std::string normalized = toUpper(trim(value));
            if (httpMethods().count(normalized) == 0) {
                throw std::runtime_error("不支持的 HTTP 方法: " + value);
            }
            method = normalized;
        } else if (name == "raw-field") {
            rawFields.push_back(parseField(value, "--raw-field"));
        } else if (name == "field") {
            typedFields.push_back(parseField(value, "--field"));
        } else if (name == "header") {
            headers.push_back(parseHeader(value));
        } else if (name == "input") {
            input = value;
        } else {
            throw std::runtime_error("未知选项 --" + name +
                                     "；api 支持 --method/-X、--raw-field/-f、--field/-F、--header/-H、--input、--include/-i");
        }
    }

    const std::string path = normalizeApiPath(rawPath);

    if (!input.empty() && (!rawFields.empty() || !typedFields.empty())) {
        throw std::runtime_error("--input 不能与 -f/-F 字段选项同时使用");
    }
    if (!input.empty() && (!rt.opts.bodyJson.empty() || !rt.opts.bodyJsonFile.empty())) {
        throw std::runtime_error("--input 不能与 --body-json/--body-json-file 同时使用");
    }

    if (method.empty()) {
        const bool hasPayload = !input.empty() || !rawFields.empty() || !typedFields.empty() ||
                                !rt.opts.bodyJson.empty() || !rt.opts.bodyJsonFile.empty();
        method = hasPayload ? "POST" : "GET";
    }

    // GET/HEAD/DELETE/OPTIONS 的字段作为查询参数，其余方法作为 JSON 请求体。
    const bool useQuery = method == "GET" || method == "HEAD" || method == "DELETE" || method == "OPTIONS";
    std::vector<std::pair<std::string, std::string>> query;
    json body = json::object();
    bool hasBody = false;

    for (const auto& [key, value] : rawFields) {
        if (useQuery) {
            query.emplace_back(key, value);
        } else {
            body[key] = value;
            hasBody = true;
        }
    }
    for (const auto& [key, value] : typedFields) {
        const json parsed = parseTypedValue(value);
        if (useQuery) {
            query.emplace_back(key, parsed.is_string() ? parsed.get<std::string>() : parsed.dump());
        } else {
            body[key] = parsed;
            hasBody = true;
        }
    }

    const auto mergeJsonObject = [&body, &hasBody](const std::string& text, const std::string& flagName) {
        json extra;
        try {
            extra = json::parse(text);
        } catch (const std::exception& e) {
            throw std::runtime_error(flagName + " 不是合法 JSON: " + std::string(e.what()));
        }
        if (!extra.is_object()) throw std::runtime_error(flagName + " 必须是 JSON 对象");
        for (auto it = extra.begin(); it != extra.end(); ++it) body[it.key()] = it.value();
        hasBody = true;
    };
    if (!rt.opts.bodyJson.empty()) mergeJsonObject(rt.opts.bodyJson, "--body-json");
    if (!rt.opts.bodyJsonFile.empty()) mergeJsonObject(readTextFile(rt.opts.bodyJsonFile), "--body-json-file");

    ApiCall call;
    call.method = method;
    call.path = path;
    call.query = query;
    call.attachAuth = true;
    call.extraHeaders = headers;

    if (!input.empty()) {
        const std::string raw = input == "-" ? readStdin() : readTextFile(input);
        call.body = RequestBody::fromMemory(raw);
        bool isJson = false;
        try {
            const json parsed = json::parse(raw);
            (void)parsed;
            isJson = true;
        } catch (const std::exception&) {
        }
        call.contentType = isJson ? "application/json" : "application/octet-stream";
    } else if (hasBody) {
        call.body = RequestBody::fromMemory(body.dump());
        call.contentType = "application/json";
    }

    const ApiResponse response = rt.api->call(call);

    if (rt.opts.includeHeaders) {
        std::cout << response.rawHeaders;
        if (!response.rawHeaders.empty() && response.rawHeaders.back() != '\n') std::cout << "\n";
        std::cout << "\n";
    }

    CommandSpec spec;
    spec.group = "api";
    spec.method = method;
    spec.path = path;
    spec.auth = AuthMode::Required;
    const Args emptyArgs;
    return render(rt, spec, emptyArgs, response);
}

// ================= 入口 =================

int run(int argc, char** argv) {
    try {
        Invocation inv = parseInvocation(argc, argv);

        if (inv.version) {
            std::cout << "noldoj-cli 1.0.0\n";
            return 0;
        }

        if (inv.words.empty()) {
            printGroups();
            return inv.help ? 0 : 1;
        }

        if (inv.words[0] == "help") {
            printHelp(inv.words.size() > 1 ? inv.words[1] : "");
            return 0;
        }

        if (inv.words[0] == "api") {
            if (inv.help || inv.words.size() == 1) {
                printHelp("api");
                return inv.help ? 0 : 1;
            }
            Runtime rt = buildRuntime(inv.opts, true);
            return runApiCommand(rt, inv);
        }

        if (inv.words.size() < 2) {
            printHelp(inv.words[0]);
            return 0;
        }

        const CommandSpec* spec = findCommand(inv.words[0], inv.words.size() > 1 ? inv.words[1] : "");
        if (spec == nullptr) {
            const std::string group = inv.words[0];
            const std::string command = inv.words.size() > 1 ? inv.words[1] : "";
            std::cerr << "未知命令: " << group << (command.empty() ? "" : " " + command) << "\n\n";
            printHelp(group);
            return 1;
        }

        if (inv.help) {
            printHelp(spec->group);
            return 0;
        }

        if (isSessionOnlyCommand(spec->group, spec->name)) {
            std::cerr << "错误: `" << spec->group << " " << spec->name
                      << "` 仅允许登录会话（JWT）调用，私人令牌无法使用；请在网页端完成该操作。\n";
            return 1;
        }

        const bool needSite = !(spec->group == "site" || spec->group == "config");
        Runtime rt = buildRuntime(inv.opts, needSite);
        Args args = parseCommandFlags(*spec, inv);

        if (spec->handler != nullptr) return spec->handler(rt, args);
        return runGeneric(rt, *spec, args);
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace noldoj::cli
