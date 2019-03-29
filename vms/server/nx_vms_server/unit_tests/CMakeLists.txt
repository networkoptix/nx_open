nx_add_test(nx_vms_server_ut
    PUBLIC_LIBS
        nx_vms_server)

set(mocableFiles ${CMAKE_CURRENT_SOURCE_DIR}/src/test_support/mediaserver_launcher.h)

nx_add_qt_mocables(nx_vms_server_ut ${mocableFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(TARGET festival_libs)
    target_link_libraries(nx_vms_server_ut PRIVATE festival_libs)
endif()

if(NOT enable_hanwha)
    nx_exclude_sources_from_target(nx_vms_server_ut src/plugins/resource/hanwha)
endif()
