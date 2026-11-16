import std;
import course.storage;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
std::string saved(Document &d) {
    std::ostringstream out;
    save_document(d, out).value();
    return out.str();
}
int main() try {
    Document d;
    auto content = d.model().content(), text = saved(d);
    check(text.starts_with("CAD_DOCUMENT\nINTERNAL_UNITS"),
          "document header records internal units");
    d.transact([](Model &m) {
         return m.set_value({1, "length"},
                            *Quantity::make(std::nextafter(1000., 2000.), Unit::length()));
     }).value();
    auto exact = d.model().value({1, "length"})->value();
    auto exact_text = saved(d);
    Document loaded;
    std::istringstream in{exact_text};
    load_document(loaded, in).value();
    check(loaded.model().value({1, "length"})->value() == exact, "max_digits10 roundtrip");
    std::istringstream base{text};
    load_document(d, base).value();
    check(d.model().content() == content && d.history_size() == 0 && !d.modified(),
          "authority and history restore");
    for (const auto &bad :
         {std::string{"invalid document"}, text.substr(0, 100), text + "garbage"}) {
        auto before = d.model().content();
        auto rev = d.revision();
        std::istringstream broken{bad};
        check(!load_document(d, broken) && d.model().content() == before && d.revision() == rev,
              "bad input protects current doc");
    }
    auto corrupt = text;
    auto pos = corrupt.find("BIND ");
    corrupt.replace(pos, 5, "BIND 999");
    std::istringstream bad_pin{corrupt};
    check(!load_document(d, bad_pin), "invalid literal binding");
    d.transact([](Model &m) -> Result<void> {
         auto r = m.register_unit({"pitch", 1, "pitch", Unit::length(), 10});
         if (!r)
             return r;
         return m.bind({1, "width"}, "2 pitch");
     }).value();
    auto custom = saved(d);
    std::istringstream custom_in{custom};
    load_document(loaded, custom_in).value();
    check(loaded.model().value({1, "width"})->value() == 20, "custom definition snapshot");
    d.transact([](Model &m) -> Result<void> {
         auto b = m.bind({1, "width"}, "length/ratio");
         if (!b)
             return b;
         return m.set_value({1, "ratio"}, *Quantity::make(0));
     }).value();
    auto failed = saved(d);
    std::istringstream failed_in{failed};
    load_document(loaded, failed_in).value();
    check(loaded.model().properties().at({1, "width"}).state == State::Failed &&
              loaded.model().properties().at({1, "area"}).state == State::Blocked,
          "failed states rebuilt");
    d.transact([](Model &m) { return m.set_value({1, "ratio"}, *Quantity::make(4)); }).value();
    std::ostream broken{nullptr};
    check(!save_document(d, broken) && d.modified(), "failed save retains baseline");
    auto file = std::filesystem::current_path() / "storage-roundtrip.cad";
    save_file(d, file).value();
    load_file(loaded, file).value();
    std::filesystem::remove(file);
    std::istringstream invalid_bounds{
        "CAD_DOCUMENT\nINTERNAL_UNITS \"mm kg s A K mol cd deg\"\nHIGH_WATER 1\nUNITS 0\n"
        "CONFIG \"Internal\" \"\" 1 \"\" 1 4 0 0 0\nINPUT_OVERRIDES 0\nDISPLAY_OVERRIDES 0\n"
        "PROPERTIES 1\nPROPERTY 1 \"tiny\" 0 0 0 0 0 0 0 0 1 2 1 1 FORMULA \"0\"\n"
        "ALIASES 0\nUNIT_BINDINGS 0\nMETADATA 0\nEND\n"};
    auto invalid = deserialize(invalid_bounds);
    check(!invalid && invalid.error().code == "constraint",
          "invalid bounds rejected before placeholder construction");
    for (auto formula : {"negate()", "negate(1,2)"}) {
        auto malformed = text;
        auto source = malformed.find("FORMULA \"length*width\"");
        check(source != std::string::npos, "area formula fixture exists");
        malformed.replace(source, std::string{"FORMULA \"length*width\""}.size(),
                          "FORMULA \"" + std::string{formula} + "\"");
        auto before = loaded.model().content();
        auto revision = loaded.revision();
        auto generation = loaded.generation();
        std::istringstream bad_formula{malformed};
        check(!load_document(loaded, bad_formula) && loaded.model().content() == before &&
                  loaded.revision() == revision && loaded.generation() == generation,
              "malformed internal operation fails atomically during load");
    }
    auto limits_file = std::filesystem::current_path() / "storage-limits.cad";
    for (auto kind : {std::string{"input"}, std::string{"display"}, std::string{"aliases"}}) {
        Document limits;
        auto set_count = [&](std::size_t count) {
            return limits.transact([&](Model &m) -> Result<void> {
                if (kind == "aliases") {
                    VariableMap aliases;
                    for (std::size_t i = 0; i < count; ++i)
                        aliases.emplace("alias" + std::to_string(i), PropertyKey{1, "length"});
                    return m.bind({1, "width"}, "length", std::move(aliases));
                }
                auto c = m.config();
                auto &overrides = kind == "input" ? c.input_overrides : c.display_overrides;
                overrides.clear();
                for (std::size_t i = 0; i < count; ++i)
                    overrides.emplace("property" + std::to_string(i), UnitRef{"mm", 1});
                return m.configure(c);
            });
        };
        set_count(1024).value();
        save_file(limits, limits_file).value();
        Document roundtrip;
        load_file(roundtrip, limits_file).value();
        check(roundtrip.model().content() == limits.model().content(),
              "1024 records roundtrip for " + kind);
        auto baseline = serialize(limits.model()).value();
        set_count(1025).value();
        auto revision = limits.revision();
        auto history = limits.history_size();
        auto before = limits.model().content();
        auto rejected = save_file(limits, limits_file);
        std::ifstream disk{limits_file, std::ios::binary};
        std::string bytes{std::istreambuf_iterator<char>{disk}, std::istreambuf_iterator<char>{}};
        check(!rejected && rejected.error().code == "size" && bytes == baseline &&
                  limits.modified() && limits.revision() == revision &&
                  limits.history_size() == history && limits.model().content() == before,
              "1025 records cannot overwrite file or change saved state for " + kind);
        std::ostringstream output;
        check(!save_document(limits, output) && output.str().empty() && limits.modified(),
              "oversized stream save writes nothing for " + kind);
        limits.undo().value();
        check(!limits.modified(), "failed save preserves previous baseline for " + kind);
    }
    std::filesystem::remove(limits_file);
    std::println("storage: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
