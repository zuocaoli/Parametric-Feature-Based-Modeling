# 第四个月第 3 周：模型生命周期

## 本周训练

- **承接与前置**：把[上周](../week02/README.md)的事务边界扩展到公式与对象生命周期；理解稳定 ID、变量映射和分配器高水位。
- **目标**：能撤销公式修改与对象删除并恢复依赖链；能证明历史持有身份和值，而非对象地址。
- **独立练习**：创建 A（120×60）与 B（30×15），绑定 B.width=`source_length/2` 并映射到 A.length；将 A.length 改为 144，删除 A，再撤销删除并重做。
- **验收**：修改后 B 为 width=72、area=2160；删除时为 MissingReference/Blocked，撤销恢复相同 A ID 与 72/2160，重做再次失效；另在撤销创建后新建对象，核对 ID 不复用。

## 71. 修改表达式的命令需要保存什么，才能在撤销时恢复公式与依赖关系？

BindFormula 的历史快照保存目标 PropertyKey、公式文本及显式 VariableMap，不能只保存文字。undo 解析旧文字，用同一映射重建依赖图，再按拓扑计算。原来未绑定的目标则恢复为普通输入；UnbindFormula 撤销时恢复原绑定和只读行为。语法、未知引用或循环错误只影响候选模型。

## 72. 如何撤销对象创建，并在重做时恢复相同的对象 ID？

CreateRectangle 创建对象并绑定 area=length*width；width 是独立输入。undo 恢复创建前快照，redo 恢复创建后快照，所以对象仍使用同一个 ID，而不是重新调用分配器。分配高水位不随撤销降低，分支上的新建对象得到新 ID；uint64 上限时拒绝分配，不能溢出回绕。

## 73. 如何撤销对象删除，同时恢复其属性和公式？

DeleteRectangle 记录删除前后的完整模型快照；undo 使用旧 ID 恢复所有带类型属性、对象自身公式及映射。图、AST 和内部订阅重新建立，恢复后只进行一轮重算。完整快照简化一致性，但复制和历史内存开销较大；大模型可进一步改为局部变更日志，本课不引入它。

## 74. 删除对象导致其他公式引用失效时，如何在撤销删除后恢复计算链？

其他对象公式继续保存旧 ID，直接使用者标 MissingReference，下游标 Blocked。恢复允许已删除对象的引用留在图中；undo 删除将原 ID 放回模型并统一重算，直接使用者和下游重新有效。同名或同尺寸的新对象使用新 ID，不会自动修复引用。

## 75. 命令长期保存在历史中时，如何避免它持有已经失效的对象指针？

命令参数和历史只有对象 ID、属性键及值快照，没有对象地址。每次操作从候选模型按 ID 查询；内部通知连接重新建立，文档观察者连接保持在稳定协调层。读取 API 返回的模型引用只用于当前操作间隙，不能跨提交保存；外部需要长期身份时使用 ID。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

自定义命令返回错误或抛出标准/非标准异常时，Document 都返回 EngineError 并丢弃候选，当前模型和历史保持不变，且不通知。标准异常保留 what()；非标准异常给出明确诊断。组合命令同时保留失败子命令序号。可用先修改候选再抛错的命令观察失败保护，再执行重做与继续编辑核对恢复。

[公式和生命周期命令](document.cpp)、[快照恢复与 ID 分配](engine.cpp)。模块自包含，继承第二周候选模型、组合事务和最终通知行为。不存在对象的删除、不存在绑定的解绑返回无变化；非法创建不消耗 ID。用户新绑定必须引用已有数值属性，历史恢复允许被删除对象引用，这两个入口不能混用。

环境：Linux、GCC 16.2.1、C++23、Python 3.14.7、clang-format/clangd 22.1.8，仅标准库，见[工具说明](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
python3 tools/builder.py run
python3 tools/builder.py check-format
```

预期演示输出：

```text
cross reference: length=100, width=10, area=1000, history=2/2
delete source: width=MissingReference, area=Blocked
undo deletion: length=100, width=10, area=1000, history=2/3
restored source id=2, area=60
replacement id=3, width=MissingReference
```

