cmake_minimum_required(VERSION 4.3)

# CMake 4.3.x requires this opt-in before enabling C++ to discover import std.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

# Apply the same dialect to the standard-library module and course sources.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

project(month03_week01 LANGUAGES CXX)

if(NOT 23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    message(FATAL_ERROR
        "This course requires C++23 import std support. Use CMake 4.3.x, Ninja, "
        "Clang 22 and the GCC 16 libstdc++ module development files.")
endif()

add_executable(demo
    main.cpp
    lexer.cpp
    parser.cpp
    dependencies.cpp
)
target_sources(demo PRIVATE
    FILE_SET CXX_MODULES FILES
        lexer.cppm
        parser.cppm
        dependencies.cppm
)
set_target_properties(demo PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    CXX_MODULE_STD YES
    POSITION_INDEPENDENT_CODE YES
)
target_compile_options(demo PRIVATE -Wall -Wextra -Wpedantic)
