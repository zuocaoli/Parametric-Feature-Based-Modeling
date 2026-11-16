// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <climits>
#include <expected>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module baseType;

namespace Base
{
namespace
{
struct TypeData
{
    std::string name;
    Type type;
    Type parent;
    Type::InstantiationMethod factory;
    BaseClass* (*toBase)(void*) noexcept;
    const void* token;
};

enum class State
{
    Uninitialized,
    Active,
    Shutdown
};

struct TypeRegistry
{
    State state = State::Uninitialized;
    std::vector<std::unique_ptr<TypeData>> data;
    std::map<std::string, Type, std::less<>> names;
    std::map<const void*, Type> tokens;
    std::shared_ptr<Type::ModuleLoader> loader;
    std::set<std::string, std::less<>> loaded;
    std::set<std::string, std::less<>> loading;
};

TypeRegistry& registry()
{
    static TypeRegistry instance;
    return instance;
}

std::expected<void, Error> requireActive()
{
    switch (registry().state) {
        case State::Active:
            return {};
        case State::Shutdown:
            return std::unexpected(Error {ErrorCode::SystemShutdown, "Type system has shut down"});
        case State::Uninitialized:
            return std::unexpected(Error {ErrorCode::NotInitialized, "Call Type::init() first"});
    }
    std::unreachable();
}

const TypeData* record(Type type) noexcept
{
    const auto& store = registry();
    return store.state == State::Active && type.getKey() < store.data.size()
        ? store.data[type.getKey()].get()
        : nullptr;
}
}  // namespace

static_assert(sizeof(Type) == sizeof(Type::TypeId));
const Type Type::BadType;

std::expected<void, Error> Type::init()
{
    auto& store = registry();
    if (store.state == State::Shutdown) {
        return std::unexpected(Error {ErrorCode::SystemShutdown, "Cannot reuse retired type IDs"});
    }
    if (store.state == State::Active) {
        return {};
    }
    auto bad = std::make_unique<TypeData>("BadType", BadType, BadType, nullptr, nullptr, nullptr);
    store.names.emplace("BadType", BadType);
    try {
        store.data.push_back(std::move(bad));
    }
    catch (...) {
        store.names.clear();
        throw;
    }
    store.state = State::Active;
    return {};
}

std::expected<void, Error> Type::destruct()
{
    auto& store = registry();
    if (!store.loading.empty()) {
        return std::unexpected(Error {ErrorCode::LoadInProgress, "Cannot shut down inside a loader"});
    }
    store.state = State::Shutdown;
    store.names.clear();
    store.tokens.clear();
    store.data.clear();
    store.loaded.clear();
    store.loader.reset();
    BaseClass::mClassTypeId = BadType;
    return {};
}

std::string_view Type::getName() const noexcept
{
    const auto* entry = record(*this);
    return entry ? std::string_view(entry->name) : std::string_view();
}

Type Type::getParent() const noexcept
{
    const auto* entry = record(*this);
    return entry ? entry->parent : BadType;
}

bool Type::isDerivedFrom(Type parent) const noexcept
{
    if (parent.isBad() || !record(parent)) {
        return false;
    }
    for (auto current = *this; !current.isBad() && record(current); current = current.getParent()) {
        if (current == parent) {
            return true;
        }
    }
    return false;
}

bool Type::canInstantiate() const noexcept
{
    const auto* entry = record(*this);
    return entry && entry->factory;
}

std::expected<void*, Error> Type::createInstance() const
{
    if (auto active = requireActive(); !active) {
        return std::unexpected(active.error());
    }
    const auto* entry = record(*this);
    if (isBad() || !entry) {
        return std::unexpected(Error {ErrorCode::UnknownType, "Invalid type"});
    }
    const auto factory = entry->factory;
    if (!factory) {
        return std::unexpected(Error {ErrorCode::NotConstructible, "Type has no factory"});
    }
    auto* instance = factory();
    if (!instance) {
        return std::unexpected(Error {ErrorCode::NullFactoryResult, "Factory returned null"});
    }
    return instance;
}

std::expected<std::unique_ptr<BaseClass>, Error> Type::createObject() const
{
    if (auto active = requireActive(); !active) {
        return std::unexpected(active.error());
    }
    const auto* entry = record(*this);
    if (isBad() || !entry) {
        return std::unexpected(Error {ErrorCode::UnknownType, "Invalid type"});
    }
    const auto converter = entry->toBase;
    if (!converter) {
        return std::unexpected(
            Error {ErrorCode::NotObjectType, "Type is not registered as a BaseClass"}
        );
    }
    auto instance = createInstance();
    if (!instance) {
        return std::unexpected(instance.error());
    }
    return std::unique_ptr<BaseClass>(converter(*instance));
}

Type Type::fromName(std::string_view name)
{
    const auto& store = registry();
    const auto found = store.names.find(name);
    return found == store.names.end() ? BadType : found->second;
}

Type Type::fromKey(TypeId key) noexcept
{
    return record(Type(key)) ? Type(key) : BadType;
}

int Type::getNumTypes() noexcept
{
    return static_cast<int>(registry().data.size());
}

int Type::getAllDerivedFrom(Type parent, std::vector<Type>& list)
{
    int count = 0;
    for (const auto& entry : registry().data) {
        if (entry->type.isDerivedFrom(parent)) {
            list.push_back(entry->type);
            ++count;
        }
    }
    return count;
}

std::expected<Type, Error> Type::createType(Type parent, std::string_view name, InstantiationMethod factory)
{
    return registerType(parent, name, factory, nullptr, nullptr);
}

std::expected<Type, Error> Type::registerType(
    Type parent,
    std::string_view name,
    InstantiationMethod factory,
    BaseConverter converter,
    const void* token
)
{
    if (auto active = requireActive(); !active) {
        return std::unexpected(active.error());
    }
    if (name.empty()) {
        return std::unexpected(Error {ErrorCode::EmptyName, "Type name must not be empty"});
    }
    if (!parent.isBad() && !record(parent)) {
        return std::unexpected(Error {ErrorCode::InvalidParent, "Parent is not registered"});
    }
    auto& store = registry();
    if (const auto named = store.names.find(name); named != store.names.end()) {
        const auto* existing = record(named->second);
        if (!named->second.isBad() && existing->parent == parent && existing->factory == factory
            && existing->toBase == converter && existing->token == token) {
            return named->second;
        }
        return std::unexpected(
            Error {ErrorCode::RegistrationConflict, "Conflicting type name: " + std::string(name)}
        );
    }
    if (token && store.tokens.contains(token)) {
        return std::unexpected(
            Error {ErrorCode::RegistrationConflict, "C++ class already has a registered name"}
        );
    }
    if (store.data.size() >= INT_MAX) {
        return std::unexpected(Error {ErrorCode::TooManyTypes, "Type registry is full"});
    }
    const Type type(static_cast<TypeId>(store.data.size()));
    auto entry = std::make_unique<TypeData>(std::string(name), type, parent, factory, converter, token);
    const auto nameEntry = store.names.emplace(std::string(name), type).first;
    try {
        if (token) {
            store.tokens.emplace(token, type);
        }
        try {
            store.data.push_back(std::move(entry));
        }
        catch (...) {
            if (token) {
                store.tokens.erase(token);
            }
            throw;
        }
    }
    catch (...) {
        store.names.erase(nameEntry);
        throw;
    }
    return type;
}

void Type::setModuleLoader(ModuleLoader loader)
{
    registry().loader = loader ? std::make_shared<ModuleLoader>(std::move(loader)) : nullptr;
}

std::string Type::getModuleName(std::string_view name)
{
    const auto separator = name.find("::");
    return separator == std::string_view::npos ? std::string()
                                               : std::string(name.substr(0, separator));
}

std::expected<void, Error> Type::importModule(std::string_view name)
{
    const auto module = getModuleName(name);
    if (module.empty()) {
        return std::unexpected(Error {ErrorCode::MissingModuleName, "Expected Module::Type"});
    }
    if (module == "Base" || module == "Core" || module == "UI") {
        return {};
    }
    auto& store = registry();
    if (store.loading.contains(module)) {
        return std::unexpected(Error {ErrorCode::LoadCycle, "Recursive module load: " + module});
    }
    if (store.loaded.contains(module)) {
        return {};
    }
    auto loader = store.loader;
    if (!loader) {
        return std::unexpected(Error {ErrorCode::MissingLoader, "No module loader configured"});
    }
    const auto position = store.loading.insert(module).first;
    struct LoadingGuard
    {
        decltype(store.loading)& modules;
        decltype(position) entry;
        ~LoadingGuard()
        {
            modules.erase(entry);
        }
    } guard {store.loading, position};
    auto result = (*loader)(module);
    if (result) {
        store.loaded.insert(module);
    }
    return result;
}

std::expected<Type, Error> Type::resolve(std::string_view name, bool loadModule, bool allowMissing)
{
    if (auto active = requireActive(); !active) {
        return std::unexpected(active.error());
    }
    const std::string requested(name);
    if (loadModule) {
        if (auto result = importModule(requested); !result) {
            return std::unexpected(result.error());
        }
    }
    auto type = fromName(requested);
    if (type.isBad() && !allowMissing) {
        return std::unexpected(Error {ErrorCode::UnknownType, "Unknown type: " + requested});
    }
    return type;
}

std::expected<Type, Error> Type::getTypeIfDerivedFrom(std::string_view name, Type parent, bool loadModule)
{
    auto type = resolve(name, loadModule, true);
    if (!type) {
        return std::unexpected(type.error());
    }
    return type->isDerivedFrom(parent) ? *type : BadType;
}

std::expected<void*, Error> Type::createInstanceByName(std::string_view name, bool loadModule)
{
    auto type = resolve(name, loadModule);
    if (!type) {
        return std::unexpected(type.error());
    }
    return type->createInstance();
}

std::expected<std::unique_ptr<BaseClass>, Error> Type::createObjectByName(
    std::string_view name,
    bool loadModule
)
{
    auto type = resolve(name, loadModule);
    if (!type) {
        return std::unexpected(type.error());
    }
    return type->createObject();
}

}  // namespace Base
