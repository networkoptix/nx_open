## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(image_library_plugin)

include_directories("../../../src") #< SDK headers.

file(GLOB_RECURSE source_files ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(image_library_plugin SHARED ${source_files})

if(WIN32)
####target_link_libraries(image_library_plugin ws2_32)
    target_compile_definitions(image_library_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(image_library_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
