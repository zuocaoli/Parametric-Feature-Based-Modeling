// SPDX-License-Identifier: LGPL-2.1-or-later

#include <expected>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

import baseType;

struct Feature: Base::TypedClass<Feature>
{
    virtual std::string_view description() const = 0;
};

struct Box: Base::TypedClass<Box, Feature>
{
    double width = 10;
    std::string_view description() const override
    {
        return "Box";
    }
};

std::expected<void, Base::Error> run()
{
    using namespace Base;
    if (auto result = Type::init(); !result) {
        return std::unexpected(result.error());
    }
    if (auto result = BaseClass::init(); !result) {
        return std::unexpected(result.error());
    }
    if (auto result = Feature::init("Core::Feature"); !result) {
        return std::unexpected(result.error());
    }

    Type::setModuleLoader([](std::string_view module) -> std::expected<void, Error> {
        if (module != "Geometry") {
            return std::unexpected(Error {ErrorCode::LoadFailed, "Unsupported module"});
        }
        if (auto result = Box::init("Geometry::Box"); !result) {
            return std::unexpected(result.error());
        }
        return {};
    });
    auto object = Type::createObjectByName("Geometry::Box", true);
    if (!object) {
        return std::unexpected(object.error());
    }
    BaseClass* pointer = object->get();
    std::cout << "Factory object: " << pointer->getTypeId().getName() << '\n';

    Box stackObject;
    BaseClass* stackPointer = &stackObject;
    std::cout << "Stack object: " << stackPointer->getTypeId().getName()
              << ", is Feature=" << stackPointer->isDerivedFrom<Feature>()
              << ", is Box=" << stackPointer->is<Box>() << '\n';
    if (const auto* box = typeCast<Box*>(pointer)) {
        std::cout << box->description() << ", width=" << box->width << '\n';
    }
    std::vector<Type> types;
    Type::getAllDerivedFrom(Feature::getClassTypeId(), types);
    for (auto type : types) {
        std::cout << type.getName() << " constructible=" << type.canInstantiate() << '\n';
    }
    return {};
}

int main()
{
    const auto result = run(); 

    const auto shutdown = Base::Type::destruct();

    if (!result) {
        std::cerr << result.error().message << '\n';
    }

    if (!shutdown) {
        std::cerr << shutdown.error().message << '\n';
    }
    
    return result && shutdown ? 0 : 1;
}
