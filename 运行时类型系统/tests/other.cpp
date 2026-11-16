// SPDX-License-Identifier: LGPL-2.1-or-later
import baseType;

#include "fixtures.h"

Base::Type registerFromOtherTranslationUnit()
{
    return fixtures::Child::init("Test::Child").value();
}

bool inspectFromOtherTranslationUnit(const Base::BaseClass* object)
{
    const auto* child = Base::typeCast<fixtures::Child*>(object);
    return child && child->value == 7 && object->getTypeId() == Base::Type::fromName("Test::Child");
}
