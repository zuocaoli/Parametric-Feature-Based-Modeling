cmake_minimum_required(VERSION 4.3)

# CMake 4.3.x opt-in for C++23 import std; update this token with CMake upgrades.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(document_month02_week04 LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "This course uses Clang. Configure with the default preset.")
endif()
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
    message(FATAL_ERROR "Clang 22 or newer is required for this course's modules.")
endif()
if(NOT 23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    message(FATAL_ERROR
        "C++23 import std is unavailable. Install Clang/clang-scan-deps 22 and "
        "the libstdc++ 16 standard-library module development files.")
endif()

set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wpedantic)
include(CTest)

add_executable(demo
    change_signal.cpp
    dependencies.cpp
    document.cpp
    engine.cpp
    evaluator.cpp
    lexer.cpp
    main.cpp
    parser.cpp
    property.cpp
    rectangle.cpp
    storage.cpp
)
target_sources(demo PRIVATE
    FILE_SET CXX_MODULES FILES
        dependencies.cppm
        document.cppm
        engine.cppm
        evaluator.cppm
        lexer.cppm
        parser.cppm
        property.cppm
        rectangle.cppm
        storage.cppm
)
