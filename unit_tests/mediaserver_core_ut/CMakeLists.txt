nx_add_test(mediaserver_core_ut
    PUBLIC_LIBS
        mediaserver_core
        GTest GMock)

if(TARGET festival_libs)
    target_link_libraries(mediaserver_core_ut PRIVATE festival_libs)
endif()

if(NOT customization STREQUAL "hanwha")
    nx_exclude_sources_from_target(mediaserver_core_ut
        hanwha_attributes_parser_ut.cpp
    )
endif()
