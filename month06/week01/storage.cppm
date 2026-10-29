export module month06.week01.storage;

import std;
export import month06.week01.document;

export namespace month06::week01 {
std::expected<void, EngineError> save_document(const Document &document, std::ostream &output);
std::expected<void, EngineError> load_document(Document &document, std::istream &input);
} // namespace month06::week01
