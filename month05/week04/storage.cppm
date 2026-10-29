export module month05.week04.storage;

import std;
export import month05.week04.document;

export namespace month05::week04 {
std::expected<void, EngineError> save_document(const Document &document, std::ostream &output);
std::expected<void, EngineError> load_document(Document &document, std::istream &input);
} // namespace month05::week04
