# Repository Guidelines

## Project Structure & Module Organization

This repository teaches the core modules of a parametric geometric modeling platform. Keep the root `README.md` concise: project purpose, category links, learning order and run entry. Category READMEs contain detailed goals, weekly questions and stage outcomes. Keep one directory and one course per week under `<中文分类>/monthNN/weekYY/`; each weekly README answers all five questions. Every runnable course owns its `CMakeLists.txt`, `CMakePresets.json`, `run.sh`, `tools/`, IDE configuration and `build/` directory. Do not add cross-course source/build dependencies or shared root tooling. 单位系统 has four independently runnable FreeCAD-inspired C++23 console courses using eight-dimensional Unit algebra and internal millimetres/degrees.

Keep each course self-contained. The primary deliverable is a concise answer in the course documentation, not a chat-only answer. For 单位系统, category and weekly READMEs contain only core objectives, learning outcomes and navigation; detailed schedules belong in 课程安排.md and the five answers, exercises and acceptance steps in each week's 课程讲义.md. This supersedes the README placement rules below for that category. Add code alongside it only when code helps explain the question; do not create a program for every question. When code is needed, document its dependencies and usage. Introduce shared modules only when multiple courses need them.

## Project Memory

用户确认的长期约定：

- 课程按中文主题归类，各分类的月份独立从 month01 编号：文档与属性系统使用 month01–month04，表达式引擎与依赖图使用 month01–month02，单位系统使用 month01，几何引擎规划 month01–month06。每月四门课程，目录为 `<中文主题>/monthNN/week01–week04`；每周 README 集中回答五题，不按问题拆课程。不保留“参数化”分类。
- 在每门课程自己的 `README.md` 中简明扼要地回答问题。
- 根 README 简明说明项目用于学习“参数化几何建模平台涉及到的核心模块”，保留分类入口、学习顺序和运行入口；详细课程目标、逐周问题和阶段成果放在分类 README，五题答案与练习放在各周 README。工具细节放在各周 `tools/README.md`，临时语法问答不写入课程 README。
- 重点是解释问题和设计取舍；需要代码辅助说明时才写代码。
- 单位系统分类及每周 README 只说明课程核心目标、学完能够掌握什么并提供导航；逐周问题放在分类《课程安排.md》，五题详解、练习与验收放在对应周《课程讲义.md》，工具说明仍在 tools/README.md。此条替代本分类此前将详解集中在 README 的约定。
- 讨论课程问题时，默认将答案落实到对应课程 README，不能只在聊天中回答。
- 七个月以同一参数化矩形案例连续递进；跨分类学习顺序见根 README。每周明确承接内容、新增目标、独立练习与验收，每月末说明下一阶段的动机。
- 含代码的课程每周以 Clang + CMake + clangd 独立构建、运行和作为 IDE 文件夹打开。各周 run.sh 依次运行 CMake 配置、构建和本周程序；CMake 生成本周 build/compile_commands.json，clangd 读取该数据库。原生 clangd 提供 IDE 导航，不保留 Python 脚本，不依赖其他周产物。
- 单位系统 month01 已按 FreeCAD 设计思想重新实现一个月四周、20题（121–140）：Unit/Quantity → 解析与数据驱动schema → 属性/表达式 → 应用/文档装配。采用八维量纲（含角度）及 mm/kg/s/A/K/mol/cd/度内部约定；每周完整本地源码、五题讲义、独立构建与CTest。通用PropertyQuantity演示三通道差异，矩形模型固定量纲统一校验。第四周仅讲解当前模型的保存、加载和单位交换。不自行添加课程或文件的新旧版本、历史兼容与迁移功能。不引入FreeCAD/Qt/Python依赖。参考FreeCAD-Unit及源码9a274695f3。

- 几何引擎规划 6 个月、24 周。各周复制完整框架源码，独立构建和演进，不引用模板或其他周源码/产物；共用 `几何引擎/occt` 第三方安装。模板入口为 `几何引擎/template/week-template`，使用 Qt Widgets + OCCT，支持参数面板及无窗口命令行。当前已实现 month01/week01–week04 的完整独立框架、五题讲义、实验与测试；其余月份仍为规划。
- 项目语言标准允许 C++20 及以上；几何模板采用 C++23 和 `import std;`，不提供 C++20 兼容实现。工具链固定为 Clang + CMake + Ninja + clangd。

项目记忆索引保存在 `.memory/index.json`。每周五个问题集中在对应周的 README；代码按需辅助解释。此规则替代之前“每个问题一门课程”的约定。

## Build, Test, and Development Commands

Run commands from the individual week directory: `cmake --preset default`, `cmake --build --preset default`, then `./build/demo`. `./run.sh [program arguments...]` performs these steps. Each CMake project selects Clang, uses Ninja for C++ module dependency scanning and exports its own `build/compile_commands.json`. The documented toolchain is CMake 4.3.x, Clang/clang-scan-deps/clangd 22.1.8, Ninja >= 1.11 and libstdc++ 16.2.1 module development files. Qt weeks also require Qt 6.11.2. See each week's `tools/README.md` for setup and verification commands.

There are no root build/run entry points or Python scripts. Use CMake, CTest and native clangd directly. CMake owns module dependencies, import std, AUTOMOC and AUTORCC. After moving a course or changing compiler, refresh its cache with `cmake --fresh --preset default`; rebuild and restart clangd after module changes.

VS Code runs native clangd from PATH, with clangd.arguments and .clangd pointing at the local CMake build/ database. Keep each week independently openable. Function definitions and declarations remain separate IDE requests; there is no navigation wrapper or script-based check.

When adding runnable code, document the exact setup, build, run, and test commands in the course README or a linked tooling document, including required tool versions. Run commands from the documented working directory.

## Coding Style & Naming Conventions

Use C++20 or newer; current runnable courses and the geometry template select C++23 with `import std;`. Use the standard library for the core. 文档与属性系统 month03 and month04 use Qt6 QML / Qt Quick; keep Qt dependencies in the UI adapter and tests. Existing C++ module names retain their original prefixes independently of directory numbering. Apply the root `.clang-format` with clang-format 22, or the geometry template's local configuration when the root file is absent. Keep each weekly example independently buildable.

Use descriptive filenames and identifiers. Keep generated outputs separate from editable source files, and add appropriate ignore rules before committing build artifacts.

## Testing Guidelines

Core tests use standard-library checks; 文档与属性系统 month03 and month04 also use Qt Test for QML input events and signal observation. Register existing tests in each week's CTest configuration. After building, run `ctest --preset default --no-tests=error`; demo-only courses instead use README acceptance exercises. No coverage threshold is imposed. Provide reproducible verification steps and expected results. Keep Qt dependencies in the UI adapter/tests and out of the console core.

## Commit & Pull Request Guidelines

There is no commit history to establish a message convention. Use concise, imperative commit subjects, such as `Add introductory exercise README`, and keep commits focused.

Pull requests should describe the change, affected exercises, and verification performed. Link relevant issues when available. Include screenshots for visual changes and explain any new dependencies or required local configuration.
