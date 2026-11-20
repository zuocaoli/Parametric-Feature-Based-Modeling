cmake_minimum_required(VERSION 4.3)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(parametric_feature_modeling LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
    message(FATAL_ERROR "Use Clang 22 or newer through the default preset.")
endif()
if(NOT 23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    message(FATAL_ERROR "Install matching clang-scan-deps and libstdc++ development files for import std.")
endif()

set(CMAKE_CXX_MODULE_STD ON)
add_compile_options(-Wall -Wextra -Wpedantic)

if(NOT DEFINED OpenCASCADE_DIR)
    set(OpenCASCADE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../occt/lib/cmake/opencascade"
        CACHE PATH "Directory containing OpenCASCADEConfig.cmake")
endif()
find_package(OpenCASCADE CONFIG REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)

add_library(model STATIC
    change_signal.cpp
    dependencies.cpp
    engine.cpp
    evaluator.cpp
    feature_model.cpp
    geometry.cpp
    lexer.cpp
    parser.cpp
    property.cpp
    rectangle.cpp
)
target_include_directories(model SYSTEM PUBLIC "${OpenCASCADE_INCLUDE_DIR}")
target_sources(model PUBLIC FILE_SET CXX_MODULES FILES
    dependencies.cppm
    engine.cppm
    evaluator.cppm
    feature_model.cppm
    geometry.cppm
    lexer.cppm
    parser.cppm
    property.cppm
    rectangle.cppm
)
target_link_libraries(model PUBLIC TKPrim TKTopAlgo TKBRep)
target_link_libraries(model PUBLIC TKBO)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(model INTERFACE "LINKER:--disable-new-dtags")
endif()
set(CMAKE_BUILD_RPATH "${OpenCASCADE_LIBRARY_DIR}")

add_library(course_scenarios STATIC scenarios.cpp)
target_sources(course_scenarios PUBLIC FILE_SET CXX_MODULES FILES scenarios.cppm)
target_link_libraries(course_scenarios PUBLIC model)
add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE course_scenarios)

include(CTest)
if(BUILD_TESTING)
    add_executable(course-tests tests.cpp)
    target_link_libraries(course-tests PRIVATE course_scenarios)
    add_test(NAME single-extrusion COMMAND course-tests single-extrusion)
    add_test(NAME rebuild-order COMMAND course-tests rebuild-order)
    add_test(NAME incremental-rebuild COMMAND course-tests incremental-rebuild)
    add_test(NAME failure-recovery COMMAND course-tests failure-recovery)
    add_test(NAME face-reference COMMAND course-tests face-reference)
    add_test(NAME full-comparison COMMAND course-tests full-comparison)
endif()
