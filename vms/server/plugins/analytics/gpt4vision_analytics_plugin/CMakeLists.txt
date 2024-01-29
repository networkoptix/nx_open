## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(gpt4vision_analytics_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        Qt6::Core
        Qt6::Gui
        nx_sdk
        nx_kit
        nx_network
        nx_utils
        nx_reflect
)
