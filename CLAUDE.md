# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A six-month CAD learning course: 24 self-contained weekly courses under `month01/`–`month06/`, `week01`–`week04`. One parametric-rectangle case evolves across the months. The **primary deliverable is each week's `README.md`** (written in Chinese), which answers that week's five questions and explains design trade-offs; code exists to demonstrate those answers. Put course answers in the week's README, not only in chat.

Every week must build, run, and open as a standalone IDE workspace. Weeks do not share source: modules needed from earlier weeks (lexer, property, rectangle, …) are intentionally copied into each week's directory. Do not "deduplicate" them across weeks or add cross-week imports.

## Commands

Run from the repository root unless noted.

```bash
# Build all 24 weeks / one month / one course (loops each week's builder.py)
./build.sh
./build.sh month02
./build.sh month02/week02

# Run a previously built course (does not build)
./run.sh month02/week02
```

Numeric aliases (`01`–`04`, `week01`–`week04`) always mean `month01/weekXX`; use `monthNN/weekYY` to avoid ambiguity.

Build + configure clangd + verify navigation + run, all in one step using only the course's own directory:

```bash
cd month02/week02 && ./run.sh
```

Each course owns its standalone `builder.py` (`build|configure|run|test|check-format`, default `build`):

```bash
python3 month02/week02/tools/builder.py build
python3 month05/week02/tools/builder.py test
python3 month02/week02/tools/builder.py check-format
```

- `month01/week01` provides an inline `demo --test`; months 05–06 run Qt `ui-tests` with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` (set automatically by `builder.py`).
- Weeks other than `month01/week01` and the Qt weeks have no `test` target; build + run them and check their output manually.
- Failures exit non-zero and report via exceptions (`assert` may be disabled).

clangd / IDE support (per course):

```bash
python3 month02/week02/tools/builder.py configure   # prebuilt modules + compile_commands.json
python3 month02/week02/tools/check_navigation.py    # verify definition/declaration targets via LSP
```

Both steps also run inside the course `run.sh`. After editing a `.cppm`, adding/removing sources, moving the course directory, or upgrading the toolchain: rerun `builder.py configure` and restart clangd (`clangd: Restart language server`).

Headless QML check:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./month05/week04/run.sh --screenshot build/editor.png
```

## Build architecture

There is no CMake and no repository-level tooling: every course is fully self-contained through its own `tools/` directory (`builder.py`, `check_navigation.py`, `clangd_server.py`); nothing is shared between weeks.

1. `builder.py build` compiles the libstdc++ `std` module source with GCC 16 (`-fmodules`) into the course's own `build/gcc/`, so every week owns an independent module cache.
2. It scans the week's `*.cppm` for `export module` and `import` lines, topologically orders the module interfaces (unknown or cyclic imports are hard errors), compiles each interface, then compiles every `*.cpp` and links `build/demo` (Qt weeks also `console-demo` and `ui-tests`).
3. Modules are named `monthNN.weekNN.<name>`; a week imports only `std` and its own week's modules.
4. For Qt weeks (months 05–06) it also runs moc/rcc, archives core objects into `libcad_core.a`, and links against Qt6 via pkg-config.
5. All generated output stays inside the course's own `build/` directory (gitignored, nothing is shared between weeks). Modules are rebuilt on every command to avoid stale caches.

Weekly layout: `README.md` (deliverable), `.cppm` interfaces, `.cpp` implementations, `main.cpp` (demo entry, plus inline `--test` in month01/week01; Qt weeks use `console.cpp`/`main.cpp`/`ui_tests.cpp`/`bridge.*`), `tools/builder.py`, `tools/check_navigation.py`, `tools/clangd_server.py`, `run.sh`, `.clangd`, and `.vscode/` for standalone use.

clangd is a separate toolchain path: `tools/builder.py configure` precompiles `std` and the week's modules with clang++ 22 into `build/clangd/prebuilt/`, because clangd 22's background indexer cannot build modules on its own. The week's `tools/clangd_server.py` wraps native clangd to add named `import`/`module` → `.cppm` navigation. Every weekly `.vscode/settings.json` points `clangd.path` at this local wrapper; do not switch back to native clangd or introduce fixed `-fmodule-file` mappings. F12 on a function call goes to the `.cpp` definition; "Go to declaration" opens the `.cppm` interface.

## Course progression

- **month01**: plain rectangle → unified Property system → change notification → object/document management.
- **month02**: lexer → AST and recursive-descent parser → evaluator and property reads → formula binding, manual recalculation, save/load.
- **month03**: dependency graph → topological order → incremental recalculation → binding maintenance and error recovery.
- **month04**: commands → transactions and undo/redo → stable object identity → document persistence (`CAD_DOCUMENT 1`).
- **month05**: Qt6/QML editor: selection, metadata-driven property/formula editing, drag, file operations (`CAD_DOCUMENT 2` only).
- **month06**: state consistency, differential verification, performance measurement, unified query/preview/apply API.

The root `README.md` indexes each month's questions and links every week. Keep it consistent when course content changes.

## Conventions and gotchas

- C++23, `import std;`, standard library only in the core. Qt6 belongs to the UI adapter and UI tests in months 05–06; console targets must not link Qt.
- Format with the root `.clang-format` (LLVM base, 4-space indent, 100 columns, `ReflowComments: Never`). `check-format` also runs `qmlformat` and `qmllint --max-warnings 0` for Qt weeks.
- Known pre-existing format diffs in `month01/week02/demo.cpp`, `month01/week02/property.cpp`, and `month01/week03/property.cppm` make `check-format all` fail; leave them alone unless asked.
- The root `run.sh` only executes an existing binary (`monthXX/weekYY/build/demo`). Course-level `run.sh` builds, configures clangd, verifies navigation, then runs, working entirely inside the course directory; snapshots resolve from `__FILE__`, so they land in the course's `build/` regardless of the invocation directory.
- Environment: Linux, GCC 16.2.1 with libstdc++ module sources, Clang/clangd/clang-format 22.1.8, Python 3.14, Qt 6.11.2 for months 05–06. Python tools use only the standard library.
- Adding a new month/week: copy an existing week's directory (including `builder.py`, `check_navigation.py`, `clangd_server.py`, `run.sh`, `.clangd`, `.vscode/`), change the `COURSE` constant in `tools/builder.py`/`tools/check_navigation.py`, and adjust the case patterns in the root `build.sh`/`run.sh` if a new month is introduced.
- `workflow.md` describes an optional, user-initiated task-planning workflow. Do not use it unless explicitly requested; never modify it.
- Long-term project conventions are recorded in `AGENTS.md` and `.memory/`: one directory and one README per week, five answers per week, code only when it helps explain the answer, and each week independently buildable with nothing shared between weeks.
- There is no commit history yet; use concise imperative commit subjects. Do not commit unless asked.

See `docs/tooling.md` for the full toolchain, IDE navigation, and Qt details.
