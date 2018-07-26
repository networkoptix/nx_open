cmake_minimum_required(VERSION 3.3.2)
project(stub_metadata_plugin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../nx_kit
    ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

set(SDK_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../src)

file(GLOB_RECURSE PLUGIN_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*)
file(GLOB_RECURSE SDK_SRC ${SDK_SRC_DIR}/*)
file(GLOB_RECURSE UT_SRC ${CMAKE_CURRENT_LIST_DIR}/unit_tests/src/*)

add_library(stub_metadata_plugin SHARED
    ${PLUGIN_SRC}
    ${SDK_SRC}
)
target_include_directories(stub_metadata_plugin PRIVATE
    src
    ${SDK_SRC_DIR}
)
target_link_libraries(stub_metadata_plugin PRIVATE
    nx_kit
)

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(stub_metadata_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

if(NOT WIN32)
    target_link_libraries(stub_metadata_plugin PRIVATE pthread)
endif()

add_executable(stub_metadata_plugin_ut
    ${UT_SRC}
)
target_include_directories(stub_metadata_plugin_ut PRIVATE
    ${SDK_SRC_DIR}
)
target_link_libraries(stub_metadata_plugin_ut PRIVATE
    nx_kit
    stub_metadata_plugin
)
add_test(NAME stub_metadata_plugin_ut COMMAND stub_metadata_plugin_ut)
