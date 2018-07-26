cmake_minimum_required(VERSION 3.3.2)
project(stub_metadata_plugin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../nx_kit
    ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

set(SDK_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../src)

include_directories(
    src
    ${SDK_SRC_DIR}
)

file(GLOB_RECURSE PLUGIN_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*)
file(GLOB_RECURSE SDK_SRC ${SDK_SRC_DIR}/*)

add_library(stub_metadata_plugin SHARED
    ${PLUGIN_SRC}
    ${SDK_SRC}
)

target_link_libraries(stub_metadata_plugin
    nx_kit
)
