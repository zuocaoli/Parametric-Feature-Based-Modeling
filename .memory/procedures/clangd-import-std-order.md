# clangd 与 import std 的顺序

2026-10-29，在几何框架最终 clangd 验收时确认。

环境：Clang/clangd 22.1.8、libstdc++ 16.2.1、CMake 4.3，标准库 BMI 来自本地构建。

现象：先包含 OCCT/Qt 和标准库头文件，再 `import std;`，编译通过，但 clangd 报 `module_odr_violation_record`，涉及 `std::__format::_Sink_iter::_M_discarding`。

处理：把实现文件的 `import std;` 放在最开头，早于全部 `#include`。重建后使用原始本地编译数据库运行 `clangd --check`；不屏蔽诊断、不加 IDE 包装或预处理替代实现。

新增每周实验沿用此顺序。头文件保持自包含，Qt MOC 输入仍为普通头文件。
