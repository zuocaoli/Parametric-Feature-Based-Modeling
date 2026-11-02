import std;
import course.application;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    Application app;
    app.set_preferences({"MeterDecimal", {4}, false}).value();
    auto a = app.create_document();
    app.activate(*a).value();
    check(a->model().config().schema == "MeterDecimal" && !a->modified(),
          "new document inherits preferences");
    auto old = app.begin_edit(*a, {1, "length"});
    check(old && old->frozen_unit->scale == 1000, "capture schema input");
    app.set_preferences({"ImperialDecimal", {4}, false}).value();
    auto b = app.create_document();
    app.activate(*b).value();
    check(*app.display(*a, {1, "length"}) == "1.0000 m" &&
              *app.display(*b, {1, "length"}) == "39.3701 in",
          "document schema independent of later preference");
    check(!app.commit(*b, *old, "2") && !app.commit(*a, *old, "2"),
          "cross-document/active conflict");
    app.activate(*a).value();
    app.commit(*a, *old, "2").value();
    check(a->model().value({1, "length"})->value() == 2000, "frozen metre input");
    app.set_preferences({"ImperialDecimal", {4}, true}).value();
    auto content = a->model().content();
    check(*app.display(*a, {1, "length"}) == "78.7402 in" && a->model().content() == content,
          "ignore project affects display service only");
    auto draft = app.begin_edit(*a, {1, "length"});
    check(draft && draft->frozen_unit->scale == 25.4, "global schema input when ignored");
    a->transact([](Model &m) {
         auto c = m.config();
         c.schema = "Centimeter";
         return m.configure(c);
     }).value();
    app.commit(*a, *draft, "3").value();
    check(std::abs(a->model().value({1, "length"})->value() - 76.2) < 1e-12,
          "configuration change does not reinterpret draft");
    auto stale = app.begin_edit(*a, {1, "length"});
    a->replace(Model{}).value();
    check(!app.commit(*a, *stale, "2"), "loaded document invalidates draft");
    app.set_preferences({"Internal", {4}, false}).value();
    a->transact([](Model &m) {
         auto c = m.config();
         c.schema = "AdaptiveLength";
         c.input_follows_display = true;
         return m.configure(c);
     }).value();
    auto adaptive = app.begin_edit(*a, {1, "length"});
    check(adaptive && adaptive->frozen_unit->scale == 1000, "follow current adaptive display");
    check(!app.set_preferences({"missing", {4}, false}), "unknown preferences rejected");
    app.set_preferences({"Internal", {0, false, true}, false}).value();
    auto grouped = app.create_document();
    check(*app.display(*grouped, {1, "length"}) == "1,000 mm",
          "format preferences copied into document");
    Document area_document;
    area_document
        .transact([](Model &m) {
            auto c = m.config();
            c.schema = "MeterDecimal";
            c.display = {"in", 1};
            c.input_follows_display = true;
            c.input_overrides.erase("area");
            c.display_overrides.erase("area");
            return m.configure(c);
        })
        .value();
    app.activate(area_document).value();
    check(*app.display(area_document, {1, "area"}) == "0.5100 m^2",
          "global length override leaves area display with schema");
    auto area_edit = app.begin_edit(area_document, {1, "area"});
    check(area_edit && area_edit->frozen_unit->unit == Unit::area() &&
              area_edit->frozen_unit->scale == 1000000,
          "area edit freezes displayed square metres");
    app.commit(area_document, *area_edit, "2").value();
    check(area_document.model().value({1, "area"})->value() == 2000000,
          "area bare input follows displayed unit");
    std::println("application: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
