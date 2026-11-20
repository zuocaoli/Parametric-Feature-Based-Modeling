# 本周工具与验收

使用 Linux、CMake 4.3.x、Ninja 1.11 或更新版本、Clang / clang-scan-deps / clangd 22.1.8，以及含 C++23 `std` 模块的 libstdc++ 16.2.1 开发包。验证使用 CMake 4.3.0、Ninja 1.13.2 和 OCCT 8.1.0 开发版。

CMake 默认从课程根目录的 `occt/lib/cmake/opencascade` 查找 `OpenCASCADEConfig.cmake`。使用其他 OCCT 安装位置时，在本周目录用 `cmake --preset default -DOpenCASCADE_DIR=/path/to/lib/cmake/opencascade` 配置。课程仅需 OCCT 的 FoundationClasses、ModelingData 和 ModelingAlgorithms。

在本周目录执行：

```sh
cmake --preset default
cmake --build --preset default
./build/demo
ctest --preset default --no-tests=error
```

`./run.sh` 执行配置、构建和演示。IDE 可单独打开本周目录，clangd 从本周的 `build/compile_commands.json` 读取配置。更换编译器或移动目录后，使用 `cmake --fresh --preset default` 重新配置，再构建并重启 clangd。
