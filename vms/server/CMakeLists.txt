## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(withSdk)
    add_subdirectory(nx_metadata_sdk)
    add_subdirectory(nx_video_source_sdk)
    add_subdirectory(nx_storage_sdk)
    add_subdirectory(nx_cloud_storage_sdk)
endif()

add_subdirectory(plugins)
