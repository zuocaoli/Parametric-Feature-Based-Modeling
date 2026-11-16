import std;
import course.storage;
using namespace units;
void check(bool ok, std::string_view message) {
    if (!ok)
        throw std::runtime_error(std::string{message});
}
int main() try {
    Model model;
    auto tolerance = *Quantity::make(.01, Unit::length());
    auto data = export_rectangle(model, "in", tolerance);
    check(data.has_value(), "export inches");
    auto imported = import_rectangle(*data, model.registry());
    check(imported && std::abs(imported->length.value() - 1000) < 1e-9 &&
              std::abs(imported->width.value() - 510) < 1e-9 &&
              std::abs(imported->tolerance.value() - .01) < 1e-12 &&
              imported->normal == data->normal,
          "dimensions and tolerance roundtrip, normal unchanged");
    auto millimetres = export_rectangle(model, "mm", tolerance);
    check(millimetres && millimetres->length == 1000 && millimetres->width == 510 &&
              millimetres->tolerance == .01,
          "millimetre boundary");
    check(!export_rectangle(model, "kg", tolerance), "export requires a length unit");
    data->unit.clear();
    check(!import_rectangle(*data, model.registry()), "explicit input unit required");
    data->unit = "mm";
    data->normal = {0, 0, 2};
    check(!import_rectangle(*data, model.registry()), "normal must be normalized");
    data->normal = {0, 0, 1};
    data->length = -1;
    check(!import_rectangle(*data, model.registry()), "invalid imported dimension");
    std::println("exchange: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
