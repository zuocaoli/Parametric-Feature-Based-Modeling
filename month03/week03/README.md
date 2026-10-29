# 第三个月第 3 周：增量重算

## 本周训练

- **承接与前置**：将[上周](../week02/README.md)的合法顺序接回属性、求值与通知；理解下游闭包、脏集合和同步回调。
- **目标**：能从输入变化推导受影响的公式；能解释批次内旧缓存、最终重算与通知重入的边界。
- **独立练习**：为 A（120×60）绑定 width=`length/2`、area=`length*width`，另建独立 B；打开批次，先后将 A.length 改为 132、144，记录输入、缓存和执行次数，关闭批次后再次赋 144。
- **验收**：批次内输入已变、派生缓存暂旧，关闭后 width/area 为 72/10368、各执行一次，B 不执行；重复输入不启动重算。解释批次为何不能回滚已经接受的输入。

## 51. 如何找到直接和间接受影响的属性？

沿 children 邻接表求下游闭包，用 seen 避免菱形重复访问。结果不包含起点；绑定新公式时单独加入目标自身，常量公式也能首次计算。影响集合按属性身份区分对象，不按显示名称传播。

代码：[DependencyGraph::affected()](dependencies.cpp)、[mark_downstream()](engine.cpp)。

## 52. 如何标记需要计算的属性？

dirty_ 是公式目标的 KeySet；输入本身提供值，不进入执行列表。输入通知只把下游公式加入集合，多次修改和多条路径自然合并。引擎拥有对象和订阅，ID 单调分配；公开 stored_value 只读快照，不交出可绕过协调层修改的对象。

代码：[引擎成员与接口](engine.cppm)、[collect_changes()/create_rectangle()](engine.cpp)。

## 53. 如何仅重算受影响公式？

对 dirty_ 诱导的子图排序，子图外前置使用当前值；随后用第二月 Evaluator 求值，通过同一映射读取本对象或跨对象属性。每个目标一轮最多执行一次；结果为 Unchanged 也不省略已经受影响的下游。set 返回输入修改结果，公式执行结果由 last_report 单独报告。

代码：[flush_if_ready()](engine.cpp)。

## 54. 如何合并多个输入修改？

begin_batch/end_batch 显式管理批次，支持嵌套，最外层结束才计算；批次内输入立即提交，派生数值暂时保持旧值。set_many 保留单对象的先校验后整体提交规则；跨次修改不做全局回滚。必须配对关闭批次，否则保持待计算状态。

代码：[批次接口及实现](engine.cppm)、[begin_batch()/end_batch()/set_many()](engine.cpp)。

## 55. 如何避免通知再次启动重算？

协调层在底层 set_many 的同步通知结束后才计算，避开原 PropertyObject 的 Reentrant 保护。recomputing_ 期间忽略公式写回通知，因为下游已排入当前轮；writing_ 或 recomputing_ 期间拒绝回调再次修改、绑定或开批次，避免递归与执行中改图。绑定目标由引擎控制，外部 set 返回 ReadOnly。

代码：[FlagGuard/set_many()/collect_changes()](engine.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

本周独立复制 Lexer/Parser/依赖图和第二月 Property、存储型 Rectangle、Evaluator。新增 engine 模块拥有矩形、绑定、图、脏集合与执行报告，所有 import 限于本周。未知或非数值引用、循环在绑定阶段拒绝；遇到求值或写回失败先停止当前轮，保留剩余脏集合，第四周再完善长期状态、无关分支继续与对象删除。

环境：Linux、GCC 16.2.1（含 libstdc++ std 模块）、C++23、Python 3.14.7、Clang/clangd/clang-format 22.1.8；仅标准库，见[工具说明](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
./run.sh month03/week03
python3 tools/builder.py check-format
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

