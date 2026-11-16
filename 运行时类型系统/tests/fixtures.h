// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

// Include after importing baseType.
namespace fixtures
{
struct Root: Base::TypedClass<Root>
{
    int value = 7;
};
struct Child: Base::TypedClass<Child, Root>
{
};
struct Leaf: Base::TypedClass<Leaf, Child>
{
};
struct Unrelated: Base::TypedClass<Unrelated>
{
};
}  // namespace fixtures
