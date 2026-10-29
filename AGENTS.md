# Repository Guidelines

## Project Structure & Module Organization

This repository contains CAD learning courses. The root `README.md` indexes weeks and their questions. Use one directory and one course per week, organized as `month01/week01/` through `month01/week04/`, and likewise for `month02/`, `month03/`, `month04/`, `month05/`, `month06/`, and `month07/`. Each README answers all five questions for that week; do not create per-question course directories. Every course is self-contained: its `tools/builder.py`, `tools/check_navigation.py`, `tools/clangd_server.py`, and generated files live inside the course directory (`monthNN/weekYY/tools/` and `monthNN/weekYY/build/`). Do not reintroduce shared tooling at the repository root.

Keep each course self-contained. The primary deliverable is a concise answer in the course README, not a chat-only answer. Add code alongside it only when code helps explain the question; do not create a program for every question. When code is needed, document its dependencies and usage. Introduce shared modules only when multiple courses need them.

## Project Memory

用户确认的长期约定：

- 每月四门课程，每周一个目录、一门课程；第一至七月分别使用 month01、month02、month03、month04、month05、month06、month07 下的 week01–week04。每周 README 集中回答该周五个问题，不按问题拆课程。
- 在每门课程自己的 `README.md` 中简明扼要地回答问题。
- 所有 README 聚焦课程目标、五题答案与必要的运行入口；通用工具细节放在 `docs/tooling.md`，临时语法问答不写入课程 README。
- 重点是解释问题和设计取舍；需要代码辅助说明时才写代码。
- 讨论课程问题时，默认将答案落实到对应课程 README，不能只在聊天中回答。
- month01–month07 以同一参数化矩形案例连续递进；每周明确承接内容、新增目标、独立练习与验收，每月末说明下一阶段的动机。
- 含代码的课程每周可独立编译、运行和作为 IDE 文件夹打开。各周 run.sh 依次构建本周、更新单周 clangd 数据库、验证定义跳转，全部成功后运行程序；不依赖其他周产物。
- 第七月讲解单位系统设计、运行机制及其与表达式、重算和参数化文档的协作，覆盖 121–140 题。当前交付为四周讲义、执行轨迹与独立练习；构建工具仍覆盖第一至六月，新增代码时遵循独立构建约定。

项目记忆索引保存在 `.memory/index.json`。每周五个问题集中在对应周的 README；代码按需辅助解释。此规则替代之前“每个问题一门课程”的约定。

## Build, Test, and Development Commands

From a course directory use `python3 tools/builder.py build|configure|run|test|check-format` (default `build`); `./run.sh` does build + configure + navigation check + run in one step. From the repository root, `./build.sh <monthXX|monthXX/weekYY|all>` loops the weekly builders and `./run.sh <monthXX/weekYY>` runs an existing binary. See `docs/tooling.md` for tool versions and commands.

From an individual course directory, use `./run.sh` to build, configure and verify navigation, then run that course. For separate steps, `configure_clangd.py <monthXX/weekYY>` generates its standalone database, and `check_navigation.py --course <monthXX/weekYY>` verifies its actual IDE workspace. Existing IDE sessions need a clangd restart after module interfaces change.

VS Code uses each week's own executable `tools/clangd_server.py` to launch native clangd and provide named import/module navigation to `.cppm`. Keep this configured in weekly workspaces; switching directly to native clangd loses named module navigation. Function definitions and declarations remain separate requests. Weekly `check_navigation.py` provides the navigation regression checks.

When adding runnable code, document the exact setup, build, run, and test commands in the course README or a linked tooling document, including required tool versions. Run commands from the documented working directory.

## Coding Style & Naming Conventions

Use C++23, `import std;`, and the standard library for the core. Month05 and Month06 UI use Qt6 QML / Qt Quick; keep Qt dependencies in the UI adapter and tests. Apply the root `.clang-format` with clang-format 22. Keep each weekly example independently buildable.

Use descriptive filenames and identifiers. Keep generated outputs separate from editable source files, and add appropriate ignore rules before committing build artifacts.

## Testing Guidelines

Core tests use standard-library checks; month05 and month06 also use Qt Test for QML input events and signal observation. No coverage threshold is imposed. For runnable contributions, provide reproducible verification steps and the expected result. Add automated tests for substantive logic when a suitable framework is introduced, and document test locations and naming conventions.

## Commit & Pull Request Guidelines

There is no commit history to establish a message convention. Use concise, imperative commit subjects, such as `Add introductory exercise README`, and keep commits focused.

Pull requests should describe the change, affected exercises, and verification performed. Link relevant issues when available. Include screenshots for visual changes and explain any new dependencies or required local configuration.
