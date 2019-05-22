add_subdirectory(nx_utils)
add_subdirectory(nx_fusion)
add_subdirectory(nx_network)
add_subdirectory(udt)
add_subdirectory(nx_sdk)
add_subdirectory(nx_plugin_utils)

if(withDesktopClient OR withMobileClient)
    add_subdirectory(nx_audio)
    add_subdirectory(nx_media)
endif()

if((withMediaServer OR withDesktopClient) AND NOT targetDevice STREQUAL edge1)
    add_subdirectory(nx_speech_synthesizer)
endif()

if(withMediaServer OR withClouds)
    add_subdirectory(nx_email)
endif()

if(withMediaServer)
    add_subdirectory(nx_onvif)
endif()

if(WINDOWS)
    add_subdirectory(nx_gdi_detours)
endif()
