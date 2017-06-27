nx_add_test(mediaserver_core_ut
    PUBLIC_LIBS
        mediaserver_core
        GTest GMock)

if(TARGET festival_libs)
    target_link_libraries(mediaserver_core_ut PRIVATE festival_libs)
endif()
