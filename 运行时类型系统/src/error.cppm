// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <string>

export module baseType:Error;


export namespace Base
{
    
enum class ErrorCode
{
    NotInitialized,
    SystemShutdown,
    EmptyName,
    InvalidParent,
    RegistrationConflict,
    UnknownType,
    NotConstructible,
    NotObjectType,
    NullFactoryResult,
    MissingLoader,
    MissingModuleName,
    LoadCycle,
    LoadFailed,
    LoadInProgress,
    TooManyTypes
};

struct Error
{
    ErrorCode code;
    std::string message;
};

}  // namespace Base
