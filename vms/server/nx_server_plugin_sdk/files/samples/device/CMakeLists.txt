## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(image_library_plugin)

if(Qt6Core_FOUND)
    add_subdirectory(axis_camera_plugin)
endif()

if(rpiFirmware)
    add_subdirectory(rpi_camera_plugin)
endif()
