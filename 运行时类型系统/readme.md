# BaseType

一个基于 C++23 命名模块的运行时类型系统，为业务对象提供统一的类型注册、继承查询和按名称创建能力。  
通过 `import baseType;` 使用，接口位于命名空间 `Base`。

## 提供什么能力

| 能力 | 作用 |
| --- | --- |
| 类型注册与查询 | 为类登记名称和父类型，通过名称查找类型，枚举某类及其派生类型 |
| 对象类型识别 | 通过 `BaseClass*` 查询实际类型，判断精确类型或继承关系 |
| 类型转换 | `typeCast<T*>()` 检查继承关系后转换指针，空指针或类型不匹配时返回空指针 |
| 对象工厂 | 按类型名称创建已注册且具备工厂的对象，支持返回 `unique_ptr<BaseClass>` |
| 按需注册 | 通过应用提供的模块加载回调补充类型注册，支持加载缓存、失败重试和循环加载检测 |

`Type` 管理类型信息，`BaseClass` 提供对象查询入口，`TypedClass<Derived, Parent>` 为业务类生成注册和类型报告代码。

## 基于它能做什么

- **统一管理建模对象**：用同一基类持有矩形、拉伸体等不同对象，再根据实际类型执行对应操作。
- **构建对象创建菜单**：枚举某个基类的派生类型，筛选可创建类型，供用户选择并实例化。
- **为文档加载提供创建入口**：文件保存类型名称，读取时按名称创建对象，再由上层恢复属性和对象关系。
- **扩展业务类型**：新增对象类并注册，让现有类型查询和工厂入口能够识别、创建它。

例如，上层可记录类型名称 `Geometry::Box`，加载时调用 `Type::createObjectByName()` 创建对象，再通过 `typeCast<Box*>()` 访问其接口。
属性读写、文件格式和几何计算由上层实现；动态库加载由应用提供。

## 构建与运行

依赖 CMake 3.28+、Ninja、支持 C++23 模块依赖扫描的 Clang 和匹配版本的 `clang-scan-deps`；标准库需支持 `std::expected`。   
当前使用 Clang 22.1.8，IDE 使用同版本 clangd。

在当前目录执行：
```sh
./run.sh
./build/baseTypeExample
ctest --test-dir build --output-on-failure --no-tests=error
```

`run.sh` 配置并编译到 `build/`，同时生成 clangd 使用的编译数据库。   
接入示例见 [examples/main.cpp](examples/main.cpp)，行为验证见 [tests/main.cpp](tests/main.cpp)。  

