## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(ftp_storage_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS
        nx_sdk ftplibpp
    ADD_TO_DISTRIBUTION_IF FALSE
)
