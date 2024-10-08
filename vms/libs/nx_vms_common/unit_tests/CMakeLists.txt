## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6
    COMPONENTS
    Test)

nx_add_test(nx_vms_common_ut
    PUBLIC_LIBS
        nx_utils
        nx_media_core
        nx_vms_common
        Qt6::Test
    PRIVATE_LIBS
        nx_vms_common_test_support
    PROJECT VMS
    COMPONENT Server
    SUITES
    FOLDER common/tests
)
