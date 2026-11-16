export module course.storage;
import std;
export import course.application;
export namespace units {
Result<std::string> serialize(const Model &model);
Result<Model> deserialize(std::istream &input);
Result<void> save_document(Document &document, std::ostream &output);
Result<void> load_document(Document &document, std::istream &input);
Result<void> save_file(Document &document, const std::filesystem::path &path);
Result<void> load_file(Document &document, const std::filesystem::path &path);
struct RectangleExchange {
    std::string unit;
    double length, width, tolerance;
    std::array<double, 3> normal{0, 0, 1};
};
struct ImportedRectangle {
    Quantity length, width, tolerance;
    std::array<double, 3> normal;
};
Result<RectangleExchange> export_rectangle(const Model &model, std::string unit,
                                           const Quantity &tolerance);
Result<ImportedRectangle> import_rectangle(const RectangleExchange &data,
                                           const UnitRegistry &registry);
} // namespace units
