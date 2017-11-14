add_subdirectory(nx_utils)
add_subdirectory(nx_fusion)
add_subdirectory(nx_network)
add_subdirectory(udt)

if(withClouds)
    add_subdirectory(nx_cassandra)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(nx_audio)
    add_subdirectory(nx_media)
    add_subdirectory(nx_vms_utils)
endif()

if(withMediaServer OR withDesktopClient)
    add_subdirectory(nx_speech_synthesizer)
endif()

if(withMediaServer OR withClouds)
    add_subdirectory(nx_email)
    add_subdirectory(nx_onvif)
endif()
