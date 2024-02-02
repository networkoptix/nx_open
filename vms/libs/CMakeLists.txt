## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(withMediaServer OR withDesktopClient)
    add_subdirectory(nx_speech_synthesizer)
endif()

if(withMediaServer
    OR withTestCamera
    OR withDesktopClient
    OR withMobileClient
    OR withClouds
    OR openSourceBuild
)
    add_subdirectory(appserver2)
    add_subdirectory(nx_sdk)
    add_subdirectory(nx_vms_api)
    add_subdirectory(nx_vms_common)
    add_subdirectory(nx_vms_license)
    add_subdirectory(nx_vms_update)
    add_subdirectory(nx_vms_utils)
endif()

if(withMediaServer)
    add_subdirectory(nx_vms_json_rpc)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(nx_audio)
    add_subdirectory(nx_media)
    add_subdirectory(nx_vms_applauncher_api)
endif()

if (withMediaServer OR withDesktopClient OR withMobileClient)
    add_subdirectory(nx_vms_rules)
endif()
