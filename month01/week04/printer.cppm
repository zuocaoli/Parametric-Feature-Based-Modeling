export module week04.printer;

import std;
import week04.property;

export namespace week04 {
// 通过公共属性接口打印对象，无需判断具体类型。
void print_properties(const PropertyObject &object, std::ostream &out = std::cout);
} // namespace week04
