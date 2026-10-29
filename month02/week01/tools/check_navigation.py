#!/usr/bin/env python3
"""Verify definition targets in month02/week01 using its VS Code clangd configuration."""

import json
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time
from urllib.parse import unquote, urlparse

from builder import COURSE_DIR, QT, course_modules, std_source

CASES = [
    ('main.cpp', 'const auto result = lex(', 'lex', ['lexer.cpp']),
]
DECLARATION_CASES = [

]
DEFINITIONS = {
    ('lexer.cpp', 'lex'): 'lex(std::string_view source)',
}

# 表中指向标准库源码的目标用占位符标记，这里换算成真实路径。
STANDARD = std_source()
SPECIAL_TARGETS = {
    "@std@": str(STANDARD),
    "@type_traits@": str(STANDARD.parent.parent / "type_traits"),
}
CASES = [(*case[:3], [SPECIAL_TARGETS.get(name, name) for name in case[3]]) for case in CASES]

# Exercise named imports before opening their .cppm files.
entry = COURSE_DIR / ("console.cpp" if QT else "main.cpp")
module_targets = dict(course_modules())
module_targets["std"] = STANDARD
imports = list(re.finditer(r"^import ([\w.]+);", entry.read_text(), re.M))
selected = next((item for item in imports if item[1] != "std"), imports[0] if imports else None)
if selected and selected[1] in module_targets:
    CASES.append((str(entry.relative_to(COURSE_DIR)), selected[0], selected[1],
                  [str(module_targets[selected[1]])]))
    implementation = module_targets[selected[1]].with_suffix(".cpp")
    if selected[1] != "std" and implementation.is_file():
        for line in implementation.read_text().splitlines():
            if line == f"module {selected[1]};":
                CASES.append((str(implementation.relative_to(COURSE_DIR)), line, selected[1],
                              [str(module_targets[selected[1]])]))
                break


def read_messages(stream, messages):
    try:
        while True:
            headers = {}
            while line := stream.readline():
                if line == b"\r\n":
                    break
                key, value = line.decode().split(":", 1)
                headers[key.lower()] = value.strip()
            if not headers:
                return
            messages.put(json.loads(stream.read(int(headers["content-length"]))))
    except (OSError, ValueError) as error:
        messages.put(error)


def check():
    workspace = COURSE_DIR
    database = COURSE_DIR / "build" / "clangd"
    if not (database / "compile_commands.json").is_file():
        raise RuntimeError("Run python3 tools/builder.py configure first")
    settings = json.loads((workspace / ".vscode/settings.json").read_text())
    # Use the editor's actual arguments. Supplying a database path only here
    # hides failures to resolve compile commands for modules outside the repo.
    arguments = [argument.replace("${workspaceFolder}", str(workspace))
                 for argument in settings.get("clangd.arguments", [])]
    server = subprocess.Popen(
        [settings.get("clangd.path", "/usr/bin/clangd").replace("${workspaceFolder}", str(workspace)), *arguments,
         "-j=2", "--log=error"],
        cwd=workspace, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    )
    messages = queue.Queue()
    threading.Thread(target=read_messages, args=(server.stdout, messages), daemon=True).start()

    def write(message):
        data = json.dumps(message).encode()
        server.stdin.write(f"Content-Length: {len(data)}\r\n\r\n".encode() + data)
        server.stdin.flush()

    def send(method, params, request_id=None):
        message = {"jsonrpc": "2.0", "method": method, "params": params}
        if request_id is not None:
            message["id"] = request_id
        write(message)

    index_ready = False
    index_timeout = 120

    def receive(timeout=30):
        nonlocal index_ready
        message = messages.get(timeout=timeout)
        if isinstance(message, Exception):
            raise message
        if "method" in message and "id" in message:
            # clangd asks the client to create the background-index progress
            # token. Ignoring that request prevents receiving its completion.
            write({"jsonrpc": "2.0", "id": message["id"], "result": None})
        if (message.get("method") == "$/progress"
                and message["params"]["token"] == "backgroundIndexProgress"
                and message["params"]["value"]["kind"] == "end"):
            index_ready = True
        return message

    def response(request_id):
        while True:
            message = receive()
            if "method" not in message and message.get("id") == request_id:
                if "error" in message:
                    raise RuntimeError(message["error"])
                return message.get("result")

    opened = set()
    try:
        send("initialize", {"processId": None, "rootUri": workspace.as_uri(),
                            "capabilities": {"window": {"workDoneProgress": True}}}, 1)
        response(1)
        send("initialized", {})
        requests = [("definition", case) for case in CASES]
        requests.extend(("declaration", case) for case in DECLARATION_CASES)
        for request_id, (method, (filename, snippet, token, targets)) in enumerate(requests, 2):
            source = COURSE_DIR / filename
            content = source.read_text()
            lineno, line = next((i, line) for i, line in enumerate(content.splitlines())
                                if snippet in line)
            if filename not in opened:
                send("textDocument/didOpen", {"textDocument": {
                    "uri": source.as_uri(), "languageId": "cpp", "version": 1, "text": content,
                }})
                opened.add(filename)
                if len(opened) == 1:
                    # A declaration fallback is expected before indexing ends.
                    # Wait for clangd's progress event, not an arbitrary sleep,
                    # and leave signal/document implementations unopened.
                    deadline = time.monotonic() + index_timeout
                    while not index_ready:
                        remaining = deadline - time.monotonic()
                        if remaining <= 0:
                            raise RuntimeError(f"Background indexing did not finish within {index_timeout} seconds")
                        try:
                            receive(timeout=remaining)
                        except queue.Empty as error:
                            raise RuntimeError(
                                f"Background indexing did not finish within {index_timeout} seconds"
                            ) from error
            column = len(line[:line.index(token)].encode("utf-16-le")) // 2
            send(f"textDocument/{method}", {"textDocument": {"uri": source.as_uri()},
                 "position": {"line": lineno, "character": column}}, request_id)
            locations = response(request_id) or []
            if isinstance(locations, dict):
                locations = [locations]
            matched = False
            for location in locations:
                target = Path(unquote(urlparse(location["uri"]).path))
                bounds = location["range"]
                start, end = bounds["start"], bounds["end"]
                target_line = target.read_text().splitlines()[start["line"]]
                spelling = target_line.encode("utf-16-le")[
                    start["character"] * 2:end["character"] * 2].decode("utf-16-le")
                expected = DEFINITIONS.get((str(target.relative_to(COURSE_DIR)), token)) \
                    if method == "definition" and target.is_relative_to(COURSE_DIR) else None
                if (target in [COURSE_DIR / name for name in targets] and spelling == token
                        and (expected is None or expected in target_line)):
                    display = target.relative_to(COURSE_DIR) if target.is_relative_to(COURSE_DIR) else target
                    print(f"{method} {filename}:{lineno + 1} {token} -> "
                          f"{display}:{start['line'] + 1}")
                    matched = True
            if not matched:
                raise RuntimeError(f"Incorrect {method} position for {filename} {token}: {locations}")
    finally:
        check_failed = sys.exc_info()[0] is not None
        try:
            send("shutdown", None, 100)
            response(100)
            send("exit", None)
            exit_code = server.wait(timeout=5)
            if exit_code != 0:
                raise RuntimeError(f"clangd exited with code {exit_code}")
        except (OSError, RuntimeError, queue.Empty, subprocess.TimeoutExpired) as error:
            if server.poll() is None:
                server.kill()
                server.wait(timeout=5)
            if not check_failed:
                raise RuntimeError(f"clangd did not shut down cleanly: {error}") from error


if __name__ == "__main__":
    try:
        check()
    except queue.Empty:
        raise SystemExit("Navigation check failed: timed out waiting for a clangd response")
    except (OSError, ValueError, RuntimeError, StopIteration) as error:
        raise SystemExit(f"Navigation check failed: {error}") from error
