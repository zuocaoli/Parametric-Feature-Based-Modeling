# 编译、验证与编辑器

本文的构建、运行、测试和 clangd 命令适用于第一至六月的 24 门可运行课程。第七月的四门[单位系统课程](../month07/week01/README.md)通过讲义、执行轨迹和独立练习学习，无需安装额外依赖或编译；当前没有 `month07` 构建目标，`all` 仍指前六个月。

## 单独打开一周课程

24 周均可作为独立 VS Code 文件夹打开。每周 `.vscode/` 提供 clangd 设置、扩展推荐及当前周任务，`.clangd` 为其他支持 clangd 的 IDE 指定相同数据库。课程完全自包含：构建脚本（`builder.py`、`check_navigation.py`、`clangd_server.py`）与全部产物都位于本周目录内，不依赖仓库根目录的任何公共文件。

在该周目录终端执行本周脚本（这里选择 month03/week03）：

```bash
./run.sh
```

各周 `run.sh` 依次执行本周 `builder.py build`、`builder.py configure` 和 `check_navigation.py`，全部成功后启动 `build/demo`；失败立即停止，命令参数传给 demo。脚本只依赖自身所在的课程目录，并把工作目录设为该目录；程序内的保存路径由源码位置（`__FILE__`）推导，从其他目录调用也不会误放保存文件。根目录原有 `run.sh <课程路径>` 继续只运行已构建程序；需要编译与导航更新时使用课程内的脚本。

VS Code 的“运行本课程”任务调用本周脚本；已有会话在模块接口变化后执行 **clangd: Restart language server** 并等待后台索引。可分别运行“构建本课程”“配置本课程 clangd”“验证本课程代码跳转”，默认构建任务支持 Ctrl+Shift+B。第一月第二至四周提供“核对本课程演示”，其余周提供“验证本课程”；格式任务选择当前周。F12 跳转定义，Shift+F12 查看引用。

### 跳转到模块接口 .cppm

每周的 `clangd.path` 指向本周 `tools/` 目录内的可执行脚本 `clangd_server.py`，由它启动 `/usr/bin/clangd`。clangd 22 对命名模块的 `import`/`module` 行没有提供定义位置，适配脚本根据实际编译数据库定位 `export module`，补齐这两类行的 F12/转到声明。它不改变 C++ 编译方式，普通类型、函数、补全和诊断请求继续交给原生 clangd；版本检查与 `--check` 也使用原生工具。

- 在 `import week04.demo;` 或 `module week04.demo;` 的模块名上按 F12，打开 `demo.cppm` 的模块声明。
- 在 `Document`、`ChangeEvent` 等类型名上按 F12，打开对应 `.cppm` 的类型定义。
- 在函数调用上按 F12，查看 `.cpp` 中的实现；要看 `.cppm` 中的接口，使用右键 **转到声明**。例如 `create_rectangle()` 的声明位于 `document.cppm`，实现位于 `document.cpp`。

第一次切换到适配脚本后必须执行 **clangd: Restart language server**。之后运行本周 `./run.sh` 会更新数据库并验证模块入口、类型和函数跳转；接口编辑后的重启规则仍适用。适配脚本处理未保存文本和 UTF-16 编辑范围，未知模块交回 clangd，不猜测同名文件或跨周目标。

每周的 `check_navigation.py` 通过真实 LSP 验证本周命名模块到接口源码，以及关键类型与函数声明；不会把函数声明当成函数实现通过。（原 `tools/tests` 集中式回归测试已随集中式工具链一起移除。）

第五、六月的脚本最终启动 GUI，需要图形会话。无桌面验证时可在仓库根目录调用：

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./month05/week04/run.sh --screenshot build/editor.png
```

截图参数使程序保存图片后退出（路径相对课程目录）；真实 QML 自动测试用 `python3 month05/week04/tools/builder.py test`，无需启动交互窗口。

单周 `builder.py configure` 只预编译 std 和该周命名模块，数据库、预编译模块及 Qt moc/rcc 索引源码位于本周 `build/clangd/`。显式数据库参数也覆盖仓库外的 std 模块源码。修改 `.cppm`、新增/删除源码、搬移课程目录或升级工具链后重新配置，并重启 clangd。

每周的 `check_navigation.py` 用该周目录作为 LSP 根工作区、读取该周真实 VS Code 参数，核对函数实现定义，未提前打开的实现依靠后台索引查找；不补充数据库参数掩盖编辑器配置问题。

课程脚本不再有集中式回归测试；构建、配置与导航验证由各周 `run.sh` 每次运行时实际执行，任一步失败立即停止。

## 公共环境与全库工作区

环境：Linux、GCC 16.2.1（含 libstdc++ 的 std 模块源码）、Clang / clangd / clang-format 22.1.8、Python 3.14.7。Python 脚本只使用标准库。C++23 的 `import std;` 需要模块工具链，旧编译器仅接受 `-std=c++23` 并不足够。

所有命令从仓库根目录运行：

```bash
./build.sh
./build.sh 02
./run.sh 01
./run.sh 02
python3 month01/week01/tools/builder.py test
python3 month06/week04/tools/builder.py check-format
python3 month01/week01/tools/builder.py configure
python3 month01/week01/tools/check_navigation.py
/usr/bin/clangd --experimental-modules-support --compile-commands-dir=month01/week01/build/clangd --check=month01/week01/main.cpp --tweaks=
```

入口脚本需要 Bash：`./build.sh [01|02|03|04|month01|month02|month03|month04|month05|month06|monthXX/weekYY|all]` 循环调用各周 `builder.py build`，省略参数构建全部；`./run.sh <01|02|03|04|monthXX/weekYY>` 直接运行已有程序，不编译。XX 为 01–06，YY 为 01–04。程序不存在或不可执行时提示先构建并返回非零退出码；修改源码后需手动重新构建。两个脚本均支持 `--help`，通过脚本自身位置定位仓库，也可在其他目录用绝对路径调用；产物始终放在各课程自己的 `build/` 目录下。

每周 `builder.py` 支持 `build|configure|run|test|check-format`，省略参数相当于 build。第一月第二至四周及第二至四月只提供演示，按课程 README 手工核对输出；对这些课程执行 test 会提示无测试入口。第一月第一周运行内嵌的 `demo --test`；第五、六月运行 Qt `ui-tests`。

第二月从根目录执行 `./build.sh month02`、`./run.sh month02/week04`，或进入各周目录执行 `python3 tools/builder.py check-format`。四周独立源码位于 month02/week01–week04，可执行文件位于 month02/weekXX/build/demo。模块名使用 month02.weekXX 前缀，不依赖第一月或其他周的模块。Lexer/Parser 逐周复制，第三、四周各有 Property 副本以保证独立构建。

第三月对应命令为 `./build.sh month03`、`./run.sh month03/week04`，或进入各周目录执行 `python3 tools/builder.py check-format`。源码在 month03/week01–week04，产物在 month03/weekXX/build/，模块前缀 month03.weekXX。第一周依赖提取与双向图，第二周排序与循环，第三周增量协调与批次，第四周错误状态、解绑和删除恢复；每周只 import std 与本周模块。运行各周演示并按 README 核对预期结果。

GCC 模块在每周自己的 `build/gcc/` 编译，各周可执行文件为 `<课程路径>/build/demo`，例如 `month01/week01/build/demo`、`month02/week01/build/demo`。每周 `builder.py` 读取 `.cppm` 的模块声明和 import 依赖，先按依赖顺序编译接口，再编译全部 `.cpp` 并链接接口与实现的目标文件；未知依赖或循环依赖会明确报错。目标文件位于本周 `build/`，模块缓存位于本周 `build/gcc/gcm.cache/`。格式检查包含 `.cpp`、`.cppm` 和 `.hpp`。每次 build/run/test 都重新构建模块，避免工具链升级后使用旧缓存；标准库模块也按周独立编译。各周之间无源码依赖，也不共享任何工具或产物。

clangd 使用独立的 Clang 模块缓存，无法读取 GCC 的 `.gcm`。配置脚本从 GCC 模块清单定位 std 源码，为 std 和本周的 `.cpp`、`.cppm` 生成 `build/clangd/compile_commands.json`，并按依赖顺序预编译模块到 `build/clangd/prebuilt/`。数据库通过 `-fprebuilt-module-path` 提供这些模块。clangd 22 的实验模块支持只为交互解析自动构建模块；后台索引需要预编译模块，否则会报 `module 'week03.property' not found`，未打开的 `.cpp` 中的函数定义便无法被完整索引。

VS Code 保留 `--experimental-modules-support`，让交互解析使用当前源码自动构建模块；数据库不使用固定的 `-fmodule-file` 映射。修改 `.cppm` 后，交互解析会更新，但后台索引使用的预编译模块也需要刷新：保存文件后重新运行 `python3 tools/builder.py configure`，再重启 clangd。新增或删除源码文件、迁移仓库、清理 build 或升级工具链后同样重新配置。脚本会清理课程和 std 源文件对应的生成索引，避免继续加载此前解析失败或位置过时的索引；系统头文件索引保留。

周工作区启动参数显式指定 `--compile-commands-dir=${workspaceFolder}/build/clangd`（本周目录），让仓库外的 `/usr/include/c++/16/bits/std.cc` 也能从该数据库获取编译命令。仅在 `.clangd` 中配置 `CompilationDatabase`，无法覆盖这个外部标准库源文件；日志会报 `Failed to build module std; due to No compile command`，导致课程模块解析和跨文件跳转失败。遇到这个错误，确认工作区参数包含上述路径、运行配置脚本，再重启 clangd。

首次切换这项配置后执行 **clangd: Restart language server**。旧的固定 `.pcm` 会保留生成时的接口行号，仅重建 GCC 程序无法刷新它；现在的编译数据库已移除这些固定映射。命令行检查同样需要加 `--experimental-modules-support`。

在各周目录运行 `python3 tools/check_navigation.py` 可验证跨模块跳转：脚本读取本周 `.vscode/settings.json` 的 clangd 路径和启动参数（展开 `${workspaceFolder}`），等待后台索引完成后，通过 LSP 请求定义位置，并核对返回文件、符号名和函数定义所在行。覆盖矩形类型、`Update`、`Value`、`notifying_`、`notification_errors_`、标准库的 `is_nothrow_swappable_v`，以及 `set()`、`result_name()`、`set_many()`、`ChangeSignal::emit()/connect()`、`Connection::disconnect()` 和文档创建接口。函数测试必须返回实现 `.cpp` 中的定义，返回 `.cppm` 声明不算通过；信号实现和文档实现保持未打开，以验证后台索引。测试不额外补充模块或数据库参数，以便发现编辑器配置缺失。后台索引最多等待 120 秒；超时明确报告原因并返回非零退出码，脚本只使用 Python 标准库。

第二月跳转检查还覆盖 Lexer、Parser、calculate/evaluate、FormulaEngine 的 bind/compute 和 load_model；分别从调用位置跳到实现文件。新增 40 个源码和模块接口均包含在同一编译数据库中。

第三月再增加 50 个 C++ 源码和模块接口；跳转检查覆盖变量提取、拓扑与候选图、受影响闭包、批次协调、状态读取及对象删除的实现。新增课程后运行 `builder.py configure` 并在 VS Code 执行 **clangd: Restart language server**。

第三月四周格式检查通过；四个 main、第一/二周的依赖图实现、第三/四周的协调引擎实现均通过 clangd --check 解析检查，跨模块定义跳转验证通过。

VS Code 打开某一周的课程目录（推荐方式），安装推荐的 clangd 扩展。工作区通过本周 `tools/clangd_server.py` 启动 `/usr/bin/clangd`，避免用户全局配置的其他版本与 `/usr/bin/clang++` 不兼容，并补齐命名模块入口导航。生成数据库后运行 **clangd: Restart language server**。F12 跳转定义、Shift+F12 查看引用、Ctrl+Shift+I 格式化；保存时自动格式化。

重启后等待后台索引完成，再用 F12 或右键“转到定义”。例如第三周 `changed_.emit(event)` 应跳到 `month01/week03/change_signal.cpp` 中的 `ChangeSignal::emit()`，声明处的 F12 也应到同一函数体。若跳转后列出调用处，检查是否使用了 Shift+F12“查看引用”；类名、类型别名和成员变量则通常在 `.cppm` 中定义，跳到接口文件是正常结果。

跳转后返回原来的文件和光标位置，使用 VS Code 的“后退”（`workbench.action.navigateBack`）。用户要求使用 **Alt+←** 后退、**Alt+→** 前进；本机用户快捷键已明确绑定这两项，不额外设置字母或减号组合键。F12 用于查找定义，不是返回导航。针对组合键未出现在按键日志中的现象，已只读查询 KDE 的运行时全局快捷键，确认没有注册占用这两组键；VS Code 用户设置中的 `window.customMenuBarAltFocus` 已改为 `false`，排除 Alt 聚焦菜单栏的可能，实际效果仍需按键验证。用户快捷键和菜单设置自动加载，无需重启 clangd。若仍无响应，开启“开发人员: 切换键盘快捷方式疑难解答”（`Developer: Toggle Keyboard Shortcuts Troubleshooting`），点击代码编辑区后按 Alt+←/→，查看是否出现 `ArrowLeft`/`ArrowRight` 和 `workbench.action.navigateBack/Forward` 的记录。Ctrl+Tab 只切换编辑器。

命令行应用格式：`clang-format -i month01/week01/main.cpp`。各周都读取根 `.clang-format`，四空格缩进；`ReflowComments: Never` 保留手写注释的分行，不自动合并或重新换行。Microsoft C/C++ IntelliSense 已在工作区禁用，避免重复诊断。

标准库符号可能跳到 `bits/std.cc` 的导出声明，这是模块接口的位置。若 std 模块无法加载，检查 Clang 与 clangd 版本一致、模块清单及源码存在，再重新生成数据库。

本机 clangd 22 的 `--check` 会额外测试重构操作，泛型代码中的“展开推导类型”和含 continue 的“提取函数”可能报告自检失败。上述 `--tweaks=` 仅在检查命令中关闭重构自检，仍检查源码解析和符号功能，不更改编辑器设置。第二月四周通过格式检查，四个 main 与 Parser/Evaluator/FormulaEngine/Storage 实现通过 clangd 解析检查，跨模块定义跳转验证通过。

当前 `check-format all` 会报告第一月原有的三处空白格式差异：month01/week02/demo.cpp、month01/week02/property.cpp、month01/week03/property.cppm。第二月实现未修改这些课程源文件；可单独执行 `check-format month02` 检查新增课程。

第四月源码位于 month04/week01–week04，模块前缀 month04.weekXX，每周独立复制必要前置模块，仅依赖标准库。命令为 `./build.sh month04`、`./run.sh month04/week04`，或进入各周目录执行 `python3 tools/builder.py check-format`。四个 main 演示历史、事务和失败恢复；第三周增加对象/公式命令，第四周增加流式保存加载与损坏文件保护。

第四周演示将文件保存在 month04/week04/build/document.txt，路径由源码位置推导，即使从其他目录运行也不会写到别处。加载只替换通过结构校验的候选文档，计算失败状态可恢复；保存检查写入结果，示例检查 flush，不保证文件替换原子性。数字 locale/精度及引用格式见第四周 README。

新增第四个月后，各周 builder.py configure 覆盖该周全部新源码，check_navigation.py 额外检查命令提交、撤销、快照恢复、批次、删除与保存加载的实现定义。旧 01–04 和 weekXX 别名仍指向第一月；第一月三处已知格式差异不在本次修改范围。

第四月四周格式检查通过，四个 main、四个文档实现、第四周引擎与保存实现的 clangd --check 均为 0 errors；检查时显式传 `--compile-commands-dir=<课程路径>/build/clangd`，与 VS Code 启动参数一致。九项新增跨模块实现定义跳转检查通过。

## 第五个月：Qt6 QML 构建与验证

已验证 Qt 6.11.2。需要 Qt6 的 Core、Gui、Qml、Quick、QuickControls2、Test 开发库，QML 的 QtQuick、Controls、Layouts、Dialogs 模块与 offscreen 平台插件。构建使用 pkg-config、ar、Qt moc/rcc；QML 检查使用 qmlformat/qmllint，无需 CMake 或额外 Python 包。标准库模块仍需前述 GCC 16 工具链。

从仓库根目录检查依赖：

```bash
pkg-config --modversion Qt6Quick Qt6Qml Qt6QuickControls2 Qt6Test
qmake6 -query QT_INSTALL_LIBEXECS
qmake6 -query QT_INSTALL_BINS
qmake6 -query QT_INSTALL_QML
```

在 Fedora 可安装 `gcc-c++ libstdc++-devel qt6-qtbase-devel qt6-qtdeclarative-devel`，并确认 QML 运行模块与平台插件可用；使用本机匹配的 Qt 工具和库。Qt 在适配层、GUI 和 UI 测试中使用，不进入纯 C++ 核心库。

```bash
./build.sh month05
./run.sh month05/week04
./month05/week04/build/console-demo
python3 month05/week04/tools/builder.py test
python3 month05/week04/tools/builder.py check-format
python3 month05/week04/tools/builder.py configure
python3 month05/week04/tools/check_navigation.py
```

本周源码先编译命名模块与核心实现，归档为 `month05/weekXX/build/libcad_core.a`。console-demo 与 demo 链接同一个库，ui-tests 同时链接 Qt Test。QObject 接口使用 Qt 类型及私有 State 指针，实现文件才 import 核心模块；moc 无需解析 C++ 命名模块。`QT_NO_KEYWORDS` 避免 Qt 的 slots/signals/emit 宏与核心命名冲突，接口使用 Q_SIGNALS/Q_EMIT。QML 通过 rcc 嵌入程序，运行不依赖当前工作目录。GUI 与 UI 测试显式使用 Quick Controls Basic 样式，以保证深色配色和控件行为一致。四周 Qt 适配类各自使用 month05::weekXX::ui 命名空间，避免 clangd 全库索引把同名类跳转到另一周。

所有生成文件、标准库模块、目标文件和截图都在本周 `build/` 目录下。核心与 Qt 源码使用 -fPIC；全局使用的旧课程别名不变。Qt 工具通过 qmake6 查询安装路径，编译和链接参数通过 pkg-config 获取。本周 clangd 数据库包含 Qt 编译参数、生成的 moc/rcc 文件及本周核心模块；模块接口改变后仍需重新配置并重启 clangd。

每周 `builder.py test` 执行该周 ui-tests，自动设置 `QT_QPA_PLATFORM=offscreen` 与 `QT_QUICK_BACKEND=software`。UI 测试直接加载真实 QML，并用 Qt Test 发送点击、拖动、Escape 与平移事件，验证命令次数、模型值和界面警告。无需屏幕或测试框架安装；Qt Test 是 Qt 自带库。

Qt 周的 `builder.py check-format` 检查 C++ 与 qmlformat 的规范输出，并运行 qmllint，要求零警告。修正格式时可用本机 qmlformat --normalize -i；QML 的 `pragma ComponentBehavior: Bound` 明确声明嵌套代理使用外围组件 ID。导航检查增加第五个月属性枚举、命令提交、保存加载及 Qt 适配接口的定义跳转。

```bash
cd month05/week04 && QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/demo --screenshot build/editor.png
```

截图来自实际 QML 窗口；人工交互仍需图形会话。第五个月只接受 CAD_DOCUMENT 2，不迁移第四个月文件；读入错误保留当前文档，GUI 使用 QSaveFile 提交保存，保存失败不会覆盖原文件。选择、视图和预览仅在内存，重开文档清空历史。第四个月独立课程仍读写版本 1。

第五个月验收使用控制台演示、课程练习及四周 UI 测试。第一月第一周运行内嵌测试，第五、六月运行 UI 测试，其余课程无测试入口。Qt 周的 check-format（含 qmllint 零警告）、课程文档链接检查、各周跨模块导航检查通过。四周适配类与核心模块使用各自命名空间，Qt 适配接口能跳到本周实现；控制台的动态链接列表不包含 Qt。最终截图位于 `month05/week04/build/editor.png`。自动交互验证采用 offscreen 软件渲染，桌面 GPU/显示缩放行为需在实际图形会话中体验。


## 第六个月：整合、验证、性能与操作接口

沿用第五个月的 Linux/GCC 16/Clang 22/Qt 6.11.2/Python 工具链与安装检查。本次 Python 实测版本为 3.14.8。新增 `month06` 及 `month06/week01`–`week04` 目标；`all` 包括它们，旧编号和 `weekXX` 仍映射第一月。四周模块前缀为 month06.weekXX，Qt 适配类在本周 ui 命名空间，不链接其他课程产物。

```bash
./build.sh month06
./run.sh month06/week04
./month06/week04/build/console-demo
python3 month06/week04/tools/builder.py test
python3 month06/week04/tools/builder.py check-format
python3 month06/week04/tools/builder.py configure
python3 month06/week04/tools/check_navigation.py
./month06/week03/build/console-demo --benchmark
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./month06/week03/build/ui-tests --benchmark
cd month06/week04 && QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/demo --screenshot build/editor.png
```

构建输出仍为 demo、console-demo、ui-tests、libcad_core.a、模块与 moc/rcc 文件。Qt Test 验证适配层与 QML；核心行为按课程练习和控制台案例复核。QML 的实际输入事件、错误范围、拖动、保存状态和草稿隔离均自动验证。

性能默认使用教学构建的 -O0，无平台性能通过阈值；方法与实际结果在 [performance.md](../month06/week03/performance.md)。修改优化级别后应整套重建并记录环境，不能直接混用原记录。GUI benchmark 的渲染阶段包含截图读回；实际桌面 GPU 帧率需另测。

所有生成数据在本周 `build/` 中，控制台案例保存在本周 `build/demo.cad`。完整操作链、预期数字与异常恢复操作见[交付验收](../month06/week04/acceptance.md)。第六个月保存格式不变，公式草稿和接口预览不写入文件；GUI 文件替换成功后才更新保存基准。

第六个月验收包括四周控制台演示、真实 QML 测试、C++/QML 格式与 qmllint 检查、文档链接及全库定义跳转。首轮全库索引等待曾超时，重新运行后全部导航用例通过；如刚清空缓存，需等待索引完成。
