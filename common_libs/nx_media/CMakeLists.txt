nx_add_target(nx_media LIBRARY
    PUBLIC_LIBS
        nx_kit
        nx_audio common
)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})

if(ANDROID)
    target_link_libraries(nx_media
        PUBLIC turbojpeg
        PRIVATE Qt5::AndroidExtras)
endif()
