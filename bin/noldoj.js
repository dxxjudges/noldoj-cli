#!/usr/bin/env node
"use strict";

// NoldOJ CLI 的 npm 启动器：直接执行包内 vendor/ 目录下对应平台的二进制。

const { spawn } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const VERSION = require("../package.json").version;

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

const asset = ASSETS[`${process.platform}-${process.arch}`];
let binary = process.env.NOLDOJ_CLI_BIN;

if (!binary) {
  if (!asset) {
    process.stderr.write(
      `noldoj-cli: 包内没有 ${process.platform}/${process.arch} 的二进制。\n` +
        "请从 https://github.com/dxxjudges/noldoj-cli 下载源码编译，" +
        "或设置 NOLDOJ_CLI_BIN 指向已有可执行文件。\n"
    );
    process.exit(1);
  }
  binary = path.join(__dirname, "..", "vendor", asset);
}

if (!fs.existsSync(binary)) {
  process.stderr.write(
    `noldoj-cli: 找不到二进制文件 ${binary}。\n` +
      `该安装可能不完整，请重新安装 noldoj-cli@${VERSION}。\n`
  );
  process.exit(1);
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
