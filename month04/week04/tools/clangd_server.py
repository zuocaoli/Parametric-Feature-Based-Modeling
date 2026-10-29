#!/usr/bin/env python3
"""Forward clangd LSP, adding source navigation for named import/module lines."""

import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
from urllib.parse import unquote, urlparse


MODULE = re.compile(r"^\s*(?:export\s+)?(?:import|module)\s+"
                    r"(?P<name>[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*(?::[A-Za-z_]\w*)?)\s*;")
INTERFACE = re.compile(r"^\s*export\s+module\s+"
                       r"(?P<name>[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*(?::[A-Za-z_]\w*)?)\s*;")


def read_message(stream):
    headers = {}
    while line := stream.readline():
        if line in (b"\r\n", b"\n"):
            break
        key, value = line.decode("ascii").split(":", 1)
        headers[key.lower()] = value.strip()
    if not headers:
        return None
    size = int(headers["content-length"])
    chunks = bytearray()
    while len(chunks) < size:
        chunk = stream.read(size - len(chunks))
        if not chunk:
            raise EOFError("Incomplete LSP message")
        chunks.extend(chunk)
    return json.loads(chunks)


def write_message(stream, message):
    data = json.dumps(message, ensure_ascii=False).encode("utf-8")
    stream.write(f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data)
    stream.flush()


def file_path(uri):
    if not isinstance(uri, str):
        return None
    parsed = urlparse(uri)
    return Path(unquote(parsed.path)).resolve() if parsed.scheme == "file" else None


def position_offset(text, position):
    lines = text.splitlines(keepends=True)
    line = position["line"]
    if line == len(lines):
        return len(text)
    prefix = lines[line].encode("utf-16-le")[:position["character"] * 2].decode("utf-16-le")
    return sum(map(len, lines[:line])) + len(prefix)


class ModuleNavigation:
    def __init__(self, database):
        self.database = Path(database).resolve() if database else None
        self.documents = {}
        self.interfaces = []
        self.database_stamp = None

    def observe(self, message):
        method = message.get("method")
        params = message.get("params") or {}
        document = params.get("textDocument") or {}
        path = file_path(document.get("uri", ""))
        if path is None:
            return
        if method == "textDocument/didOpen":
            self.documents[path] = document["text"]
        elif method == "textDocument/didChange" and path in self.documents:
            text = self.documents[path]
            for change in params["contentChanges"]:
                if "range" in change:
                    start = position_offset(text, change["range"]["start"])
                    end = position_offset(text, change["range"]["end"])
                    text = text[:start] + change["text"] + text[end:]
                else:
                    text = change["text"]
            self.documents[path] = text
        elif method == "textDocument/didClose":
            self.documents.pop(path, None)

    def text(self, path):
        return self.documents[path] if path in self.documents else path.read_text()

    def interface_paths(self):
        if self.database is None:
            return []
        database = self.database / "compile_commands.json"
        try:
            stamp = database.stat().st_mtime_ns
            if stamp != self.database_stamp:
                commands = json.loads(database.read_text())
                paths = set()
                for command in commands:
                    path = Path(command["file"])
                    if not path.is_absolute():
                        path = Path(command["directory"]) / path
                    if path.suffix == ".cppm" or "c++-module" in command.get("arguments", []):
                        paths.add(path.resolve())
                self.interfaces = sorted(paths)
                self.database_stamp = stamp
            return self.interfaces
        except (OSError, ValueError, KeyError):
            # Native clangd remains responsible for missing/invalid databases.
            return []

    def location(self, params):
        try:
            path = file_path(params["textDocument"]["uri"])
            if path is None:
                return None
            lines = self.text(path).splitlines()
            position = params["position"]
            line = lines[position["line"]]
            match = MODULE.match(line)
            if not match:
                return None
            start = len(line[:match.start("name")].encode("utf-16-le")) // 2
            end = len(line[:match.end("name")].encode("utf-16-le")) // 2
            if not start <= position["character"] < end:
                return None
            name = match["name"]
            for target in self.interface_paths():
                try:
                    for number, target_line in enumerate(self.text(target).splitlines()):
                        declaration = INTERFACE.match(target_line)
                        if declaration and declaration["name"] == name:
                            begin = len(target_line[:declaration.start("name")].encode("utf-16-le")) // 2
                            finish = len(target_line[:declaration.end("name")].encode("utf-16-le")) // 2
                            return {"uri": target.as_uri(), "range": {
                                "start": {"line": number, "character": begin},
                                "end": {"line": number, "character": finish},
                            }}
                except (OSError, UnicodeError):
                    continue
        except (OSError, IndexError, KeyError, TypeError, UnicodeError):
            return None
        return None


def database_argument(arguments):
    for index, argument in enumerate(arguments):
        if argument.startswith("--compile-commands-dir="):
            return argument.split("=", 1)[1]
        if argument == "--compile-commands-dir" and index + 1 < len(arguments):
            return arguments[index + 1]
    return None


def serve(command, arguments):
    navigation = ModuleNavigation(database_argument(arguments))
    native = subprocess.Popen([*command, *arguments], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    output_lock = threading.Lock()
    incoming = queue.Queue()

    def emit(message):
        with output_lock:
            write_message(sys.stdout.buffer, message)

    def forward_responses():
        try:
            while (message := read_message(native.stdout)) is not None:
                emit(message)
        except (OSError, EOFError, ValueError) as error:
            print(f"clangd navigation bridge: {error}", file=sys.stderr)
        finally:
            incoming.put(None)

    def read_requests():
        try:
            # A daemon may still be reading when native clangd fails. Raw I/O
            # avoids holding Python's buffered stdin lock during shutdown.
            with os.fdopen(os.dup(sys.stdin.fileno()), "rb", buffering=0) as stream:
                while (message := read_message(stream)) is not None:
                    incoming.put(message)
        except (OSError, EOFError, ValueError) as error:
            print(f"clangd navigation bridge: {error}", file=sys.stderr)
        finally:
            incoming.put(None)

    reader = threading.Thread(target=forward_responses, daemon=True)
    reader.start()
    threading.Thread(target=read_requests, daemon=True).start()
    try:
        while (message := incoming.get()) is not None:
            navigation.observe(message)
            if message.get("method") in ("textDocument/definition", "textDocument/declaration"):
                location = navigation.location(message["params"])
                if location is not None:
                    emit({"jsonrpc": "2.0", "id": message["id"], "result": [location]})
                    continue
            write_message(native.stdin, message)
            if message.get("method") == "exit":
                break
    except (OSError, EOFError, ValueError) as error:
        print(f"clangd navigation bridge: {error}", file=sys.stderr)
        native.terminate()
    finally:
        try:
            native.stdin.close()
        except BrokenPipeError:
            pass
        try:
            native.wait(timeout=5)
        except subprocess.TimeoutExpired:
            native.kill()
            native.wait()
        reader.join(timeout=5)
    return native.returncode


if __name__ == "__main__":
    arguments = sys.argv[1:]
    if any(arg in ("--version", "--help", "--help-hidden") or arg.startswith("--check=")
           for arg in arguments):
        os.execv("/usr/bin/clangd", ["/usr/bin/clangd", *arguments])
    sys.exit(serve(["/usr/bin/clangd"], arguments))
