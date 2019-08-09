if(withMediaServer OR withClouds)
    add_subdirectory(nx_vms_server_db)
endif()

if(withMediaServer)
    add_subdirectory(nx_vms_server)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
    add_subdirectory(storage_plugins)
    add_subdirectory(rtsp_perf)

    if(withSdk)
        # TODO: Combine all SDKs (nx_storage_sdk, nx_camera_sdk, nx_metadata_sdk) into one like this.
        add_subdirectory(nx_metadata_sdk)
        add_subdirectory(nx_camera_sdk)
    endif()

    if(withMediaDbUtil)
        add_subdirectory(media_db_util)
    endif()

    if(withRootTool)
        add_subdirectory(root_tool)
    endif()
endif()

if(withP2PConnectionTestingUtility)
    add_subdirectory(p2p_connection_testing_utility)
endif()
