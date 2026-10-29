import std;
import month05.week02.storage;

namespace {
using namespace month05::week02;
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
            throw std::invalid_argument("usage: console-demo");
        }
        Document document;
        require(document.execute(std::make_unique<CreateRectangle>(20, 3)));
        require(document.execute(std::make_unique<BindFormula>(
            PropertyKey{1, "width"}, "source / 2", VariableMap{{"source", {2, "length"}}})));
        show("before save", document);
        const auto root = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
        const auto path = root / "month05/week02/build/document.txt";
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream output{path};
            auto result = save_document(document, output);
            if (!result) {
                throw std::runtime_error(result.error().message);
            }
            output.flush();
            if (!output) {
                throw std::runtime_error("file flush failed");
            }
        }
        std::cout << "saved month05/week02/build/document.txt\n";
        Document restored;
        std::ifstream input{path};
        auto result = load_document(restored, input);
        if (!result) {
            throw std::runtime_error(result.error().message);
        }
        show("loaded", restored);
        require(restored.execute(std::make_unique<SetProperty>(PropertyKey{2, "length"}, 40.0)));
        show("continued edit", restored);
        std::istringstream corrupt{"CAD_DOCUMENT 99"};
        result = load_document(restored, corrupt);
        std::cout << "rejected load: " << result.error().message << '\n';
        show("preserved", restored);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
