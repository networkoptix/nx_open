## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(test_storage_plugin_ut NO_MOC
    PRIVATE_LIBS
        test_storage_plugin
        nx_sdk
        nx_utils #< Needed for nx_add_test() to work properly; to be eliminated.
)
