# Repository Guidelines

## Project Structure & Module Organization

This directory contains month three of the CAD learning curriculum: dependency graphs, cycle detection, incremental recomputation, and error recovery. Keep one self-contained course in each of `week01/`–`week04/`.

Each weekly `README.md` answers five questions and describes prerequisites, goals, an independent exercise, and acceptance criteria. Continue the shared parametric rectangle example. Put course answers in the relevant README; add code only when it clarifies the explanation.

C++ module interfaces (`*.cppm`), implementations (`*.cpp`), and `main.cpp` live directly in each week. Keep build and navigation scripts in that week's `tools/`, IDE configuration in `.vscode/`, and generated files in `build/`. Do not depend on another week's sources or build outputs. General tooling documentation lives at `../docs/tooling.md`.

## Build, Test, and Development Commands

Run these commands from a course directory, such as `month03/week01/`:

- `./run.sh`: build, configure clangd, verify definition navigation, then run the demo.
- `python3 tools/builder.py build`: compile the standalone example.
- `python3 tools/builder.py configure`: regenerate the weekly clangd database and precompiled modules.
- `python3 tools/check_navigation.py`: verify navigation using the actual IDE configuration.
- `python3 tools/builder.py check-format`: check C++ formatting.

The documented environment uses Linux, GCC 16.2.1 with standard-library modules, Python 3.14.7, and Clang/clangd/clang-format 22.1.8. Keep the weekly `tools/clangd_server.py` launcher configured; restart clangd after module interface changes.

## Coding Style & Naming Conventions

Use C++23, `import std;`, and standard-library facilities. Follow the parent `.clang-format`: four-space indentation, attached braces, and a 100-column limit. Use descriptive filenames and identifiers; preserve week-specific module names such as `month03.week01.dependencies`. Keep generated artifacts out of commits.

## Testing Guidelines

Month-three courses currently provide demos and manual acceptance exercises; `builder.py test` reports no test entry point. Compare demo output with the weekly README and complete its acceptance checks. Run formatting and navigation checks after code changes. No coverage threshold applies. Document reproducible commands and expected results for new logic and any added tests.

## Commit & Pull Request Guidelines

The repository currently has no commits establishing a convention. Use focused, imperative subjects such as `Clarify dependency cycle rejection`. Pull requests should identify affected weeks, explain behavior or teaching changes, list verification performed, and link relevant issues. Include screenshots for visual changes and document new dependencies.
