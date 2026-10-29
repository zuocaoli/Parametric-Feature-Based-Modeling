import std;

// 尺寸修改的结果
enum class SetResult {
    Changed,     // 输入合法，尺寸已改变
    Unchanged,   // 输入合法，但与原值相同
    InvalidValue // 输入非法，保持原值不变
};

class Rectangle {
public:
    Rectangle(double initial_length, double initial_width)
        : length_(initial_length), width_(initial_width) {
        if (!valid_size(length_) || !valid_size(width_)) {
            // 抛出参数非法异常
            throw std::invalid_argument("dimensions must be finite and positive");
        }
    }

    double length() const {
        return length_;
    }
    double width() const {
        return width_;
    }

    // const 表示这个成员函数不会修改调用它的对象的普通成员变量
    double area() const {
        return length_ * width_;
    }

    SetResult set_length(double value) {
        return assign_size(length_, value);
    }
    SetResult set_width(double value) {
        return assign_size(width_, value);
    }

private:
    static bool valid_size(double value) {
        // std::isfinite 是 C++<cmath>中的函数，
        // 用来判断一个浮点数是不是有限值(不是正无穷、不是负无穷、也不是 NaN 的数值).
        return std::isfinite(value) && value > 0.0;
    }

    static SetResult assign_size(double &target, double value) {
        if (!valid_size(value)) {
            return SetResult::InvalidValue;
        }
        if (target == value) {
            return SetResult::Unchanged;
        }
        target = value;
        return SetResult::Changed;
    }

    double length_;
    double width_;
};

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void run_tests() {
    Rectangle rectangle{5.0, 3.0};
    require(rectangle.area() == 15.0, "initial area");
    require(Rectangle{2.5, 4.0}.area() == 10.0, "fractional dimensions");
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        require(rectangle.set_length(invalid) == SetResult::InvalidValue, "reject length");
        require(rectangle.set_width(invalid) == SetResult::InvalidValue, "reject width");
        require(rectangle.length() == 5.0 && rectangle.width() == 3.0, "no partial change");
        for (bool invalid_length : {true, false}) {
            bool rejected = false;
            try {
                Rectangle candidate{invalid_length ? invalid : 5.0, invalid_length ? 3.0 : invalid};
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            require(rejected, "constructor validates both dimensions");
        }
    }
    require(rectangle.set_length(5.0) == SetResult::Unchanged, "same value");
    require(rectangle.set_length(10.0) == SetResult::Changed, "changed length");
    require(rectangle.area() == 30.0, "computed area follows dimensions");
    require(rectangle.set_width(2.5) == SetResult::Changed, "changed width");
    require(rectangle.area() == 25.0, "second area update");
    require(rectangle.set_width(std::nextafter(2.5, 3.0)) == SetResult::Changed,
            "exact comparison, not a tolerance test");
    std::cout << "week01 tests passed\n";
}

void demo() {
    // 只读对象，初始化后不能修改它的普通成员变量
    const Rectangle first{5.0, 3.0};
    const Rectangle second{2.5, 4.0};
    std::cout << "length=" << first.length() //
              << ", width=" << first.width() //
              << ", area=" << first.area() << '\n';
    std::cout << "length=" << second.length() //
              << ", width=" << second.width() //
              << ", area=" << second.area() << '\n';

    Rectangle editable{5.0, 3.0};
    require(editable.set_length(-1.0) == SetResult::InvalidValue, "reject invalid input");
    require(editable.set_length(5.0) == SetResult::Unchanged, "detect repeated assignment");
    require(editable.set_length(10.0) == SetResult::Changed, "accept valid change");
    std::cout << "invalid rejected; repeated value unchanged; new area=" << editable.area() << '\n';
}

int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--test") {
            run_tests();
        } else if (argc == 1) {
            demo();
        } else {
            throw std::invalid_argument("usage: demo [--test]");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
