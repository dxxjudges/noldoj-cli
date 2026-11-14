"use strict";

// 发布前校验：vendor/ 必须包含全部平台的发布产物，避免发出空壳包。

const fs = require("node:fs");
const path = require("node:path");

const REQUIRED = [
  "noldoj-linux-x64",
  "noldoj-linux-x86",
  "noldoj-linux-arm64",
  "noldoj-macos-x64",
  "noldoj-macos-arm64",
  "noldoj-windows-x64.exe",
  "noldoj-windows-x86.exe",
  "noldoj-windows-arm64.exe",
];

const vendor = path.join(__dirname, "..", "vendor");
const missing = REQUIRED.filter((name) => {
  const file = path.join(vendor, name);
  return !fs.existsSync(file) || fs.statSync(file).size === 0;
});

if (missing.length > 0) {
  console.error(`发布中止：vendor/ 缺少以下产物：\n  ${missing.join("\n  ")}`);
  console.error("请先下载对应 Release 的产物到 vendor/ 后再发布。");
  process.exit(1);
}

console.log(`vendor/ 校验通过（${REQUIRED.length} 个平台产物）`);
