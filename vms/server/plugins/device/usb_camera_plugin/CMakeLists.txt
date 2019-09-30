nx_add_server_plugin(usb_camera_plugin NO_MOC
    PRIVATE_LIBS
        common
        nx_vms_utils
        nx_network
        nx_utils
        ffmpeg
        nx_sdk
        nx_kit
)

if(LINUX)
    target_link_libraries(usb_camera_plugin PRIVATE asound udev)
endif()

if(WINDOWS)
    target_link_libraries(usb_camera_plugin PRIVATE strmiids strmbase comsuppw setupapi)
endif()
