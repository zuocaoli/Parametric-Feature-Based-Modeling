# 配置 clangd、代码跳转与格式化

目标：让当前 C++23 import std 示例能够被 clangd 正确解析并跳转，提供统一代码格式及 VS Code 保存时格式化。

| 子任务编号 | 目标与设计 | 依赖 | 状态 |
|---|---|---|---|
| 6 | 添加配置脚本生成 Clang 标准库模块与编译数据库；配置 clangd、clang-format 和 VS Code；补充操作说明 | workflow.md、001 源码及说明、本机工具和编辑器设置 | 已完成 |
| 7 | 验证诊断、实际 LSP 定义跳转、格式化和 GCC 编译运行；归档后清空状态 | 子任务 6 及 task_archive/6.md | 已完成 |

已确认：GCC 16.2.1、Clang/clangd/clang-format 22.1.8；VS Code 全局 clangd 路径指向 23，需在工作区覆盖为 /usr/bin/clangd。Clang 22 已成功预编译本机 bits/std.cc。

设计：保留 GCC 构建方式；Clang 解析产物仅放在 001/build/clangd 中。格式采用四空格缩进及现有大括号风格。保留用户已有注释，不修改 workflow.md。

验收：clangd 无诊断错误；area、length 及 std::cout 的定义跳转有效；格式检查通过；程序输出保持一致。完成记录追加编号归档，不覆盖历史记录。
