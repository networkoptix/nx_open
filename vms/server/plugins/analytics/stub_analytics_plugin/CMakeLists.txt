## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(stub_analytics_plugin NO_MOC OPTIONAL DEDICATED_DIR
    PRIVATE_LIBS
        nx_sdk
        nx_kit
    EXTRA_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/object_streamer_files/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/object_streamer_files/readme.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/object_streamer_files/config.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/object_streamer_files/stream.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/object_streamer_files/generate.py"
    EXTRA_FILES_SUBDIR "object_streamer"
)

if(LINUX)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()
