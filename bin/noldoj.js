#!/usr/bin/env node
"use strict";

// NoldOJ CLI 的 npm 启动器：按平台下载对应 Release 二进制并缓存，然后透传参数执行。
// 安装阶段不联网；首次运行（或版本变化后）才下载。

const { spawn } = require("node:child_process");
const fs = require("node:fs");
const https = require("node:https");
const os = require("node:os");
const path = require("node:path");

const VERSION = require("../package.json").version;
const REPO = "dxxjudges/noldoj-cli";

const ASSETS = {
  "win32-x64": "noldoj-windows-x64.exe",
  "win32-ia32": "noldoj-windows-x86.exe",
  "win32-arm64": "noldoj-windows-arm64.exe",
  "linux-x64": "noldoj-linux-x64",
  "linux-ia32": "noldoj-linux-x86",
  "linux-arm64": "noldoj-linux-arm64",
  "darwin-x64": "noldoj-macos-x64",
  "darwin-arm64": "noldoj-macos-arm64",
};

function assetName() {
  const key = `${process.platform}-${process.arch}`;
  return ASSETS[key] || null;
}

function cacheDir() {
  const root = process.env.NOLDOJ_CLI_CACHE || path.join(os.homedir(), ".noldoj-cli", "bin");
  return path.join(root, VERSION);
}

function download(url, target, redirects = 0) {
  return new Promise((resolve, reject) => {
    if (redirects > 5) {
      reject(new Error("下载重定向次数过多"));
      return;
    }
    https
      .get(url, { headers: { "user-agent": `noldoj-cli-npm/${VERSION}` } }, (response) => {
        const status = response.statusCode || 0;
        if (status >= 300 && status < 400 && response.headers.location) {
          response.resume();
          download(response.headers.location, target, redirects + 1).then(resolve, reject);
          return;
        }
        if (status !== 200) {
          response.resume();
          reject(new Error(`下载失败（HTTP ${status}）：${url}`));
          return;
        }

        const temporary = `${target}.part`;
        const stream = fs.createWriteStream(temporary, { mode: 0o755 });
        response.pipe(stream);
        stream.on("finish", () => {
          stream.close(() => {
            try {
              fs.renameSync(temporary, target);
              resolve();
            } catch (error) {
              reject(error);
            }
          });
        });
        stream.on("error", (error) => {
          fs.rmSync(temporary, { force: true });
          reject(error);
        });
      })
      .on("error", (error) => {
        reject(new Error(`无法连接下载服务器：${error.message}`));
      });
  });
}

async function ensureBinary(asset) {
  const target = path.join(cacheDir(), asset);
  if (fs.existsSync(target)) return target;

  fs.mkdirSync(cacheDir(), { recursive: true });
  const url = `https://github.com/${REPO}/releases/download/v${VERSION}/${asset}`;
  process.stderr.write(`noldoj-cli: 正在下载 ${asset}（v${VERSION}）…\n`);
  await download(url, target);
  if (process.platform !== "win32") fs.chmodSync(target, 0o755);
  return target;
}

async function main() {
  const override = process.env.NOLDOJ_CLI_BIN;
  const asset = assetName();

  if (!override && !asset) {
    process.stderr.write(
      `noldoj-cli: 暂不支持 ${process.platform}/${process.arch} 的预编译二进制。\n` +
        `请从 https://github.com/${REPO} 下载源码编译，或设置 NOLDOJ_CLI_BIN 指向已有可执行文件。\n`
    );
    process.exit(1);
  }

  let binary = override;
  if (!binary) {
    try {
      binary = await ensureBinary(asset);
    } catch (error) {
      process.stderr.write(
        `noldoj-cli: ${error.message}\n` +
          `可手动下载 v${VERSION} 的 Release 资产，或用 NOLDOJ_CLI_BIN 指定路径。\n`
      );
      process.exit(1);
    }
  }

  const child = spawn(binary, process.argv.slice(2), { stdio: "inherit" });
  child.on("error", (error) => {
    process.stderr.write(`noldoj-cli: 无法启动 ${binary}：${error.message}\n`);
    process.exit(1);
  });
  child.on("exit", (code, signal) => {
    if (signal) {
      process.kill(process.pid, signal);
      return;
    }
    process.exit(code === null ? 1 : code);
  });
}

main();
