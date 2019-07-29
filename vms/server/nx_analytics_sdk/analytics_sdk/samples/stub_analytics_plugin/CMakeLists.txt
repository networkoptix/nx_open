## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.3.2)
project(stub_analytics_plugin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    # In Linux, for the plugin .so library, set `rpath` to "$ORIGIN" and do not set `runpath`, thus
    # enabling the lookup of the dependencies in the plugin dir first.
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--disable-new-dtags")
endif()
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

enable_testing()

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../nx_kit
    ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

set(SDK_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../src)

file(GLOB_RECURSE PLUGIN_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*)
file(GLOB_RECURSE SDK_SRC ${SDK_SRC_DIR}/*)
file(GLOB_RECURSE UNIT_TESTS_SRC ${CMAKE_CURRENT_LIST_DIR}/unit_tests/src/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_compile_definitions(nx_sdk
    PUBLIC NX_PLUGIN_API=
)
target_include_directories(nx_sdk PUBLIC
    ${SDK_SRC_DIR}
)
target_link_libraries(nx_sdk PRIVATE
    nx_kit
)

add_library(stub_analytics_plugin SHARED
    ${PLUGIN_SRC}
    ${SDK_SRC}
)
target_include_directories(stub_analytics_plugin PRIVATE
    src
    ${SDK_SRC_DIR}
)
target_link_libraries(stub_analytics_plugin PRIVATE
    nx_kit
)

target_compile_definitions(stub_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO} #< Needed to link plugin ut lib to plugin lib.
)

if(NOT WIN32)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

add_executable(stub_analytics_plugin_ut
    ${UNIT_TESTS_SRC}
    ${SDK_SRC}
)
target_include_directories(stub_analytics_plugin_ut PRIVATE
    ${SDK_SRC_DIR}
)
target_link_libraries(stub_analytics_plugin_ut PRIVATE
    nx_kit
    stub_analytics_plugin
)
add_test(NAME stub_analytics_plugin_ut COMMAND stub_analytics_plugin_ut)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../unit_tests ${CMAKE_CURRENT_BINARY_DIR}/unit_tests)
