export module week04.document;

import std;
export import week04.property;

export namespace week04 {

// Document 负责为对象分配稳定 ID，并统一创建、查找、列出和删除对象。
// 它用强引用保存对象；
// find() 返回弱引用，使用前需要调用 lock()。
// 删除后 ID 不会复用.
class Document {
public:
    Document() = default;
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    ObjectId create_rectangle(double length = 5.0, double width = 3.0);
    ObjectId create_annotation(std::string text);
    std::weak_ptr<PropertyObject> find(ObjectId id) const;
    bool erase(ObjectId id);
    std::vector<ObjectId> ids() const;

private:
    template <class T, class... Args> ObjectId create(Args &&...args);

    ObjectId next_id_ = 1;
    std::map<ObjectId, std::shared_ptr<PropertyObject>> objects_;
};
} // namespace week04
