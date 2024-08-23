## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin_test(test_storage_plugin_ut
    NO_QT
    PRIVATE_LIBS
        test_storage_plugin
        nx_sdk
    FOLDER server/tests
)
