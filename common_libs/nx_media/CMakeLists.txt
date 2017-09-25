nx_add_target(nx_media LIBRARY
    PUBLIC_LIBS
        nx_kit
        nx_audio common nx_fusion nx_utils
)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})

if(ANDROID OR IOS)
    target_link_libraries(nx_media PUBLIC turbojpeg)
endif()

if(ANDROID)
    target_link_libraries(nx_media PRIVATE Qt5::AndroidExtras)
endif()
