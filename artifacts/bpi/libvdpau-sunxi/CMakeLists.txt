cmake_minimum_required(VERSION 3.3)
project(libvdpau_sunxi)

# ATTENTION: This cmake file is not intended for building but rather just for IDE convenience.

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    kernel-headers/drv_display.h
    kernel-headers/g2d_driver.h
    kernel-headers/sunxi_disp_ioctl.h
    kernel-headers/sunxi_display2.h
    vdpau_private.c
    decoder.c
    device.c
    h264.c
    h265.c
    handles.c
    mpeg12.c
    mpeg4.c
    presentation_queue.c
    rgba.c
    rgba.h
    rgba_g2d.c
    rgba_g2d.h
    rgba_pixman.c
    rgba_pixman.h
    sunxi_disp.c
    sunxi_disp.h
    sunxi_disp1_5.c
    sunxi_disp2.c
    surface_bitmap.c
    surface_output.c
    surface_video.c
    tiled_yuv.h
    vdpau_private.h
    video_mixer.c
    ini.h
    ini.cpp
    ../../../buildenv/packages/any/nx_kit/src/nx/kit/ini_config.cpp
)

# Directory containing all necessary headers from the libraries used.
include_directories(
    ../../../bpi/src/include
    ../../../buildenv/packages/any/nx_kit/src
)

add_executable(libvdpau_sunxi ${SOURCE_FILES})
