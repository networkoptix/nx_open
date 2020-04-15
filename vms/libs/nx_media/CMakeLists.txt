nx_add_target(nx_media LIBRARY
    PUBLIC_LIBS
        nx_kit
        nx_audio common nx_fusion nx_utils
        intel-media-sdk
)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})

if(LINUX)
    target_link_libraries(nx_media PUBLIC X11 dl libva GL)
endif()

if(WINDOWS)
    target_link_libraries(nx_media PUBLIC opengl32 dxva2 d3d9 glew)
endif()

if(ANDROID OR IOS)
    target_link_libraries(nx_media PUBLIC turbojpeg)
endif()

if(ANDROID)
    target_link_libraries(nx_media PRIVATE Qt5::AndroidExtras)
endif()

if(targetDevice STREQUAL "bpi")
    add_definitions(-DENABLE_PROXY_DECODER)
    target_link_libraries(nx_media PRIVATE proxy_decoder)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
