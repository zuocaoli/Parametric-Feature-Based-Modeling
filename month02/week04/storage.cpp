module month02.week04.storage;

import std;

namespace month02::week04 {
namespace {
Error storage_error(std::string message) {
    return {ErrorStage::Storage, 0, std::move(message)};
}

std::expected<Value, Error> read_value(std::istream &input, std::string_view type) {
    if (type == "double") {
        double value;
        if (input >> value && std::isfinite(value)) {
            return Value{value};
        }
    } else if (type == "int64") {
        std::int64_t value;
        if (input >> value) {
            return Value{value};
        }
    } else if (type == "bool") {
        bool value;
        if (input >> std::boolalpha >> value) {
            return Value{value};
        }
    } else if (type == "string") {
        std::string value;
        if (input >> std::quoted(value)) {
            return Value{std::move(value)};
        }
    }
    return std::unexpected(storage_error("invalid property value or type"));
}
} // namespace

RectangleModel::RectangleModel(double length, double width)
    : rectangle(length, width), engine(rectangle) {}

std::expected<void, Error> save_model(const RectangleModel &model, std::ostream &output) {

    try {
        std::ostringstream text;
        // std::ostringstream 的 imbue 函数用于设置该流所使用的区域设置（locale），
        // 从而直接影响流在格式化输出时的具体行为（如数字的小数点符号、千位分隔符等）。
        // std::locale::classic() 是 C++ 标准库中的一个静态成员函数，用于获取经典的 "C" 区域设置（locale） 的引用。
        text.imbue(std::locale::classic());

        // [处理 bool 值的序列化和反序列化]：
        // std::boolalpha 是 C++ 标准库 <ios> 中定义的一个输入/输出操纵符（I/O Manipulator）。
        // 它的作用非常直接：改变 bool 类型值在流中的显示或解析方式。
        // 默认情况下，bool 值在流中显示为整数 1 或 0。使用 std::boolalpha 后，输出会变为文本 "true" 或 "false"；
        // 输入时，它也能让流将文本 "true" / "false" 解析为 bool 值，而非仅接受数字。
        //
        // [保证浮点数在序列化和反序列化中往返转换的精确性]：
        // std::numeric_limits<double>::max_digits10 的值是 17。
        // 它表示将 double 类型的浮点数转换为文本再读回时，为保证能还原出完全相同的原始浮点值，所需的最少十进制有效数字位数。
        //
        // max_digits10 这个常量主要服务于序列化与反序列化场景。
        // 浮点数的二进制表示通常无法用有限位十进制小数精确表达，直接按较短位数输出会丢失精度。
        // max_digits10 提供了安全阈值：只要输出时使用至少 17 位有效数字，
        // 即使中间文本并非精确值，读回时也能保证得到与原来位级完全相同的 double 值
        //
        // std::setprecision(17) , 设置浮点数的有效位数为17,
        // 这样输出一个 double 时，会给出足够多的有效数字，保证读回后位级完全一致。这是浮点数序列化的标准做法。
        // 这里的std::setprecision 没有配合 std::fixed，所以精度含义是有效数字总位数，而非小数位数。
        text << std::setprecision(std::numeric_limits<double>::max_digits10) << std::boolalpha;

        // 取出 model 的属性
        const auto properties = model.rectangle.properties();

        text << "CAD_RECTANGLE 1\nvalues " << properties.size() << '\n';

        // 持久化 model 的属性
        for (const auto &property : properties) {

            // 1. 属性名称，属性类型名称：
            // std::quoted 是 C++14 引入的流操纵符，用于在输出时给字符串加上引号并转义内部特殊字符，在输入时解析被引号包裹的字符串。
            // 它很适合处理 CSV、配置文件或任何需要“带空格的字符串作为一个整体”的场景。
            text << std::quoted(property.name) << ' ' << type_name(property.value) << ' ';

            // 2. 读取属性的值：
            // std::visit 是 C++17 引入的 <variant> 头文件中的核心工具，用于对 std::variant 当前持有的值进行类型安全的访问与分派。
            // std::visit 的输入参数有两个：一个访问器（Visitor） 和一个或多个 std::variant 对象。
            //
            //
            // if constexpr 是 C++17 的编译期分支语句。
            // 括号内必须是常量表达式，类型为 bool（或可隐式转换为 bool）。
            // 条件在编译期求值，未选中分支不会被实例化——这正是你代码里能让 std::quoted(value) 只在字符串分支存在的关键。
            //
            // decltype 是 C++11 引入的类型推导说明符，用于在编译期获取一个表达式或实体的精确类型。
            //
            // std::is_same_v<A, B>，用来判断两个类型是否完全相同
            //    A = std::decay_t<decltype(value)>
            //    B = std::string
            //
            // 这段代码用 std::visit 对 property.value（一个 std::variant）做类型分派：
            // 如果当前持有的是 std::string，就用 std::quoted 加引号输出；否则直接输出
            std::visit(
                [&](const auto &value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                        text << std::quoted(value);
                    } else {
                        text << value;
                    }
                },
                property.value);

            //
            text << '\n';
        }

        //
        // 表达式和绑定关系的持久化
        //
        const auto formulas = model.engine.formulas();

        text << "formulas " << formulas.size() << '\n';

        for (const auto &[target, source] : formulas) {
            text << std::quoted(target) << ' ' << std::quoted(source) << '\n';
        }

        text << "end\n";

        //
        //  写入文件
        //
        output << text.str();

        if (!output) {
            return std::unexpected(storage_error("cannot write model"));
        }
        return {};

    } catch (const std::ios_base::failure &) {
        return std::unexpected(storage_error("cannot write model"));
    }
}

std::expected<std::unique_ptr<RectangleModel>, Error> load_model(std::istream &input) {
    try {
        if (!input) {
            return std::unexpected(storage_error("cannot read model"));
        }

        //
        // std::istreambuf_iterator 是 C++ 标准库 <iterator> 中的一个输入迭代器，
        // 用于从流缓冲区（stream buffer）中逐字符读取数据。它最常见的用途是高效地把整个流（如文件、std::cin）读入字符串或容器。
        //
        // std::istreambuf_iterator<char>{input} 开始迭代器，
        // std::istreambuf_iterator<char>{} 结束迭代器。默认构造的 istreambuf_iterator 就是"流结束"哨兵，表示 EOF。
        //
        // std::string payload{ begin, end } 用一对迭代器构造字符串，把 [begin, end) 范围内的所有字符拷贝进 payload。
        //
        const std::string payload{std::istreambuf_iterator<char>{input},
                                  std::istreambuf_iterator<char>{}};

        //
        // std::istream::bad() 用于检查流是否发生了不可恢复的严重错误。
        // 当它返回 true 时，意味着底层流缓冲区（streambuf）的完整性已受损，通常不应再继续使用该流进行正常的输入输出操作
        if (input.bad()) {
            return std::unexpected(storage_error("cannot read model"));
        }

        //
        std::istringstream text{payload};
        text.imbue(std::locale::classic());
        std::string marker;
        int version;

        // 链式读取两个字段，读取 CAD_RECTANGLE 1 到 marker、version 中
        if (!(text >> marker >> version) || marker != "CAD_RECTANGLE" || version != 1) {
            return std::unexpected(storage_error("unsupported model header or version"));
        }

        //
        //
        auto model = std::make_unique<RectangleModel>();
        const auto expected_count = model->rectangle.properties().size();
        std::size_t count;

        //
        // 开始读取属性
        //

        // 读取 values 6
        if (!(text >> marker >> count) || marker != "values" || count != expected_count) {
            return std::unexpected(storage_error("invalid property count"));
        }

        std::vector<Update> updates;
        for (std::size_t i = 0; i < count; ++i) {

            //
            // 到了这一步， text 中每一行的格式为： "属性名称"  类型名称  属性值
            //

            std::string name, type;
            if (!(text >> std::quoted(name) >> type)) {
                return std::unexpected(storage_error("truncated property record"));
            }

            // 根据文件中的类型名，从输入流读取一个值，包装为 Value；读取失败则返回错误
            auto value = read_value(text, type);

            if (!value) {
                return std::unexpected(value.error());
            }

            updates.push_back({std::move(name), std::move(*value)});
        }

        // 批量更新属性值
        const auto assigned = model->rectangle.set_many(updates);
        if (assigned != SetResult::Changed && assigned != SetResult::Unchanged) {
            return std::unexpected(
                storage_error("invalid properties: " + std::string{result_name(assigned)}));
        }

        //
        // 开始读取属性表达式
        //
        if (!(text >> marker >> count) || marker != "formulas" || count > 3) {
            return std::unexpected(storage_error("invalid formula count"));
        }

        std::set<std::string> targets; // 有序、去重的字符串集合，存放属性名称
        for (std::size_t i = 0; i < count; ++i) {

            // 读取属性名称，表达式
            std::string target, source;
            if (!(text >> std::quoted(target) >> std::quoted(source))) {
                return std::unexpected(storage_error("truncated formula record"));
            }

            //
            // std::set 的 insert 函数：
            // std::pair<std::iterator, bool> insert(const T& v);
            // std::pair<std::iterator, bool> insert(T&& v);
            // 插入成功（元素原来不存在）:   <指向新插入的元素, true>
            // 插入失败（元素已存在）:   	<指向已存在的那个元素（原值不变）, false>
            if (!targets.insert(target).second) {
                // targets 用于检查文件中的公式目标属性是否重复，不能有重复的，因为一个属性只能有一个表达式。
                return std::unexpected(storage_error("duplicate formula target"));
            }

            // 绑定属性和表达式
            const auto bound = model->engine.bind(target, source);
            if (!bound) {
                return std::unexpected(
                    storage_error("invalid saved formula: " + bound.error().message));
            }
        }

        if (!(text >> marker) || marker != "end") {
            return std::unexpected(storage_error("missing end marker"));
        }

        // 从输入流 text 的当前位置开始，跳过连续的空白字符，如空格、制表符和换行，直到遇到非空白字符或文件末尾。
        // std::ws 是输入流操纵符，不会读取或丢弃后面的非空白字符。
        text >> std::ws;
        if (!text.eof()) {
            // 返回一个 bool，表示流是否已经设置了到达输入末尾的状态标志。
            // 如果没有到达就说明有问题了。
            return std::unexpected(storage_error("unexpected trailing data"));
        }
        return model;
    } catch (const std::ios_base::failure &) {
        return std::unexpected(storage_error("cannot read model"));
    }
}
} // namespace month02::week04
