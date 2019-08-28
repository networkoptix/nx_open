add_subdirectory(generic_rtsp_plugin)

add_subdirectory(image_library_plugin)

add_subdirectory(mjpeg_link_plugin)

add_subdirectory(generic_multicast_plugin)

if(NOT (box STREQUAL "edge1" OR box STREQUAL "bpi"))
    add_subdirectory(usb_camera_plugin)
endif()

if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()

# The following plugins used only as samples included into Camera SDK.

add_subdirectory(axis_camera_plugin)

if(targetDevice STREQUAL "linux_arm32")
    add_subdirectory(rpi_camera_plugin)
endif()
