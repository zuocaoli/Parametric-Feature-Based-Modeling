// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <concepts>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>

export module baseType:BaseClass;

export import :Type;

export namespace Base
{
namespace detail
{
template<typename T>
inline constexpr char typeToken {};
}

class BaseClass
{
public:
    using TypeSystemClass = BaseClass;

    BaseClass() = default;
    BaseClass(const BaseClass&) = default;
    BaseClass& operator=(const BaseClass&) = default;
    BaseClass(BaseClass&&) = default;
    BaseClass& operator=(BaseClass&&) = default;
    virtual ~BaseClass() = default;

    [[nodiscard]] static std::expected<Type, Error> init();
    [[nodiscard]] static Type getClassTypeId() noexcept;
    [[nodiscard]] virtual Type getTypeId() const noexcept;
    [[nodiscard]] bool isDerivedFrom(Type type) const noexcept;

    template<typename T>
        requires std::derived_from<T, BaseClass> && std::same_as<typename T::TypeSystemClass, T>
    [[nodiscard]] bool is() const noexcept
    {
        const auto actual = getTypeId();
        return !actual.isBad() && actual == T::getClassTypeId();
    }

    template<typename T>
        requires std::derived_from<T, BaseClass> && std::same_as<typename T::TypeSystemClass, T>
    [[nodiscard]] bool isDerivedFrom() const noexcept
    {
        return isDerivedFrom(T::getClassTypeId());
    }

protected:
    template<typename Derived, typename Parent>
        requires std::derived_from<Derived, Parent> && std::derived_from<Parent, BaseClass>
    static std::expected<Type, Error> initSubclass(Type& classTypeId, std::string_view name)
    {
        const auto parent = Parent::getClassTypeId();
        if (parent.isBad()) {
            return std::unexpected(
                Error {ErrorCode::InvalidParent, "Initialize the parent before " + std::string(name)}
            );
        }
        Type::InstantiationMethod factory = nullptr;
        if constexpr (std::default_initializable<Derived>) {
            factory = []() -> void* {
                return new Derived();
            };
        }
        auto result = Type::registerType(
            parent,
            name,
            factory,
            [](void* pointer) noexcept -> BaseClass* {
                return static_cast<BaseClass*>(static_cast<Derived*>(pointer));
            },
            &detail::typeToken<Derived>
        );
        if (result) {
            classTypeId = *result;
        }
        return result;
    }

private:
    static Type mClassTypeId;
    friend class Type;
};

template<typename T>
concept ObjectPointer = std::is_pointer_v<T>
    && std::derived_from<std::remove_cv_t<std::remove_pointer_t<T>>, BaseClass>
    && std::same_as<typename std::remove_cv_t<std::remove_pointer_t<T>>::TypeSystemClass,
                    std::remove_cv_t<std::remove_pointer_t<T>>>
    && requires(BaseClass* pointer) { static_cast<T>(pointer); };

template<ObjectPointer T>
[[nodiscard]] T typeCast(BaseClass* object) noexcept
{
    using Target = std::remove_cv_t<std::remove_pointer_t<T>>;
    if (object && object->isDerivedFrom(Target::getClassTypeId())) {
        return static_cast<T>(object);
    }
    return nullptr;
}

template<ObjectPointer T>
[[nodiscard]] const std::remove_pointer_t<T>* typeCast(const BaseClass* object) noexcept
{
    using Target = std::remove_cv_t<std::remove_pointer_t<T>>;
    if (object && object->isDerivedFrom(Target::getClassTypeId())) {
        return static_cast<const Target*>(object);
    }
    return nullptr;
}

}  // namespace Base
