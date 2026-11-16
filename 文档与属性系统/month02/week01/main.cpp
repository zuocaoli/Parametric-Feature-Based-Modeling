import std;
import month04.week01.document;

namespace {
using namespace month04::week01;
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
        show("initial", document);
        require(document.execute(std::make_unique<SetProperty>(PropertyKey{1, "length"}, 200.0)));
        show("edit", document);
        require(document.undo());
        show("undo", document);
        require(document.redo());
        show("redo", document);
        require(document.undo());
        require(document.execute(std::make_unique<SetProperty>(PropertyKey{1, "length"}, 150.0)));
        show("new branch", document);
        std::cout << "redo available=" << std::boolalpha << document.redo().value() << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
