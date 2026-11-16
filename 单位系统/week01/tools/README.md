# 独立构建、测试与 IDE

本周使用 Linux、CMake **4.3.x**、Clang/clang-scan-deps/clangd **22.1.8**、Ninja **≥1.11**、libstdc++ **16.2.1** 的 std 模块开发文件。核心仅依赖标准库，语言为 C++23，使用命名模块与 `import std;`。模块名在独立课程副本间相同，不允许跨周链接。

在本周目录执行：

```bash
clang++ --version
clang-scan-deps --version
clangd --version
cmake --version
ninja --version
./run.sh
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
clang-format --dry-run --Werror *.cpp *.cppm
clangd --check=main.cpp --compile-commands-dir=build --enable-config --experimental-modules-support
```

`run.sh` 定位自身目录后依次配置、构建、运行，参数原样传递。`./build/demo --help` 查看本周入口。测试使用标准库检查，失败返回非零状态；构建产物和演示文件放在本周 build/，不提交。

VS Code 单独打开本周目录，使用 PATH 的原生 clangd。`.clangd` 和 IDE 参数指向本周 `build/compile_commands.json`；先构建，再使用转到定义/声明。修改模块接口后重新构建并重启 clangd。搬移目录或更换编译器后执行 `cmake --fresh --preset default`。

格式使用本周 LLVM 风格配置和 clang-format **22**。CMake 负责 std 模块及依赖扫描；升级 CMake 主次版本时需核对 import std 的实验开关。没有导航包装器或 Python 脚本。
