## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(axis_camera_plugin OPTIONAL NO_GTEST
    PRIVATE_LIBS
        Qt6::Core
        Qt6::Network
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)
