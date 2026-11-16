# 本周构建、运行与 clangd

本周是独立的 CMake 工程，可直接作为 IDE 文件夹打开。CMakeLists.txt 显式列出本周源码与命名模块，CMakePresets.json 选择 Clang 和 Ninja；不读取其他课程的源码或构建产物。

## 环境准备

- Linux，CMake **4.3.x**，Ninja **≥ 1.11**。
- Clang、clang-scan-deps、clangd **22.1.8**，Clang 与扫描器使用相同版本，并将工具加入 PATH。
- **libstdc++ 16.2.1** 标准库，需安装 std 模块开发文件及 libstdc++.modules.json。C++ 编译器为 clang++。
- 格式检查使用 clang-format **22**。

检查 `cmake --version`、`ninja --version`、`clang++ --version`、`clang-scan-deps --version` 和 `clangd --version`。Qt 安装在自定义位置时，在配置命令后增加 `-DCMAKE_PREFIX_PATH=/path/to/Qt`。

## 构建与运行

在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

`./run.sh [程序参数...]` 依次完成配置、构建和运行，程序参数原样透传；从其他目录以绝对路径调用也会定位到本周。所有产物与 compile_commands.json 均在本周 build/ 内，重复构建按依赖增量执行。

CMake 通过 CXX_MODULES 文件集扫描并构建本周模块及 import std。模块名称保留原有前缀，与分类内月份编号无关。CMake 4.3 的 import std 支持为实验功能，升级 CMake 主次版本时需核对 CMakeLists.txt 中的启用标识。

搬移课程或切换编译器后，先执行 `cmake --fresh --preset default`，再构建。

## IDE 与原生 clangd

用 VS Code 打开本周目录，安装推荐的 clangd 与 CMake Tools 扩展。clangd.path 为 PATH 中的原生 clangd；clangd.arguments 和 .clangd 均指向本周 build/。先完成配置与构建，再在类型或函数名上使用“转到定义”“转到声明”；模块接口也可从本周文件列表打开。

修改模块接口后重新构建并执行 **clangd: Restart language server**；VS Code 的默认构建任务会先配置本周工程。

## 格式与测试

在本周目录检查源码格式：

```bash
clang-format --dry-run --Werror *.cpp
```

构建后执行 `ctest --preset default --no-tests=error`，CTest 调用 `demo --test`。标准库检查位于 main.cpp；成功输出 `week01 tests passed` 并返回 0。
