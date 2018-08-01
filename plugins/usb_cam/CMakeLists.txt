nx_add_server_plugin(usb_cam LIBRARY
    NO_MOC PUBLIC_LIBS common nx_network nx_utils ffmpeg)

if(WINDOWS)
    target_link_libraries(usb_cam PRIVATE strmiids strmbase comsuppw)
endif()