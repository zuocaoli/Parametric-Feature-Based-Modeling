# Repository Guidelines

## Project Structure & Module Organization

This directory contains month two of the 表达式引擎与依赖图 category: dependency graphs, cycle detection, incremental recomputation, and error recovery. Keep one self-contained course in each of `week01/`–`week04/`.

Each weekly `README.md` answers five questions and describes prerequisites, goals, an independent exercise, and acceptance criteria. Continue the shared parametric rectangle example. Put course answers in the relevant README; add code only when it clarifies the explanation.

C++ module interfaces (`*.cppm`), implementations (`*.cpp`), and `main.cpp` live directly in each week. Keep build and navigation scripts in that week's `tools/`, IDE configuration in `.vscode/`, and generated files in `build/`. Do not depend on another week's sources or build outputs. Tooling documentation lives in each week's `tools/README.md`.

## Build, Test, and Development Commands

Each week is an independent Clang + CMake + clangd project. From a week directory:

- `cmake --preset default`: configure Clang/Ninja and export build/compile_commands.json.
- `cmake --build --preset default`: build only this week, including C++23 modules.
- `./run.sh`: configure, build, then run demo; arguments are forwarded.
- `clang-format --dry-run --Werror *.cpp *.cppm`: optional formatting check.

Use CMake 4.3.x, Ninja >= 1.11, Clang/clang-scan-deps/clangd 22.1.8 and libstdc++ 16.2.1 module development files. Compiler is clang++; CMake owns modules and compile_commands.json. Native clangd reads the local CMake database. No Python scripts or separate module precompilation. See [week01/tools/README.md](week01/tools/README.md) for setup and commands.

## Coding Style & Naming Conventions

Use C++23, `import std;`, and standard-library facilities. Follow the parent `.clang-format`: four-space indentation, attached braces, and a 100-column limit. Use descriptive filenames and identifiers; preserve week-specific module names such as `month03.week01.dependencies`. Keep generated artifacts out of commits.

## Testing Guidelines

These dependency-graph courses currently provide demos and manual acceptance exercises; `ctest --preset default --no-tests=error` reports no tests. Compare demo output with the weekly README and complete its acceptance checks. Check formatting and verify type/function navigation in native clangd after code changes. No coverage threshold applies. Document reproducible commands and expected results for new logic and any added tests.

## Commit & Pull Request Guidelines

The repository currently has no commits establishing a convention. Use focused, imperative subjects such as `Clarify dependency cycle rejection`. Pull requests should identify affected weeks, explain behavior or teaching changes, list verification performed, and link relevant issues. Include screenshots for visual changes and document new dependencies.
