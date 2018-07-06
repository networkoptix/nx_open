cmake_minimum_required(VERSION 3.1)

set(RPI_TARGET rpiplugin)

project(${RPI_TARGET})

# Path to NX PLUGIN SDK
include_directories(
    "../../../include"
)

# Path to raspbery SDK - change it as needed
include_directories(
    "userland-master"
    "userland-master/interface/vmcs_host/khronos"
    "userland-master/host_applications/linux/libs/bcm_host/include"
)

add_library(${RPI_TARGET} SHARED
        "camera_manager.cpp"
        "camera_manager.h"
        "discovery_manager.cpp"
        "discovery_manager.h"
        "media_encoder.cpp"
        "media_encoder.h"
        "ref_counter.h"
        "rpi_camera.cpp"
        "rpi_camera.h"
        "rpi_omx.cpp"
        "rpi_omx.h"
        "rpi_omx_broadcom.h"
        "rpi_omx_print.cpp"
        "rw_lock.h"
        "stream_reader.cpp"
        "stream_reader.h"
        "timer.h"
        "video_packet.cpp"
        "video_packet.h"
)

set_target_properties(${RPI_TARGET} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
