// SPDX-License-Identifier: LGPL-2.1-or-later
#include <concepts>
#include <expected>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

import baseType;

#include "fixtures.h"

Base::Type registerFromOtherTranslationUnit();
bool inspectFromOtherTranslationUnit(const Base::BaseClass* object);

using namespace Base;
using namespace fixtures;

namespace
{
#define CHECK(...) check(bool((__VA_ARGS__)), #__VA_ARGS__, __LINE__)

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        throw std::runtime_error(std::string(expression) + " at line " + std::to_string(line));
    }
}

template<typename T>
void expectError(const std::expected<T, Error>& result, ErrorCode code)
{
    CHECK(!result);
    CHECK(result.error().code == code);
    CHECK(!result.error().message.empty());
}

static_assert(sizeof(Type) == sizeof(Type::TypeId));
static_assert(std::is_trivially_copyable_v<Type>);
static_assert(std::has_virtual_destructor_v<BaseClass>);
static_assert(!std::default_initializable<TypedClass<Root>>);
struct UnregisteredChild: Root
{
};
static_assert(!ObjectPointer<UnregisteredChild*>);
static_assert(
    std::same_as<decltype(typeCast<Child*>(std::declval<const BaseClass*>())), const Child*>
);

void initialization()
{
    CHECK(Type::getNumTypes() == 0);
    CHECK(BaseClass::getClassTypeId().isBad());
    Root uninitialized;
    CHECK(uninitialized.getTypeId().isBad());
    CHECK(!uninitialized.is<Root>());
    CHECK(!uninitialized.isDerivedFrom<BaseClass>());
    expectError(Type::createType(Type::BadType, "BeforeInit"), ErrorCode::NotInitialized);
    expectError(Type::createObjectByName("BeforeInit"), ErrorCode::NotInitialized);
    CHECK(Type::init());
    CHECK(Type::getNumTypes() == 1);
    CHECK(Type::BadType.getName() == "BadType");
    CHECK(!Type::BadType.canInstantiate());
    CHECK(Type::BadType.getParent().isBad());
    CHECK(!Type::BadType.isDerivedFrom(Type::BadType));
    expectError(Root::init("Test::Root"), ErrorCode::InvalidParent);
    CHECK(BaseClass::init());
    CHECK(Type::getNumTypes() == 2);
    CHECK(BaseClass::getClassTypeId() == Type::fromName("Base::BaseClass"));
    CHECK(Type::init());  // Idempotent, never erases existing registrations.
    CHECK(BaseClass::init());
    CHECK(Type::getNumTypes() == 2);
    BaseClass base;
    CHECK(base.getTypeId() == BaseClass::getClassTypeId());
    CHECK(base.is<BaseClass>());
    CHECK(!BaseClass::getClassTypeId().canInstantiate());
}

void registration()
{
    CHECK(Type::fromName("missing").isBad());
    CHECK(Type::fromKey(0).isBad());
    CHECK(Type::fromKey(1000).isBad());
    expectError(Type::createType(Type::BadType, ""), ErrorCode::EmptyName);
    expectError(Child::init("Test::Child"), ErrorCode::InvalidParent);
    const auto root = Root::init("Test::Root").value();
    const auto child = Child::init("Test::Child").value();
    const auto leaf = Leaf::init("Test::Leaf").value();
    const auto unrelated = Unrelated::init("Test::Unrelated").value();
    CHECK(root == Type::fromName("Test::Root"));
    CHECK(root == Type::fromKey(root.getKey()));
    CHECK(root.getParent() == BaseClass::getClassTypeId());
    CHECK(child.getParent() == root);
    CHECK(leaf.isDerivedFrom(root));
    CHECK(leaf.isDerivedFrom(child));
    CHECK(root.isDerivedFrom(root));
    CHECK(!root.isDerivedFrom(leaf));
    CHECK(!leaf.isDerivedFrom(unrelated));
    CHECK(!leaf.isDerivedFrom(Type::BadType));
    std::vector<Type> types {unrelated};
    CHECK(Type::getAllDerivedFrom(root, types) == 3);
    CHECK(types == std::vector<Type>({unrelated, root, child, leaf}));
    CHECK(Type::getAllDerivedFrom(Type::BadType, types) == 0);
    const auto count = Type::getNumTypes();
    CHECK(Root::init("Test::Root").value() == root);
    expectError(Root::init("Alias"), ErrorCode::RegistrationConflict);
    expectError(Child::init("Test::Root"), ErrorCode::RegistrationConflict);
    expectError(Type::createType(Type::BadType, "Test::Root"), ErrorCode::RegistrationConflict);
    expectError(Type::createType(Type::BadType, "BadType"), ErrorCode::RegistrationConflict);
    CHECK(Type::getNumTypes() == count);
    CHECK(Type::getTypeIfDerivedFrom("Test::Leaf", root).value() == leaf);
    CHECK(Type::getTypeIfDerivedFrom("Test::Unrelated", root).value().isBad());
    CHECK(Type::getTypeIfDerivedFrom("Unknown", root).value().isBad());
}

void objectIdentity()
{
    Leaf stack;
    BaseClass* base = &stack;
    CHECK(base->getTypeId() == Leaf::getClassTypeId());
    CHECK(base->is<Leaf>());
    CHECK(!base->is<Root>());
    CHECK(base->isDerivedFrom<Root>());
    CHECK(base->isDerivedFrom(BaseClass::getClassTypeId()));
    CHECK(typeCast<Leaf*>(base) == &stack);
    CHECK(typeCast<Root*>(base) == static_cast<Root*>(&stack));
    CHECK(!typeCast<Unrelated*>(base));
    CHECK(!typeCast<Child*>(static_cast<BaseClass*>(nullptr)));
    const BaseClass* constant = base;
    CHECK(typeCast<Leaf*>(constant) == &stack);
    std::unique_ptr<BaseClass> directlyCreated = std::make_unique<Child>();
    CHECK(directlyCreated->getTypeId() == Child::getClassTypeId());
    auto factoryCreated = Type::createObjectByName("Test::Child").value();
    CHECK(factoryCreated->getTypeId() == directlyCreated->getTypeId());
    CHECK(inspectFromOtherTranslationUnit(directlyCreated.get()));
    CHECK(inspectFromOtherTranslationUnit(factoryCreated.get()));
    CHECK(registerFromOtherTranslationUnit() == Child::getClassTypeId());
}

struct Abstract: TypedClass<Abstract>
{
    virtual int value() const = 0;
};
struct Concrete: TypedClass<Concrete, Abstract>
{
    int value() const override
    {
        return 42;
    }
};
struct NeedsArgument: TypedClass<NeedsArgument>
{
    explicit NeedsArgument(int number)
        : value(number)
    {}
    int value;
};
struct ArgumentChild: TypedClass<ArgumentChild, NeedsArgument>
{
    ArgumentChild()
        : TypedClass(std::in_place, 5)
    {}
};
struct Throws: TypedClass<Throws>
{
    Throws()
    {
        throw std::runtime_error("constructor failure");
    }
};

void factories()
{
    CHECK(Abstract::init("Test::Abstract"));
    CHECK(Concrete::init("Test::Concrete"));
    CHECK(NeedsArgument::init("Test::NeedsArgument"));
    CHECK(ArgumentChild::init("Test::ArgumentChild"));
    CHECK(Throws::init("Test::Throws"));
    CHECK(!Abstract::getClassTypeId().canInstantiate());
    CHECK(!NeedsArgument::getClassTypeId().canInstantiate());
    CHECK(Concrete::getClassTypeId().canInstantiate());
    expectError(Type::BadType.createInstance(), ErrorCode::UnknownType);
    expectError(Abstract::getClassTypeId().createInstance(), ErrorCode::NotConstructible);
    expectError(Abstract::getClassTypeId().createObject(), ErrorCode::NotConstructible);
    expectError(Type::createObjectByName("Test::NeedsArgument"), ErrorCode::NotConstructible);
    expectError(Type::createInstanceByName("Missing"), ErrorCode::UnknownType);
    auto concrete = Concrete::getClassTypeId().createObject().value();
    CHECK(typeCast<Abstract*>(concrete.get())->value() == 42);
    auto raw = Type::createInstanceByName("Test::Concrete").value();
    std::unique_ptr<Concrete> owned(static_cast<Concrete*>(raw));
    CHECK(owned->getTypeId() == Concrete::getClassTypeId());
    NeedsArgument stack(1);
    CHECK(stack.is<NeedsArgument>());
    auto argumentChild = ArgumentChild::getClassTypeId().createObject().value();
    CHECK(typeCast<NeedsArgument*>(argumentChild.get())->value == 5);
    bool threw = false;
    try {
        (void)Throws::getClassTypeId().createObject();
    }
    catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "constructor failure";
    }
    CHECK(threw);
}

struct Plain
{
    int value = 11;
};
void* makePlain()
{
    return new Plain();
}
void* makeNull()
{
    return nullptr;
}

void independentBranch()
{
    const auto root = Type::createType(Type::BadType, "Core::Extension").value();
    const auto child = Type::createType(root, "Core::PlainExtension", makePlain).value();
    CHECK(child.isDerivedFrom(root));
    CHECK(!child.isDerivedFrom(BaseClass::getClassTypeId()));
    CHECK(child.getParent() == root);
    CHECK(Type::createType(root, "Core::PlainExtension", makePlain).value() == child);
    expectError(Type::createType(root, "Core::PlainExtension", makeNull), ErrorCode::RegistrationConflict);
    std::unique_ptr<Plain> plain(static_cast<Plain*>(child.createInstance().value()));
    CHECK(plain->value == 11);
    expectError(child.createObject(), ErrorCode::NotObjectType);
    auto nullType = Type::createType(root, "Core::NullExtension", makeNull).value();
    CHECK(nullType.canInstantiate());
    expectError(nullType.createInstance(), ErrorCode::NullFactoryResult);
}

struct Prefix
{
    virtual ~Prefix() = default;
    int padding = 3;
};
struct Offset: Prefix, TypedClass<Offset, Root>
{
    inline static int destroyed = 0;
    ~Offset() override
    {
        ++destroyed;
    }
};

void pointerAdjustmentAndOwnership()
{
    CHECK(Offset::init("Test::Offset"));
    Offset::destroyed = 0;
    {
        Offset stack;
        BaseClass* base = &stack;
        CHECK(static_cast<void*>(base) != static_cast<void*>(&stack));
        CHECK(typeCast<Offset*>(base) == &stack);
        CHECK(base->getTypeId() == Offset::getClassTypeId());
        auto owned = Offset::getClassTypeId().createObject().value();
        CHECK(typeCast<Offset*>(owned.get()));
        CHECK(owned->is<Offset>());
        auto moved = std::move(owned);
        CHECK(!owned);
        CHECK(moved->isDerivedFrom<Root>());
    }
    CHECK(Offset::destroyed == 2);
}

struct Loaded: TypedClass<Loaded>
{
};
struct RetryRoot: TypedClass<RetryRoot>
{
};
struct Retried: TypedClass<Retried, RetryRoot>
{
};

void loading()
{
    CHECK(Type::getModuleName("Geometry::Nested::Box") == "Geometry");
    CHECK(Type::getModuleName("Box").empty());
    expectError(Type::createObjectByName("Box", true), ErrorCode::MissingModuleName);
    expectError(Type::createObjectByName("Geometry::Loaded", true), ErrorCode::MissingLoader);
    expectError(Type::createObjectByName("Core::Missing", true), ErrorCode::UnknownType);
    int calls = 0;
    Type::setModuleLoader([&](std::string_view module) -> std::expected<void, Error> {
        ++calls;
        CHECK(module == "Geometry");
        expectError(Type::destruct(), ErrorCode::LoadInProgress);
        CHECK(Loaded::init("Geometry::Loaded"));
        return {};
    });
    CHECK(Type::fromName("Geometry::Loaded").isBad());
    expectError(Type::createObjectByName("Geometry::Loaded"), ErrorCode::UnknownType);
    CHECK(calls == 0);
    CHECK(Type::createObjectByName("Geometry::Loaded", true));
    CHECK(Type::createObjectByName("Geometry::Loaded", true));
    CHECK(
        Type::getTypeIfDerivedFrom("Geometry::Loaded", BaseClass::getClassTypeId(), true).value()
        == Loaded::getClassTypeId()
    );
    expectError(Type::createObjectByName("Geometry::Missing", true), ErrorCode::UnknownType);
    CHECK(calls == 1);
    // loadModule=true requests import even when the type already exists.
    CHECK(Type::createType(Type::BadType, "External::AlreadyPresent", makePlain));
    Type::setModuleLoader([&](std::string_view module) -> std::expected<void, Error> {
        CHECK(module == "External");
        ++calls;
        return {};
    });
    std::unique_ptr<Plain> plain(
        static_cast<Plain*>(Type::createInstanceByName("External::AlreadyPresent", true).value())
    );
    CHECK(calls == 2);
    Type::setModuleLoader({});
}

void loadFailures()
{
    Type::setModuleLoader([](std::string_view) -> std::expected<void, Error> {
        return std::unexpected(Error {ErrorCode::UnknownType, "loader lookup failed"});
    });
    expectError(
        Type::getTypeIfDerivedFrom("LoaderError::Thing", BaseClass::getClassTypeId(), true),
        ErrorCode::UnknownType
    );
    Type::setModuleLoader([calls = 0](std::string_view) mutable -> std::expected<void, Error> {
        CHECK(RetryRoot::init("Retry::Root"));
        if (++calls == 1) {
            return std::unexpected(Error {ErrorCode::LoadFailed, "try again"});
        }
        CHECK(calls == 2);
        CHECK(Retried::init("Retry::Retried"));
        return {};
    });
    expectError(Type::createObjectByName("Retry::Retried", true), ErrorCode::LoadFailed);
    CHECK(!Type::fromName("Retry::Root").isBad());
    CHECK(Type::createObjectByName("Retry::Retried", true));
    Type::setModuleLoader([](std::string_view module) -> std::expected<void, Error> {
        auto result = Type::createObjectByName(module == "A" ? "B::Thing" : "A::Thing", true);
        CHECK(!result);
        return std::unexpected(result.error());
    });
    expectError(Type::createObjectByName("A::Thing", true), ErrorCode::LoadCycle);
    Type::setModuleLoader([](std::string_view) -> std::expected<void, Error> { return {}; });
    expectError(Type::createObjectByName("A::Thing", true), ErrorCode::UnknownType);
    Type::setModuleLoader([](std::string_view) -> std::expected<void, Error> {
        throw std::runtime_error("loader failure");
    });
    bool threw = false;
    try {
        (void)Type::createObjectByName("Throw::Thing", true);
    }
    catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "loader failure";
    }
    CHECK(threw);
    Type::setModuleLoader([](std::string_view) -> std::expected<void, Error> { return {}; });
    expectError(Type::createObjectByName("Throw::Thing", true), ErrorCode::UnknownType);
    Type::setModuleLoader({});
}

void loaderReplacement()
{
    int replacementCalls = 0;
    Type::setModuleLoader([&](std::string_view) -> std::expected<void, Error> {
        Type::setModuleLoader([&](std::string_view) -> std::expected<void, Error> {
            ++replacementCalls;
            return {};
        });
        return {};
    });
    expectError(Type::createObjectByName("First::Missing", true), ErrorCode::UnknownType);
    expectError(Type::createObjectByName("Second::Missing", true), ErrorCode::UnknownType);
    CHECK(replacementCalls == 1);
    Type::setModuleLoader({});
    expectError(Type::createObjectByName("New::Missing", true), ErrorCode::MissingLoader);
}

void shutdown()
{
    const auto previous = Root::getClassTypeId();
    CHECK(Type::destruct());
    CHECK(Type::getNumTypes() == 0);
    CHECK(Type::fromName("Test::Root").isBad());
    CHECK(Type::fromKey(previous.getKey()).isBad());
    CHECK(previous.getName().empty());
    CHECK(!previous.isDerivedFrom(previous));
    CHECK(!previous.canInstantiate());
    expectError(previous.createInstance(), ErrorCode::SystemShutdown);
    expectError(Type::init(), ErrorCode::SystemShutdown);
    CHECK(Type::destruct());
}
}  // namespace

int main()
{
    struct Test
    {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"initialization", initialization},
        {"registration", registration},
        {"object identity and cross-TU registry", objectIdentity},
        {"factories", factories},
        {"independent type branch", independentBranch},
        {"pointer adjustment and ownership", pointerAdjustmentAndOwnership},
        {"module loading", loading},
        {"load failures and state", loadFailures},
        {"loader replacement", loaderReplacement},
        {"shutdown", shutdown}
    };
    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    return failures ? 1 : 0;
}
