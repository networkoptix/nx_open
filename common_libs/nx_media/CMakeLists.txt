nx_add_target(nx_media LIBRARY PUBLIC_LIBS nx_audio)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})

if(ANDROID)
    rdep_add_package(libjpeg-turbo)
    target_link_libraries(nx_media
        PUBLIC turbojpeg
        PRIVATE Qt5::AndroidExtras)
endif()
