if(withMediaServer
    OR withTestCamera
    OR withTrayTool
    OR withDesktopClient
    OR withMobileClient
    OR withClouds
)
    add_subdirectory(common)
    add_subdirectory(appserver2)
    add_subdirectory(nx_vms_api)
    add_subdirectory(nx_sdk)
    add_subdirectory(nx_update)
endif()

if(withMediaServer OR withRootTool OR withClouds)
    add_subdirectory(nx_system_commands)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(nx_vms_applauncher_api)
    add_subdirectory(nx_vms_utils)
    add_subdirectory(nx_audio)
    add_subdirectory(nx_media)
endif()

if((withMediaServer OR withDesktopClient) AND NOT targetDevice STREQUAL edge1)
    add_subdirectory(nx_speech_synthesizer)
endif()

if(withMediaServer)
    add_subdirectory(nx_email)
    add_subdirectory(nx_plugin_utils)
endif()
