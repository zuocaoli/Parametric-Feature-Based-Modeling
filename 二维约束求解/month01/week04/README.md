# 第 1 月第 4 周：Newton 求解

## 本周训练

- **承接与前置**：承接中心差分 Jacobian；补充线性化与最小二乘。
- **唯一新增功能**：实现具有回溯线搜索和失败回退的全量 Newton 求解。
- **学习方式**：先推导本周必要公式，再阅读模块实现，最后改变输入并核对结果。

## 1. 为什么能用线性方程近似非线性约束？

在当前构型附近，F(z+Δ)≈F(z)+JΔ，因此求 JΔ=−F。这个近似只在局部有效，需要重新计算残差和 Jacobian，而不能一次更新后就假设成功。

## 2. 如何计算 Newton 更新量？

以 Eigen CompleteOrthogonalDecomposition 解最小二乘子问题，取得最小范数更新，再乘 L 回到模型坐标。差分 Jacobian 和稠密分解均由本周数值模块封装。

## 3. 非方阵或秩不足时为什么不能直接使用普通 LU？

约束行数与参数数可能不同，欠约束和重复方程也会导致秩不足。秩揭示 QR 支持这些线性子问题；它本身不替代之后课程中的自由度与约束诊断。

## 4. 回溯线搜索怎样避免残差增大？

从完整步开始按 1/2 缩短，要求总残差平方和下降或已满足全部行容差。遇到负半径等无效候选时拒绝该步；搜索停滞时返回明确失败。

## 5. 怎样区分收敛、停滞和迭代超限？

全部行达到容差才算收敛；没有可接受步为停滞，达到上限为迭代超限。每次保存最终试算残差、次数和原因；失败恢复进入求解前的成功几何。

## 独立练习与验收

扰动矩形的多个点并恢复尺寸；比较完整步与回溯结果，再把迭代上限设为 0，验证失败时几何没有被试算覆盖。

solver 验证矩形在 10 次内收敛、11 类关系从扰动初值可解及热启动；pathology 验证退化线、同心相切圆、非法选项和空系统。

实现入口：[src/solver.cpp](src/solver.cpp)；验证入口：[核心测试](tests/core_tests.cpp)。

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
ctest --preset default -R "^(solver|pathology)$" --no-tests=error
./build/console-demo --headless
./build/demo
```

依赖版本、Qt 可选配置、运行参数与 clangd 设置见 [tools/README.md](tools/README.md)。

**下一步**：求解成功还不能说明剩余多少自由度，下一月补上系统分析。

[课程目录](../../README.md) · [上一周](../../month01/week03/README.md) · [下一周](../../month02/week01/README.md)
