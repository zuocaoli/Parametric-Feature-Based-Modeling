# 文档与属性系统·第二月第 4 周：保存与加载

## 本周训练

- **承接与前置**：将[上周](../week03/README.md)可恢复的权威内容写入文档文件；理解版本、流错误和分阶段重建。
- **目标**：能分辨必须保存的数据与可重建缓存；能验证加载顺序无关以及坏文件不会替换当前文档。
- **独立练习**：保存 A.length=144 与 B.length=30、B.width=`source_length/2`→A.length 的文档；重排完整对象记录的顺序后加载，再尝试截断文件，最后重新加载有效文件并把 A.length 改为 160。
- **验收**：有效重排文件得到 B 的 72/2160；坏文件保留当前内容与历史；修复加载后可继续编辑得到 80/2400，历史从加载后的新起点建立。

## 月末综合验收与衔接

独立完成“创建 A/B → 跨对象公式 → 组合修改 → 撤销/重做 → 保存加载 → 继续编辑”，将练习文件放在本周 `build/` 目录。再保存一个合法编辑导致除零的模型，加载后核对 Failed/Blocked，修复输入后恢复；解释为何磁盘只保存权威内容，而内存历史可携带更多快照数据。

此时控制台已有可撤销、可保存的参数化文档。[文档与属性系统·第三月](../../month03/week01/README.md)将让 QML 显示和编辑同一条模型链，加入 x/y 并使用版本 2；用新阶段接口重建 A/B，版本 1 文件不自动迁移。

## 76. 为了完整恢复参数化文档，文件必须保存哪些数据？

保存对象类型、稳定 ID、ID 分配高水位、未绑定属性的类型和值，以及公式目标、文本和显式变量映射。高水位包含已删除对象使用过的 ID，避免加载后新建对象意外接管失效引用。固定 Rectangle 属性定义和校验来自程序，不重复写入文件。撤销历史只在内存，成功打开文件后从空历史开始。

## 77. 属性计算结果、依赖图和脏标记，哪些需要保存，哪些可以重建？

未绑定属性是权威输入，必须保存；绑定目标的数值是缓存，不保存。AST、依赖图、脏标记和计算状态都重建，加载最后统一计算。因此成功结果可恢复，Failed/Blocked/MissingReference 也会重新产生；失败公式加载后的缓存使用默认占位值，不保证与保存前相同，value 仍禁止读取无效结果。

## 78. 如何分阶段加载对象、属性和公式，使引用不受文件记录顺序影响？

先解析全部记录，检查属性完整性；再创建所有对象并恢复输入；随后解析公式和映射，建立完整图并拒绝循环；最后进行一轮拓扑计算。对象或公式记录可以任意排列。恢复入口允许引用已分配但被删除的对象 ID；已有对象上的未知属性、非数值引用及从未分配过的 ID 属于文件错误。

## 79. 如何在文件损坏或加载失败时，保留当前已经打开的文档？

所有工作在独立候选模型完成，只有结构校验成功才调用 Document::replace_model，发布一个文档替换事件并清空历史。截断、重复 ID、非法类型/输入、公式语法或循环、尾随数据等错误不改变当前模型、历史位置、重做分支和观察者。合法模型的重算失败可以加载，错误状态不等于损坏文件。

## 80. 如何为文件设置格式版本，并明确处理不支持的版本？

标头固定为 CAD_DOCUMENT 1，解析器先核对标识和版本，再读取记录。未知版本明确拒绝，不能尝试套用旧规则或偷偷忽略字段。本课不迁移表达式引擎与依赖图·第一月的 CAD_RECTANGLE 1；未来格式改变应增加版本并提供显式转换。

## 格式和接口

[保存/加载接口](storage.cppm)、[文本格式实现](storage.cpp)、[文档替换](document.cpp)。save_document 接受 const Document 与 ostream；load_document 接受当前 Document 与 istream，返回 expected<void, EngineError>，计算结果在文档状态和报告中读取。save 不改变历史。

格式示例（初始矩形；width、area 结果没有存入 PROPERTY）：

```text
CAD_DOCUMENT 1
HIGH_WATER 1
OBJECTS 1
OBJECT rectangle 1
VALUES 4
PROPERTY "length" double 100
PROPERTY "segments" integer 1
PROPERTY "visible" bool 1
PROPERTY "name" string "Rectangle"
FORMULAS 2
FORMULA 1 "area" "length * width"
ALIASES 0
FORMULA 1 "width" "length / 2"
ALIASES 0
END
```

每个对象后有 VALUES 数量；每个公式后有 ALIASES 数量，映射记录为 `ALIAS "变量名" 对象ID "属性键"`。数字用经典 locale 与 max_digits10；字符串用 std::quoted，可恢复引号、反斜杠和换行。数量字段上限 100000；类型标签仅 double、integer、bool（0/1）、string。输入属性和公式目标必须共同覆盖固定六个属性，且不重复。ID 为非零 uint64；高水位可以为零表示从未分配过对象。

内存历史仍保留完整快照，磁盘只存权威数据。仅支持单线程和一层组合事务；replace_model 是明确的文档替换入口，调用者必须提供已校验且已重算的候选模型，不是普通编辑入口。保存报告流写入错误，但不提供文件替换原子性或崩溃恢复；演示显式检查 flush。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

自定义命令返回错误或抛出标准/非标准异常时，Document 都返回 EngineError 并丢弃候选，当前模型和历史保持不变，且不通知。标准异常保留 what()；非标准异常给出明确诊断。组合命令同时保留失败子命令序号。可用先修改候选再抛错的命令观察失败保护，再执行重做与继续编辑核对恢复。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
clang-format --dry-run --Werror *.cpp *.cppm
```

演示写入仓库的 `文档与属性系统/month02/week04/build/document.txt`，重新打开后继续修改跨对象公式的输入：

```text
before save: length=100, width=10, area=1000, history=2/2
saved 文档与属性系统/month02/week04/build/document.txt
loaded: length=100, width=10, area=1000, history=0/0
continued edit: length=100, width=20, area=2000, history=1/1
rejected load: unsupported document version
preserved: length=100, width=20, area=2000, history=1/1
```

