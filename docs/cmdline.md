# noldoj-cli 命令行手册

> 适用于 `noldoj-cli 1.0.0` 及以上版本。返回 [首页](index.html) · [平台支持](support.html) · [GitHub 仓库](https://github.com/dxxjudges/noldoj-cli)

`noldoj-cli` 是一个把 NoldOJ 平台全部 REST 接口封装成子命令的命令行工具，并额外提供类似 `gh api` 的通用请求能力，可直接访问任意接口而不必等待命令封装。

---

## 目录

- [基本用法](#基本用法)
- [全局选项](#全局选项)
- [认证与私人令牌](#认证与私人令牌)
- [输出与退出码](#输出与退出码)
- [命令分组总览](#命令分组总览)
- [命令参考](#命令参考)
- [直接请求接口：noldoj api](#直接请求接口noldoj-api)
- [典型工作流](#典型工作流)

---

## 基本用法

```text
noldoj [全局选项] <分组> <命令> [选项]
noldoj api <路径> [选项]
```

示例：

```bash
# 添加一个站点（令牌在网页端「令牌」设置页创建，明文只显示一次）
noldoj site add --name main --url https://oj.example.com --token "noldoj_pat_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# 查看当前令牌信息
noldoj auth tokeninfo

# 列出题目
noldoj problem list --page 1 --limit 20 --search 图论

# 提交代码并等待评测结果
noldoj submission create --problem-id 1000 --language cpp --file main.cpp --watch
```

查看帮助：

```bash
noldoj --help              # 全局帮助与分组列表
noldoj help problem        # 查看 problem 分组下所有命令
noldoj --version           # 显示版本号
```

---

## 全局选项

全局选项可以出现在命令的任意位置，对所有分组生效。

| 选项 | 说明 |
| --- | --- |
| `--site <name\|url>` | 选择站点。可填配置中的站点名，也可直接写完整 URL（如 `https://oj.example.com`） |
| `--access-token <token>` | 本次调用临时覆盖私人令牌，不写入本地配置；值为 `noldoj_pat_...`，首尾空白会被自动去除 |
| `--config-dir <path>` | 覆盖配置目录（默认 `~/.noldoj-cli`，Windows 为 `%USERPROFILE%\.noldoj-cli`） |
| `--body-json <json>` | 直接指定请求体 JSON，可与命令的字段选项合并 |
| `--body-json-file <file>` | 从文件读取请求体 JSON |
| `-o, --output <path>` | 把输出写入文件；`-` 表示标准输出 |
| `--raw` | 输出紧凑 JSON（默认带缩进） |
| `--verbose` | 打印请求与响应摘要，便于排查 |
| `-i, --include` | 同时输出响应状态行与响应头 |
| `-y, --yes` | 对需要确认的破坏性操作直接确认 |
| `-h, --help` | 显示帮助 |
| `-v, --version` | 显示版本号 |

---

## 认证与私人令牌

CLI 只使用私人访问令牌（Personal Access Token，PAT）作为凭据，不提供用户名/密码登录，也不处理 refresh token。

- 令牌格式为 `noldoj_pat_` + 32 位 base64url 字符，在网页端「令牌」设置页创建；明文只在创建时返回一次。
- 创建时指定有效期（最长 3650 天，`0` 表示永久）与权限范围：`read` / `submit` / `write` / `account`。
- 权限不足返回 403；令牌过期、被吊销或账号被封禁时请求会被拒绝。
- 令牌的创建与吊销只能在网页端「令牌」页完成（这些接口拒绝 PAT），CLI 没有对应子命令。
- 以下接口只允许登录会话（JWT），私人令牌无法调用，CLI 会在本地直接报错并提示改用网页端：
  - `auth change-password`
  - `user sudo-login`
  - `user reset-password`
- `--access-token` 用于临时覆盖已保存的 PAT。若传入网页登录得到的 JWT，仅 GET 等只读请求可用；写请求会被服务端的 CSRF 防护拒绝。

---

## 输出与退出码

- 默认以缩进 JSON 输出到标准输出；`--raw` 输出单行紧凑 JSON。
- 支持 `@文件` 形式的字段值（如 `--description @readme.md`），以及 `--output` 下载文件。
- 退出码：`0` 成功或显示帮助；`1` 请求失败、参数错误或认证失败（错误信息输出到标准错误）。

---

## 命令分组总览

| 分组 | 用途 |
| --- | --- |
| `auth` | 邮箱验证码、密码相关、令牌状态与令牌设置 |
| `site` | 站点配置的增删改查与切换 |
| `config` | 查看配置文件与路径 |
| `system` | 健康检查、平台统计、系统状态与任务 |
| `problem` | 题目、测试点、测试数据、子任务分组、题解、查重 |
| `tag` | 题目标签管理 |
| `submission` | 提交、评测详情、重测、轮询 |
| `ide` | 在线运行、语言列表、AI 代码审查 |
| `user` | 用户资料、管理、角色、封禁、Rating |
| `group` | 用户组与成员管理 |
| `achievement` | 成就列表 |
| `statistics` | 个人数据看板与提交云图 |
| `favorite` | 题目收藏 |
| `language` | 评测语言管理 |
| `contest` | 比赛、题目、参赛者、排行榜、队伍、查重 |
| `exam` | 试卷、交卷、批改、成绩单 |
| `article` | 文章与题解文章 |
| `upload` | 文件上传与下载 |
| `category` | 题目分类 |
| `announcement` | 公告 |
| `problemset` | 题单与进度 |
| `vcontest` | 虚拟比赛详情与排名 |
| `plagiarism` | 查重任务与查重对 |
| `notification` | 站内通知 |
| `notebook` | 错题本 |
| `daily` | 每日一题与打卡 |
| `discussion` | 讨论、回复、置顶、锁定 |
| `message` | 私信会话 |
| `api` | 通用接口请求（类似 `gh api`） |

---

## 命令参考

### auth

| 命令 | 说明 |
| --- | --- |
| `auth send-code` | 发送邮箱验证码 |
| `auth verify-code` | 校验邮箱验证码 |
| `auth email-enabled` | 查询邮箱功能是否启用 |
| `auth change-password` | 修改密码（仅网页端可用：该接口拒绝私人令牌） |
| `auth forgot-password` | 申请重置密码 |
| `auth reset-password` | 校验验证码并重置密码 |
| `auth status` | 查看当前站点令牌状态（本地） |
| `auth tokeninfo` | 检测当前凭据类型、权限范围与到期时间 |
| `auth set-token` | 设置或更新私人令牌（长期有效，无需刷新） |

### site

| 命令 | 说明 |
| --- | --- |
| `site list` | 列出已配置站点 |
| `site add` | 添加站点（`--name --url --token <pat>`） |
| `site remove` | 删除站点（`--name`） |
| `site use` | 设置默认站点（`--name`） |
| `site show` | 显示当前生效的站点 |

### config

| 命令 | 说明 |
| --- | --- |
| `config show` | 显示配置文件内容 |
| `config path` | 显示配置与凭证文件路径、加密主密钥的存放位置 |

> 私人令牌不会以明文落盘：`credentials.json` 使用 AES-128-CTR + HMAC-SHA256 加密，主密钥为首次启动时生成的 128 位随机密钥，保存在系统凭据库（Windows 凭据管理器 / macOS 钥匙串 / Linux Secret Service），不可用时回退到配置目录下权限 0600 的 `master.key`。

### system

| 命令 | 说明 |
| --- | --- |
| `system health` | 健康检查 |
| `system stats` | 平台统计 |
| `system status` | 系统运行状态（admin+） |
| `system jobs` | 任务列表（重定向到 submissions） |
| `system job` | 任务详情（重定向） |

### problem

| 命令 | 说明 |
| --- | --- |
| `problem list` | 题目列表 |
| `problem get` | 题目详情 |
| `problem create` | 创建题目（教师+） |
| `problem update` | 更新题目（教师+） |
| `problem delete` | 删除题目（教师+） |
| `problem reindex` | 题目重编号（admin+） |
| `problem export` | 导出题目（json/zip） |
| `problem import` | 导入题目（.json/.zip） |
| `problem visibility-get` | 读取题目可见性 |
| `problem visibility-set` | 设置题目可见性（admin+） |
| `problem testcase-add` | 批量添加测试点 |
| `problem testcase-update` | 更新测试点 |
| `problem testcase-delete` | 删除测试点 |
| `problem testcase-clear` | 删除全部测试点 |
| `problem testcase-group` | 设置测试点所属分组 |
| `problem batch-testcases` | 批量更新测试点 |
| `problem testdata-list` | 查看测试点列表 |
| `problem testdata-upload` | 上传测试数据（`name.in` / `name.out`） |
| `problem testdata-zip` | 上传 ZIP 测试数据 |
| `problem ai-testdata` | AI 生成测试数据（教师+） |
| `problem groups` | 测试分组列表 |
| `problem group-create` | 创建测试分组 |
| `problem group-update` | 更新测试分组 |
| `problem group-delete` | 删除测试分组 |
| `problem scoring-script` | 设置题目级计分脚本 |
| `problem solutions` | 题解列表 |
| `problem solution-add` | 链接文章为题解 |
| `problem solution-remove` | 移除题解 |
| `problem discussions` | 题目讨论列表 |
| `problem ac-submissions` | 通过代码列表 |
| `problem hint` | AI 提示（AI Hint） |
| `problem plagiarism-check` | 发起代码查重 |
| `problem plagiarism-report` | 获取查重报告 |

### tag

| 命令 | 说明 |
| --- | --- |
| `tag list` | 标签列表 |
| `tag get` | 标签详情 |
| `tag create` | 创建标签（教师+） |
| `tag update` | 更新标签 |
| `tag delete` | 删除标签 |
| `tag problem-get` | 获取题目标签 |
| `tag problem-set` | 设置题目标签 |

### submission

| 命令 | 说明 |
| --- | --- |
| `submission list` | 提交列表 |
| `submission get` | 提交详情 |
| `submission create` | 提交代码 |
| `submission delete` | 删除提交（admin+） |
| `submission detail` | 提交详细信息 |
| `submission diff` | 与上次提交对比 |
| `submission rejudge` | 重测提交（教师+） |
| `submission code` | 读取通过代码 |
| `submission watch` | 轮询提交直到判题完成 |

### ide

| 命令 | 说明 |
| --- | --- |
| `ide languages` | 可用语言列表 |
| `ide review` | AI 代码安全审查 |
| `ide run` | 运行代码（`--wait` 自动轮询） |
| `ide run-status` | 查询运行状态 |

### user

| 命令 | 说明 |
| --- | --- |
| `user list` | 用户列表（admin+） |
| `user get` | 用户管理详情（admin+） |
| `user create` | 创建用户（su） |
| `user delete` | 删除用户（su） |
| `user me` | 当前用户信息 |
| `user update-me` | 更新个人资料 |
| `user my-virtual-contests` | 我的虚拟比赛 |
| `user profile` | 公开用户资料 |
| `user rating` | Rating 排行榜 |
| `user sudo-login` | 免密登录指定用户（su；仅网页端可用：该接口拒绝私人令牌） |
| `user role` | 修改角色（su） |
| `user ban` | 封禁用户 |
| `user unban` | 解封用户 |
| `user force-logout` | 强制登出 |
| `user reset-password` | 重置密码（su；仅网页端可用：该接口拒绝私人令牌） |
| `user set-rating` | 修改 Rating（su） |
| `user hide-rating` | 切换隐藏 Rating |
| `user upload-limits` | 设置上传限制（su） |
| `user submit-lock-exempt` | 设置提交锁豁免（su） |
| `user register-enabled` | 查询注册开关（su） |
| `user set-register-enabled` | 切换注册开关（su） |

### group

| 命令 | 说明 |
| --- | --- |
| `group list` | 全部用户组（admin+） |
| `group mine` | 我加入的组 |
| `group create` | 创建用户组（admin+） |
| `group update` | 修改用户组 |
| `group delete` | 删除用户组 |
| `group members` | 组内成员 |
| `group member-add` | 添加成员 |
| `group member-remove` | 移除成员 |
| `group import` | 批量导入成员（su） |

### achievement

| 命令 | 说明 |
| --- | --- |
| `achievement list` | 成就列表 |

### statistics

| 命令 | 说明 |
| --- | --- |
| `statistics me` | 个人数据看板 |
| `statistics wordcloud` | 提交云图 |

### favorite

| 命令 | 说明 |
| --- | --- |
| `favorite list` | 收藏列表 |
| `favorite status` | 批量检查收藏状态 |
| `favorite add` | 收藏题目 |
| `favorite remove` | 取消收藏 |

### language

| 命令 | 说明 |
| --- | --- |
| `language list` | 语言列表 |
| `language create` | 添加语言（su） |
| `language update` | 更新语言 |
| `language delete` | 删除语言 |

### contest

| 命令 | 说明 |
| --- | --- |
| `contest list` | 比赛列表 |
| `contest get` | 比赛详情 |
| `contest create` | 创建比赛（教师+） |
| `contest update` | 更新比赛 |
| `contest delete` | 删除比赛 |
| `contest visibility-get` | 读取比赛可见性 |
| `contest visibility-set` | 设置比赛可见性（admin+） |
| `contest problem-add` | 添加题目到比赛 |
| `contest problem-remove` | 从比赛中移除题目 |
| `contest invite` | 邀请用户参赛 |
| `contest join` | 加入比赛 |
| `contest participants` | 参赛用户列表 |
| `contest participant-remove` | 移除参赛用户 |
| `contest leaderboard` | 比赛排行榜 |
| `contest stats` | 比赛数据统计 |
| `contest unfreeze` | 解除排行榜冻结 |
| `contest announcements` | 比赛内公告 |
| `contest virtual-start` | 发起虚拟参赛 |
| `contest plagiarism-check` | 比赛一键查重 |
| `contest discussions` | 比赛讨论列表 |
| `contest export-csv` | 导出排行榜 CSV |
| `contest teams` | 队伍列表 |
| `contest team-create` | 创建队伍 |
| `contest team-join` | 用邀请码加入队伍 |
| `contest team-mine` | 我的队伍 |
| `contest team-leave` | 退出或解散队伍 |
| `contest team-dissolve` | 解散队伍 |

### exam

| 命令 | 说明 |
| --- | --- |
| `exam list` | 试卷列表 |
| `exam get` | 试卷详情 |
| `exam create` | 创建试卷 |
| `exam update` | 编辑试卷 |
| `exam delete` | 删除试卷 |
| `exam visibility-get` | 读取试卷可见性 |
| `exam visibility-set` | 设置试卷可见性（admin+） |
| `exam submit` | 交卷 |
| `exam submission` | 考生答卷详情 |
| `exam submissions` | 试卷提交列表（教师） |
| `exam grade` | 手动批改（`--body-json` 传分数） |
| `exam ai-grade` | AI 批改 |
| `exam leaderboard` | 试卷排行榜 |
| `exam stats` | 试卷数据统计 |
| `exam export-csv` | 导出成绩单 CSV |

### article

| 命令 | 说明 |
| --- | --- |
| `article list` | 文章列表 |
| `article get` | 文章详情 |
| `article create` | 发布文章（教师+） |
| `article update` | 编辑文章 |
| `article delete` | 删除文章 |

### upload

| 命令 | 说明 |
| --- | --- |
| `upload list` | 文件列表（教师+） |
| `upload create` | 上传文件（教师+） |
| `upload get` | 下载或访问文件 |
| `upload delete` | 删除文件（admin+） |

### category

| 命令 | 说明 |
| --- | --- |
| `category list` | 分类列表 |
| `category get` | 分类详情 |
| `category create` | 创建分类（admin+） |
| `category update` | 更新分类 |
| `category delete` | 删除分类 |
| `category problem-add` | 把题目加入分类 |
| `category problem-remove` | 把题目移出分类 |

### announcement

| 命令 | 说明 |
| --- | --- |
| `announcement list` | 公告列表 |
| `announcement get` | 公告详情 |
| `announcement create` | 创建公告（教师+） |
| `announcement update` | 更新公告 |
| `announcement delete` | 删除公告 |

### problemset

| 命令 | 说明 |
| --- | --- |
| `problemset list` | 题单列表 |
| `problemset mine` | 我创建的题单 |
| `problemset get` | 题单详情 |
| `problemset create` | 创建题单 |
| `problemset update` | 更新题单信息 |
| `problemset delete` | 删除题单 |
| `problemset set-problems` | 覆盖式设置题目列表 |
| `problemset progress` | 当前用户解题进度 |
| `problemset mark-progress` | 标记题目已 AC |
| `problemset visibility-get` | 读取题单可见性 |
| `problemset visibility-set` | 设置题单可见性（admin+） |

### vcontest

| 命令 | 说明 |
| --- | --- |
| `vcontest get` | 虚拟比赛详情 |
| `vcontest ranking` | 本人排名数据 |

### plagiarism

| 命令 | 说明 |
| --- | --- |
| `plagiarism task` | 查重任务进度与结果 |
| `plagiarism pair` | 查重对详情 |

### notification

| 命令 | 说明 |
| --- | --- |
| `notification list` | 通知列表 |
| `notification unread-count` | 未读数量 |
| `notification read-all` | 全部标记已读 |
| `notification read` | 标记单条已读 |
| `notification delete` | 删除通知 |

### notebook

| 命令 | 说明 |
| --- | --- |
| `notebook list` | 错题列表 |
| `notebook mark` | 标记或保存笔记 |
| `notebook remove` | 移出错题本 |

### daily

| 命令 | 说明 |
| --- | --- |
| `daily today` | 今日题目与打卡状态 |
| `daily checkin` | 打卡 |

### discussion

| 命令 | 说明 |
| --- | --- |
| `discussion get` | 讨论详情 |
| `discussion create` | 创建讨论 |
| `discussion update` | 更新讨论 |
| `discussion delete` | 删除讨论 |
| `discussion reply` | 发表回复 |
| `discussion delete-reply` | 删除回复 |
| `discussion pin` | 置顶或取消置顶 |
| `discussion lock` | 锁定或解锁 |
| `discussion official` | 标记官方题解 |

### message

| 命令 | 说明 |
| --- | --- |
| `message list` | 会话列表 |
| `message unread-count` | 未读私信总数 |
| `message history` | 与某用户的历史消息 |
| `message send` | 发送私信 |
| `message read` | 标记会话已读 |
| `message delete` | 删除单条消息 |

---

## 直接请求接口：noldoj api

`noldoj api` 可以绕过命令封装，直接向站点接口发起请求，用法与 `gh api` 类似。

```text
noldoj api <路径> [选项]
```

**路径规则**：路径是相对站点的。`/auth/tokeninfo`、`auth/tokeninfo`、`/api/v1/auth/tokeninfo` 三种写法等价，工具会自动去掉 `/api/v1` 前缀。

**方法规则**：默认使用 GET。当出现 `-f` / `-F` 字段或 `--input` 时默认改为 POST，可通过 `-X` 显式指定。

| 选项 | 说明 |
| --- | --- |
| `-X, --method <方法>` | 指定 HTTP 方法：GET、POST、PUT、PATCH、DELETE、HEAD、OPTIONS |
| `-f, --raw-field k=v` | 追加字符串字段，可重复。GET/HEAD/DELETE 作为查询参数，其余方法作为 JSON 请求体 |
| `-F, --field k=v` | 追加类型化字段，可重复。数字、布尔、`null`、数组会按 JSON 解析 |
| `-H, --header "K: V"` | 追加请求头，可重复，可覆盖默认的 `Content-Type` / `Accept` |
| `--input <文件\|->` | 用文件或标准输入的内容作为原始请求体（`-` 表示标准输入） |
| `-i, --include` | 同时输出响应状态行与响应头 |

示例：

```bash
# 查询令牌信息
noldoj api /auth/tokeninfo

# GET 查询参数
noldoj api -X GET /problems -f page=1 -f limit=20

# POST 提交 JSON 请求体（-F 会做类型转换）
noldoj api /problems -X POST -F title=示例 -F is_public=true -F time_limit=1000

# 查看响应头
noldoj api /problems/1 -i

# 从标准输入传入原始请求体
cat body.json | noldoj api /problems --input -

# 自定义请求头
noldoj api /system/status -H "X-Request-Id: demo" -H "Accept: application/json"
```

---

## 典型工作流

```bash
# 1. 配置站点与令牌
noldoj site add --name main --url https://oj.example.com --token "$NOLDOJ_TOKEN"
noldoj site use --name main
noldoj auth tokeninfo

# 2. 写题与测试数据
noldoj problem create --title "A+B Problem" --time-limit 1000 --memory-limit 256 --is-public true
noldoj problem testdata-zip --id 1001 --file testdata.zip

# 3. 本地代码提交并等待结果
noldoj submission create --problem-id 1001 --language cpp --file main.cpp --watch

# 4. 使用通用请求访问未封装接口
noldoj api -X GET /contests -f page=1 --raw

# 5. 导出数据
noldoj contest export-csv --id 42 -o leaderboard.csv
```
