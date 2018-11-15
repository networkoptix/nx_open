cmake_minimum_required(VERSION 3.3.2)
project(stub_analytics_plugin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")

enable_testing()

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../nx_kit
    ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

set(SDK_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../src)

file(GLOB_RECURSE PLUGIN_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*)
file(GLOB_RECURSE SDK_SRC ${SDK_SRC_DIR}/*)
file(GLOB_RECURSE UT_SRC ${CMAKE_CURRENT_LIST_DIR}/unit_tests/src/*)

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

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(stub_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
    PRIVATE NX_SDK_API=
)

if(NOT WIN32)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

add_executable(stub_analytics_plugin_ut
    ${UT_SRC}
    ${SDK_SRC}
)
target_include_directories(stub_analytics_plugin_ut PRIVATE
    ${SDK_SRC_DIR}
)
target_link_libraries(stub_analytics_plugin_ut PRIVATE
    nx_kit
    stub_analytics_plugin
)
target_compile_definitions(stub_analytics_plugin_ut
    PRIVATE NX_SDK_API=
)
add_test(NAME stub_analytics_plugin_ut COMMAND stub_analytics_plugin_ut)
