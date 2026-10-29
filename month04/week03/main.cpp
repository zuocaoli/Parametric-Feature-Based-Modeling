import std;
import month04.week03.document;

namespace {
using namespace month04::week03;
void require(std::expected<bool, EngineError> result) {
    if (!result || !*result) {
        throw std::runtime_error(result ? "operation made no change" : result.error().message);
    }
}
void show(std::string_view label, const Document &document) {
    const auto &model = document.model();
    std::cout << label << ": length=" << std::get<double>(*model.value({1, "length"}))
              << ", width=" << std::get<double>(*model.value({1, "width"}))
              << ", area=" << std::get<double>(*model.value({1, "area"}))
              << ", history=" << document.position() << '/' << document.history_size() << '\n';
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }
        Document document;
        require(document.execute(std::make_unique<CreateRectangle>(20, 3)));
        require(document.execute(std::make_unique<BindFormula>(
            PropertyKey{1, "width"}, "source / 2", VariableMap{{"source", {2, "length"}}})));
        show("cross reference", document);
        require(document.execute(std::make_unique<DeleteRectangle>(2)));
        std::cout << "delete source: width="
                  << state_name(document.model().status({1, "width"}).state)
                  << ", area=" << state_name(document.model().status({1, "area"}).state) << '\n';
        require(document.undo());
        show("undo deletion", document);
        std::cout << "restored source id=2, area="
                  << std::get<double>(*document.model().value({2, "area"})) << '\n';
        require(document.redo());
        require(document.execute(std::make_unique<CreateRectangle>(20, 3)));
        std::cout << "replacement id=" << document.model().high_water()
                  << ", width=" << state_name(document.model().status({1, "width"}).state) << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
