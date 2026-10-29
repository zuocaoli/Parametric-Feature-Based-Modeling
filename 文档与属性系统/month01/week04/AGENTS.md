# Repository Guidelines

## Project Structure & Module Organization

This repository teaches CAD concepts through 28 independent weekly courses in Chinese topic categories with independently numbered months. Each week's `README.md` answers five questions and gives its exercise and acceptance criteria. Keep explanations in that README; add code only when it clarifies a concept. The courses carry one parameterized rectangle example forward, but each week must build and run on its own.

This week's C++ module interfaces are `*.cppm`, implementations are `*.cpp`, and `main.cpp` launches the demo. This week owns its CMakeLists.txt, CMakePresets.json, run.sh and IDE configuration; setup instructions are in `tools/README.md`. Generated outputs belong in `build/`, never beside source files.

## Build, Test, and Development Commands

Each week is an independent Clang + CMake + clangd project. From a week directory:

- `cmake --preset default`: configure Clang/Ninja and export build/compile_commands.json.
- `cmake --build --preset default`: build only this week, including C++23 modules.
- `./run.sh`: configure, build, then run demo; arguments are forwarded.
- `clang-format --dry-run --Werror *.cpp *.cppm`: optional formatting check.

Use CMake 4.3.x, Ninja >= 1.11, Clang/clang-scan-deps/clangd 22.1.8 and libstdc++ 16.2.1 module development files. Compiler is clang++; CMake owns modules and compile_commands.json. Native clangd reads the local CMake database. No Python scripts or separate module precompilation. See [tools/README.md](tools/README.md) for setup and commands.

## Coding Style & Naming Conventions

Use C++23 named modules, `import std;`, and the standard library in core code. Follow the root `.clang-format`: four-space indentation, attached braces, and a 100-column limit; format with clang-format 22. Use descriptive names and this week's `week04` namespace. Keep Qt6 QML dependencies in the UI adapters of 文档与属性系统 months 03 and 04.

## Testing Guidelines

Core checks use the standard library; later UI courses use Qt Test. There is no coverage threshold. For logic changes, add a meaningful check and document how to run it and its expected result in the relevant course README.

## Commit & Pull Request Guidelines

This repository has no commit history yet. Use focused, imperative subjects such as `Explain document ownership`. PRs should identify affected courses, summarize verification, link relevant issues, and include screenshots for visual changes. Document any new dependencies or local setup.
