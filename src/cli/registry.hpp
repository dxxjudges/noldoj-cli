#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace noldoj::cli {

enum class AuthMode { None, Optional, Required };

enum class FlagType { String, Int, Float, Bool, Json, IntList, StrList };

enum class FlagPlace { Path, Query, Body };

struct FlagSpec {
    std::string name;
    FlagType type = FlagType::String;
    FlagPlace place = FlagPlace::Body;
    bool required = false;
    std::string help;
};

struct GlobalOptions {
    std::string site;        // --site：站点名或完整 URL
    std::string token;       // --token：临时覆盖令牌（私人令牌 / access token）
    std::string configDir;   // --config-dir：覆盖配置目录
    std::string bodyJson;    // --body-json：直接指定请求体 JSON
    std::string bodyJsonFile;// --body-json-file：从文件读取请求体 JSON
    std::string output;      // --output / -o：输出（下载）路径，"-" 表示标准输出
    bool raw = false;        // --raw：紧凑 JSON 输出
    bool verbose = false;    // --verbose：打印请求/响应摘要
    bool includeHeaders = false; // --include / -i：同时输出响应状态行与响应头
    bool yes = false;        // --yes：跳过交互确认
};

struct Args {
    std::map<std::string, std::string> values;
    std::set<std::string> present;
    std::vector<std::pair<std::string, std::string>> entries;  // 保留重复选项（如多个 --file）
    std::vector<std::string> positional;

    bool has(const std::string& name) const { return present.count(name) > 0; }
    std::string get(const std::string& name, const std::string& fallback = std::string()) const {
        const auto it = values.find(name);
        return it == values.end() ? fallback : it->second;
    }
    std::vector<std::string> all(const std::string& name) const {
        std::vector<std::string> out;
        for (const auto& [key, value] : entries) {
            if (key == name) out.push_back(value);
        }
        return out;
    }
};

struct Runtime;

using HandlerFn = int (*)(Runtime&, const Args&);

struct CommandSpec {
    std::string group;
    std::string name;
    std::string method;
    std::string path;       // 以 :name 占位，如 /problems/:id
    AuthMode auth = AuthMode::None;
    std::vector<FlagSpec> flags;
    HandlerFn handler = nullptr;
    bool download = false;
    std::string downloadName;  // 例如 problem_{id}.zip
    std::string help;
};

const std::vector<CommandSpec>& commandTable();
const CommandSpec* findCommand(const std::string& group, const std::string& name);
void printHelp(const std::string& group);
void printGroups();

}  // namespace noldoj::cli
