#include "cli/registry.hpp"

#include <algorithm>
#include <iostream>
#include <set>

#include "cli/handlers.hpp"

namespace noldoj::cli {
namespace {

FlagSpec P(const char* name, FlagType type = FlagType::String, bool required = true) {
    return FlagSpec{name, type, FlagPlace::Path, required, ""};
}

FlagSpec Q(const char* name, FlagType type = FlagType::String, bool required = false) {
    return FlagSpec{name, type, FlagPlace::Query, required, ""};
}

FlagSpec B(const char* name, FlagType type = FlagType::String, bool required = false) {
    return FlagSpec{name, type, FlagPlace::Body, required, ""};
}

std::vector<CommandSpec> buildTable() {
    std::vector<CommandSpec> t;

    // ===== auth（仅私人令牌，无用户名/密码登录） =====
    t.push_back({"auth", "send-code", "POST", "/auth/send-code", AuthMode::None, {B("email", FlagType::String, true)}, nullptr, false, "", "发送邮箱验证码"});
    t.push_back({"auth", "verify-code", "POST", "/auth/verify-code", AuthMode::None, {B("email", FlagType::String, true), B("code", FlagType::String, true)}, nullptr, false, "", "校验邮箱验证码"});
    t.push_back({"auth", "email-enabled", "GET", "/auth/email-enabled", AuthMode::None, {}, nullptr, false, "", "查询邮箱功能是否启用"});
    t.push_back({"auth", "change-password", "POST", "/auth/change-password", AuthMode::Required,
                 {B("old-password", FlagType::String, true), B("new-password", FlagType::String, true)}, nullptr, false, "", "修改密码"});
    t.push_back({"auth", "forgot-password", "POST", "/auth/forgot-password", AuthMode::None,
                 {B("username", FlagType::String, true), B("email", FlagType::String, true)}, nullptr, false, "", "申请重置密码"});
    t.push_back({"auth", "reset-password", "POST", "/auth/reset-password", AuthMode::None,
                 {B("username", FlagType::String, true), B("email", FlagType::String, true), B("code", FlagType::String, true), B("new-password", FlagType::String, true)}, nullptr, false, "", "校验验证码并重置密码"});
    t.push_back({"auth", "status", "GET", "", AuthMode::None, {}, &cmdAuthStatus, false, "", "查看当前站点令牌状态（本地）"});
    t.push_back({"auth", "tokeninfo", "GET", "/auth/tokeninfo", AuthMode::Required, {}, nullptr, false, "", "检测当前凭据类型、权限范围与到期时间"});
    t.push_back({"auth", "set-token", "GET", "", AuthMode::None, {B("token"), B("token-file")}, &cmdSetToken, false, "", "设置/更新私人令牌（长期有效，无需刷新）"});

    // ===== site / config =====
    t.push_back({"site", "list", "GET", "", AuthMode::None, {}, &cmdSiteList, false, "", "列出已配置站点"});
    t.push_back({"site", "add", "GET", "", AuthMode::None,
                 {B("name", FlagType::String, true), B("url", FlagType::String, true), B("token", FlagType::String, true),
                  B("default-language")}, &cmdSiteAdd, false, "", "添加站点（--name --url --token <pat>）"});
    t.push_back({"site", "remove", "GET", "", AuthMode::None, {B("name", FlagType::String, true)}, &cmdSiteRemove, false, "", "删除站点（--name）"});
    t.push_back({"site", "use", "GET", "", AuthMode::None, {B("name", FlagType::String, true)}, &cmdSiteUse, false, "", "设置默认站点（--name）"});
    t.push_back({"site", "show", "GET", "", AuthMode::None, {}, &cmdSiteShow, false, "", "显示当前生效的站点"});
    t.push_back({"config", "show", "GET", "", AuthMode::None, {}, &cmdConfigShow, false, "", "显示配置文件内容"});
    t.push_back({"config", "path", "GET", "", AuthMode::None, {}, &cmdConfigPath, false, "", "显示配置与凭证文件路径"});

    // ===== system =====
    t.push_back({"system", "health", "GET", "/health", AuthMode::None, {}, nullptr, false, "", "健康检查"});
    t.push_back({"system", "stats", "GET", "/stats", AuthMode::None, {}, nullptr, false, "", "平台统计"});
    t.push_back({"system", "status", "GET", "/system/status", AuthMode::Required, {}, nullptr, false, "", "系统运行状态（admin+）"});
    t.push_back({"system", "jobs", "GET", "/jobs", AuthMode::None, {}, nullptr, false, "", "任务列表（重定向到 submissions）"});
    t.push_back({"system", "job", "GET", "/jobs/:id", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "任务详情（重定向）"});

    // ===== problems =====
    t.push_back({"problem", "list", "GET", "/problems", AuthMode::Optional,
                 {Q("page", FlagType::Int), Q("limit", FlagType::Int), Q("search"), Q("tag")}, nullptr, false, "", "题目列表"});
    t.push_back({"problem", "get", "GET", "/problems/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "题目详情"});
    t.push_back({"problem", "create", "POST", "/problems", AuthMode::Required,
                 {B("title", FlagType::String, true), B("description"), B("input-desc"), B("output-desc"), B("hint"),
                  B("time-limit", FlagType::Int), B("memory-limit", FlagType::Int), B("problem-type"), B("compare-mode"),
                  B("real-number-tolerance", FlagType::Json), B("spj-code"), B("allowed-languages", FlagType::StrList),
                  B("is-public", FlagType::Bool), B("provider"), B("samples", FlagType::Json),
                  B("sample-input"), B("sample-output")}, nullptr, false, "", "创建题目（教师+）"});
    t.push_back({"problem", "update", "PUT", "/problems/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("description"), B("input-desc"), B("output-desc"), B("hint"),
                  B("time-limit", FlagType::Int), B("memory-limit", FlagType::Int), B("problem-type"), B("compare-mode"),
                  B("real-number-tolerance", FlagType::Json), B("spj-code"), B("allowed-languages", FlagType::StrList),
                  B("is-public", FlagType::Bool), B("provider"), B("samples", FlagType::Json),
                  B("sample-input"), B("sample-output")}, nullptr, false, "", "更新题目（教师+）"});
    t.push_back({"problem", "delete", "DELETE", "/problems/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除题目（教师+）"});
    t.push_back({"problem", "reindex", "PUT", "/problems/:id/reindex", AuthMode::Required, {P("id", FlagType::Int), B("new-id", FlagType::Int, true)}, nullptr, false, "", "题目重编号（admin+）"});
    t.push_back({"problem", "export", "GET", "/problems/:id/export", AuthMode::Required,
                 {P("id", FlagType::Int), Q("format")}, nullptr, true, "problem_{id}.{format}", "导出题目（json/zip）"});
    t.push_back({"problem", "import", "POST", "/problems/import", AuthMode::Required, {B("file", FlagType::String, true)}, &cmdImportProblem, false, "", "导入题目（.json/.zip）"});
    t.push_back({"problem", "visibility-get", "GET", "/problems/:id/visibility", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "读取题目可见性"});
    t.push_back({"problem", "visibility-set", "PUT", "/problems/:id/visibility", AuthMode::Required,
                 {P("id", FlagType::Int), B("visibility", FlagType::String, true), B("group-ids", FlagType::IntList)}, nullptr, false, "", "设置题目可见性（admin+）"});
    t.push_back({"problem", "testcase-add", "POST", "/problems/:id/testcases", AuthMode::Required,
                 {P("id", FlagType::Int), B("test-cases", FlagType::Json, true)}, nullptr, false, "", "批量添加测试点"});
    t.push_back({"problem", "testcase-update", "PUT", "/problems/:id/testcases/:tcid", AuthMode::Required,
                 {P("id", FlagType::Int), P("tcid", FlagType::Int), B("input-data"), B("output-data"), B("score", FlagType::Float), B("sort-order", FlagType::Int)}, nullptr, false, "", "更新测试点"});
    t.push_back({"problem", "testcase-delete", "DELETE", "/problems/:id/testcases/:tcid", AuthMode::Required,
                 {P("id", FlagType::Int), P("tcid", FlagType::Int)}, nullptr, false, "", "删除测试点"});
    t.push_back({"problem", "testcase-clear", "DELETE", "/problems/:id/testcases", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除全部测试点"});
    t.push_back({"problem", "testcase-group", "PUT", "/problems/:id/testcases/:tcid/group", AuthMode::Required,
                 {P("id", FlagType::Int), P("tcid", FlagType::Int), B("group-id", FlagType::Int)}, nullptr, false, "", "设置测试点所属分组"});
    t.push_back({"problem", "batch-testcases", "PUT", "/problems/:id/batch-testcases", AuthMode::Required,
                 {P("id", FlagType::Int), B("testcases", FlagType::Json, true)}, nullptr, false, "", "批量更新测试点"});
    t.push_back({"problem", "testdata-list", "GET", "/problems/:id/testdata", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "查看测试点列表"});
    t.push_back({"problem", "testdata-upload", "POST", "/problems/:id/testdata", AuthMode::Required,
                 {P("id", FlagType::Int), B("file", FlagType::String, true)}, &cmdUploadTestdata, false, "", "上传测试数据（name.in/name.out）"});
    t.push_back({"problem", "testdata-zip", "POST", "/problems/:id/testdata-zip", AuthMode::Required,
                 {P("id", FlagType::Int), B("file", FlagType::String, true)}, &cmdUploadTestdataZip, false, "", "上传 ZIP 测试数据"});
    t.push_back({"problem", "ai-testdata", "POST", "/problems/:id/ai-testdata", AuthMode::Required,
                 {P("id", FlagType::Int), B("samples", FlagType::Int), B("edge-cases", FlagType::Int)}, nullptr, false, "", "AI 生成测试数据（教师+）"});
    t.push_back({"problem", "groups", "GET", "/problems/:id/groups", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "测试分组列表"});
    t.push_back({"problem", "group-create", "POST", "/problems/:id/groups", AuthMode::Required,
                 {P("id", FlagType::Int), B("subtask-id", FlagType::String, true), B("score", FlagType::Float), B("aggregator"),
                  B("dependency", FlagType::IntList), B("scoring-script")}, nullptr, false, "", "创建测试分组"});
    t.push_back({"problem", "group-update", "PUT", "/problems/:id/groups/:gid", AuthMode::Required,
                 {P("id", FlagType::Int), P("gid", FlagType::Int), B("subtask-id"), B("score", FlagType::Float), B("aggregator"),
                  B("dependency", FlagType::IntList), B("scoring-script")}, nullptr, false, "", "更新测试分组"});
    t.push_back({"problem", "group-delete", "DELETE", "/problems/:id/groups/:gid", AuthMode::Required,
                 {P("id", FlagType::Int), P("gid", FlagType::Int)}, nullptr, false, "", "删除测试分组"});
    t.push_back({"problem", "scoring-script", "PUT", "/problems/:id/scoring-script", AuthMode::Required,
                 {P("id", FlagType::Int), B("scoring-script")}, nullptr, false, "", "设置题目级计分脚本"});
    t.push_back({"problem", "solutions", "GET", "/problems/:id/solutions", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "题解列表"});
    t.push_back({"problem", "solution-add", "POST", "/problems/:id/solutions", AuthMode::Required,
                 {P("id", FlagType::Int), B("article-id", FlagType::Int, true), B("sort-order", FlagType::Int), B("show-after-contest", FlagType::Bool)}, nullptr, false, "", "链接文章为题解"});
    t.push_back({"problem", "solution-remove", "DELETE", "/problems/:id/solutions/:sid", AuthMode::Required,
                 {P("id", FlagType::Int), P("sid", FlagType::Int)}, nullptr, false, "", "移除题解"});
    t.push_back({"problem", "discussions", "GET", "/problems/:id/discussions", AuthMode::None,
                 {P("id", FlagType::Int), Q("page", FlagType::Int)}, nullptr, false, "", "题目讨论列表"});
    t.push_back({"problem", "ac-submissions", "GET", "/problems/:id/ac-submissions", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "通过代码列表"});
    t.push_back({"problem", "hint", "POST", "/problems/:id/hint", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "AI 提示（AI Hint）"});
    t.push_back({"problem", "plagiarism-check", "POST", "/problems/:id/plagiarism-check", AuthMode::Required,
                 {P("id", FlagType::Int), B("threshold", FlagType::Int), B("min-submissions", FlagType::Int), B("start-time"), B("end-time")}, nullptr, false, "", "发起代码查重"});
    t.push_back({"problem", "plagiarism-report", "GET", "/problems/:id/plagiarism-report", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "获取查重报告"});

    // ===== tags =====
    t.push_back({"tag", "list", "GET", "/tags", AuthMode::Optional, {}, nullptr, false, "", "标签列表"});
    t.push_back({"tag", "get", "GET", "/tags/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "标签详情"});
    t.push_back({"tag", "create", "POST", "/tags", AuthMode::Required, {B("name", FlagType::String, true), B("color")}, nullptr, false, "", "创建标签（教师+）"});
    t.push_back({"tag", "update", "PUT", "/tags/:id", AuthMode::Required, {P("id", FlagType::Int), B("name"), B("color")}, nullptr, false, "", "更新标签"});
    t.push_back({"tag", "delete", "DELETE", "/tags/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除标签"});
    t.push_back({"tag", "problem-get", "GET", "/tags/problem/:problem-id", AuthMode::None, {P("problem-id", FlagType::Int)}, nullptr, false, "", "获取题目标签"});
    t.push_back({"tag", "problem-set", "POST", "/tags/problem/:problem-id", AuthMode::Required, {P("problem-id", FlagType::Int), B("tag-ids", FlagType::IntList)}, nullptr, false, "", "设置题目标签"});

    // ===== submissions =====
    t.push_back({"submission", "list", "GET", "/submissions", AuthMode::Required,
                 {Q("page", FlagType::Int), Q("limit", FlagType::Int), Q("user-id", FlagType::Int), Q("problem-id", FlagType::Int), Q("status")}, nullptr, false, "", "提交列表"});
    t.push_back({"submission", "get", "GET", "/submissions/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "提交详情"});
    t.push_back({"submission", "create", "POST", "/submissions", AuthMode::Required,
                 {B("problem-id", FlagType::Int, true), B("language"), B("source-code"), B("source-file"),
                  B("answer-data"), B("contest-id", FlagType::Int), B("watch", FlagType::Bool)}, &cmdSubmit, false, "", "提交代码"});
    t.push_back({"submission", "delete", "DELETE", "/submissions/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除提交（admin+）"});
    t.push_back({"submission", "detail", "GET", "/submissions/:id/detail", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "提交详细信息"});
    t.push_back({"submission", "diff", "GET", "/submissions/:id/diff", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "与上次提交对比"});
    t.push_back({"submission", "rejudge", "POST", "/submissions/:id/rejudge", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "重测提交（教师+）"});
    t.push_back({"submission", "code", "GET", "/submissions/:id/code", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "读取通过代码"});
    t.push_back({"submission", "watch", "GET", "/submissions/:id", AuthMode::Required, {P("id", FlagType::Int)}, &cmdWatchSubmission, false, "", "轮询提交直到判题完成"});

    // ===== ide =====
    t.push_back({"ide", "languages", "GET", "/ide/languages", AuthMode::None, {}, nullptr, false, "", "可用语言列表"});
    t.push_back({"ide", "review", "POST", "/ide/review", AuthMode::None, {B("language", FlagType::String, true), B("source-code", FlagType::String, true)}, nullptr, false, "", "AI 代码安全审查"});
    t.push_back({"ide", "run", "POST", "/ide/run", AuthMode::Required,
                 {B("language", FlagType::String, true), B("source-code"), B("source-file"), B("stdin"), B("stdin-file"),
                  B("wait", FlagType::Bool)}, &cmdIdeRun, false, "", "运行代码（--wait 自动轮询）"});
    t.push_back({"ide", "run-status", "GET", "/ide/run/:id", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "查询运行状态"});

    // ===== users =====
    t.push_back({"user", "list", "GET", "/users", AuthMode::Required,
                 {Q("page", FlagType::Int), Q("limit", FlagType::Int), Q("search"), Q("role")}, nullptr, false, "", "用户列表（admin+）"});
    t.push_back({"user", "get", "GET", "/users/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "用户管理详情（admin+）"});
    t.push_back({"user", "create", "POST", "/users", AuthMode::Required,
                 {B("username", FlagType::String, true), B("password", FlagType::String, true), B("nickname"), B("role")}, nullptr, false, "", "创建用户（su）"});
    t.push_back({"user", "delete", "DELETE", "/users/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除用户（su）"});
    t.push_back({"user", "me", "GET", "/users/me", AuthMode::Required, {}, nullptr, false, "", "当前用户信息"});
    t.push_back({"user", "update-me", "PUT", "/users/me", AuthMode::Required,
                 {B("nickname"), B("signature"), B("bio"), B("hide-rating", FlagType::Bool), B("hide-achievements", FlagType::Bool),
                  B("hide-dashboard", FlagType::Bool), B("hide-favorites", FlagType::Bool), B("preferred-language")}, nullptr, false, "", "更新个人资料"});
    t.push_back({"user", "my-virtual-contests", "GET", "/users/me/virtual-contests", AuthMode::Required, {}, nullptr, false, "", "我的虚拟比赛"});
    t.push_back({"user", "profile", "GET", "/users/:id/profile", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "公开用户资料"});
    t.push_back({"user", "rating", "GET", "/users/rating", AuthMode::None, {Q("page", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "Rating 排行榜"});
    t.push_back({"user", "sudo-login", "POST", "/users/sudo-login", AuthMode::Required,
                 {B("user-id", FlagType::Int, true)}, &cmdSudoLogin, false, "", "免密登录指定用户（su）"});
    t.push_back({"user", "role", "PUT", "/users/:id/role", AuthMode::Required, {P("id", FlagType::Int), B("role", FlagType::String, true)}, nullptr, false, "", "修改角色（su）"});
    t.push_back({"user", "ban", "POST", "/users/:id/ban", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "封禁用户"});
    t.push_back({"user", "unban", "POST", "/users/:id/unban", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "解封用户"});
    t.push_back({"user", "force-logout", "POST", "/users/:id/force-logout", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "强制登出"});
    t.push_back({"user", "reset-password", "POST", "/users/:id/reset-password", AuthMode::Required,
                 {P("id", FlagType::Int), B("new-password", FlagType::String, true)}, nullptr, false, "", "重置密码（su）"});
    t.push_back({"user", "set-rating", "PUT", "/users/:id/rating", AuthMode::Required,
                 {P("id", FlagType::Int), B("rating", FlagType::Int, true)}, nullptr, false, "", "修改 Rating（su）"});
    t.push_back({"user", "hide-rating", "PUT", "/users/:id/hide-rating", AuthMode::Required,
                 {P("id", FlagType::Int), B("hide-rating", FlagType::Int, true)}, nullptr, false, "", "切换隐藏 Rating"});
    t.push_back({"user", "upload-limits", "PUT", "/users/:id/upload-limits", AuthMode::Required,
                 {P("id", FlagType::Int), B("max-file-size", FlagType::Int), B("max-storage", FlagType::Int)}, nullptr, false, "", "设置上传限制（su）"});
    t.push_back({"user", "submit-lock-exempt", "PUT", "/users/:id/submit-lock-exempt", AuthMode::Required,
                 {P("id", FlagType::Int), B("submit-lock-exempt", FlagType::Bool, true)}, nullptr, false, "", "设置提交锁豁免（su）"});
    t.push_back({"user", "register-enabled", "GET", "/users/register-enabled", AuthMode::Required, {}, nullptr, false, "", "查询注册开关（su）"});
    t.push_back({"user", "set-register-enabled", "PUT", "/users/register-enabled", AuthMode::Required,
                 {B("enabled", FlagType::Bool, true)}, nullptr, false, "", "切换注册开关（su）"});

    // ===== groups =====
    t.push_back({"group", "list", "GET", "/groups", AuthMode::Required, {}, nullptr, false, "", "全部用户组（admin+）"});
    t.push_back({"group", "mine", "GET", "/groups/mine", AuthMode::Required, {}, nullptr, false, "", "我加入的组"});
    t.push_back({"group", "create", "POST", "/groups", AuthMode::Required, {B("name", FlagType::String, true), B("description")}, nullptr, false, "", "创建用户组（admin+）"});
    t.push_back({"group", "update", "PUT", "/groups/:id", AuthMode::Required, {P("id", FlagType::Int), B("name"), B("description")}, nullptr, false, "", "修改用户组"});
    t.push_back({"group", "delete", "DELETE", "/groups/:id", AuthMode::Required, {P("id", FlagType::Int), Q("force", FlagType::Int)}, nullptr, false, "", "删除用户组"});
    t.push_back({"group", "members", "GET", "/groups/:id/members", AuthMode::Required,
                 {P("id", FlagType::Int), Q("page", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "组内成员"});
    t.push_back({"group", "member-add", "POST", "/groups/:id/members", AuthMode::Required,
                 {P("id", FlagType::Int), B("user-id", FlagType::Int, true)}, nullptr, false, "", "添加成员"});
    t.push_back({"group", "member-remove", "DELETE", "/groups/:id/members/:uid", AuthMode::Required,
                 {P("id", FlagType::Int), P("uid", FlagType::Int)}, nullptr, false, "", "移除成员"});
    t.push_back({"group", "import", "POST", "/groups/:id/import", AuthMode::Required,
                 {P("id", FlagType::Int), B("users", FlagType::Json, true), B("default-password")}, nullptr, false, "", "批量导入成员（su）"});

    // ===== achievements / statistics =====
    t.push_back({"achievement", "list", "GET", "/achievements", AuthMode::Required, {Q("user-id", FlagType::Int)}, nullptr, false, "", "成就列表"});
    t.push_back({"statistics", "me", "GET", "/statistics/me/stats", AuthMode::Required, {Q("user-id", FlagType::Int)}, nullptr, false, "", "个人数据看板"});
    t.push_back({"statistics", "wordcloud", "GET", "/statistics/wordcloud", AuthMode::Optional, {}, nullptr, false, "", "提交云图"});

    // ===== favorites =====
    t.push_back({"favorite", "list", "GET", "/favorites", AuthMode::Required,
                 {Q("user-id", FlagType::Int), Q("page", FlagType::Int), Q("size", FlagType::Int)}, nullptr, false, "", "收藏列表"});
    t.push_back({"favorite", "status", "GET", "/favorites/status", AuthMode::Required, {Q("ids", FlagType::String, true)}, nullptr, false, "", "批量检查收藏状态"});
    t.push_back({"favorite", "add", "POST", "/favorites/:problem-id", AuthMode::Required, {P("problem-id", FlagType::Int)}, nullptr, false, "", "收藏题目"});
    t.push_back({"favorite", "remove", "DELETE", "/favorites/:problem-id", AuthMode::Required, {P("problem-id", FlagType::Int)}, nullptr, false, "", "取消收藏"});

    // ===== languages =====
    t.push_back({"language", "list", "GET", "/languages", AuthMode::None, {}, nullptr, false, "", "语言列表"});
    t.push_back({"language", "create", "POST", "/languages", AuthMode::Required,
                 {B("name", FlagType::String, true), B("display-name", FlagType::String, true), B("compile-cmd"),
                  B("run-cmd", FlagType::String, true), B("extension", FlagType::String, true)}, nullptr, false, "", "添加语言（su）"});
    t.push_back({"language", "update", "PUT", "/languages/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("name"), B("display-name"), B("compile-cmd"), B("run-cmd"), B("extension")}, nullptr, false, "", "更新语言"});
    t.push_back({"language", "delete", "DELETE", "/languages/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除语言"});

    // ===== contests =====
    t.push_back({"contest", "list", "GET", "/contests", AuthMode::Optional, {}, nullptr, false, "", "比赛列表"});
    t.push_back({"contest", "get", "GET", "/contests/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "比赛详情"});
    t.push_back({"contest", "create", "POST", "/contests", AuthMode::Required,
                 {B("title", FlagType::String, true), B("description"), B("start-time", FlagType::String, true),
                  B("end-time", FlagType::String, true), B("is-virtual", FlagType::Int), B("freeze-minutes", FlagType::Int),
                  B("is-hidden", FlagType::Int), B("stats-public", FlagType::Int)}, nullptr, false, "", "创建比赛（教师+）"});
    t.push_back({"contest", "update", "PUT", "/contests/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("description"), B("start-time"), B("end-time"), B("is-virtual", FlagType::Int),
                  B("freeze-minutes", FlagType::Int), B("is-hidden", FlagType::Int), B("stats-public", FlagType::Int)}, nullptr, false, "", "更新比赛"});
    t.push_back({"contest", "delete", "DELETE", "/contests/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除比赛"});
    t.push_back({"contest", "visibility-get", "GET", "/contests/:id/visibility", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "读取比赛可见性"});
    t.push_back({"contest", "visibility-set", "PUT", "/contests/:id/visibility", AuthMode::Required,
                 {P("id", FlagType::Int), B("visibility", FlagType::String, true), B("group-ids", FlagType::IntList)}, nullptr, false, "", "设置比赛可见性（admin+）"});
    t.push_back({"contest", "problem-add", "POST", "/contests/:id/problems", AuthMode::Required,
                 {P("id", FlagType::Int), B("problem-id", FlagType::Int, true), B("alias"), B("sort-order", FlagType::Int)}, nullptr, false, "", "添加题目到比赛"});
    t.push_back({"contest", "problem-remove", "DELETE", "/contests/:id/problems/:pid", AuthMode::Required,
                 {P("id", FlagType::Int), P("pid", FlagType::Int)}, nullptr, false, "", "从比赛中移除题目"});
    t.push_back({"contest", "invite", "POST", "/contests/:id/invite", AuthMode::Required,
                 {P("id", FlagType::Int), B("user-id", FlagType::Int, true)}, nullptr, false, "", "邀请用户参赛"});
    t.push_back({"contest", "join", "POST", "/contests/:id/join", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "加入比赛"});
    t.push_back({"contest", "participants", "GET", "/contests/:id/participants", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "参赛用户列表"});
    t.push_back({"contest", "participant-remove", "DELETE", "/contests/:id/participants/:uid", AuthMode::Required,
                 {P("id", FlagType::Int), P("uid", FlagType::Int)}, nullptr, false, "", "移除参赛用户"});
    t.push_back({"contest", "leaderboard", "GET", "/contests/:id/leaderboard", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "比赛排行榜"});
    t.push_back({"contest", "stats", "GET", "/contests/:id/stats", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "比赛数据统计"});
    t.push_back({"contest", "unfreeze", "POST", "/contests/:id/unfreeze", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "解除排行榜冻结"});
    t.push_back({"contest", "announcements", "GET", "/contests/:id/announcements", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "比赛内公告"});
    t.push_back({"contest", "virtual-start", "POST", "/contests/:id/virtual-start", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "发起虚拟参赛"});
    t.push_back({"contest", "plagiarism-check", "POST", "/contests/:id/plagiarism-check", AuthMode::Required,
                 {P("id", FlagType::Int), B("threshold", FlagType::Int), B("min-submissions", FlagType::Int)}, nullptr, false, "", "比赛一键查重"});
    t.push_back({"contest", "discussions", "GET", "/contests/:id/discussions", AuthMode::None,
                 {P("id", FlagType::Int), Q("page", FlagType::Int)}, nullptr, false, "", "比赛讨论列表"});
    t.push_back({"contest", "export-csv", "GET", "/contests/:id/export.csv", AuthMode::Optional,
                 {P("id", FlagType::Int), Q("scope")}, nullptr, true, "contest_{id}.csv", "导出排行榜 CSV"});
    t.push_back({"contest", "teams", "GET", "/contests/:id/teams", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "队伍列表"});
    t.push_back({"contest", "team-create", "POST", "/contests/:id/teams", AuthMode::Required,
                 {P("id", FlagType::Int), B("name", FlagType::String, true)}, nullptr, false, "", "创建队伍"});
    t.push_back({"contest", "team-join", "POST", "/contests/:id/teams/join", AuthMode::Required,
                 {P("id", FlagType::Int), B("invite-code", FlagType::String, true)}, nullptr, false, "", "用邀请码加入队伍"});
    t.push_back({"contest", "team-mine", "GET", "/contests/:id/teams/mine", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "我的队伍"});
    t.push_back({"contest", "team-leave", "DELETE", "/contests/:id/teams/mine", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "退出/解散队伍"});
    t.push_back({"contest", "team-dissolve", "DELETE", "/contests/:id/teams/:tid", AuthMode::Required,
                 {P("id", FlagType::Int), P("tid", FlagType::Int)}, nullptr, false, "", "解散队伍"});

    // ===== exams =====
    t.push_back({"exam", "list", "GET", "/exams", AuthMode::None, {}, nullptr, false, "", "试卷列表"});
    t.push_back({"exam", "get", "GET", "/exams/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "试卷详情"});
    t.push_back({"exam", "create", "POST", "/exams", AuthMode::Required,
                 {B("title", FlagType::String, true), B("description"), B("time-limit", FlagType::Int), B("pass-score", FlagType::Float),
                  B("max-attempts", FlagType::Int), B("show-answer", FlagType::Int), B("is-public", FlagType::Int),
                  B("is-hidden", FlagType::Int), B("allow-ai-grading", FlagType::Int), B("start-time"), B("end-time"),
                  B("freeze-minutes", FlagType::Int), B("leaderboard-enabled", FlagType::Int),
                  B("leaderboard-view-incomplete", FlagType::Int), B("stats-public", FlagType::Int),
                  B("is-survey", FlagType::Int), B("is-anonymous", FlagType::Int), B("questions", FlagType::Json)}, nullptr, false, "", "创建试卷"});
    t.push_back({"exam", "update", "PUT", "/exams/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("description"), B("time-limit", FlagType::Int), B("pass-score", FlagType::Float),
                  B("max-attempts", FlagType::Int), B("show-answer", FlagType::Int), B("is-public", FlagType::Int),
                  B("is-hidden", FlagType::Int), B("allow-ai-grading", FlagType::Int), B("start-time"), B("end-time"),
                  B("freeze-minutes", FlagType::Int), B("leaderboard-enabled", FlagType::Int),
                  B("leaderboard-view-incomplete", FlagType::Int), B("stats-public", FlagType::Int),
                  B("is-survey", FlagType::Int), B("is-anonymous", FlagType::Int), B("questions", FlagType::Json),
                  B("manual-frozen", FlagType::Int), B("unfrozen", FlagType::Int)}, nullptr, false, "", "编辑试卷"});
    t.push_back({"exam", "delete", "DELETE", "/exams/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除试卷"});
    t.push_back({"exam", "visibility-get", "GET", "/exams/:id/visibility", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "读取试卷可见性"});
    t.push_back({"exam", "visibility-set", "PUT", "/exams/:id/visibility", AuthMode::Required,
                 {P("id", FlagType::Int), B("visibility", FlagType::String, true), B("group-ids", FlagType::IntList)}, nullptr, false, "", "设置试卷可见性（admin+）"});
    t.push_back({"exam", "submit", "POST", "/exams/:id/submit", AuthMode::Required,
                 {P("id", FlagType::Int), B("answers", FlagType::Json, true)}, nullptr, false, "", "交卷"});
    t.push_back({"exam", "submission", "GET", "/exams/:id/submission/:sid", AuthMode::Required,
                 {P("id", FlagType::Int), P("sid", FlagType::Int)}, nullptr, false, "", "考生答卷详情"});
    t.push_back({"exam", "submissions", "GET", "/exams/:id/submissions", AuthMode::Required,
                 {P("id", FlagType::Int), Q("page", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "试卷提交列表（教师）"});
    t.push_back({"exam", "grade", "POST", "/exams/:id/grade/:sid", AuthMode::Required,
                 {P("id", FlagType::Int), P("sid", FlagType::Int)}, nullptr, false, "", "手动批改（--body-json 传分数）"});
    t.push_back({"exam", "ai-grade", "POST", "/exams/:id/ai-grade/:sid", AuthMode::Required,
                 {P("id", FlagType::Int), P("sid", FlagType::Int)}, nullptr, false, "", "AI 批改"});
    t.push_back({"exam", "leaderboard", "GET", "/exams/:id/leaderboard", AuthMode::Optional,
                 {P("id", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "试卷排行榜"});
    t.push_back({"exam", "stats", "GET", "/exams/:id/stats", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "试卷数据统计"});
    t.push_back({"exam", "export-csv", "GET", "/exams/:id/export.csv", AuthMode::Required,
                 {P("id", FlagType::Int), Q("scope")}, nullptr, true, "exam_{id}.csv", "导出成绩单 CSV"});

    // ===== articles =====
    t.push_back({"article", "list", "GET", "/articles", AuthMode::Optional,
                 {Q("page", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "文章列表"});
    t.push_back({"article", "get", "GET", "/articles/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "文章详情"});
    t.push_back({"article", "create", "POST", "/articles", AuthMode::Required,
                 {B("title", FlagType::String, true), B("content"), B("provider"), B("is-published", FlagType::Int)}, nullptr, false, "", "发布文章（教师+）"});
    t.push_back({"article", "update", "PUT", "/articles/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("content"), B("provider"), B("is-published", FlagType::Int)}, nullptr, false, "", "编辑文章"});
    t.push_back({"article", "delete", "DELETE", "/articles/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除文章"});

    // ===== uploads =====
    t.push_back({"upload", "list", "GET", "/uploads", AuthMode::Required, {Q("page", FlagType::Int), Q("limit", FlagType::Int)}, nullptr, false, "", "文件列表（教师+）"});
    t.push_back({"upload", "create", "POST", "/uploads", AuthMode::Required, {B("file", FlagType::String, true)}, &cmdUploadFile, false, "", "上传文件（教师+）"});
    t.push_back({"upload", "get", "GET", "/uploads/:filename", AuthMode::None, {P("filename")}, nullptr, true, "{filename}", "下载/访问文件"});
    t.push_back({"upload", "delete", "DELETE", "/uploads/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除文件（admin+）"});

    // ===== categories =====
    t.push_back({"category", "list", "GET", "/categories", AuthMode::None, {}, nullptr, false, "", "分类列表"});
    t.push_back({"category", "get", "GET", "/categories/:id", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "分类详情"});
    t.push_back({"category", "create", "POST", "/categories", AuthMode::Required, {B("name", FlagType::String, true)}, nullptr, false, "", "创建分类（admin+）"});
    t.push_back({"category", "update", "PUT", "/categories/:id", AuthMode::Required, {P("id", FlagType::Int), B("name")}, nullptr, false, "", "更新分类"});
    t.push_back({"category", "delete", "DELETE", "/categories/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除分类"});
    t.push_back({"category", "problem-add", "POST", "/categories/:id/problems/:pid", AuthMode::Required,
                 {P("id", FlagType::Int), P("pid", FlagType::Int)}, nullptr, false, "", "把题目加入分类"});
    t.push_back({"category", "problem-remove", "DELETE", "/categories/:id/problems/:pid", AuthMode::Required,
                 {P("id", FlagType::Int), P("pid", FlagType::Int)}, nullptr, false, "", "把题目移出分类"});

    // ===== announcements =====
    t.push_back({"announcement", "list", "GET", "/announcements", AuthMode::None, {Q("type")}, nullptr, false, "", "公告列表"});
    t.push_back({"announcement", "get", "GET", "/announcements/:id", AuthMode::None, {P("id", FlagType::Int)}, nullptr, false, "", "公告详情"});
    t.push_back({"announcement", "create", "POST", "/announcements", AuthMode::Required,
                 {B("title", FlagType::String, true), B("content"), B("type"), B("pinned", FlagType::Bool)}, nullptr, false, "", "创建公告（教师+）"});
    t.push_back({"announcement", "update", "PUT", "/announcements/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("content"), B("pinned", FlagType::Bool)}, nullptr, false, "", "更新公告"});
    t.push_back({"announcement", "delete", "DELETE", "/announcements/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除公告"});

    // ===== problem sets =====
    t.push_back({"problemset", "list", "GET", "/problem-sets", AuthMode::Optional, {}, nullptr, false, "", "题单列表"});
    t.push_back({"problemset", "mine", "GET", "/problem-sets/mine", AuthMode::Required, {}, nullptr, false, "", "我创建的题单"});
    t.push_back({"problemset", "get", "GET", "/problem-sets/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "题单详情"});
    t.push_back({"problemset", "create", "POST", "/problem-sets", AuthMode::Required,
                 {B("title", FlagType::String, true), B("description"), B("is-public", FlagType::Int), B("problemIds", FlagType::IntList)}, nullptr, false, "", "创建题单"});
    t.push_back({"problemset", "update", "PUT", "/problem-sets/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("description"), B("is-public", FlagType::Int)}, nullptr, false, "", "更新题单信息"});
    t.push_back({"problemset", "delete", "DELETE", "/problem-sets/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除题单"});
    t.push_back({"problemset", "set-problems", "PUT", "/problem-sets/:id/problems", AuthMode::Required,
                 {P("id", FlagType::Int), B("problemIds", FlagType::IntList, true)}, nullptr, false, "", "覆盖式设置题目列表"});
    t.push_back({"problemset", "progress", "GET", "/problem-sets/:id/progress", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "当前用户解题进度"});
    t.push_back({"problemset", "mark-progress", "POST", "/problem-sets/:id/progress/:pid", AuthMode::Required,
                 {P("id", FlagType::Int), P("pid", FlagType::Int)}, nullptr, false, "", "标记题目已 AC"});
    t.push_back({"problemset", "visibility-get", "GET", "/problem-sets/:id/visibility", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "读取题单可见性"});
    t.push_back({"problemset", "visibility-set", "PUT", "/problem-sets/:id/visibility", AuthMode::Required,
                 {P("id", FlagType::Int), B("visibility", FlagType::String, true), B("group-ids", FlagType::IntList)}, nullptr, false, "", "设置题单可见性（admin+）"});

    // ===== virtual contests =====
    t.push_back({"vcontest", "get", "GET", "/virtual-contests/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "虚拟比赛详情"});
    t.push_back({"vcontest", "ranking", "GET", "/virtual-contests/:id/ranking", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "本人排名数据"});

    // ===== plagiarism =====
    t.push_back({"plagiarism", "task", "GET", "/plagiarism/:task-id", AuthMode::Required, {P("task-id", FlagType::Int)}, nullptr, false, "", "查重任务进度/结果"});
    t.push_back({"plagiarism", "pair", "GET", "/plagiarism/pairs/:pair-id", AuthMode::Required, {P("pair-id", FlagType::Int)}, nullptr, false, "", "查重对详情"});

    // ===== notifications =====
    t.push_back({"notification", "list", "GET", "/notifications", AuthMode::Required,
                 {Q("page", FlagType::Int), Q("size", FlagType::Int), Q("unread", FlagType::Int)}, nullptr, false, "", "通知列表"});
    t.push_back({"notification", "unread-count", "GET", "/notifications/unread-count", AuthMode::Required, {}, nullptr, false, "", "未读数量"});
    t.push_back({"notification", "read-all", "PUT", "/notifications/read-all", AuthMode::Required, {}, nullptr, false, "", "全部标记已读"});
    t.push_back({"notification", "read", "PUT", "/notifications/:id/read", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "标记单条已读"});
    t.push_back({"notification", "delete", "DELETE", "/notifications/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除通知"});

    // ===== notebook =====
    t.push_back({"notebook", "list", "GET", "/notebook", AuthMode::Required, {Q("page", FlagType::Int), Q("size", FlagType::Int)}, nullptr, false, "", "错题列表"});
    t.push_back({"notebook", "mark", "PUT", "/notebook/:problem-id", AuthMode::Required,
                 {P("problem-id", FlagType::Int), B("status"), B("note")}, nullptr, false, "", "标记/保存笔记"});
    t.push_back({"notebook", "remove", "DELETE", "/notebook/:problem-id", AuthMode::Required, {P("problem-id", FlagType::Int)}, nullptr, false, "", "移出错题本"});

    // ===== daily =====
    t.push_back({"daily", "today", "GET", "/daily/today", AuthMode::Optional, {}, nullptr, false, "", "今日题目与打卡状态"});
    t.push_back({"daily", "checkin", "POST", "/daily/checkin", AuthMode::Required, {}, nullptr, false, "", "打卡"});

    // ===== discussions =====
    t.push_back({"discussion", "get", "GET", "/discussions/:id", AuthMode::Optional, {P("id", FlagType::Int)}, nullptr, false, "", "讨论详情"});
    t.push_back({"discussion", "create", "POST", "/discussions", AuthMode::Required,
                 {B("title", FlagType::String, true), B("content", FlagType::String, true), B("problem-id", FlagType::Int), B("contest-id", FlagType::Int)}, nullptr, false, "", "创建讨论"});
    t.push_back({"discussion", "update", "PUT", "/discussions/:id", AuthMode::Required,
                 {P("id", FlagType::Int), B("title"), B("content"), B("pinned", FlagType::Bool), B("locked", FlagType::Bool), B("is-official", FlagType::Bool)}, nullptr, false, "", "更新讨论"});
    t.push_back({"discussion", "delete", "DELETE", "/discussions/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除讨论"});
    t.push_back({"discussion", "reply", "POST", "/discussions/:id/replies", AuthMode::Required,
                 {P("id", FlagType::Int), B("content", FlagType::String, true), B("parent-id", FlagType::Int)}, nullptr, false, "", "发表回复"});
    t.push_back({"discussion", "delete-reply", "DELETE", "/discussions/:id/replies/:rid", AuthMode::Required,
                 {P("id", FlagType::Int), P("rid", FlagType::Int)}, nullptr, false, "", "删除回复"});
    t.push_back({"discussion", "pin", "PUT", "/discussions/:id/pin", AuthMode::Required,
                 {P("id", FlagType::Int), B("pinned", FlagType::Bool, true)}, nullptr, false, "", "置顶/取消置顶"});
    t.push_back({"discussion", "lock", "PUT", "/discussions/:id/lock", AuthMode::Required,
                 {P("id", FlagType::Int), B("locked", FlagType::Bool, true)}, nullptr, false, "", "锁定/解锁"});
    t.push_back({"discussion", "official", "PUT", "/discussions/:id/official", AuthMode::Required,
                 {P("id", FlagType::Int), B("is-official", FlagType::Bool, true)}, nullptr, false, "", "标记官方题解"});

    // ===== messages =====
    t.push_back({"message", "list", "GET", "/messages", AuthMode::Required, {Q("page", FlagType::Int), Q("size", FlagType::Int)}, nullptr, false, "", "会话列表"});
    t.push_back({"message", "unread-count", "GET", "/messages/unread-count", AuthMode::Required, {}, nullptr, false, "", "未读私信总数"});
    t.push_back({"message", "history", "GET", "/messages/:username", AuthMode::Required,
                 {P("username"), Q("limit", FlagType::Int), Q("before-id", FlagType::Int)}, nullptr, false, "", "与某用户的历史消息"});
    t.push_back({"message", "send", "POST", "/messages", AuthMode::Required,
                 {B("to"), B("receiver-id", FlagType::Int), B("content", FlagType::String, true)}, nullptr, false, "", "发送私信"});
    t.push_back({"message", "read", "PUT", "/messages/:username/read", AuthMode::Required, {P("username")}, nullptr, false, "", "标记会话已读"});
    t.push_back({"message", "delete", "DELETE", "/messages/:id", AuthMode::Required, {P("id", FlagType::Int)}, nullptr, false, "", "删除单条消息"});

    return t;
}

const std::vector<CommandSpec> kTable = buildTable();

}  // namespace

const std::vector<CommandSpec>& commandTable() { return kTable; }

const CommandSpec* findCommand(const std::string& group, const std::string& name) {
    for (const auto& spec : kTable) {
        if (spec.group == group && spec.name == name) return &spec;
    }
    return nullptr;
}

void printGroups() {
    std::set<std::string> groups;
    for (const auto& spec : kTable) groups.insert(spec.group);
    groups.insert("api");  // api 为通用请求分组，命令位置由裸路径决定，不进入命令表

    std::cout << "用法: noldoj [全局选项] <分组> <命令> [选项]\n\n全局选项:\n"
              << "  --site <name|url>      选择站点（缺省使用配置中的 default_site）\n"
              << "  --access-token <t>     临时覆盖私人令牌（不写入本地）\n"
              << "  --config-dir <path>    覆盖配置目录\n"
              << "  --body-json <json>     直接指定请求体 JSON（可与字段选项合并）\n"
              << "  --body-json-file <f>   从文件读取请求体 JSON\n"
              << "  --output, -o <path>    输出/下载到文件（- 表示标准输出）\n"
              << "  --raw                  紧凑 JSON 输出\n"
              << "  --verbose              打印请求与响应摘要\n\n分组:\n";
    for (const auto& group : groups) {
        std::cout << "  " << group << "\n";
    }
    std::cout << "\n使用 `noldoj help <分组>` 查看该分组下的命令。\n"
              << "直接调用任意接口: noldoj api <路径> [选项]（详见 `noldoj help api`）。\n";
}

void printHelp(const std::string& group) {
    if (group.empty()) {
        printGroups();
        return;
    }

    if (group == "api") {
        std::cout << "分组: api\n\n"
                  << "  noldoj api <路径> [选项]  直接向接口发起请求（类似 gh api）\n\n"
                  << "  路径为站点相对路径，例如 /auth/tokeninfo；也可写成 auth/tokeninfo\n"
                  << "  或带前缀的 /api/v1/auth/tokeninfo（会自动去掉该前缀）。\n"
                  << "  默认方法为 GET；带 -f/-F 字段或 --input 时默认 POST。\n\n"
                  << "  选项:\n"
                  << "    -X, --method <方法>     指定 HTTP 方法（GET/POST/PUT/PATCH/DELETE/HEAD/OPTIONS）\n"
                  << "    -f, --raw-field k=v     追加字符串字段（可重复）；GET/HEAD/DELETE 作为查询参数，其余作为 JSON 请求体\n"
                  << "    -F, --field k=v         追加类型化字段（可重复）；数字/布尔/null/数组按 JSON 解析\n"
                  << "    -H, --header \"K: V\"     追加请求头（可重复，可覆盖默认的 Content-Type / Accept）\n"
                  << "    --input <文件|->        用文件或标准输入的内容作为原始请求体\n"
                  << "    -i, --include           同时输出响应状态行与响应头\n\n"
                  << "  全局选项 --site / --access-token / --raw / --verbose / -o 同样适用。\n\n"
                  << "  示例:\n"
                  << "    noldoj api /auth/tokeninfo\n"
                  << "    noldoj api -X GET /problems -f page=1 -f limit=20\n"
                  << "    noldoj api /problems -X POST -F title=示例 -F is_public=true\n"
                  << "    noldoj api /problems/1 -i\n"
                  << "    cat body.json | noldoj api /problems --input -\n";
        return;
    }

    bool found = false;
    for (const auto& spec : kTable) {
        if (spec.group != group) continue;
        if (!found) {
            std::cout << "分组: " << group << "\n\n";
            found = true;
        }
        std::cout << "  " << spec.group << " " << spec.name << "  " << spec.help << "\n";
        if (!spec.method.empty()) {
            std::cout << "      " << spec.method << " " << spec.path << "\n";
        }
        if (spec.auth == AuthMode::Required) std::cout << "      [需要认证]\n";
        if (spec.auth == AuthMode::Optional) std::cout << "      [可选认证]\n";
        for (const auto& flag : spec.flags) {
            std::cout << "      --" << flag.name << (flag.required ? " (必填)" : "") << "  "
                      << (flag.place == FlagPlace::Path ? "[路径]" : flag.place == FlagPlace::Query ? "[查询]" : "[请求体]")
                      << "\n";
        }
        std::cout << "\n";
    }
    if (!found) std::cout << "未知分组: " << group << "\n";
}

}  // namespace noldoj::cli
