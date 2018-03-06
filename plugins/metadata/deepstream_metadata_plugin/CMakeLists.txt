if(targetDevice STREQUAL "tx1")
    nx_add_server_plugin(deepstream_metadata_plugin LIBRARY NO_MOC NO_PCH
        PUBLIC_LIBS
            common
            nx_kit
            gstreamer glib deepstream
    )
else()
    nx_add_server_plugin(deepstream_metadata_plugin LIBRARY NO_MOC NO_PCH
        PUBLIC_LIBS
            common
            nx_kit
    )
endif()
