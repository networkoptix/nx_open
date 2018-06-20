if(targetDevice STREQUAL "tx1")
    set(specific_libs gstreamer glib deepstream)
else()
    set(specific_libs)
endif()

nx_add_server_plugin(deepstream_metadata_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        nx_kit nx_sdk
        ${specific_libs}
)
