nx_add_target(nx_media LIBRARY
    PUBLIC_LIBS
        nx_kit
        nx_audio common nx_fusion nx_utils dl
        /home/lbusygin/develop/msdk/build/__bin/release/libmfxhw64.so
        /home/lbusygin/develop/msdk/build/__bin/release/libmfx.so
        /usr/local/lib/libva.so
        libva-x11.so
        X11
        GL
)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS} /home/lbusygin/develop/msdk/api/include /usr/local/include)


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
