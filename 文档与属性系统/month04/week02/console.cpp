import std;
import month06.week02.storage;

namespace month06::week02 {}
namespace {
using namespace month06::week02;
void require(std::expected<bool, EngineError> result) {
    if (!result || !*result)
        throw std::runtime_error(result ? "expected change" : result.error().message);
}
void show(std::string_view label, const Document &document) {
    std::cout << label << ": length=" << std::get<double>(*document.model().value({2, "length"}))
              << ", width=" << std::get<double>(*document.model().value({2, "width"}))
              << ", area=" << std::get<double>(*document.model().value({2, "area"}))
              << ", modified=" << document.modified() << '\n';
}
} // namespace
int main(int argc, char **) {
    using namespace month06::week02;
    try {

        if (argc != 1)
            throw std::invalid_argument("usage: console-demo");
        Document document;
        require(document.execute(std::make_unique<CreateRectangle>(80, 40)));
        require(document.execute(
            std::make_unique<SetProperty>(PropertyKey{2, "name"}, std::string{"A"})));
        require(
            document.execute(std::make_unique<BindFormula>(PropertyKey{2, "width"}, "length / 2")));
        require(document.execute(std::make_unique<SetProperty>(PropertyKey{2, "length"}, 200.0)));

        show("edited A", document);
        require(document.undo());
        show("undo", document);
        require(document.redo());
        show("redo", document);
        const auto root = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
        const auto path = root / "month04/week02/build/demo.cad";
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream output{path};
            auto result = save_document(document, output);
            if (!result)
                throw std::runtime_error(result.error().message);
            output.flush();
            if (!output)
                throw std::runtime_error("save flush failed");
            output.close();
            if (!output)
                throw std::runtime_error("save close failed");
        }
        document.mark_saved();
        show("saved", document);
        Document restored;
        std::ifstream input{path};
        load_document(restored, input).value();
        show("loaded", restored);
        if (!same_content(document.model().snapshot(), restored.model().snapshot()))
            throw std::runtime_error("roundtrip mismatch");
        std::istringstream corrupt{"CAD_DOCUMENT 99"};
        if (load_document(restored, corrupt))
            throw std::runtime_error("corrupt load accepted");
        show("failed load preserved A", restored);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
