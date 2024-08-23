## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(image_library_plugin)

# The following plugins used only as samples included into Video Source SDK.

add_subdirectory(axis_camera_plugin)

if(targetDevice STREQUAL "linux_arm32")
    add_subdirectory(rpi_camera_plugin)
endif()
