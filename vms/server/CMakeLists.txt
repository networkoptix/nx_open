if(withMediaServer OR withClouds)
    add_subdirectory(nx_vms_server_db)
endif()

if(withMediaServer)
    if(withSdk) #< Should come before nx_vms_server because the latter packs sdk zips to resources.
        # TODO: Combine all SDKs (storage_sdk, camera_sdk, metadata_sdk) into one.
        add_subdirectory(nx_metadata_sdk)
        add_subdirectory(nx_camera_sdk)
        add_subdirectory(nx_storage_sdk)
    endif()

    add_subdirectory(nx_vms_server)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
    add_subdirectory(rtsp_perf)

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
