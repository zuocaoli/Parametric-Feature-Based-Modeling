module course.application;
import std;
namespace units {
Result<void> Application::set_preferences(UnitPreferences p) {
    auto s = service_.schemas().get(p.schema);
    if (!s)
        return std::unexpected(s.error());
    auto valid = s->translate(*Quantity::make(1, Unit::length()), p.format);
    if (!valid)
        return std::unexpected(valid.error());
    service_.schemas().select(p.schema).value();
    preferences_ = std::move(p);
    return {};
}
const UnitPreferences &Application::preferences() const {
    return preferences_;
}
std::unique_ptr<Document> Application::create_document() const {
    auto d = std::make_unique<Document>();
    Model m = d->model();
    auto c = m.config();
    c.schema = preferences_.schema;
    c.precision = preferences_.format.precision;
    c.significant = preferences_.format.significant;
    c.group_digits = preferences_.format.group_digits;
    auto v = m.configure(c);
    if (!v)
        throw std::logic_error(v.error().message);
    d->replace(std::move(m)).value();
    return d;
}
UnitConfig Application::effective_config(const Document &d) const {
    auto c = d.model().config();
    if (preferences_.ignore_project_schema)
        c.schema = preferences_.schema;
    return c;
}
Result<void> Application::activate(const Document &d) {
    auto selected = service_.schemas().select(effective_config(d).schema);
    if (!selected)
        return selected;
    active_ = d.identity();
    return {};
}
Result<std::string> Application::display(const Document &d, const PropertyKey &key) const {
    auto q = d.model().value(key);
    if (!q)
        return std::unexpected(q.error());
    auto c = effective_config(d);
    UnitsApi api{d.model().registry()};
    auto selected = api.schemas().select(c.schema);
    if (!selected)
        return std::unexpected(selected.error());
    auto ref = c.display_overrides.contains(key.name) ? c.display_overrides.at(key.name)
               : q->unit() == Unit::length()          ? c.display
                                                      : UnitRef{"", 1};
    auto text = api.format(*q, {c.precision, c.significant, c.group_digits}, ref);
    if (!text)
        return std::unexpected(text.error());
    return text->text;
}
Result<InputContext> Application::begin_edit(const Document &d, const PropertyKey &key) const {
    if (active_ != d.identity())
        return std::unexpected(error("inactive_document", "activate document before editing"));
    auto spec = d.model().spec(key);
    if (!spec)
        return std::unexpected(spec.error());
    UnitsApi api{d.model().registry()};
    auto c = effective_config(d);
    auto context = api.capture(*spec, c, key.name, d.generation());
    if (!context)
        return std::unexpected(context.error());
    if (c.input_follows_display && !c.input_overrides.contains(key.name)) {
        auto value = d.model().value(key);
        if (!value)
            return std::unexpected(value.error());
        api.schemas().select(c.schema).value();
        auto ref = c.display_overrides.contains(key.name) ? c.display_overrides.at(key.name)
                   : value->unit() == Unit::length()      ? c.display
                                                          : UnitRef{"", 1};
        auto translated = api.format(*value, {c.precision, c.significant}, ref);
        if (!translated)
            return std::unexpected(translated.error());
        auto unit = parse_unit(translated->unit_string, api.registry());
        if (!unit)
            return std::unexpected(unit.error());
        context->frozen_unit = unit->definition;
        context->input_unit = {unit->definition.id, unit->definition.version};
    }
    context->document_identity = d.identity();
    context->object = key.object;
    return context;
}
Result<bool> Application::commit(Document &d, const InputContext &c,
                                 std::string_view source) const {
    if (active_ != d.identity() || c.document_identity != d.identity())
        return std::unexpected(error("edit_conflict", "document identity changed"));
    auto spec = d.model().spec({c.object, c.target});
    if (!spec || *spec != c.spec)
        return std::unexpected(error("edit_conflict", "target changed"));
    auto q = commit_edit(source, d.model().registry(), c, d.generation());
    if (!q)
        return std::unexpected(q.error());
    return d.transact([&](Model &m) { return m.set_value({c.object, c.target}, *q); });
}
} // namespace units
