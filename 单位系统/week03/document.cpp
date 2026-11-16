module course.document;
import std;
namespace units {
std::string_view state_name(State s) {
    switch (s) {
    case State::Valid:
        return "Valid";
    case State::Failed:
        return "Failed";
    case State::Blocked:
        return "Blocked";
    }
    return "Unknown";
}
Model::Model(bool initialize) {
    config_.display_overrides["area"] = {"mm2", 1};
    config_.input_overrides["area"] = {"mm2", 1};
    config_.display_overrides["ratio"] = {"1", 1};
    config_.input_overrides["ratio"] = {"1", 1};
    if (!initialize)
        return;
    auto add = [&](std::string name, double value, Unit unit, std::optional<double> lower) {
        auto result = add_property({1, name}, *Quantity::make(value, unit), {unit, lower, {}});
        if (!result)
            throw std::logic_error(result.error().message);
    };
    add("length", 1000, Unit::length(), 0);
    add("width", 510, Unit::length(), 0);
    add("area", 510000, Unit::area(), 0);
    add("ratio", 2, Unit{}, {});
    auto a = bind({1, "width"}, "length/2 + 10 mm"), b = bind({1, "area"}, "length*width");
    if (!a || !b)
        throw std::logic_error("invalid initial model");
    recompute(true);
}
const std::map<PropertyKey, Property> &Model::properties() const {
    return properties_;
}
const std::map<PropertyKey, MetadataValue> &Model::metadata() const {
    return metadata_;
}
Result<void> Model::set_metadata(PropertyKey key, MetadataValue value) {
    if (key.object == 0 || key.name.empty() || properties_.contains(key))
        return std::unexpected(error("metadata", "invalid metadata key"));
    high_water_ = std::max(high_water_, key.object);
    metadata_.insert_or_assign(std::move(key), std::move(value));
    return {};
}
std::uint64_t Model::high_water() const {
    return high_water_;
}
Result<void> Model::set_high_water(std::uint64_t value) {
    if (value < high_water_)
        return std::unexpected(error("identity", "high-water mark below object id"));
    high_water_ = value;
    return {};
}
const UnitRegistry &Model::registry() const {
    return registry_;
}
const UnitConfig &Model::config() const {
    return config_;
}
Result<QuantitySpec> Model::spec(const PropertyKey &key) const {
    auto it = properties_.find(key);
    if (it == properties_.end())
        return std::unexpected(error("reference", "unknown property"));
    return it->second.spec;
}
Result<Quantity> Model::value(const PropertyKey &key) const {
    auto it = properties_.find(key);
    if (it == properties_.end())
        return std::unexpected(error("reference", "unknown property"));
    if (it->second.state != State::Valid)
        return std::unexpected(error("blocked", "referenced property is not Valid"));
    return it->second.value;
}
Result<void> Model::add_property(PropertyKey key, Quantity value, QuantitySpec spec) {
    if (key.object == 0 || key.name.empty() || properties_.contains(key) || metadata_.contains(key))
        return std::unexpected(error("property", "invalid or duplicate property"));
    auto v = validate(value, spec);
    if (!v)
        return v;
    high_water_ = std::max(high_water_, key.object);
    properties_.emplace(key, Property{spec, value, State::Valid, {}, {}});
    dirty_.insert(key);
    return {};
}
Result<void> Model::set_value(const PropertyKey &key, Quantity value) {
    auto s = spec(key);
    if (!s)
        return std::unexpected(s.error());
    auto v = validate(value, *s);
    if (!v)
        return v;
    auto &p = properties_.at(key);
    if (p.value == value && !p.formula && p.state == State::Valid)
        return {};
    p.value = value;
    p.formula.reset();
    p.state = State::Valid;
    p.failure.reset();
    dirty_.insert(key);
    return {};
}
Result<void> Model::bind(const PropertyKey &key, std::string source, VariableMap aliases,
                         std::span<const UnitBinding> pins, bool require) {
    auto s = spec(key);
    if (!s)
        return std::unexpected(s.error());
    auto expression = bind_expression(
        source, registry_, [&](const PropertyKey &k) { return spec(k); }, key.object,
        std::move(aliases), pins, require);
    if (!expression)
        return std::unexpected(expression.error());
    if (expression->root->unit != s->unit) {
        auto e = error("unit_mismatch", "formula result incompatible with target");
        e.expected = describe(s->unit);
        e.actual = describe(expression->root->unit);
        return std::unexpected(e);
    }
    if (expression->root->constant) {
        auto v = validate(*expression->root->constant, *s);
        if (!v)
            return v;
    }
    auto old = properties_.at(key).formula;
    properties_.at(key).formula = *expression;
    auto graph = check_graph();
    if (!graph) {
        properties_.at(key).formula = std::move(old);
        return graph;
    }
    dirty_.insert(key);
    return {};
}
Result<void> Model::register_unit(UnitDefinition d) {
    d.builtin = false;
    return registry_.add(std::move(d));
}
Result<void> Model::configure(UnitConfig config) {
    UnitsApi api{registry_};
    auto schema = api.schemas().get(config.schema);
    if (!schema)
        return std::unexpected(schema.error());
    for (const auto &ref : {config.input, config.display})
        if (!ref.id.empty()) {
            auto u = registry_.find_id(ref.id, ref.version);
            if (!u)
                return std::unexpected(u.error());
            if (u->unit != Unit::length())
                return std::unexpected(
                    error("unit_mismatch", "document default override must be Length"));
        }
    for (const auto *overrides : {&config.input_overrides, &config.display_overrides})
        for (const auto &[name, ref] : *overrides) {
            auto u = registry_.find_id(ref.id, ref.version);
            if (!u)
                return std::unexpected(u.error());
            for (const auto &[key, p] : properties_)
                if (key.name == name && u->unit != p.spec.unit)
                    return std::unexpected(
                        error("unit_mismatch", "override incompatible with property"));
        }
    if (config.precision < 0 || config.precision > 17 ||
        (config.significant && config.precision == 0))
        return std::unexpected(error("format", "invalid precision"));
    config.version = config_.version + 1;
    config_ = std::move(config);
    return {};
}
Result<std::string> Model::display(const PropertyKey &key) const {
    auto q = value(key);
    if (!q)
        return std::unexpected(q.error());
    UnitsApi api{registry_};
    auto selected = api.schemas().select(config_.schema);
    if (!selected)
        return std::unexpected(selected.error());
    auto ref = config_.display_overrides.contains(key.name) ? config_.display_overrides.at(key.name)
               : q->unit() == Unit::length()                ? config_.display
                                                            : UnitRef{"", 1};
    auto text = api.format(*q, {config_.precision, config_.significant, config_.group_digits}, ref);
    if (!text)
        return std::unexpected(text.error());
    return text->text;
}
Result<void> Model::check_graph() const {
    std::map<PropertyKey, int> marks;
    std::function<Result<void>(const PropertyKey &)> visit =
        [&](const PropertyKey &k) -> Result<void> {
        if (marks[k] == 1)
            return std::unexpected(error("cycle", "formula dependency cycle"));
        if (marks[k] == 2)
            return {};
        if (!properties_.contains(k))
            return std::unexpected(error("reference", "unknown property in graph"));
        marks[k] = 1;
        const auto &p = properties_.at(k);
        if (p.formula)
            for (const auto &d : p.formula->dependencies) {
                auto v = visit(d);
                if (!v)
                    return v;
            }
        marks[k] = 2;
        return {};
    };
    for (const auto &[k, p] : properties_) {
        (void)p;
        auto v = visit(k);
        if (!v)
            return v;
    }
    return {};
}
std::vector<RecomputeStep> Model::recompute(bool all) {
    auto affected = dirty_;
    if (all)
        for (const auto &[k, p] : properties_) {
            (void)p;
            affected.insert(k);
        }
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto &[k, p] : properties_)
            if (p.formula && !affected.contains(k))
                for (const auto &d : p.formula->dependencies)
                    if (affected.contains(d)) {
                        affected.insert(k);
                        grew = true;
                        break;
                    }
    }
    std::set<PropertyKey> visited;
    std::vector<RecomputeStep> report;
    std::function<void(const PropertyKey &)> visit = [&](const PropertyKey &key) {
        if (!visited.insert(key).second)
            return;
        auto &p = properties_.at(key);
        if (p.formula) {
            for (const auto &d : p.formula->dependencies)
                if (affected.contains(d))
                    visit(d);
            auto q = evaluate(*p.formula, [&](const PropertyKey &d) { return value(d); });
            if (q) {
                auto v = validate(*q, p.spec);
                if (!v)
                    q = std::unexpected(v.error());
            }
            if (q) {
                p.value = *q;
                p.state = State::Valid;
                p.failure.reset();
            } else {
                p.state = q.error().code == "blocked" ? State::Blocked : State::Failed;
                p.failure = q.error();
            }
            report.push_back({key, p.state});
        }
    };
    for (const auto &k : affected)
        visit(k);
    dirty_.clear();
    last_report_ = report;
    return report;
}
const std::vector<RecomputeStep> &Model::last_report() const {
    return last_report_;
}
std::string Model::content() const {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "HIGH_WATER " << high_water_ << '\n';
    for (const auto &[k, v] : metadata_) {
        out << "META " << k.object << ' ' << std::quoted(k.name) << ' ' << v.index() << ' ';
        std::visit(
            [&](const auto &item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, std::string>)
                    out << std::quoted(item);
                else
                    out << item;
            },
            v);
        out << '\n';
    }
    for (const auto &[k, p] : properties_) {
        out << k.object << ' ' << std::quoted(k.name) << ' ' << describe(p.spec.unit) << ' '
            << p.spec.lower.has_value() << ' ';
        if (p.spec.lower)
            out << *p.spec.lower;
        out << ' ' << p.spec.upper.has_value() << ' ';
        if (p.spec.upper)
            out << *p.spec.upper;
        out << ' ';
        if (!p.formula)
            out << p.value.value();
        else {
            out << std::quoted(p.formula->source) << " ALIASES " << p.formula->aliases.size()
                << ' ';
            for (const auto &[alias, key] : p.formula->aliases)
                out << std::quoted(alias) << ' ' << key.object << ' ' << std::quoted(key.name);
            out << " UNITS " << p.formula->units.size() << ' ';
            for (const auto &b : p.formula->units)
                out << b.position << ' ' << b.length << ' ' << std::quoted(b.unit.id) << ' '
                    << b.unit.version << ' ';
        }
        out << '\n';
    }
    for (const auto &d : registry_.custom_definitions())
        out << std::quoted(d.id) << ' ' << d.version << ' ' << std::quoted(d.symbol) << ' '
            << describe(d.unit) << ' ' << d.scale << ' ' << std::quoted(d.source) << '\n';
    out << std::quoted(config_.schema) << ' ' << std::quoted(config_.input.id) << ' '
        << config_.input.version << ' ' << std::quoted(config_.display.id) << ' '
        << config_.display.version << ' ' << config_.input_follows_display << ' '
        << config_.precision << ' ' << config_.significant << ' ' << config_.group_digits << '\n';
    for (const auto *map : {&config_.input_overrides, &config_.display_overrides}) {
        out << map->size() << '\n';
        for (const auto &[key, ref] : *map)
            out << std::quoted(key) << ' ' << std::quoted(ref.id) << ' ' << ref.version << '\n';
    }
    return out.str();
}
Document::Document() : saved_(model_.content()) {
    static std::atomic<std::uint64_t> next{1};
    identity_ = next.fetch_add(1);
}
const Model &Document::model() const {
    return model_;
}
Result<Model> Document::preview(const Edit &edit) const {
    if (busy_)
        return std::unexpected(error("busy", "reentrant document operation"));
    try {
        auto candidate = model_;
        auto v = edit(candidate);
        if (!v)
            return std::unexpected(v.error());
        auto graph = candidate.check_graph();
        if (!graph)
            return std::unexpected(graph.error());
        candidate.recompute();
        return candidate;
    } catch (const std::exception &e) {
        return std::unexpected(error("exception", e.what()));
    }
}
Result<bool> Document::transact(const Edit &edit) {
    auto candidate = preview(edit);
    if (!candidate)
        return std::unexpected(candidate.error());
    if (candidate->content() == model_.content())
        return false;
    auto report = candidate->last_report();
    const bool model_change =
        candidate->properties().size() != model_.properties().size() || !report.empty() || [&] {
            for (const auto &[k, p] : model_.properties()) {
                const auto &q = candidate->properties().at(k);
                if (p.formula.has_value() != q.formula.has_value() ||
                    (!p.formula && p.value != q.value))
                    return true;
            }
            return false;
        }();
    history_.resize(position_);
    history_.push_back({model_, *candidate, model_change});
    ++position_;
    model_ = std::move(*candidate);
    report_ = std::move(report);
    ++revision_;
    publish(!model_change);
    return true;
}
Result<bool> Document::move_history(bool forward) {
    if (busy_)
        return std::unexpected(error("busy", "reentrant history operation"));
    if ((forward && position_ == history_.size()) || (!forward && position_ == 0))
        return false;
    const auto &h = history_.at(forward ? position_ : position_ - 1);
    model_ = forward ? h.after : h.before;
    report_ = h.model_change ? model_.recompute(true) : std::vector<RecomputeStep>{};
    if (forward)
        ++position_;
    else
        --position_;
    ++revision_;
    publish(!h.model_change);
    return true;
}
Result<bool> Document::undo() {
    return move_history(false);
}
Result<bool> Document::redo() {
    return move_history(true);
}
Result<void> Document::replace(Model model) {
    if (busy_)
        return std::unexpected(error("busy", "reentrant replace"));
    auto graph = model.check_graph();
    if (!graph)
        return graph;
    report_ = model.recompute(true);
    model_ = std::move(model);
    history_.clear();
    position_ = 0;
    ++revision_;
    ++generation_;
    saved_ = model_.content();
    publish(false);
    return {};
}
bool Document::modified() const {
    return saved_ != model_.content();
}
void Document::mark_saved() {
    saved_ = model_.content();
}
std::uint64_t Document::revision() const {
    return revision_;
}
std::uint64_t Document::generation() const {
    return generation_;
}
std::uint64_t Document::identity() const {
    return identity_;
}
std::size_t Document::history_size() const {
    return history_.size();
}
std::size_t Document::position() const {
    return position_;
}
const std::vector<RecomputeStep> &Document::last_report() const {
    return report_;
}
void Document::observe(std::function<void(const DocumentEvent &)> f) {
    observers_.push_back(std::move(f));
}
void Document::publish(bool format_only) {
    busy_ = true;
    const DocumentEvent event{revision_, report_, format_only};
    const auto observers = observers_;
    for (const auto &f : observers) {
        try {
            f(event);
        } catch (...) { /* 观察者异常不能撤销已提交文档。 */
        }
    }
    busy_ = false;
}
} // namespace units
