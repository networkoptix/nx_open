## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.1)

project(axis_camera_plugin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories("../../../src") #< SDK headers.

add_library(axis_camera_plugin SHARED
    "axis_cam_params.h"
    "axis_camera_manager.cpp"
    "axis_camera_manager.h"
    "axis_camera_plugin.cpp"
    "axis_camera_plugin.h"
    "axis_discovery_manager.cpp"
    "axis_discovery_manager.h"
    "axis_media_encoder.cpp"
    "axis_media_encoder.h"
    "axis_relayio_manager.cpp"
    "axis_relayio_manager.h"
    "sync_http_client.cpp"
    "sync_http_client.h"
    "sync_http_client_delegate.cpp"
    "sync_http_client_delegate.h"
)

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
target_link_libraries(axis_camera_plugin PUBLIC Qt5::Core Qt5::Network)

if(WIN32)
    target_compile_definitions(axis_camera_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(axis_camera_plugin PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
