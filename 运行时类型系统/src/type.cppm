// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <compare>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

export module baseType:Type;

export import :Error;

export namespace Base
{
class BaseClass;

// A value handle into the one process-wide type registry.
class Type final
{
public:
    using TypeId = unsigned int;
    using InstantiationMethod = void* (*)();
    using ModuleLoader = std::function<std::expected<void, Error>(std::string_view)>;

    Type() = default;
    [[nodiscard]] TypeId getKey() const noexcept
    {
        return mIndex;
    }
    [[nodiscard]] bool isBad() const noexcept
    {
        return mIndex == 0;
    }
    auto operator<=>(const Type&) const = default;

    [[nodiscard]] std::string_view getName() const noexcept;
    [[nodiscard]] Type getParent() const noexcept;
    [[nodiscard]] bool isDerivedFrom(Type parent) const noexcept;
    [[nodiscard]] bool canInstantiate() const noexcept;
    [[nodiscard]] std::expected<void*, Error> createInstance() const;
    [[nodiscard]] std::expected<std::unique_ptr<BaseClass>, Error> createObject() const;

    [[nodiscard]] static Type fromName(std::string_view name);
    [[nodiscard]] static Type fromKey(TypeId key) noexcept;
    static int getAllDerivedFrom(Type parent, std::vector<Type>& list);
    [[nodiscard]] static int getNumTypes() noexcept;
    [[nodiscard]] static std::expected<Type, Error> getTypeIfDerivedFrom(
        std::string_view name,
        Type parent,
        bool loadModule = false
    );
    [[nodiscard]] static std::expected<Type, Error> createType(
        Type parent,
        std::string_view name,
        InstantiationMethod method = nullptr
    );
    [[nodiscard]] static std::expected<void*, Error> createInstanceByName(
        std::string_view name,
        bool loadModule = false
    );
    [[nodiscard]] static std::expected<std::unique_ptr<BaseClass>, Error> createObjectByName(
        std::string_view name,
        bool loadModule = false
    );

    [[nodiscard]] static std::expected<void, Error> init();
    [[nodiscard]] static std::expected<void, Error> destruct();
    static void setModuleLoader(ModuleLoader loader);
    [[nodiscard]] static std::string getModuleName(std::string_view name);
    static const Type BadType;

private:
    explicit Type(TypeId index) noexcept
        : mIndex(index)
    {}
    using BaseConverter = BaseClass* (*)(void*) noexcept;
    static std::expected<Type, Error> registerType(
        Type parent,
        std::string_view name,
        InstantiationMethod factory,
        BaseConverter converter,
        const void* token
    );
    static std::expected<Type, Error> resolve(
        std::string_view name,
        bool loadModule,
        bool allowMissing = false
    );
    static std::expected<void, Error> importModule(std::string_view name);
    TypeId mIndex {};
    friend class BaseClass;
};

}  // namespace Base
