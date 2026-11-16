import std;
import course.document;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    Model m;
    auto resolve = [&](const PropertyKey &k) { return m.spec(k); };
    auto bind = [&](std::string_view s) { return bind_expression(s, m.registry(), resolve); };
    for (auto s : {"sin(90 deg)", "sin(90)", "cos(0 deg)", "asin(1)", "sqrt(4 mm2)",
                   "pow(4 mm2,0.5)", "atan2(1 mm,2 mm)", "min(2 in,3 m)"})
        check(bind(s).has_value(), s);
    for (auto s : {"length+1 kg", "length+1", "sin(1 mm)", "sqrt(1 mm)", "pow(length,ratio)",
                   "atan2(0,0)", "log(-1)", "length/0", "1e309 mm", "unknown", "min(1)"})
        check(!bind(s), s);
    for (auto s : {"negate()", "negate(1)", "negate(1,2)"})
        check(!bind(s), "internal negate is not a callable function");
    auto negative = bind("-length");
    check(negative &&
              evaluate(*negative, [&](const auto &k) { return m.value(k); })->value() == -1000,
          "unary minus still evaluates");
    auto q = *bind("length/2+10 mm");
    check(evaluate(q, [&](const auto &k) { return m.value(k); })->value() == 510,
          "internal mm evaluation");
    check(bind("asin(1)")->root->constant->value() == 90, "inverse trig degrees");
    auto comments = *bind("length [first] / 2 + 10 mm");
    check(comments.source.find("[first]") != std::string::npos &&
              comments.units.front().position == 24,
          "comments preserve original binding span");
    check(m.register_unit({"pitch", 1, "pitch", Unit::length(), 10}).has_value(), "definition1");
    auto old = *bind("2 pitch");
    m.register_unit({"pitch", 2, "pitch", Unit::length(), 20}).value();
    auto pinned = bind_expression(old.source, m.registry(), resolve, 1, {}, old.units, true);
    check(pinned && pinned->root->constant->value() == 20 &&
              bind("2 pitch")->root->constant->value() == 40,
          "bound version stable");
    auto bad = old.units;
    bad.front().position++;
    check(!bind_expression(old.source, m.registry(), resolve, 1, {}, bad, true),
          "binding integrity");
    check(!bind(std::string(100, '(') + "1" + std::string(100, ')')), "nesting limit");
    std::println("expression: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
