# SPDX-License-Identifier: LGPL-2.1-or-later
cmake_minimum_required(VERSION 3.28)
project(BaseType LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_library(baseType STATIC)
target_compile_features(baseType PUBLIC cxx_std_23)
target_sources(baseType
    PUBLIC FILE_SET CXX_MODULES FILES
        src/base_type.cppm
        src/error.cppm
        src/type.cppm
        src/base_class.cppm
        src/typed_class.cppm
    PRIVATE
        src/type.cpp
        src/base_class.cpp
)
target_compile_options(baseType PRIVATE -Wall -Wextra -Wpedantic)

add_executable(baseTypeExample examples/main.cpp)
target_link_libraries(baseTypeExample PRIVATE baseType)
target_compile_options(baseTypeExample PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(baseTypeTests tests/main.cpp tests/other.cpp)
    target_link_libraries(baseTypeTests PRIVATE baseType)
    target_compile_options(baseTypeTests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME baseTypeContract COMMAND baseTypeTests)
    add_test(NAME baseTypeExample COMMAND baseTypeExample)
endif()
