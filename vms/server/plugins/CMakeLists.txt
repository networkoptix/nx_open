## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(NOT openSourceBuild OR withSdk)
    add_subdirectory(analytics)
    add_subdirectory(cloud_storage)
    add_subdirectory(device)
    add_subdirectory(storage)
endif()
