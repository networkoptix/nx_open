nx_add_server_plugin(webcam_plugin LIBRARY
    NO_MOC PUBLIC_LIBS common nx_network nx_utils ffmpeg)

if(WINDOWS)
    target_link_libraries(webcam_plugin PRIVATE strmiids strmbase comsuppw)
endif()
