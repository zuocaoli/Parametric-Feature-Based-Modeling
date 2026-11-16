# 第 1 月第 3 周：数值 Jacobian

## 本周训练

- **承接与前置**：承接方程残差、缩放与来源映射；补充偏导数概念。
- **唯一新增功能**：使用中心差分组装 Jacobian，验证步长与尺度的影响。
- **学习方式**：先推导本周必要公式，再阅读模块实现，最后改变输入并核对结果。

## 1. Jacobian 每一行、每一列分别表示什么？

行对应残差方程，列对应全局参数；Jij 表示参数 j 微小改变对方程 i 的影响。这里对 z=x/L 求导，因此更新量也在缩放坐标中解释。

## 2. 中心差分怎样近似偏导数？

用 [F(z+h ej)−F(z−h ej)]/(2h) 估计第 j 列。分别在模型副本上扰动，不改变原模型；同一列的两次计算必须使用同一个 L。

## 3. 步长过大或过小分别产生什么误差？

过大使局部线性近似变差，过小会放大浮点相减误差。默认 h=1e−7×max(1,|xj/L|)，允许实验调整，不能认为越小越精确。

## 4. 参数尺度与残差尺度怎样影响数值结果？

先把全部长度参数转换成相近量级，再缩放长度残差，改善线性系统的条件。本模型没有独立角度参数；角度作为约束目标进入角度残差。

## 5. 怎样用已知导数验证 Jacobian？

用固定、水平、点距的手算导数检查符号和量级，并比较不同差分步长。角度在 ±π 处的两侧差值必须再次按周期包裹，避免产生假大导数。

## 独立练习与验收

对相距 10 mm 的两点及水平线，手写固定、水平、距离的导数，再与程序比较；改变步长观察差异。

jacobian 测试将抽查导数误差控制在 1e−5 内，覆盖步长非法输入与角度跨 ±π 时的周期差分。

实现入口：[src/numerics.cpp](src/numerics.cpp)；验证入口：[核心测试](tests/core_tests.cpp)。

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
ctest --preset default -R "^(jacobian)$" --no-tests=error
./build/console-demo --headless
./build/demo
```

依赖版本、Qt 可选配置、运行参数与 clangd 设置见 [tools/README.md](tools/README.md)。

**下一步**：用 Jacobian 计算迭代更新。

[课程目录](../../README.md) · [上一周](../../month01/week02/README.md) · [下一周](../../month01/week04/README.md)
