# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project and layout

Learning repository for the core modules of a parametric geometric modeling platform, organized as CAD course categories. The 文档/表达式/单位 categories thread one parameterized rectangle case through their weeks; 几何引擎 extends the case into 3D (box, cylinder, hole plate) with OCCT. Chinese categories have independent month numbering:

- 文档与属性系统/month01–month04: objects/properties, commands/documents, QML editor, verification/delivery.
- 表达式引擎与依赖图/month01–month02: expression parsing/evaluation, dependency graphs/recomputation.
- 单位系统/month01: four independently implemented FreeCAD-inspired C++23/import std console courses (question IDs 121–140): Unit/Quantity → parsing/schema tables → properties/expressions → application/document assembly. Uses eight dimensions including angle and internal mm/kg/s/A/K/mol/cd/degrees. Generic property channels are demonstrations; the rectangle model always validates its fixed dimensions/bounds. Persistence teaches saving/loading the current model and explicit-unit exchange. Do not introduce course/file version comparisons, legacy compatibility or migrations. No FreeCAD/Qt/Python runtime dependency.
- 几何引擎/month01–month06 (planned): 几何与拓扑表示 → 建模与曲面 → 投影求交裁剪 → 布尔与局部建模 → 容差与修复 → 综合实践. month01/week01–week04 are fully implemented; later months are plan-only (see 几何引擎/README.md and 几何引擎/plan/).
- 二维约束求解, 三维约束求解, 事务系统, 几何表达: reserved categories (README only or empty).

Each month has week01–week04. Every weekly README answers five questions and gives prerequisites, an independent exercise and acceptance criteria. Put course answers in that README. Preserve original question IDs and C++ module names; directory numbering is independent. Follow the cross-category learning order in the root README. AGENTS.md holds the detailed repository guidelines; long-term conventions live in `.memory/index.json`.

## Independent build, run and IDE

All 32 runnable weeks use **Clang + CMake + clangd**. Each week owns its CMakeLists.txt, CMakePresets.json, run.sh, tools/, .clangd, .vscode/ and build/ directory. No root build/run scripts, shared tooling, cross-week source imports or dependencies on another week's generated files. The one shared resource: geometry weeks link the vendored **OCCT 8.1.0** install at `几何引擎/occt/` (never modify or mix in other OCCT versions).

From the individual week directory:

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

`./run.sh [program arguments...]` performs those three steps and works from other working directories. Non-Qt weeks also provide `./build/console-demo` without Qt dependencies in the core library.

Dependencies: Linux, CMake **4.3.x**, Ninja **>= 1.11**, Clang/clang-scan-deps/clangd **22.1.8**, libstdc++ **16.2.1** with std module development files, and for Qt weeks Qt **6.11.2**. C++ compilation always uses clang++. Setup and exact commands live in each week's tools/README.md.

CMake explicitly lists local sources and CXX_MODULES file sets, scans dependencies, builds import std and exports build/compile_commands.json. CMake 4.3 import std support is experimental: update the opt-in token if changing CMake major/minor versions. Use CMake and CTest directly; no Python build, test, navigation or compatibility scripts.

### Geometry weeks (几何引擎/monthNN/weekYY)

Copied wholesale from `几何引擎/template/week-template` (never import from the template or other weeks afterwards):

```bash
mkdir -p 几何引擎/monthNN/weekYY
tar -C 几何引擎/template/week-template --exclude=./build -cf - . | tar -C 几何引擎/monthNN/weekYY -xf -
```

Layout per week: `framework/` (geometry_core, Qt-free: experiment/inspect/artifacts/command_line plus `ui/`), `experiments/` (box-transform base example + the week's course experiment), `tests/`, `main.cpp` (Qt Widgets + OCCT viewer demo), `console.cpp` (headless console-demo). CMake targets: geometry_core → geometry_ui (AUTOMOC, Linux X11 only) → demo; console-demo links geometry_core alone. OCCT is found via `../../occt/lib/cmake/opencascade` and linked per component; the `geometry_runtime()` helper sets DT_RPATH because the OCCT DSOs lack RUNPATH — no `LD_LIBRARY_PATH` needed. `GEOMETRY_BUILD_GUI=OFF` (preset `headless`) drops Qt/X11 entirely. Implementation files put `import std;` **before** header includes (clangd duplicate-definition workaround); keep that order in new experiments. VS Code tasks configure before building. clangd.path selects native clangd from PATH; clangd.arguments and .clangd point at build/. Rebuild after module changes and restart clangd. After relocating a course or changing compilers, first refresh its CMake cache with `cmake --fresh --preset default`.

## Verification

After building a course with automated tests:

```bash
ctest --preset default --no-tests=error
```

文档与属性系统/month01/week01 registers demo --test. Qt weeks register ui-tests with offscreen/software rendering. Geometry weeks register core/course/cli tests plus an offscreen ui test (`ctest --preset headless` for the 5 GUI-free tests); real-viewer acceptance is opt-in and manual: `xvfb-run -a -s '-screen 0 1600x1100x24' ./build/viewer-tests` (never registered as a ctest test). 单位系统 weeks register standard-library quantity/input/expression/document/operations/storage/migration checks as introduced, plus the console acceptance demo. Other runnable weeks provide demonstrations and README acceptance exercises.

Use clang-format 22 directly for optional formatting checks, and qmllint for QML. Known pre-existing whitespace differences in 文档与属性系统/month01/week02 and week03 should not be changed unless asked.

## Conventions

Use C++23, import std and the standard library in core code; Qt belongs in UI adapters/tests (QML in 文档与属性系统 month03–04, Qt Widgets + OCCT viewer in geometry weeks). Keep each week self-contained even when it copies earlier modules. Generated files stay in the local gitignored build/ directory. Apply the repository C++ formatting conventions with clang-format 22; geometry weeks use their local .clang-format; Qt format checks also use qmlformat/qmllint.

Keep READMEs concise and focused on course objectives, five answers and necessary run commands. Tool details belong in the weekly tools/README.md. Do not introduce code solely to make a lecture runnable. Use focused, imperative commit subjects and do not commit unless asked.

workflow.md is an optional user-invoked workflow (task.md → task_archive/); do not use or edit it unless explicitly requested.

Note: `.github/instructions/` and `.github/prompts/` are stale PipeCAD leftovers referencing a `src/` tree that does not exist in this repository — do not follow them.
