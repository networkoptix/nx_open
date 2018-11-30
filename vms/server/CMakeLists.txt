if(withMediaServer OR withClouds)
    add_subdirectory(nx_vms_server_db)
endif()

if(withMediaServer)
    add_subdirectory(nx_vms_server)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
    add_subdirectory(storage_plugins)

    if(withAnalyticsSdk)
        # TODO: Combine all SDKs (nx_storage_sdk, nx_sdk, nx_analytics_sdk) into one like this.
        add_subdirectory(nx_analytics_sdk)
    endif()

    if(withMediaDbUtil)
        add_subdirectory(media_db_util)
    endif()

    if(withRootTool)
        add_subdirectory(root_tool)
    endif()
endif()
