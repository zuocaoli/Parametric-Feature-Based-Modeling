import std;
import course.storage;
using namespace units;
template <class T> T require(Result<T> r) {
    if (!r)
        throw std::runtime_error(diagnostic(r.error()));
    return std::move(*r);
}
void require(Result<void> r) {
    if (!r)
        throw std::runtime_error(diagnostic(r.error()));
}
void show(const Document &d) {
    for (const auto &[key, p] : d.model().properties())
        std::println("{}.{}={} {} [{}]", key.object, key.name, p.value.value(),
                     internal_symbol(p.value.unit()), state_name(p.state));
    std::println("modified={}, history={}", d.modified(), d.history_size());
}
int main() try {
    const std::filesystem::path document_path{"build/rectangle.cad"};
    Application app;
    auto document = app.create_document();
    Document &d = *document;
    require(app.activate(d));
    std::ostringstream baseline;
    require(save_document(d, baseline));
    auto context = require(app.begin_edit(d, {1, "length"}));
    require(app.commit(d, context, "2 m"));
    show(d);
    require(d.undo());
    Operations operations{d};
    auto config = d.model().config();
    config.schema = "ImperialDecimal";
    require(operations.apply({d.revision(), {SetConfig{config}}}));
    std::println("display={}, recomputed={}, modified={}", require(app.display(d, {1, "length"})),
                 d.last_report().size(), d.modified());
    require(save_file(d, document_path));
    std::println("saved {}", document_path.string());
    Document reopened;
    require(load_file(reopened, document_path));
    std::println("loaded {}", document_path.string());
    show(reopened);
    auto preview = require(operations.preview(
        {d.revision(), {SetValue{{1, "length"}, require(Quantity::make(2000, Unit::length()))}}}));
    std::println("preview area={}, current area={}", preview.after.value({1, "area"})->value(),
                 d.model().value({1, "area"})->value());
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
