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
int main(int argc, char **argv) try {
    std::optional<std::filesystem::path> output, input;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help") {
            std::println("demo [--save file] | --load file");
            return 0;
        }
        if (i + 1 >= argc)
            return 2;
        if (arg == "--save")
            output = argv[++i];
        else if (arg == "--load")
            input = argv[++i];
        else
            return 2;
    }
    if (input && output)
        return 2;
    Application app;
    auto document = app.create_document();
    Document &d = *document;
    require(app.activate(d));
    if (input) {
        require(load_file(d, *input));
        show(d);
        return 0;
    }
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
    std::ostringstream stored;
    require(save_document(d, stored));
    Document reopened;
    std::istringstream content{stored.str()};
    require(load_document(reopened, content));
    show(reopened);
    auto preview = require(operations.preview(
        {d.revision(), {SetValue{{1, "length"}, require(Quantity::make(2000, Unit::length()))}}}));
    std::println("preview area={}, current area={}", preview.after.value({1, "area"})->value(),
                 d.model().value({1, "area"})->value());
    if (output) {
        require(save_file(d, *output));
        std::println("saved {}", output->string());
    }
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
