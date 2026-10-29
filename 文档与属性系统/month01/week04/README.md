# 第四周：对象管理与整合

## 本周训练

- **承接与前置**：把[上周](../week03/README.md)带通知的对象交给 Document 管理；会使用映射、weak_ptr::lock，并区分对象身份和显示名称。
- **目标**：能通过稳定 ID 创建、查找和删除对象；能解释文档所有权、临时强引用与自动退订的协作。
- **独立练习**：创建 A（120×60）和 B（30×15），记录 ID；将它们改为同名，批量修改 A 为 144×72，保存其弱引用，释放临时强引用后删除 A，再创建 C。
- **验收**：同名不影响 ID 查找，A 面积为 10368、B 为 450；删除后查找为空且弱引用失效，C 不复用 A 的 ID。按本周演示方式核对。

## 月末综合验收与衔接

独立完成“创建 A/B → 通用打印 → 修改 A → 拒绝非法输入 → 退订 → 删除”的流程，记录 ID、面积和通知次数；非法修改后再提交合法值，验证对象仍能编辑。解释稳定 ID 与弱引用各解决什么问题，以及快照读取的复制代价。

此时 area 按需计算，width 与 length 的比例仍需手工维护。[表达式引擎与依赖图·第一月](../../../表达式引擎与依赖图/month01/week01/README.md)将把 `length/2` 变成可解析、可绑定的公式；沿用这两个对象的概念，先在单矩形 A 上建立表达式能力。

## 16. 如何建立不随显示名称改变的身份？

由文档分配递增整数 ID，名称只是普通属性。ID 在文档生命周期内唯一且删除后不复用；名称可以重复。代价是维护计数器；跨文档、跨进程唯一性和持久化不在本例范围。

代码位置：[ID 定义](property.cppm#L6)、[文档分配 ID](document.cpp#L8)。

## 17. 文档如何统一管理对象？

`Document` 是本周对象的容器和 ID 分配者。`create_rectangle(length, width)`（默认 5×3）和 `create_annotation(text)` 创建对象并返回从 1 递增的 `ObjectId`；私有的 `create<T>()` 共用分配、构造和入表流程。`objects_` 用 `map<ObjectId, shared_ptr<PropertyObject>>` 按 ID 保存不同类型的对象，文档持有其强引用。初始参数不合法时构造会抛出异常，对象不会入表，也不会消耗 ID。

`find(id)` 返回 `weak_ptr<PropertyObject>`：调用者先 `lock()`，成功后才能访问；未知 ID 返回空弱引用。`erase(id)` 从表中移除对象并返回是否找到，若外部仍持有 `shared_ptr`，对象会在最后一个强引用释放时销毁。`ids()` 返回当前 ID 列表，按 `map` 的键升序排列；删除的 ID 不再出现，也不会被后续创建复用。`Document` 禁止复制，避免无意复制对象表和 ID 分配状态。名称只是对象属性，可以重复或修改，不影响这些操作。

在 `demo.cpp` 中，`create_rectangle()` 使用默认长 5、宽 3，创建第一个矩形；`create_rectangle(2.5, 4.0)` 创建长 2.5、宽 4 的第二个矩形；`create_annotation("Use properties, not concrete types")` 创建文本为该字符串的注释。三个 `const auto` 变量保存返回的 `ObjectId`，并非对象本身。在新建的空文档里，它们依次是 1、2、3；之后可用这些 ID 调用 `find()` 或 `erase()`。

私有的 `create<T>(args...)` 以类型参数包转发构造参数，统一完成 ID 分配、对象构造和入表；矩形与注释对象因此共用同一套生命周期规则。

`std::forward<Args>(args)...` 把每个构造参数按传入 `create` 时的左值或右值性质继续转发。`Args` 是类型参数包，`args` 是对应的函数参数包。`std::forward<A>(a)` 中，`<A>` 指定类型，`(a)` 传入形参；本例中可理解为 `static_cast<A&&>(a)`。末尾的 `...` 展开整个表达式：若有两组参数，就得到 `std::forward<A0>(a0), std::forward<A1>(a1)`。

`Args&&... args` 使用转发引用，编译器从实参推导类型；有名字的形参表达式在函数体内都是左值，因此需要根据推导出的类型进行转发：

| 传入 `create` 的表达式 | 对应的 `Args` 类型 | 形参类型（引用折叠后） | `std::forward` 的结果 |
|---|---|---|---|
| 左值 `text` | `std::string&` | `std::string&` | 左值 |
| 右值 `std::move(text)` | `std::string` | `std::string&&` | 右值 |

`create<Rectangle>(length, width)` 的两个实参都是左值，展开后相当于：

```cpp
std::make_shared<Rectangle>(
    std::forward<double&>(length), std::forward<double&>(width), id);
```

`create<Annotation>(std::move(text))` 只有一个实参，展开后相当于：

```cpp
std::make_shared<Annotation>(std::forward<std::string>(args), id);
```

这里的 `args` 表示唯一的形参。前者保持左值，后者将有名字的形参转为右值，让接收文本的构造函数使用移动构造。`std::forward` 本身只转换表达式的值类别，实际复制或移动由接收方决定；`id` 在参数包展开后另行追加。

代码位置：[Document 接口与对象表](document.cppm#L7)、[创建](document.cpp#L8)、[查找](document.cpp#L27)、[删除](document.cpp#L32)。

## 18. 删除后如何避免失效引用？

外部保存 `weak_ptr`，每次使用前 `lock()`，成功才访问。演示中的 `handle` 是指向第一个矩形的弱引用；`auto object = handle.lock()` 尝试取得 `shared_ptr<PropertyObject>`：对象还存在时得到强引用，对象已销毁时得到空指针。写在 `if` 条件里时，只有取得强引用才进入代码块，且 `object` 在该语句结束前让对象保持存活。删除会移除文档所有权；已有临时 `shared_ptr` 会延长对象生命，直到释放才真正销毁。安全访问不等于删除后立即销毁，长期持有强引用会延迟释放。

代码位置：[查找](document.cpp#L54)、[删除](document.cpp#L59)、[弱引用使用示例](demo.cpp#L21)。

## 19. 如何不针对矩形编写特殊打印逻辑？

打印器只接收 `PropertyObject`，枚举属性并访问 variant 值。本例同一个函数打印矩形和注释对象，不使用类型判断；新增对象遵循公共接口即可复用。

派生对象在构造时通过受保护的 `PropertyObject::add()` 注册属性，统一保存值、显示信息和读写规则；外部随后通过 `get()`、`properties()`、`set()` 和 `set_many()` 访问。

| `add()` 参数 | 作用 |
|---|---|
| `name` | 属性查找键，如 `"length"`；同一对象内必须唯一 |
| `label` | 显示标签，如 `"Length"` |
| `unit` | 单位，如 `"mm"`；空字符串表示无单位 |
| `value` | 初始 `Value`；计算属性中用作类型占位 |
| `validate` | `Validator = std::function<bool(const Value&)>`，检查初值及后续修改是否有效 |
| `compute` | `Reader = std::function<Value()>`，每次读取时计算值；提供后属性自动成为只读 |

两个回调的默认值 `{}` 表示空的 `std::function`：未提供校验器就跳过该校验，未提供计算器就读取存储值。例如 `add("length", "Length", "mm", length, positive_size)` 注册可编辑的长度，并要求有限且大于 0；`add("area", "Area", "mm^2", 0.0, {}, [this] { return Value{std::get<double>(*get("length")) * std::get<double>(*get("width"))}; })` 注册按需计算的只读面积，`0.0` 作为占位值。

`add()` 先检查名称重复和初值合法性，失败时抛出 `std::invalid_argument`，成功后追加到 `properties_`；返回类型为 `void`，注册过程不发出修改通知。面积在读取时使用当前长宽计算，直接赋值会得到 `SetResult::ReadOnly`。

代码位置：[add() 声明](property.cppm#L102)、[add() 实现与读取](property.cpp#L133)、[矩形属性注册](rectangle.cpp#L13)、[print_properties()](printer.cpp#L7)、[通用调用](demo.cpp#L45)。

## 20. 如何证明所有机制正确协作？

创建两个矩形和一个注释，执行合法、非法、重复及批量修改，再通用打印并删除对象。核对错误和重复赋值不通知、批量修改后面积为 80、退订后 view 次数保持 2、删除后弱引用失效，判断各机制是否正确协作。

代码位置：[完整演示](demo.cpp#L8)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

代码使用 C++ 命名模块，接口位于 `.cppm`，实现位于 `.cpp`，通过 `import` 引用。类型和函数位于 `week04` 命名空间，本周独立构建。

| 文件 | 职责 |
|---|---|
| [property.cppm](property.cppm)、[property.cpp](property.cpp) | `week04.property`：对象 ID、属性、事件与批量修改 |
| [change_signal.cpp](change_signal.cpp) | 同属 `week04.property`：订阅连接、自动退订与通知分发 |
| [rectangle.cppm](rectangle.cppm)、[rectangle.cpp](rectangle.cpp) | `week04.rectangle`：矩形尺寸与派生面积 |
| [annotation.cppm](annotation.cppm)、[annotation.cpp](annotation.cpp) | `week04.annotation`：注释对象 |
| [document.cppm](document.cppm)、[document.cpp](document.cpp) | `week04.document`：分配 ID、创建、查询和删除对象 |
| [printer.cppm](printer.cppm)、[printer.cpp](printer.cpp) | `week04.printer`：通过公共属性接口打印对象 |
| [demo.cppm](demo.cppm)、[demo.cpp](demo.cpp) | `week04.demo`：对象管理与通知的整合演示 |
| [main.cpp](main.cpp) | 只调用 `week04::demo()` |

通知连接与属性对象共用模块声明、分开实现，避免循环依赖。事件携带稳定 `source_id`，来源指针仅在回调中借用。直接构造对象时默认 ID 为 0，文档管理的对象从 1 开始。沿用第三周单线程、禁止重入写入及回调异常隔离的约定；没有 UI、依赖图或持久化。

环境见[公共工具说明](tools/README.md)。在本课程目录执行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
clang-format --dry-run --Werror *.cpp *.cppm
```

演示先打印以下变化，再通过统一接口列出三个对象的属性：

```text
created ids=1,2,3
[id=1] length=5->10
view area=30
invalid=InvalidValue, repeated=Unchanged
[id=1] length=10->20 width=3->4
view area=80
[id=1] width=4->5
log calls=3, view calls=2
```

最后输出 `deleted id=1, expired=true`。通过核对以上输出验证演示；`./run.sh` 配置并构建本周后运行程序。
