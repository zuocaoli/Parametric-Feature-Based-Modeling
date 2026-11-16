export module course.application;
import std;
export import course.operations;
export namespace units {
struct UnitPreferences {
    std::string schema = "Internal";
    QuantityFormat format;
    bool ignore_project_schema = false;
};
class Application {
  public:
    Result<void> set_preferences(UnitPreferences preferences);
    const UnitPreferences &preferences() const;
    std::unique_ptr<Document> create_document() const;
    Result<void> activate(const Document &document);
    UnitConfig effective_config(const Document &document) const;
    Result<std::string> display(const Document &document, const PropertyKey &key) const;
    Result<InputContext> begin_edit(const Document &document, const PropertyKey &key) const;
    Result<bool> commit(Document &document, const InputContext &context,
                        std::string_view source) const;

  private:
    UnitsApi service_;
    UnitPreferences preferences_;
    std::uint64_t active_ = 0;
};
} // namespace units
