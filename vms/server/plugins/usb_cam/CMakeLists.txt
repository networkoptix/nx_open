nx_add_server_plugin(usb_cam LIBRARY
    NO_MOC PUBLIC_LIBS common nx_network nx_utils ffmpeg nx_sdk nx_kit)

if(LINUX)
    target_link_libraries(usb_cam PRIVATE asound udev)
endif()

if(WINDOWS)
    target_link_libraries(usb_cam PRIVATE strmiids strmbase comsuppw)
endif()
