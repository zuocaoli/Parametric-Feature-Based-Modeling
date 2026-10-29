# CLAUDE.md

## Project and layout

CAD courses use one parameterized rectangle case across seven months and 28 weekly courses. Chinese categories have independent month numbering:

- 文档与属性系统/month01–month04: objects/properties, commands/documents, QML editor, verification/delivery.
- 表达式引擎与依赖图/month01–month02: expression parsing/evaluation, dependency graphs/recomputation.
- 单位系统/month01: four lecture-only weeks with traces and exercises.
- 几何引擎 is reserved for future courses.

Each month has week01–week04. Every weekly README answers five questions and gives prerequisites, an independent exercise and acceptance criteria. Put course answers in that README. Preserve original question IDs and C++ module names; directory numbering is independent. Follow the cross-category learning order in the root README.

## Independent build, run and IDE

All 24 runnable weeks use **Clang + CMake + clangd**. Each week owns its CMakeLists.txt, CMakePresets.json, run.sh, tools/, .clangd, .vscode/ and build/ directory. No root build/run scripts, shared tooling, cross-week source imports or dependencies on another week's generated files.

From the individual week directory:

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

`./run.sh [program arguments...]` performs those three steps and works from other working directories. Qt weeks also provide `./build/console-demo` without Qt dependencies in the core library.

Dependencies: Linux, CMake **4.3.x**, Ninja **>= 1.11**, Clang/clang-scan-deps/clangd **22.1.8**, and libstdc++ **16.2.1** with std module development files. C++ compilation always uses clang++. Qt weeks (文档与属性系统/month03–month04) require Qt **6.11.2**. Setup and exact commands live in each week's tools/README.md.

CMake explicitly lists local sources and CXX_MODULES file sets, scans dependencies, builds import std and exports build/compile_commands.json. Qt AUTOMOC/AUTORCC belong to the UI object library; cad_core and console-demo stay free of Qt. CMake 4.3 import std support is experimental: update the opt-in token if changing CMake major/minor versions.

Use CMake and CTest directly. Do not add Python build, test, navigation or compatibility scripts.

VS Code tasks configure before building. clangd.path selects native clangd from PATH; clangd.arguments and .clangd point at build/. Function definition/declaration requests remain separate. Rebuild after module changes and restart clangd. After relocating a course or changing compilers, first refresh its CMake cache with `cmake --fresh --preset default`.

## Verification

After building a course with automated tests:

```bash
ctest --preset default --no-tests=error
```

文档与属性系统/month01/week01 registers demo --test. Qt weeks register ui-tests with offscreen/software rendering. Other runnable weeks provide demonstrations and README acceptance exercises; they do not fabricate automatic tests.

Use clang-format 22 directly for optional formatting checks, and qmllint for QML. IDE navigation uses native clangd with the completed CMake build database. No Python scripts or navigation wrappers are kept. Known pre-existing whitespace differences in 文档与属性系统/month01/week02 and week03 should not be changed unless asked.

## Conventions

Use C++23, import std and the standard library in core code; Qt belongs in UI adapters/tests. Keep each week self-contained even when it copies earlier modules. Generated files stay in the local gitignored build/ directory. Apply the repository C++ formatting conventions with clang-format 22; Qt format checks also use qmlformat/qmllint.

Keep READMEs concise and focused on course objectives, five answers and necessary run commands. Tool details belong to the weekly tools/README.md. Do not introduce code solely to make a lecture runnable. workflow.md is an optional user-invoked workflow; do not use or edit it unless explicitly requested. Use focused, imperative commit subjects and do not commit unless asked.
