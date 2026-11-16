import std;
import course.operations;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    Document d;
    Operations ops{d};
    auto length = [](double v) { return *Quantity::make(v, Unit::length()); };
    OperationRequest request{d.revision(), {SetValue{{1, "length"}, length(2000)}}};
    auto preview = ops.preview(request);
    check(preview && preview->after.value({1, "area"})->value() == 2020000 && !d.modified(),
          "preview actual isolated result");
    ops.apply(request).value();
    check(!ops.apply(request) && ops.apply(request).error().code == "stale_revision",
          "stale request");
    d.undo().value();
    auto content = d.model().content();
    auto revision = d.revision();
    check(!ops.apply({revision,
                      {SetValue{{1, "length"}, length(2000)},
                       SetFormula{{1, "width"}, "length+1 kg", {}}}}) &&
              content == d.model().content() && d.revision() == revision,
          "batch atomic");
    auto unchanged = ops.apply({d.revision(), {SetValue{{1, "length"}, length(1000)}}});
    check(unchanged && !unchanged->changed && unchanged->report.empty(), "no-op diagnostics");
    UnitDefinition unit{"pitch", 1, "pitch", Unit::length(), 10};
    ops.apply({d.revision(), {AddUnit{unit}, SetFormula{{1, "width"}, "2 pitch", {}}}}).value();
    unit.version = 2;
    unit.scale = 20;
    ops.apply({d.revision(), {AddUnit{unit}}}).value();
    check(d.model().value({1, "width"})->value() == 20, "new definition does not rebind");
    ops.apply({d.revision(), {RebindUnit{{"pitch", 2}}}}).value();
    check(d.model().value({1, "width"})->value() == 40, "explicit unit rebind");
    d.undo().value();
    check(d.model().value({1, "width"})->value() == 20, "unit rebind undo");
    check(!ops.apply({d.revision(), {}}), "empty request");
    std::println("operations: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
