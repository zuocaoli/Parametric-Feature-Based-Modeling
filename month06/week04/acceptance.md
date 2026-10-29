# 第六个月交付验收

所有命令从仓库根目录执行。依赖与版本见[工具说明](../../docs/tooling.md)，本周五个问题的答案见 [README](README.md)。无在线服务、模型 API Key 或其他周的编译产物依赖。

## 构建与启动

```bash
pkg-config --modversion Qt6Quick Qt6Qml Qt6QuickControls2 Qt6Test
./build.sh month06
./build/month06/week04/console-demo
./run.sh month06/week04
for w in month06/week0[1-4]; do python3 "$w/tools/builder.py" test || exit 1; done
for w in month06/week0[1-4]; do python3 "$w/tools/builder.py" check-format || exit 1; done
for w in month06/week0[1-4]; do python3 "$w/tools/builder.py" configure || exit 1; done
for w in month06/week0[1-4]; do python3 "$w/tools/check_navigation.py" || exit 1; done
```

交互启动需要桌面会话；自动 Qt 测试使用 offscreen + software，加载的是真实 QML。首次配置/改模块接口后重启 VS Code clangd。只运行一个课程可将 month06 换成 month06/week04；运行已有 GUI 不会自动编译。

## 2026-10-12 验证记录

以下是当时的验收记录；移除各周核心测试文件后，当前 `test month06` 只运行 QML/Qt 测试。核心行为通过下方控制台与手动案例复核。

| 验证 | 结果 / 覆盖 |
|---|---|
| 四周独立构建 | 全部通过 |
| 四周 QML/Qt Test | 全部通过；包括真实键盘草稿输入、刷新后保留、非法数值回退；第四周真实确认框提交/撤销与截图 |
| 四周完整控制台案例 | 200/100/20000 → undo 80/40/3200 → redo 200/100/20000；保存/加载后 modified=0；坏格式不替换 |
| 第二至四周确定性差分 | 各 24 次记录排列＋24 次增量/全量对照，种子 20261012；无关分支不执行 |
| 错误恢复与撤销 | 循环拒绝、除零失败、后继阻塞、删除引用、修复、全部撤销/重做、保存往返均通过 |
| AST 与批次 | 数值编辑/undo/redo 不新增解析；新公式解析一次；一组编辑一次事件和历史项 |
| 操作接口 | 预览不改正式模型；第二项失败整体不提交；解析范围/操作下标；过期修订拒绝；计算失败可修复/撤销 |
| 保存与草稿 | 独立草稿跨选择/刷新保留；失败保存仍未保存；撤销回到磁盘内容恢复干净；新分支正确标记 |
| C++/QML 格式与静态检查 | clang-format、qmlformat、qmllint 零警告 |
| 课程链接与题号 | 所有文档链接可解析，四周各五题，覆盖 101–120 |
| clangd 配置与导航 | 数据库生成成功；全仓库定义跳转通过，包括语义比较、AST 缓存和 preview/apply 新接口 |
| 实际图形与依赖边界 | 编辑器/确认框截图已检查；控制台 ldd 无 Qt 动态依赖 |

性能是实际观测，不设跨机器通过阈值，见[第三周性能记录](../week03/performance.md)。控制台不链接 Qt；图形依赖仅在适配层、main 与 Qt 测试中。

## 手动完整案例

1. 启动第四周，点击“新建矩形”，选择对象 #2，把 Name 改为 A。默认 Length=80、Width=40，Area=3200。
2. 为 Width 绑定 `length / 2`。将 Length 改为 200，核对 Width=100、Area=20000，标题显示未保存。
3. 撤销：80/40/3200；重做：200/100/20000。保存为 .cad，未保存标记消失；打开该文件后对象、公式、位置恢复，历史与选择清空。
4. 输入未提交的 Width 公式草稿 `length / 4`，切换选择再回来，草稿仍存在；保存文档不会提交草稿。“放弃公式草稿”恢复正式公式。
5. 把 Length 改为 150，点击“预览批量修改”。确认框显示对象 ID、属性、公式和派生结果的前后差异；取消不修改，确认后得到 200/100/20000，一次撤销回到 150/75/11250。
6. 拖动对象，松开后新增一次历史；Escape 取消不修改。缩放和平移不影响内容或未保存状态。

## 异常与恢复案例

| 操作 | 预期 | 修复 |
|---|---|---|
| Length 输入 -1 / 非数字 | 命令拒绝，数值框回到正式值，无新历史 | 输入合法正数 |
| Width 草稿 `length +` | 光标定位末尾，保留草稿、原公式和几何 | 改成 `length / 2` 再绑定，或放弃 |
| Length 公式 `area` | 循环拒绝，旧图与绑定不变 | 放弃或输入无循环公式 |
| Width 公式 `length/(segments-1)`，segments=1 | Width Failed、Area Blocked，旧几何有警告 | segments 改为 2，恢复 Valid |
| 删除跨对象公式的来源 | 直接引用 MissingReference，后继 Blocked | 撤销删除，原 ID 与计算链恢复 |
| 打开坏文件或 CAD_DOCUMENT 1 | 保留当前内容、历史、选择与预览 | 选择有效版本 2 文件 |
| 保存到不存在的父目录 | 报错，modified 保持 true，不设置成功保存基准 | 选择可写路径 |
| 预览后发生其他编辑或撤销（进程内/UI 自动测试） | 确认返回 stale_revision，不覆盖新状态 | 重新预览 |

界面未提供跨对象变量映射编辑器，此项使用核心接口及已有映射文件验证。批量创建暂不支持在同一请求中用临时 ID 引用新对象。AI、HTTP 服务、三维、约束和单位不在本次范围。

## 实际界面

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/month06/week04/demo --screenshot build/month06/week04/editor.png
```

[编辑器截图](../../build/month06/week04/editor.png)由真实 QML 生成；第四周 UI 测试还生成[批量修改确认截图](../../build/month06/week04/preview.png)。截图与 CSV 均为 build 下的生成文件。自动验证使用软件渲染；桌面 GPU、系统文件对话框与缩放比例尚需用户在实际图形会话中体验。
