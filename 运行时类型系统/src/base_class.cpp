// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <expected>

module baseType;

namespace Base
{
Type BaseClass::mClassTypeId;

std::expected<Type, Error> BaseClass::init()
{
    auto result = Type::registerType(
        Type::BadType,
        "Base::BaseClass",
        nullptr,
        [](void* pointer) noexcept { return static_cast<BaseClass*>(pointer); },
        &detail::typeToken<BaseClass>
    );
    if (result) {
        mClassTypeId = *result;
    }
    return result;
}

Type BaseClass::getClassTypeId() noexcept
{
    return mClassTypeId;
}
Type BaseClass::getTypeId() const noexcept
{
    return mClassTypeId;
}
bool BaseClass::isDerivedFrom(Type type) const noexcept
{
    return getTypeId().isDerivedFrom(type);
}
}  // namespace Base
