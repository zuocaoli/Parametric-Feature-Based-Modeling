# 第三周：修改通知

## 本周训练

- **承接与前置**：[上周](../week02/README.md)完成统一属性读写，本周为成功修改添加通知；先复习回调、shared_ptr/weak_ptr、移动语义与 RAII。
- **目标**： 能预测多订阅者的通知次数和读取状态；能用连接生命周期与批量提交避免失效回调和中间状态。
- **独立练习**：给 A（120×60）保存两份订阅连接；批量改为 144×72，再重复提交并提交负 width；断开其中一份连接后把 length 改为 160，先列出每次回调的读值。
- **验收**：首次批量各通知一次且面积为 10368，重复与非法提交均不通知；最后仅剩余订阅收到一次通知、面积为 11520。通过演示核对，并解释回调重入为何被拒绝。

## 11. 如何让其他模块知道哪个对象的哪个属性变了？

对象在合法且实际改变的提交后发送 `ChangeEvent`，其中 `source` 指向本次修改的对象，`changes` 列出属性名与新旧值。来源指针只在同步回调期间借用；下周引入稳定 ID。一次批量修改可在同一事件中包含多条变化。

代码位置：[事件类型](property.cppm#L36)、[事件构造](property.cpp#L85)。

## 12. 通知应该在赋值之前还是之后？

校验和赋值完成后再通知，让订阅者读到提交后的状态；拒绝或重复赋值不通知。通知期间 `NotifyGuard` 用 RAII 设置并恢复 `notifying_`，同一对象的回调写入返回 `Reentrant`，避免递归通知。

代码位置：[重入保护与提交](property.cpp#L74)、[通知](property.cpp#L148)。

## 13. 两个模块如何分别收到通知？

每次 `observe()` 建立独立订阅，`log` 和 `view` 因而都能接收同一事件。发送时遍历订阅快照：新订阅从下次通知生效，已断开的订阅立即跳过；回调异常计入 `notification_errors_`，其他订阅继续执行，已提交修改不回滚。代价是保存订阅状态并复制一次指针列表。

代码位置：[订阅与发送](change_signal.cpp#L30)、[双订阅演示](demo.cpp#L10)。

## 14. 订阅模块销毁后，如何避免失效回调？

保存不可复制、可移动的 `Connection`。它弱引用订阅状态，析构或调用 `disconnect()` 时退订；发布者先销毁也安全。若不保存返回的连接，临时连接会立即析构并退订。

代码位置：[连接断开](change_signal.cpp#L16)、[作用域退订](demo.cpp#L23)。

## 15. 连续修改长宽，如何避免处理中间状态？

使用 `set_many()` 先校验全部输入，再整体提交，并发送一次包含所有变化的事件。任一输入非法则全部保持原值；空批次、全部未变不通知。同一批次的重复名称视为错误，避免覆盖顺序含糊。

代码位置：[接口](property.cppm#L131)、[实现](property.cpp#L72)、[演示](demo.cpp#L43)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

代码使用 C++ 命名模块，接口位于 `.cppm`，实现位于 `.cpp`，通过 `import` 引用。所有类型和函数位于 `week03` 命名空间，本周独立构建。

| 文件 | 职责 |
|---|---|
| [property.cppm](property.cppm)、[property.cpp](property.cpp) | `week03.property`：属性、事件、校验与批量提交 |
| [change_signal.cpp](change_signal.cpp) | 同属 `week03.property`：订阅连接、自动退订与通知分发 |
| [rectangle.cppm](rectangle.cppm)、[rectangle.cpp](rectangle.cpp) | `week03.rectangle`：矩形属性与派生面积，再导出属性模块 |
| [demo.cppm](demo.cppm)、[demo.cpp](demo.cpp) | `week03.demo`：修改通知演示 |
| [main.cpp](main.cpp) | 直接调用 `week03::demo()` |

通知连接与属性对象共用模块声明、分开实现，避免事件来源指针带来模块循环依赖。事件只列直接修改的属性，面积由订阅者读取当前尺寸计算。模型为单线程；回调写回同一模型返回 `Reentrant`，避免递归通知。回调异常被隔离，其他订阅继续，累计错误可通过 `notification_errors()` 查看，不回滚已提交数据。

环境见[公共工具说明](../../docs/tooling.md)。在本课程目录执行：

```bash
./build.sh 03
./run.sh 03
python3 tools/builder.py check-format
```

演示输出：

```text
 01 
changed: length=5->10
view area=30
 02 
 03 
 04 
 05 
 06 
 07 
changed: length=10->20 width=3->4
view area=80
 08 
 09 
changed: width=4->5
 10 
final area=100
```

运行后核对以上输出，确认单次与批量修改的通知、相同值和非法值不通知，以及 view 退订后只通知 log 的行为。
