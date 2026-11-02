# Repository Guidelines

## Project Structure & Module Organization

This directory is the incremental recomputation course at `表达式引擎与依赖图/month02/week03/`. Keep it independently buildable; never reference another week's sources or build outputs.

Module interfaces (`*.cppm`), implementations (`*.cpp`), and `main.cpp` live here. Lexer, parser, evaluator, property, rectangle, dependency graph, and recomputation engine modules form the console example. Keep IDE settings in `.vscode/`, tooling instructions in `tools/README.md`, and generated artifacts in `build/`.

`README.md` contains the five course answers, prerequisites, exercise, and acceptance criteria. Update it when teaching behavior changes; add code only when it clarifies the explanation.

## Build, Test, and Development Commands

Run these commands from this directory:

- `cmake --preset default`: configure Clang/Ninja and generate `build/compile_commands.json`.
- `cmake --build --preset default`: build the local C++23 modules and demo.
- `./build/demo`: run the incremental recomputation example.
- `./run.sh`: configure, build, and run in sequence.
- `clang-format --dry-run --Werror *.cpp *.cppm`: check formatting.

Use CMake 4.3.x, Ninja ≥ 1.11, Clang/clang-scan-deps/clangd 22.1.8, and libstdc++ 16.2.1 module development files. See `tools/README.md`. After moving the course or changing compilers, run `cmake --fresh --preset default`. Native clangd reads the local build database; rebuild and restart it after module changes.

## Coding Style & Naming Conventions

Use C++23, `import std;`, and standard-library facilities. Apply the local `.clang-format` with clang-format 22: four-space indentation, attached braces, and a 100-column limit. Preserve existing `month03.week03.*` module names despite the directory numbering. Use descriptive filenames and identifiers; exclude generated outputs from commits.

## Testing Guidelines

There is no registered automated test suite or coverage threshold. `ctest --preset default --no-tests=error` reports no tests. Run the demo, compare its output with `README.md`, and complete the independent exercise: batched inputs retain old derived caches until closure, affected formulas execute once, and unrelated objects remain untouched. Document reproducible acceptance steps for changed behavior. Register any added automated tests with CTest.

## Commit & Pull Request Guidelines

History mixes Chinese subjects, English imperative subjects, and older `feat:` entries. Use concise, imperative subjects such as `Clarify incremental recomputation batching`; keep commits focused. PRs should identify the affected course, explain behavior or documentation changes, list verification, link relevant issues, and document new dependencies. Include screenshots for visual changes.
