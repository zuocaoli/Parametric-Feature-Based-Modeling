# 第二周：属性抽象

## 本周训练

- **承接与前置**：沿用[上周](../week01/README.md)的合法尺寸与统一修改入口，将具体 setter 提升为属性接口；先复习 variant、optional、visit 和 lambda。
- **目标**：能不依赖矩形类型枚举和读取属性；能解释类型、只读与取值校验分别阻止什么错误。
- **独立练习**：用统一属性接口将 A（120×60）的 length 改为 144，再分别提交整数类型的 length、负 width 和只读 area，最后查询未知名称。
- **验收**：合法修改后面积为 8640；后三次修改分别返回 TypeMismatch、InvalidValue、ReadOnly，未知查询为空，模型保持合法状态；按本周演示方式核对。

## 6. 不知道具体类型，如何枚举属性？

通过公共 `PropertyObject::properties()` 获取属性快照，无需知道对象是否为矩形。统一接口便于工具遍历；快照有复制成本，但不能绕过修改入口。

代码位置：[properties()](property.cpp#L39)。

## 7. 属性除了数值，还需要什么信息？

至少有稳定名称、显示标签、类型、单位和是否只读。名称用于查找，其他信息帮助展示和编辑；取值约束由校验函数执行，不把任意规则硬编码进界面。

代码位置：[展示用 PropertyView](property.cppm#L31)、[内部 Property](property.cppm#L63)。

## 8. 如何统一表示四种属性类型？

用 `std::variant<double, std::int64_t, bool, std::string>` 保存值，通过 `std::visit` 读取。类型明确，但增加新类型需要扩展 variant；字符串显式构造为 `std::string`。

`using Value = ...` 为这个 variant 类型取别名 `Value`，表示“属性值类型”，不是变量。一个 `Value` 对象一次保存其中一种类型的值，例如 `Value length = 5.0;` 保存浮点数，`Value visible = true;` 保存布尔值。

代码位置：[Value 类型](property.cppm#L14)、[std::visit 读取](property.cpp#L29)。

## 9. 按名称查找时，如何报告不存在？

`get(name)` 返回 `std::optional<Value>`，空值表示不存在；`set` 返回 `NotFound`。不偷偷创建属性，也不用合法数值代替错误。

代码位置：[get()](property.cpp#L49)、[set()](property.cpp#L58)。

## 10. 统一修改时，如何保证类型和取值合法？

依次检查名称、只读限制、精确类型、取值规则，再比较旧值并赋值；错误不改变对象。尺寸须有限且大于零；面积通过读取函数计算，只读。代价是每次修改经过统一检查。

代码位置：[统一修改与校验](property.cpp#L58)、[矩形取值规则](rectangle.cpp#L7)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

代码使用 C++ 命名模块：`.cppm` 通过 `export module` 声明模块并导出接口，`.cpp` 通过 `module` 实现接口，使用方通过 `import` 引用。标准库使用 `import std;`。

| 文件 | 职责 |
|---|---|
| [property.cppm](property.cppm)、[property.cpp](property.cpp) | `week02.property`：属性类型和通用属性对象 |
| [rectangle.cppm](rectangle.cppm)、[rectangle.cpp](rectangle.cpp) | `week02.rectangle`：矩形属性、尺寸校验和面积；再导出属性模块 |
| [demo.cppm](demo.cppm)、[demo.cpp](demo.cpp) | `week02.demo`：属性打印与演示流程 |
| [main.cpp](main.cpp) | 直接调用 `week02::demo()` |

例如入口使用 `import week02.demo;` 后调用 `week02::demo()`。接口位于 `week02` 命名空间，避免与其他周的同名类型和函数混淆。CMake 根据模块依赖调度接口、实现和入口的编译与链接。

矩形还提供整数 segments、布尔 visible、字符串 name，以演示四种类型。属性读取返回值副本，不暴露内部可写引用；默认值也经过校验。派生属性读取函数捕获对象，因此禁用对象复制和移动。

环境见[公共工具说明](tools/README.md)。在本课程目录构建并运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
```

[CMake 构建定义](CMakeLists.txt) 只使用本周源码，产物放在本周 `build/` 下。

在本周目录执行 `./run.sh` 可配置、构建并运行本周程序；IDE 使用原生 clangd 读取本周编译数据库。`main()` 只调用 `week02::demo()`。

演示先列出六个属性，随后显示：

```text
missing: NotFound
wrong type: TypeMismatch
invalid size: InvalidValue
read-only: ReadOnly
set length: Changed
new area=30
```

运行后核对以上输出，确认属性枚举、错误报告和面积更新符合预期。
