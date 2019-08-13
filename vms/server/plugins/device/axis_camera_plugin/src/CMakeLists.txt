## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(axis_camera_plugin)

set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE source_files ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(axis_camera_plugin SHARED ${source_files})

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
target_link_libraries(axis_camera_plugin PRIVATE
    Qt5::Core
    Qt5::Network
)

target_include_directories(axis_camera_plugin PRIVATE ../../../src) #< SDK headers.

if(WIN32)
    target_compile_definitions(axis_camera_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(axis_camera_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
