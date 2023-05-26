## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_network_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_network
    PRIVATE_LIBS
        udt
    PROJECT NXLIB
    FOLDER common/tests
)
