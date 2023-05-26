## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_client_desktop_ut
    PUBLIC_LIBS
        nx_vms_common_test_support
        nx_vms_client_desktop
        Qt5::Test
    PROJECT VMS
    COMPONENT Client
    FOLDER client/tests
)
