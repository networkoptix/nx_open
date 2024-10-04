## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_network_ut
    PUBLIC_LIBS
        nx_network
        nx_fusion # nx_network itself does not depend on nx_fusion.
                  # This dependency is needed to verify legacy compatibility with nx_fusion.
    PRIVATE_LIBS
        udt
    PROJECT NXLIB
    FOLDER common/tests
)
