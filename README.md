# NoldOJ CLI

NoldOJ 评测平台的跨平台命令行客户端。配置站点地址与私人令牌后，可以在终端管理题目、提交代码、参加比赛，或像 `gh api` 一样直接请求该站点的任意接口。

- 文档：<https://nojc.dxxj.de5.net>
- 源码：<https://github.com/dxxjudges/noldoj-cli>
- 发布：<https://github.com/dxxjudges/noldoj-cli/releases>

> CLI 本身不包含题目或数据，也不内置服务器地址：必须先配置一个可访问的 NoldOJ 站点与私人令牌，否则命令无法工作。

## 安装

```bash
npm install -g noldoj-cli
```

也可直接从 [Releases](https://github.com/dxxjudges/noldoj-cli/releases) 下载对应平台的二进制：Linux x64/x86/arm64、macOS x64/arm64、Windows x64/x86/arm64 均已提供。Android / Termux 请在 Termux 内从源码编译。

npm 包在首次运行时才会下载对应平台的二进制，缓存在 `~/.noldoj-cli/bin/<版本>/`；可用 `NOLDOJ_CLI_CACHE` 改变缓存位置，或用 `NOLDOJ_CLI_BIN` 直接指定已有可执行文件。

## 快速开始

```bash
noldoj site add --name main --url https://your-oj.example.com --token <私人令牌>
noldoj auth tokeninfo
noldoj problem list --page 1
noldoj help problem
```

## 凭据

令牌不会明文落盘：`credentials.json` 使用 AES-256-CTR + HMAC-SHA256 加密，主密钥为首次启动时生成的 256 位随机密钥，保存在系统凭据库（Windows 凭据管理器 / macOS 钥匙串 / Linux Secret Service），不可用时回退到配置目录下权限 0600 的 `master.key`。用 `noldoj config path` 查看实际位置。

## 从源码构建

```bash
git clone https://github.com/dxxjudges/noldoj-cli.git
cd noldoj-cli
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
python tests/run_tests.py build/noldoj      # Windows 为 build/noldoj.exe
```

Windows 使用系统 WinHTTP，其余平台使用 libcurl（Linux 需 `libcurl4-openssl-dev`，macOS 使用系统 libcurl）。

## 发布

- 提交信息为 `build X.Y.Z` 或打 `vX.Y.Z` 标签时，GitHub Actions 会构建全部平台并创建 Release。
- npm 包版本通过 `npm version patch|minor|major` 更新，不要手改 `package.json` 的版本号。
