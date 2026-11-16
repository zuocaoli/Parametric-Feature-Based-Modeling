// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <concepts>
#include <expected>
#include <string_view>
#include <utility>

export module baseType:TypedClass;

export import :BaseClass;

export namespace Base
{
// Keeps type identity on each object while generating its registration boilerplate.
// Derived is incomplete here; validate its inheritance when init() is instantiated.
template<typename Derived, typename Parent = BaseClass>
    requires std::derived_from<Parent, BaseClass>
    && std::same_as<typename Parent::TypeSystemClass, Parent>
class TypedClass: public Parent
{
public:
    using TypeSystemClass = Derived;

    [[nodiscard]] static std::expected<Type, Error> init(std::string_view name)
    {
        static_assert(std::derived_from<Derived, TypedClass>);
        return BaseClass::initSubclass<Derived, Parent>(mClassTypeId, name);
    }

    [[nodiscard]] static Type getClassTypeId() noexcept
    {
        return mClassTypeId;
    }
    [[nodiscard]] Type getTypeId() const noexcept override
    {
        return mClassTypeId;
    }

protected:
    // Only the concrete class may construct this helper: slicing it would report
    // Derived's type for an object that is not actually a Derived.
    TypedClass() = default;
    TypedClass(const TypedClass&) = default;
    TypedClass& operator=(const TypedClass&) = default;
    TypedClass(TypedClass&&) = default;
    TypedClass& operator=(TypedClass&&) = default;
    ~TypedClass() override = default;

    template<typename... Args>
    explicit TypedClass(std::in_place_t, Args&&... args)
        : Parent(std::forward<Args>(args)...)
    {}

private:
    inline static Type mClassTypeId;
};

}  // namespace Base
