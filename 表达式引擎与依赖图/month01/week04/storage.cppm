export module month02.week04.storage;

import std;
export import month02.week04.formulas;
export import month02.week04.rectangle;

export namespace month02::week04 {

//
// 把矩形数据和它的公式引擎放在一起，组成一个完整的参数化矩形模型
//
struct RectangleModel {

    explicit RectangleModel(double length = 100, double width = 1);

    Rectangle rectangle;  // 按声明逆序销毁：先引擎，后对象
    FormulaEngine engine; // 属性公式计算引擎，用于管理和执行属性上的公式
};

std::expected<void, Error> save_model(const RectangleModel &model, std::ostream &output);
std::expected<std::unique_ptr<RectangleModel>, Error> load_model(std::istream &input);
} // namespace month02::week04
