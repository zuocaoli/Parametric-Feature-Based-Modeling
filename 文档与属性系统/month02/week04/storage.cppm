export module month04.week04.storage;

import std;
export import month04.week04.document;

export namespace month04::week04 {
std::expected<void, EngineError> save_document(const Document &document, std::ostream &output);
std::expected<void, EngineError> load_document(Document &document, std::istream &input);
} // namespace month04::week04
