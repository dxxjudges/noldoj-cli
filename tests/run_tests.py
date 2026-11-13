#!/usr/bin/env python3
"""noldoj-cli 端到端测试。

用法:
    python tests/run_tests.py <noldoj 可执行文件路径>

测试会在本机启动一个 HTTP 回显服务，覆盖版本、帮助、tokeninfo、
api 子命令（查询参数 / JSON 请求体 / 自定义头 / stdin 输入）以及错误处理。
"""

import json
import os
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# 控制台可能不是 UTF-8（例如 Windows 上的 cp1252），统一改成 UTF-8 并容错，
# 避免中文测试名在输出时抛出 UnicodeEncodeError。
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass


class EchoHandler(BaseHTTPRequestHandler):
    server_version = "noldoj-test"

    def _handle(self):
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length).decode("utf-8", "replace") if length else ""
        try:
            parsed = json.loads(raw) if raw else {}
        except ValueError:
            parsed = {"_raw": raw}

        if "/error" in self.path:
            self._send(404, {"code": 40401, "reason": "NOT_FOUND", "message": "resource not found"})
            return
        if "/auth/tokeninfo" in self.path:
            self._send(200, {
                "user": {"username": "tester", "role": "user"},
                "token": {"type": "pat", "scopes": ["read", "write"], "expires_at": None},
            })
            return

        self._send(200, {
            "method": self.command,
            "path": self.path,
            "headers": {key.lower(): value for key, value in self.headers.items()},
            "body": raw,
            "json": parsed,
        })

    def _send(self, code, payload):
        data = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("X-Test-Response", "yes")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    do_GET = do_POST = do_PUT = do_DELETE = do_PATCH = _handle

    def log_message(self, *args):
        pass


def run_cli(binary, config_dir, args, stdin=None):
    command = [binary, "--config-dir", config_dir] + args
    return subprocess.run(
        command,
        input=stdin,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
    )


def parse_json(process, label):
    try:
        return json.loads(process.stdout.strip())
    except ValueError as exc:
        raise AssertionError(f"{label}: 输出不是合法 JSON: {process.stdout!r}") from exc


def main():
    if len(sys.argv) < 2:
        print("用法: python tests/run_tests.py <noldoj 可执行文件路径>", file=sys.stderr)
        return 2

    binary = os.path.abspath(sys.argv[1])
    if not os.path.exists(binary):
        print(f"找不到可执行文件: {binary}", file=sys.stderr)
        return 2

    server = ThreadingHTTPServer(("127.0.0.1", 0), EchoHandler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    port = server.server_address[1]
    site = f"http://127.0.0.1:{port}"
    config_dir = tempfile.mkdtemp(prefix="noldoj-cli-test-")

    failures = []

    def check(name, action):
        try:
            action()
            print(f"PASS  {name}")
        except AssertionError as exc:
            failures.append(name)
            print(f"FAIL  {name}: {exc}")
        except Exception as exc:  # noqa: BLE001
            failures.append(name)
            print(f"ERROR {name}: {exc}")

    def base_args(*args):
        return ["--site", site, "--access-token", "demo-token", "--raw", *args]

    def test_version():
        proc = run_cli(binary, config_dir, ["--version"])
        assert proc.returncode == 0, f"退出码 {proc.returncode}: {proc.stderr}"
        assert "noldoj-cli" in proc.stdout, proc.stdout

    def test_help_api():
        proc = run_cli(binary, config_dir, ["help", "api"])
        assert proc.returncode == 0, proc.stderr
        assert "noldoj api" in proc.stdout, proc.stdout

    def test_tokeninfo():
        proc = run_cli(binary, config_dir, base_args("auth", "tokeninfo"))
        assert proc.returncode == 0, proc.stderr
        data = parse_json(proc, "auth tokeninfo")
        assert data["token"]["type"] == "pat", data
        assert data["user"]["username"] == "tester", data

    def test_api_get_query():
        proc = run_cli(binary, config_dir, base_args("api", "-X", "GET", "/echo/sample", "-f", "page=2", "-f", "q=hello"))
        assert proc.returncode == 0, proc.stderr
        data = parse_json(proc, "api GET")
        assert data["method"] == "GET", data
        assert "page=2" in data["path"] and "q=hello" in data["path"], data["path"]
        assert data["headers"].get("authorization") == "Bearer demo-token", data["headers"]

    def test_api_post_typed():
        proc = run_cli(binary, config_dir, base_args("api", "/echo", "-X", "POST", "-F", "count=3", "-F", "ok=true", "-f", "name=alice"))
        assert proc.returncode == 0, proc.stderr
        data = parse_json(proc, "api POST")
        assert data["method"] == "POST", data
        assert data["json"] == {"count": 3, "ok": True, "name": "alice"}, data["json"]
        assert "application/json" in data["headers"].get("content-type", ""), data["headers"]

    def test_api_header_and_include():
        proc = run_cli(binary, config_dir, base_args("api", "/echo", "-i", "-H", "X-Custom: hi"))
        assert proc.returncode == 0, proc.stderr
        lowered = proc.stdout.lower()
        assert "http/" in lowered and " 200" in lowered, proc.stdout
        assert "x-test-response" in lowered, proc.stdout
        assert '"x-custom":"hi"' in lowered or '"x-custom": "hi"' in lowered, proc.stdout

    def test_api_stdin_body():
        proc = run_cli(binary, config_dir, base_args("api", "PUT", "/echo", "--input", "-"),
                       stdin='{"title":"abc"}')
        assert proc.returncode == 0, proc.stderr
        data = parse_json(proc, "api --input")
        assert data["method"] == "PUT", data
        assert json.loads(data["body"]) == {"title": "abc"}, data["body"]

    def test_api_error():
        proc = run_cli(binary, config_dir, ["--site", site, "api", "/error"])
        assert proc.returncode != 0, "404 应返回非零退出码"
        assert "resource not found" in (proc.stderr + proc.stdout), proc.stderr

    def test_config_path():
        proc = run_cli(binary, config_dir, ["config", "path"])
        assert proc.returncode == 0, proc.stderr
        assert "config.json" in proc.stdout and "credentials.json" in proc.stdout, proc.stdout

    check("--version 输出版本号", test_version)
    check("help api 显示用法", test_help_api)
    check("auth tokeninfo 解析响应", test_tokeninfo)
    check("api GET 查询参数与鉴权", test_api_get_query)
    check("api POST 类型化字段", test_api_post_typed)
    check("api 自定义头与 --include", test_api_header_and_include)
    check("api --input 读取标准输入", test_api_stdin_body)
    check("api 404 错误处理", test_api_error)
    check("config path 输出配置目录", test_config_path)

    server.shutdown()

    if failures:
        print(f"\n{len(failures)} 项测试失败: {', '.join(failures)}")
        return 1
    print("\n全部测试通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
