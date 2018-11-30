add_subdirectory(generic_rtsp_plugin)

add_subdirectory(image_library_plugin)

add_subdirectory(mjpeg_link_plugin)

add_subdirectory(generic_multicast_plugin)

if(NOT box MATCHES "bpi|bananapi|edge1")
    add_subdirectory(usb_camera_plugin)
endif()

if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()
