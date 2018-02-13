if(targetDevice STREQUAL "tx1")
    nx_add_server_plugin(deepstream_plugin LIBRARY NO_MOC NO_PCH
        PUBLIC_LIBS
            common
            nx_kit
            gstreamer glib deepstream deepstream_sample_models
    )
else()
    nx_add_server_plugin(deepstream_plugin LIBRARY NO_MOC NO_PCH
        PUBLIC_LIBS
            common
            nx_kit
    )
endif()
