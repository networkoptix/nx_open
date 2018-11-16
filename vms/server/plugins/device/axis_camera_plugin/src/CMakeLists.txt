cmake_minimum_required(VERSION 3.1)

set(AXIS_TARGET axis_camera_plugin)

project(${AXIS_TARGET})

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories("../../../include")

if(WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_library( ${AXIS_TARGET} SHARED
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
        "axis_cam_params.h"
)

find_package(Qt5Network REQUIRED)
target_link_libraries(${AXIS_TARGET} PUBLIC Qt5::Network)

if(WINDOWS)
    target_link_libraries(${AXIS_TARGET} ws2_32)
endif()

set_target_properties(${AXIS_TARGET} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
