# 本周构建、运行与 IDE

所有命令从本周目录执行，各周源码与构建产物完全独立。没有根构建入口，不需要 Python 或前一周产物。

## 工具链与依赖

| 组件 | 验证版本／要求 |
|---|---|
| 语言 | C++23、具名模块、`import std;` |
| Clang / clang-scan-deps / clangd | 匹配的 22.1.8 |
| CMake | 4.3.0（4.3.x） |
| Ninja | 本机 1.13.2，要求 ≥1.11 |
| 标准库 | libstdc++ 16.2.1 与标准库模块开发文件 |
| Eigen | 5.0.1，CMake CONFIG 包与头文件（从第 3 周起使用） |
| Qt | 6.11.2 Widgets；测试需要 Qt Test，可使用 headless preset 关闭 |
| clang-format | 22，使用本周 `.clang-format` |

本机使用 Fedora 44，依赖安装入口为（不保证其他发行版提供相同版本）：

```bash
sudo dnf install clang clang-tools-extra cmake ninja-build libstdc++-devel eigen3-devel qt6-qtbase-devel
```

无 Qt 构建可省略 `qt6-qtbase-devel`，并使用下文 headless preset。版本低于上表时先准备匹配工具链；自定义依赖目录用 `cmake --preset default -DCMAKE_PREFIX_PATH=/absolute/sdk/prefix`，不会自动下载依赖。

先检查工具链；标准库模块须与当前 Clang 配套使用。

```bash
cmake --version
ninja --version
clang++ --version
clang-scan-deps --version
clangd --version
pkg-config --modversion eigen3
pkg-config --modversion Qt6Widgets Qt6Test
```

依赖由系统安装或 CMake prefix 提供；Eigen 使用 `Eigen3::Eigen`，不下载或复制数学库。Qt 只链接 UI 和 UI 测试；关闭 GUI 时不查找 Qt。CMake 4.3 的标准库模块支持使用本周配置中的实验 token，升级 CMake 后需核对 token，不能只改最低版本。

## 配置、构建、运行、测试

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
./build/demo
./build/console-demo --headless
```

`./run.sh` 配置、构建并启动 Qt 观察器；`./run.sh --headless` 使用同一构建启动控制台验证，不创建 QApplication。`console-demo` 从不链接 Qt。示例构造与实验流程在 `src/lesson.cpp`，不从其他周读取配置。

完全不安装 Qt 时使用独立配置：

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless --no-tests=error
./build/headless/demo --headless
```

本周新增功能测试：

```bash
ctest --preset default -R "^(jacobian)$" --no-tests=error
```

`default` 为 Debug；Release 使用 `cmake --preset release`、`cmake --build --preset release` 和 `ctest --preset release --no-tests=error`。各 preset 分别写入本周 `build/`、`build/headless/` 和 `build/release/`，没有跨周缓存。

本周尚无求解器，不提供性能基准。

## IDE 与模块

把本周目录作为 IDE 文件夹打开。默认 `.clangd` 与 `.vscode/settings.json` 指向 `build/compile_commands.json`，使用 PATH 中的原生 clangd，不提供导航包装器。Eigen 仍以头文件放在数值实现文件的全局模块片段，Qt 仍用普通头文件，不建立第三方头单元，也不把 Eigen 类型泄露到公开模块接口。

```bash
cmake --fresh --preset default
cmake --build --preset default
clangd --check=src/model.cpp --compile-commands-dir=build --experimental-modules-support --tweaks=ExtractVariable --log=error
```

切换编译器、模块配置或标准库后 fresh 配置、重建并重启 clangd。仅构建 headless 时，需把 clangd 数据库路径调整为本周 `build/headless/`。定义和声明使用 IDE 各自的原生请求。上面的离线检查限定 ExtractVariable，避免 clangd 穷举其他重构动作产生与语法检查无关的失败；IDE 保留默认重构功能。

## 图形验证

CTest 的 `ui` 用 `QT_QPA_PLATFORM=offscreen` 执行，截图保存在测试工作目录 `build/workbench.png`。它验证实际 Qt 控件事件与绘制；人工运行 `./build/demo` 补充检查显示环境下的布局和操作。无需 OCCT、OpenGL 专用视窗或跨分类依赖。

本周测试组：model, residual, jacobian, console, ui。早期周只提供已经学习的功能；界面从第一周可观察，最后一周才支持鼠标拖拽和创建几何。
