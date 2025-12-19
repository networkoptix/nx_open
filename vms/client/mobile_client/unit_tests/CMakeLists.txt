## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(mobile_client_ut
    NO_WERROR
    PUBLIC_LIBS
        mobile_client_library
    PROJECT VMS
    SUITES
    COMPONENT Client
    FOLDER client/tests
)
