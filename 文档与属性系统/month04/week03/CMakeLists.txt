cmake_minimum_required(VERSION 4.3)

# CMake 4.3.x opt-in for C++23 import std; update this token with CMake upgrades.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(document_month04_week03 LANGUAGES CXX)

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

add_library(cad_core STATIC
    benchmark.cpp
    change_signal.cpp
    dependencies.cpp
    document.cpp
    engine.cpp
    evaluator.cpp
    lexer.cpp
    parser.cpp
    property.cpp
    rectangle.cpp
    storage.cpp
)
target_sources(cad_core PUBLIC
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

add_executable(console-demo console.cpp)
target_link_libraries(console-demo PRIVATE cad_core)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
# Object linkage keeps the QML resources in both executables without a Qt dependency in cad_core.
add_library(cad_ui OBJECT bridge.cpp bridge.hpp resources.qrc)
set_target_properties(cad_ui PROPERTIES AUTOMOC ON AUTORCC ON)
target_compile_definitions(cad_ui PUBLIC QT_NO_KEYWORDS COURSE_WEEK=3)
target_link_libraries(cad_ui PUBLIC cad_core Qt6::Core Qt6::Gui Qt6::Qml
    Qt6::Quick Qt6::QuickControls2)

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE cad_ui)

if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    add_executable(ui-tests ui_tests.cpp)
    target_link_libraries(ui-tests PRIVATE cad_ui Qt6::Test)
    add_test(NAME ui COMMAND ui-tests)
    set_tests_properties(ui PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
