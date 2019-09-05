## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(image_library_plugin)

# Define nx_kit lib, static.
set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

file(GLOB_RECURSE source_files ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(image_library_plugin SHARED ${source_files})

target_link_libraries(image_library_plugin PRIVATE nx_kit)

target_include_directories(image_library_plugin PRIVATE ../../../src) #< SDK headers.

if(WIN32)
    target_compile_definitions(image_library_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(image_library_plugin PRIVATE -Wno-deprecated-declarations)
endif()

set_target_properties(image_library_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
