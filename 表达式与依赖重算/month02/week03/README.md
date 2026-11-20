# 表达式引擎与依赖图·第二月第 3 周：增量重算

## 本周训练

- **承接与前置**：将[上周](../week02/README.md)的合法顺序接回属性、求值与通知；理解下游闭包、脏集合和同步回调。
- **目标**：能从输入变化推导受影响的公式；能解释批次内旧缓存、最终重算与通知重入的边界。
- **独立练习**：为 A（120×60）绑定 width=`length/2`、area=`length*width`，另建独立 B；打开批次，先后将 A.length 改为 132、144，记录输入、缓存和执行次数，关闭批次后再次赋 144。
- **验收**：批次内输入已变、派生缓存暂旧，关闭后 width/area 为 72/10368、各执行一次，B 不执行；重复输入不启动重算。解释批次为何不能回滚已经接受的输入。

## RecomputeEngine 的用途

[`RecomputeEngine`](engine.cppm) 是增量重算的协调器：**输入属性改变后，找出受影响的公式，按依赖顺序求值，并把结果写回属性。** 它将保存和校验属性的 `Rectangle/PropertyObject`、维护依赖关系的 `DependencyGraph`、计算表达式的 Evaluator 接成完整流程；具体实现见 [engine.cpp](engine.cpp)。

- **管理对象**：`create_rectangle()` 创建矩形、分配 ID 并订阅属性变化；`objects_` 保存对象与订阅连接。
- **绑定公式**：`bind()` 解析公式、检查引用和循环，将源码、语法树及别名映射存入 `bindings_`，更新 `graph_`，并安排目标及其下游计算。公式必须显式绑定；仅创建矩形不会自动计算面积。
- **响应修改**：`set()/set_many()` 提交输入；变化通知经 `collect_changes()` 将下游公式加入 `dirty_`，随后 `flush_if_ready()` 按拓扑顺序执行，每个受影响目标一轮最多计算一次。
- **合并修改**：`begin_batch()/end_batch()` 延迟到最外层批次关闭再重算；期间输入已更新，公式目标仍保存旧缓存。
- **提供查询并保护执行**：`stored_value()` 读取当前存储值，`last_report()` 查看公式执行结果；`writing_`、`recomputing_` 防止回调重入和公式写回触发重复调度。已绑定的目标由引擎写入，外部修改返回 `ReadOnly`。

例如 `main.cpp` 将 `segments` 设为 2，绑定 `width = length / segments` 和 `area = length * width`。当 `length` 从 100 改为 200 时，执行流程为：

```text
set(length, 200.0) → 属性变化通知 → dirty_ = {width, area}
                  → 算出并写回 width = 100 → 算出并写回 area = 20000
```

修改未被公式引用的 `name` 不会重算这些公式。`stored_value()` 只读取缓存，不触发求值；`set()` 返回输入修改结果，公式是否计算成功需查看 `last_report()`。

### Managed：把对象与变化订阅一起保存

`RecomputeEngine` 的私有嵌套结构体 `Managed` 表示一个受引擎管理的对象记录，作为 `objects_` 中每个 `ObjectId` 对应的值：

```cpp
struct Managed {
    std::shared_ptr<Rectangle> object;
    ChangeSignal::Connection connection;
};
```

- `object`：持有矩形对象，让它在引擎管理期间保持存活；`find_object(id)` 通过它取得矩形指针。
- `connection`：保存引擎对该矩形属性变化的订阅凭证。属性改变时，订阅回调调用 `collect_changes()`，将受影响的公式标脏。

`create_rectangle()` 创建矩形并订阅变化后，通过 `Managed{std::move(object), std::move(connection)}` 将两者一起放入 `objects_`。**必须保存连接**：`Connection` 析构会自动退订；如果它只作为局部变量存在，函数返回时就会断开，引擎之后便收不到该矩形的变化通知。

`Managed` 成员按声明的逆序析构，因此先销毁 `connection` 并退订，再销毁 `object` 这个智能指针、释放其持有的对象所有权。它本身不执行公式计算，负责将对象所有权和引擎订阅的生命周期组织在一起。

### objects_：按 ID 保存引擎管理的所有矩形

`std::map<ObjectId, Managed> objects_` 是引擎的对象登记表：键 `ObjectId` 是矩形的唯一标识，值 `Managed` 保存该矩形的智能指针及引擎对它的变化订阅。一个条目对应一个矩形，包含该矩形的全部属性。

- `create_rectangle()` 分配 ID，将新矩形和订阅连接插入 `objects_`，向调用者返回 ID。
- `find_object(id)` 根据 ID 查表，找到时返回矩形指针，未找到时返回 `nullptr`；读取属性、修改输入及公式结果写回都通过它定位对象。
- `objects_` 持有每个矩形及订阅，使它们在引擎管理期间持续有效；本周尚未提供单独删除对象的接口。

例如连续创建 A、B 后，表中保存 `A 的 ID → A 的 Managed`、`B 的 ID → B 的 Managed`。调用 `set({A 的 ID, "length"}, 200.0)` 时，先通过 `objects_` 找到 A，再修改 A 的 `length`。`objects_` 按对象 ID 登记矩形，`bindings_` 则按“对象 ID + 属性名”保存公式；同一个矩形可以有多个公式绑定。

`create_rectangle()` 中的登记语句将局部创建的对象及订阅交给引擎长期保存：

```cpp
objects_.emplace(id, Managed{std::move(object), std::move(connection)});
```

- `Managed{...}` 将矩形智能指针和订阅连接组成一条记录；`emplace` 以 `id` 为键，将该记录移动到 `objects_` 的新条目中。
- `std::move(object)` 使智能指针通过移动构造转移持有关系，原局部智能指针变空；矩形本身不被复制或搬动。
- `std::move(connection)` 转移订阅连接及退订责任，原局部连接变空；该类型禁止复制，必须移动。移动不会断开订阅。

成功插入后，函数返回时两个局部变量的析构不会销毁矩形或断开这项订阅；它们由表内的 `Managed` 继续管理。若省略此行，矩形没有登记到引擎中，局部连接离开作用域时也会自动退订。

### Binding：保存一个属性的公式定义

`RecomputeEngine` 的私有嵌套结构体 `Binding` 保存一个公式目标在后续重算时需要的信息：

```cpp
struct Binding {
    std::string source;
    Tree tree;
    VariableMap aliases;
};
```

- `source`：公式原文，例如 `"length * width"`；`formula(target)` 返回它。使用 `std::string` 持有文本，调用者传入的 `std::string_view` 不必一直存活。
- `tree`：已解析的抽象语法树（AST），类型为 `std::unique_ptr<Node>`。重算时直接交给 `evaluate()`，避免每次重新解析公式。
- `aliases`：公式变量名到 `PropertyKey`（对象 ID + 属性名）的映射，可引用其他对象的属性。未命中别名的变量默认指向公式目标所属对象的同名属性；保存的是引用关系，每次求值仍读取当前属性值。

### bindings_：按目标属性登记公式

`std::map<PropertyKey, Binding> bindings_` 是引擎的**公式绑定表**：键 `PropertyKey` 指定“哪个属性由公式驱动”（对象 ID + 属性名），值 `Binding` 保存“用什么公式、变量指向哪里”（原文、语法树及别名映射）。一个条目对应一个公式目标，同一矩形可以有多个条目。

例如 `bind({id, "area"}, "length * width")` 成功后，表中以 `{id, "area"}` 为键保存面积公式；重算时读取该矩形当前的 `length`、`width`，将计算结果写回 `area`。假设 `id` 为 1，并按 `main.cpp` 绑定两个公式，表的内容可概括为：

| 键：目标属性 | 值：对应的 Binding |
| --- | --- |
| `{1, "width"}` | `source = "length / segments"`、对应语法树、空别名表 |
| `{1, "area"}` | `source = "length * width"`、对应语法树、空别名表 |

`length` 和 `segments` 在此例中是输入，没有自己的公式，因此不作为绑定表的键出现。`{1, "area"}` 与 `{2, "area"}` 是不同的键，可分别登记两个矩形的面积公式。`std::map` 的键唯一，因此同一目标最多保存一份公式；成功再次绑定该目标时，`bind()` 通过 `insert_or_assign()` 替换原记录。

`bind()` 完成解析、引用和循环检查后才保存或替换记录；`formula(target)` 查表返回公式原文；`flush_if_ready()` 按依赖顺序查表，取出语法树和别名映射执行；`set_many()` 通过 `contains()` 识别公式目标并拒绝外部直接赋值。

`graph_` 保存依赖关系并安排顺序，`bindings_` 保存计算所需的公式定义，计算结果存放在目标对象的属性中。`std::map` 按键排序，其遍历顺序不代表重算顺序；实际执行顺序由依赖图的拓扑排序决定。

## 51. 如何找到直接和间接受影响的属性？

沿 children 邻接表求下游闭包，用 seen 避免菱形重复访问。结果不包含起点；绑定新公式时单独加入目标自身，常量公式也能首次计算。影响集合按属性身份区分对象，不按显示名称传播。

代码：[DependencyGraph::affected()](dependencies.cpp)、[mark_downstream()](engine.cpp)。

## 52. 如何标记需要计算的属性？

dirty_ 是公式目标的 KeySet；输入本身提供值，不进入执行列表。输入通知只把下游公式加入集合，多次修改和多条路径自然合并。引擎拥有对象和订阅，ID 单调分配；公开 stored_value 只读快照，不交出可绕过协调层修改的对象。

**`collect_changes(event)` 把“属性已经改变”的通知转换为“哪些公式需要重新计算”的记录。** `create_rectangle()` 注册的观察回调在属性变化时调用它。`event.source_id` 表示发生变化的对象，`event.changes` 列出本次实际改变的属性；函数遍历这些属性，组合出 `PropertyKey`，交给 `mark_downstream()` 查找直接、间接受影响的公式并加入 `dirty_`，集合自动去重。

例如绑定 `width = length / 2`、`area = length * width` 后，修改 `length`，它会将 `width`、`area` 标脏。它只收集待计算目标；实际求值和写回由后续的 `flush_if_ready()` 完成，因此可以在批次内合并多次修改。若 `recomputing_` 为 `true`，说明正在处理公式计算与写回，受影响的下游已安排，本次通知直接跳过，避免重复标脏。

`mark_downstream()` 有两个直接调用者：

- `collect_changes()`：收到属性变化事件后，对每个变化属性调用 `mark_downstream({event.source_id, change.name})`，将受影响的下游公式标脏。这是 `set_many()` 触发的通知路径。
- `bind()`：成功保存或替换公式后，先用 `dirty_.insert(target)` 将目标自身标脏，再调用 `mark_downstream(target)` 将依赖该目标的下游公式标脏。

代码：[引擎成员与接口](engine.cppm)、[collect_changes()/create_rectangle()](engine.cpp)。

## 53. 如何仅重算受影响公式？

对 dirty_ 诱导的子图排序，子图外前置使用当前值；随后用表达式引擎与依赖图·第一月 Evaluator 求值，通过同一映射读取本对象或跨对象属性。每个目标一轮最多执行一次；结果为 Unchanged 也不省略已经受影响的下游。set 返回输入修改结果，公式执行结果由 last_report 单独报告。

代码：[flush_if_ready()](engine.cpp)。

## 54. 如何合并多个输入修改？

`RecomputeEngine::set_many(object, updates)` 的目的是：**一次修改同一个对象的多个输入属性，并在这些修改全部提交、同步通知结束后，统一安排受影响公式的重算。** `object` 是对象 ID，`updates` 中每项包含属性名和新值；单项 `set()` 也复用这个入口。

例如矩形仅绑定 `area = length * width`，长、宽都可直接修改：

```cpp
engine.set_many(id, {{"length", 120.0}, {"width", 60.0}});
```

底层 `PropertyObject::set_many()` 先校验全部输入，通过后整体提交，并发送一次包含实际变化的通知；任一输入不合法则整次不提交。引擎还会预先拒绝对公式目标的直接赋值（`ReadOnly`），并用 `writing_` 防止通知回调再次修改引擎。通知把受影响的公式加入 `dirty_`；通知结束且没有打开批次时，按依赖顺序重算，上例的 `area` 只计算一次，得到 7200，避免先改长、再改宽造成两轮计算。

**`RecomputeEngine::set_many()` 通过同步通知间接触发标脏。** `create_rectangle()` 创建对象时已经注册回调：

```cpp
auto connection = object->observe([this](const ChangeEvent &event) { collect_changes(event); });
```

因此执行 `result = object->set_many(updates);` 时，实际调用链为：

```text
RecomputeEngine::set_many()
  → PropertyObject::set_many() 提交输入
    → changed_.emit(event) 同步调用已注册的回调
      → collect_changes(event) 遍历变化的属性
        → mark_downstream({event.source_id, change.name})
          → graph_.affected({source}) 找出直接、间接受影响的属性
          → 对其中已绑定公式的目标执行 dirty_.insert(key)
```

标脏发生在底层 `object->set_many()` 返回之前。此时 `writing_` 为 `true`，允许收集变化，同时防止回调再次修改引擎；等底层返回、`FlagGuard` 析构将 `writing_` 恢复为 `false` 后，外层才调用 `flush_if_ready()` 尝试重算。输入全部未变时，底层不发送通知，因此不会新增脏目标。

`begin_batch()/end_batch()` 可以进一步合并多次调用及多个对象的修改，支持嵌套，最外层结束才计算；批次内输入立即提交，派生数值暂时保持旧值。必须配对关闭批次，否则保持待计算状态。跨次修改不做全局回滚，公式重算失败也不撤销已接受的输入。

`set_many()` 返回输入修改的 `SetResult`，公式执行是否成功要看 `last_report()`。返回 `Changed` 或 `Unchanged` 都会尝试处理已有的脏集合；后者可重试上轮失败遗留的任务，若没有待计算目标则不执行公式。

代码：[批次接口及实现](engine.cppm)、[begin_batch()/end_batch()/set_many()](engine.cpp)。

## 55. 如何避免通知再次启动重算？

`recomputing_` 表示**引擎当前是否正在执行一轮公式求值和结果写回**，包括写回触发的同步通知。`flush_if_ready()` 排好执行顺序后，通过 `FlagGuard guard{recomputing_}` 将它设为 `true`；离开函数时由守卫析构恢复为 `false`，提前停止或异常退出也会恢复。它表示当前执行状态；待计算目标由 `dirty_` 记录，因此批次内可以有脏目标而 `recomputing_` 仍为 `false`。

它有两个作用：

- **跳过公式写回引起的重复标脏**：例如 `length` 改变后，`width`、`area` 已一起标脏并按顺序安排。计算并写回 `width` 时仍会发送变化通知，但 `collect_changes()` 看到 `recomputing_ == true` 就直接返回；`area` 会在当前轮随后计算，无须再次加入脏集合。外部观察者仍能收到写回通知。
- **阻止通知回调重入修改引擎**：`busy()` 返回 `writing_ || recomputing_`。公式写回的通知回调中再次调用 `set()/set_many()` 会返回 `Reentrant`；绑定公式、开关批次等修改入口也会拒绝，保护当前执行顺序和公式定义。

`writing_` 保护输入修改及其同步通知阶段，此时允许收集变化、标记下游；输入通知结束、`writing_` 恢复后才开始重算，由 `recomputing_` 保护公式计算与写回阶段。这两个标志用于单线程同步回调的重入保护，不提供线程同步。绑定目标由引擎控制，外部正常调用 `set()` 修改它会返回 `ReadOnly`。

代码：[FlagGuard/set_many()/collect_changes()](engine.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

本周独立复制 Lexer/Parser/依赖图和表达式引擎与依赖图·第一月 Property、存储型 Rectangle、Evaluator。新增 engine 模块拥有矩形、绑定、图、脏集合与执行报告，所有 import 限于本周。未知或非数值引用、循环在绑定阶段拒绝；遇到求值或写回失败先停止当前轮，保留剩余脏集合，第四周再完善长期状态、无关分支继续与对象删除。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
clang-format --dry-run --Werror *.cpp *.cppm
```

默认输出：

```text
initial: width=50, area=5000
order: 1.width(count=1,Changed) 1.area(count=1,Changed)
width=100, area=20000
unrelated executions=0
during batch: width=100, area=20000
order: 1.width(count=1,Unchanged) 1.area(count=1,Changed)
after batch: width=100, area=40000
```

