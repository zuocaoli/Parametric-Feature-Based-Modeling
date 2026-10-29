# Repository Guidelines

## Project Structure & Module Organization

This repository teaches CAD concepts through 24 independent weekly courses in `month01/week01` through `month06/week04`. Each week's `README.md` answers five questions and gives its exercise and acceptance criteria. Keep explanations in that README; add code only when it clarifies a concept. The courses carry one parameterized rectangle example forward, but each week must build and run on its own.

This week's C++ module interfaces are `*.cppm`, implementations are `*.cpp`, and `main.cpp` launches the demo. This week is self-contained: `tools/builder.py`, `tools/check_navigation.py`, and `tools/clangd_server.py` live in this directory's `tools/` folder; common setup instructions are in `docs/tooling.md`. Generated outputs belong in `build/`, never beside source files.

## Build, Test, and Development Commands

From `month01/week04`, run `./run.sh` to build this course, refresh its clangd database, verify definition navigation, and run the demo. From the repository root:

- `python3 tools/builder.py build` (from this directory) builds this week alone.
- `python3 tools/builder.py run` builds and runs the demo.
- `python3 tools/builder.py check-format` checks source formatting.

See `docs/tooling.md` for required GCC, Clang, Python, and Qt versions. Keep this week's `tools/clangd_server.py` configured in VS Code; restart clangd after changing module interfaces.

## Coding Style & Naming Conventions

Use C++23 named modules, `import std;`, and the standard library in core code. Follow the root `.clang-format`: four-space indentation, attached braces, and a 100-column limit; format with clang-format 22. Use descriptive names and this week's `week04` namespace. Keep Qt6 QML dependencies in the UI adapters of months 05 and 06.

## Testing Guidelines

Core checks use the standard library; later UI courses use Qt Test. There is no coverage threshold. For logic changes, add a meaningful check and document how to run it and its expected result in the relevant course README.

## Commit & Pull Request Guidelines

This repository has no commit history yet. Use focused, imperative subjects such as `Explain document ownership`. PRs should identify affected courses, summarize verification, link relevant issues, and include screenshots for visual changes. Document any new dependencies or local setup.
