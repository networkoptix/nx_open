find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)

include_directories(${GSTREAMER_INCLUDE_DIRS})

nx_add_server_plugin(deepstream_analytics_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        nx_sdk
        ${GSTREAMER_LDFLAGS} deepstream
)
