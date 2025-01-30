## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(image_library_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS
        nx_sdk
        nx_kit
)

target_include_directories(image_library_plugin PRIVATE "${PROJECT_SOURCE_DIR}/common/src")
