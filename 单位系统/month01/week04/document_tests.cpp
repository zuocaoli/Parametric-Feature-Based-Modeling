import std;
import course.document;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
Quantity q(double v, Unit u = {}) {
    return *Quantity::make(v, u);
}
int main() try {
    Document d;
    int events = 0;
    bool observed = true;
    d.observe([&](const DocumentEvent &e) {
        events++;
        observed = observed && d.revision() == e.revision &&
                   !d.transact([](Model &) { return Result<void>{}; });
    });
    auto preview =
        d.preview([](Model &m) { return m.set_value({1, "length"}, q(2000, Unit::length())); });
    check(preview && preview->value({1, "area"})->value() == 2020000 && d.history_size() == 0 &&
              !d.modified(),
          "preview actual isolated values");
    d.transact([](Model &m) {
         return m.set_value({1, "length"}, q(2000, Unit::length()));
     }).value();
    check(d.model().value({1, "width"})->value() == 1010 &&
              d.model().value({1, "area"})->value() == 2020000 && d.last_report().size() == 2 &&
              events == 1,
          "single recompute and event");
    auto content = d.model().content();
    auto revision = d.revision();
    check(!d.transact([](Model &m) { return m.bind({1, "width"}, "length+1 kg"); }) &&
              !d.transact([](Model &m) { return m.bind({1, "length"}, "width"); }),
          "bad dimension/cycle rejected");
    check(d.model().content() == content && d.revision() == revision, "failed transaction atomic");
    d.undo().value();
    check(d.model().value({1, "area"})->value() == 510000 && !d.modified(), "undo baseline");
    d.redo().value();
    d.undo().value();
    d.transact([](Model &m) { return m.bind({1, "width"}, "length/ratio"); }).value();
    d.transact([](Model &m) { return m.set_value({1, "ratio"}, q(0)); }).value();
    check(d.model().properties().at({1, "width"}).state == State::Failed &&
              d.model().properties().at({1, "area"}).state == State::Blocked &&
              !d.model().value({1, "width"}),
          "failed/blocked stale value unavailable");
    d.undo().value();
    check(d.model().value({1, "width"})->value() == 500, "undo recovery");
    d.redo().value();
    d.transact([](Model &m) { return m.set_value({1, "ratio"}, q(4)); }).value();
    check(d.model().value({1, "width"})->value() == 250, "repair");
    d.mark_saved();
    auto number = *d.model().value({1, "length"});
    d.transact([](Model &m) {
         auto c = m.config();
         c.schema = "ImperialDecimal";
         return m.configure(c);
     }).value();
    check(d.last_report().empty() && *d.model().value({1, "length"}) == number && d.modified() &&
              *d.model().display({1, "length"}) == "39.3701 in",
          "schema-only event");
    d.undo().value();
    check(!d.modified() && d.last_report().empty(), "schema undo");
    check(!d.transact([](Model &m) { return m.set_value({1, "length"}, q(-1, Unit::length())); }),
          "fixed model enforces bounds");
    check(!d.transact([](Model &m) { return m.set_value({1, "length"}, q(1, Unit::area())); }),
          "fixed model never adopts wrong Unit");
    check(observed, "observer sees final state, no reentry");
    d.transact([](Model &m) -> Result<void> {
         auto added = m.add_property({1, "tiny"}, q(.05), {Unit{}, 0, .1});
         if (!added)
             return added;
         return m.bind({1, "tiny"}, "ratio/100");
     }).value();
    check(d.model().value({1, "tiny"})->value() == .04,
          "type check must not apply target bounds to dummy sample");
    Document leaf;
    std::vector<bool> format_events;
    leaf.observe([&](const DocumentEvent &e) { format_events.push_back(e.format_only); });
    leaf.transact([](Model &m) { return m.set_value({1, "area"}, q(123, Unit::area())); }).value();
    check(leaf.last_report().empty() && !format_events.back() &&
              !leaf.model().properties().at({1, "area"}).formula &&
              leaf.model().value({1, "area"})->value() == 123,
          "formula removal is a model event without derived recomputation");
    leaf.undo().value();
    check(!format_events.back() && leaf.model().properties().at({1, "area"}).formula &&
              leaf.model().value({1, "area"})->value() == 510000,
          "undo restores formula and model event");
    leaf.redo().value();
    check(!format_events.back() && !leaf.model().properties().at({1, "area"}).formula &&
              leaf.model().value({1, "area"})->value() == 123,
          "redo removes formula and remains a model event");
    leaf.transact([](Model &m) {
            auto config = m.config();
            config.schema = "MeterDecimal";
            return m.configure(config);
        })
        .value();
    check(format_events.back(), "schema-only edit remains a format event");
    std::println("document: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
