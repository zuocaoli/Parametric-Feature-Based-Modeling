import std;
import month04.week02.document;

namespace {
using namespace month04::week02;
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
        std::vector<std::unique_ptr<Command>> commands;
        commands.push_back(std::make_unique<SetProperty>(PropertyKey{1, "length"}, 200.0));
        commands.push_back(
            std::make_unique<SetProperty>(PropertyKey{1, "segments"}, std::int64_t{3}));
        require(document.execute(std::make_unique<CompositeCommand>(std::move(commands))));
        show("group", document);
        std::cout << "recomputed targets=" << document.model().last_report().size() << '\n';
        require(document.undo());
        show("undo group", document);
        commands.clear();
        commands.push_back(std::make_unique<SetProperty>(PropertyKey{1, "length"}, 300.0));
        commands.push_back(
            std::make_unique<SetProperty>(PropertyKey{1, "segments"}, std::int64_t{0}));
        auto failed = document.execute(std::make_unique<CompositeCommand>(std::move(commands)));
        std::cout << "rejected: " << failed.error().message << '\n';
        show("rollback", document);
        require(document.redo());
        show("redo group", document);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
