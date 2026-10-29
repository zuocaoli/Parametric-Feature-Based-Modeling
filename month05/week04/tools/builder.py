#!/usr/bin/env python3
"""Standalone builder for month05/week04: build, clangd setup, run, test, format check."""

import argparse
import json
import os
import re
import shlex
import subprocess
from pathlib import Path

COURSE = "month05/week04"
COURSE_DIR = Path(__file__).resolve().parent.parent
BUILD = COURSE_DIR / "build"
QT = True
FLAGS = ["-std=c++23", "-Wall", "-Wextra", "-Wpedantic", "-fPIC"]


def run(command, cwd=None, env=None):
    subprocess.run([str(part) for part in command], cwd=cwd, check=True, env=env)


def output(*command):
    return subprocess.check_output([str(part) for part in command], text=True).strip()


def qt_flags():
    return shlex.split(output("pkg-config", "--cflags", "Qt6Quick", "Qt6Qml", "Qt6QuickControls2", "Qt6Test")) + ["-DQT_NO_KEYWORDS"]


def qt_tool(name):
    directory = "QT_INSTALL_LIBEXECS" if name in ("moc", "rcc") else "QT_INSTALL_BINS"
    return Path(output("qmake6", "-query", directory)) / name


def generate_qt():
    moc = BUILD / "moc_bridge.cpp"
    resources = BUILD / "qrc_resources.cpp"
    run([qt_tool("moc"), *qt_flags(), COURSE_DIR / "bridge.hpp", "-o", moc])
    run([qt_tool("rcc"), COURSE_DIR / "resources.qrc", "-o", resources])
    return [moc, resources]


def source_flags(source):
    if QT and source.name in ("main.cpp", "bridge.cpp", "ui_tests.cpp", "moc_bridge.cpp", "qrc_resources.cpp"):
        # Qt headers are external dependencies, while project diagnostics stay enabled.
        flags = [f"-isystem{flag[2:]}" if flag.startswith("-I") else flag for flag in qt_flags()]
        return [*flags, f"-DCOURSE_WEEK={int(COURSE[-2:])}"]
    return []


def std_source():
    manifest = Path(output("g++", "-print-file-name=libstdc++.modules.json"))
    if not manifest.is_file():
        raise RuntimeError("Install GCC 16 and its libstdc++ module development files")
    for module in json.loads(manifest.read_text())["modules"]:
        if module["logical-name"] == "std":
            source = (manifest.parent / module["source-path"]).resolve()
            if source.is_file():
                return source
    raise RuntimeError("The libstdc++ std module source is missing")


def course_sources():
    sources = sorted(COURSE_DIR.glob("*.cpp"))
    if not sources:
        raise RuntimeError(f"No C++ source files found in {COURSE}")
    return sources


def course_modules():
    """Return named module interfaces in dependency order (std is built separately)."""
    interfaces = {}
    for source in sorted(COURSE_DIR.glob("*.cppm")):
        declaration = re.search(r"^export module ([\w.]+);", source.read_text(), re.M)
        if not declaration:
            raise RuntimeError(f"Missing module declaration in {source}")
        name = declaration[1]
        if name in interfaces:
            raise RuntimeError(f"Duplicate module {name}")
        imports = re.findall(r"^(?:export )?import ([\w.]+);", source.read_text(), re.M)
        interfaces[name] = (source, [name for name in imports if name != "std"])
    ordered = []
    visiting = set()
    complete = set()

    def visit(name):
        if name in complete:
            return
        if name in visiting:
            raise RuntimeError(f"Cyclic module dependency: {name}")
        if name not in interfaces:
            raise RuntimeError(f"Unknown module dependency: {name}")
        visiting.add(name)
        source, imports = interfaces[name]
        for dependency in imports:
            visit(dependency)
        visiting.remove(name)
        complete.add(name)
        ordered.append((name, source))

    for name in interfaces:
        visit(name)
    return ordered


def build():
    modules = BUILD / "gcc"
    modules.mkdir(parents=True, exist_ok=True)
    run(["g++", *FLAGS, "-fmodules", "-x", "c++", "-c", std_source(),
         "-o", modules / "std.o"], cwd=modules)
    objects = []
    for _, source in course_modules():
        target = BUILD / f"{source.name}.o"
        run(["g++", *FLAGS, "-fmodules", "-x", "c++", "-c", source, "-o", target], cwd=modules)
        objects.append(target)
    sources = course_sources()
    if QT:
        sources += generate_qt()
    compiled = {}
    for source in sources:
        target = BUILD / f"{source.name}.o"
        run(["g++", *FLAGS, *source_flags(source), "-fmodules", "-c", source, "-o", target], cwd=modules)
        compiled[source.name] = target
    if QT:
        ui_names = {"main.cpp", "bridge.cpp", "ui_tests.cpp", "moc_bridge.cpp", "qrc_resources.cpp", "console.cpp"}
        core = [*objects, *(target for name, target in compiled.items() if name not in ui_names)]
        library = BUILD / "libcad_core.a"
        # 创建新归档，避免源码删除后遗留旧成员。
        library.unlink(missing_ok=True)
        run(["ar", "rcs", library, *core, modules / "std.o"])
        run(["g++", compiled["console.cpp"], library, "-o", BUILD / "console-demo"])
        libs = shlex.split(output("pkg-config", "--libs", "Qt6Quick", "Qt6Qml", "Qt6QuickControls2", "Qt6Test"))
        bridge = [compiled[name] for name in ("bridge.cpp", "moc_bridge.cpp", "qrc_resources.cpp")]
        for source, executable in (("main.cpp", "demo"), ("ui_tests.cpp", "ui-tests")):
            run(["g++", compiled[source], *bridge, library, *libs, "-o", BUILD / executable])
    else:
        run(["g++", *objects, *compiled.values(), modules / "std.o", "-o", BUILD / "demo"], cwd=modules)
    print(f"Built {COURSE}", flush=True)


def configure_clangd():
    compiler = "/usr/bin/clang++"
    server = "/usr/bin/clangd"
    compiler_version = re.search(r"clang version (\d+\.\d+\.\d+)", output(compiler, "--version"))
    server_version = re.search(r"clangd version (\d+\.\d+\.\d+)", output(server, "--version"))
    if not compiler_version or not server_version or compiler_version[1] != server_version[1]:
        raise RuntimeError("clang++ and clangd must have matching versions")
    directory = BUILD / "clangd"
    directory.mkdir(parents=True, exist_ok=True)
    # The interactive parser builds modules itself, but clangd 22's background
    # indexer does not. Give the latter prebuilt modules so unopened .cpp files
    # contribute definitions. Avoid fixed -fmodule-file mappings: the interactive
    # parser must still rebuild imports from current sources.
    standard = std_source()
    interfaces = [("std", standard), *course_modules()]
    prebuilt = directory / "prebuilt"
    prebuilt.mkdir(exist_ok=True)
    module_path = f"-fprebuilt-module-path={prebuilt}"
    for name, source in interfaces:
        run([compiler, *FLAGS, "-Wno-reserved-module-identifier", module_path,
             "-x", "c++-module", "--precompile", source, "-o", prebuilt / f"{name}.pcm"], cwd=directory)
    entries = [{
        "directory": str(directory),
        "file": str(standard),
        "arguments": [compiler, module_path, "-std=c++23", "-Wno-reserved-module-identifier",
                      "-x", "c++-module", "-c", str(standard),
                      "-o", str(directory / "std.o")],
    }]
    for _, source in course_modules():
        entries.append({
            "directory": str(directory),
            "file": str(source),
            "arguments": [compiler, module_path, *FLAGS, "-x", "c++-module", "-c", str(source),
                          "-o", str(directory / f"{source.name}.o")],
        })
    sources = course_sources()
    if QT:
        sources += generate_qt()
    for source in sources:
        entries.append({
            "directory": str(directory),
            "file": str(source),
            "arguments": [compiler, module_path, *FLAGS, *source_flags(source), "-c", str(source),
                          "-o", str(directory / f"{source.name}.o")],
        })
    (directory / "compile_commands.json").write_text(json.dumps(entries, indent=2) + "\n")
    # Failed background parses are persisted too. Regenerate project
    # shards after rebuilding modules; system-header shards can remain.
    index = directory / ".cache" / "clangd" / "index"
    for filename in {Path(entry["file"]).name for entry in entries}:
        for shard in index.glob(f"{filename}.*.idx"):
            shard.unlink()
    print(f"Generated {directory / 'compile_commands.json'}; restart the clangd language server.", flush=True)


def check_format():
    files = [*sorted(COURSE_DIR.glob("*.cpp")), *sorted(COURSE_DIR.glob("*.cppm")),
             *sorted(COURSE_DIR.glob("*.hpp"))]
    run(["/usr/bin/clang-format", "--dry-run", "--Werror", *files])
    if QT:
        for qml in sorted(COURSE_DIR.glob("*.qml")):
            formatted = subprocess.check_output([qt_tool("qmlformat"), "--normalize", qml], text=True)
            if qml.read_text() != formatted:
                raise RuntimeError(f"QML format differs: {qml}")
        run([qt_tool("qmllint"), "--max-warnings", "0", *sorted(COURSE_DIR.glob("*.qml"))])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["build", "configure", "run", "test", "check-format"],
                        nargs="?", default="build")
    args = parser.parse_args()
    if args.action == "configure":
        configure_clangd()
        return
    if args.action == "check-format":
        check_format()
        return
    build()
    if args.action == "run":
        run([BUILD / "demo"], cwd=COURSE_DIR)
    elif args.action == "test":
        if QT:
            run([BUILD / "ui-tests"], cwd=COURSE_DIR,
                env={**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"})
        elif COURSE == "month01/week01":
            run([BUILD / "demo", "--test"], cwd=COURSE_DIR)
        else:
            raise SystemExit(f"{COURSE} only provides a demo; use build and run")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"Course command failed: {error}") from error
