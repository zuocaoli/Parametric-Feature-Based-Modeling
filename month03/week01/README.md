# 第三个月第 1 周：建立依赖关系

## 课程核心

把公式中的“引用了谁”转换为属性之间可查询、可更新的依赖图，为后续确定计算顺序和增量重算打基础。

主线是：**遍历 AST 提取变量 → 解析为对象 ID＋属性键 → 去重 → 同步维护前置与下游关系**。例如 `A.width = A.length / 2` 建立 `A.length → A.width`：从 width 能查到它依赖 length，从 length 能查到哪些属性受它影响。

关键是保证关系准确：区分不同对象的同名属性，替换公式时移除旧边，解除公式时只清除该属性的前置关系，保留别人对它的引用。本周只建立和维护关系；循环检测、计算顺序与实际重算由后续课程逐步加入。

## 本周训练

- **承接与前置**：[第二月末](../../month02/week04/README.md)暴露了手动计算的顺序问题；本周从已有 AST 提取关系，先复习集合、映射和邻接表。
- **目标**：能把变量名解析为对象 ID＋属性键；能维护前置与下游两份一致的关系表。
- **独立练习**：将 A/B 视为 ID 1/2，给 A.width 的 `length/2`、A.area 的 `length*width` 建图；给 B.width 的 `source_length/2 + source_length*0` 显式映射 source_length→A.length，列出各节点的前置与下游，再解除 B 的公式。
- **验收**：重复引用只形成一条 A.length→B.width 边，两个对象的 width 不混淆；解除只移除 B.width 的前置，A 的关系保持。此周验证关系，暂不创建和重算完整模型。

## 41. 如何从 AST 提取变量引用？

遍历节点，只收集 Variable 的 Token 文本，再递归访问左右子树。数字和运算符不产生依赖；一元、二元与括号对应的子树都被覆盖。沿用第二月 Parser 的有效 AST，不重新扫描字符串猜测变量。

代码：[collect_variables()/collect()](dependencies.cpp)。

## 42. 同一变量多次出现，如何去重？

变量名放进 set；解析为属性标识后再放进 KeySet。同名多次出现会去重，两个不同别名映射到同一属性也只保留一项。代价是有序集合插入成本，本课换取明确的唯一性和稳定输出。

代码：[collect_variables()/resolve_dependencies()](dependencies.cpp)。

## 43. 如何准确标识属性？

PropertyKey 保存对象 ID 与实际属性键，用默认比较支持有序容器。不同对象的 length 是不同节点；Length 显示标签不是 length 键。未映射变量默认指向公式所属对象，显式 VariableMap 可把 source_width 指向另一个对象。本周只解析标识，不查对象是否存在；对象与类型验证在整合周加入。

代码：[PropertyKey/VariableMap](dependencies.cppm)、[resolve_key()](dependencies.cpp)。

## 44. 如何表示“width 依赖 length”？

边统一为前置到目标：length → width。parents_[width] 存 length，children_[length] 存 width。图只记录数据关系，本周允许录入任意边，下一周加入循环校验；不把保存顺序当作计算顺序。

代码：[DependencyGraph](dependencies.cppm)、[replace_dependencies()](dependencies.cpp)。

## 45. 如何同时查询依赖与使用者？

dependencies(target) 查前置，dependents(source) 查使用者，两份邻接表同步维护。替换前先删旧反向边，再加入新关系；解除公式只清除前置，不删除别人对这个属性的引用。保留孤立节点，常量公式也有目标。代价是两份表与维护一致性的责任。

代码：[查询与 remove_formula()](dependencies.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

Lexer/Parser 是第二月的本周副本；dependencies 模块新增变量提取、属性标识和图。本周独立构建，只 import month03.week01 与 std，不执行公式。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps 22.1.8、C++23；IDE 与手动检查另需 clangd/clang-format 22.1.8、Python 3.14。编译器使用 `clang++`，标准库使用 GCC 16.2.1 提供的 libstdc++（含 `std` 模块开发文件）。无第三方库。安装这些工具后，在本课程目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

[CMakeLists.txt](CMakeLists.txt) 显式列出源码和 `CXX_MODULES` 接口；CMake 自动扫描依赖、构建 `std` 与本周模块，Ninja 按依赖增量编译。产物均在本周 `build/`，重复构建且源码未变时应显示 `ninja: no work to do.`。`import std` 使用 CMake 4.3 的实验支持，升级 CMake 主次版本时需检查对应启用标识。

`run.sh` 直接依次调用上述两条 CMake 命令，生成 clangd 编译数据库、构建模块，成功后运行 `build/demo`；任一步失败立即停止，脚本参数传给程序。从其他目录调用脚本也会自动切换到本周目录，构建与运行不调用 Python。原有 `builder.py build` 入口仍可使用。一键运行：

```bash
./run.sh
```

需要检查格式或验证 IDE 跳转时，单独执行：

```bash
python3 tools/builder.py check-format
python3 tools/check_navigation.py
```

CMake 通过预设中的 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 自动生成 `build/compile_commands.json`，clangd 直接读取它并生成后台索引；模块也由 CMake 构建，Python 无需另行生成数据库或预编译模块。`builder.py configure` 保留为 CMake 配置与构建的兼容入口，`check_navigation.py` 仅验证跳转。修改模块后重新构建，并在 IDE 中重启 clangd。已有 GCC 构建缓存时，首次切换先执行 `cmake --fresh --preset default`。IDE 通用说明见[工具说明](../../docs/tooling.md)，本周路径以本节为准。本周无自动测试入口，运行演示核对下列输出，再完成本周训练中的验收。

默认输出：

```text
variables: length source_width
dependencies 1.width: 1.length 2.width
dependents 1.width: 1.area
```

