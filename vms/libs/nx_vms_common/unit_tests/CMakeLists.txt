## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_common_ut
    PUBLIC_LIBS
        nx_utils
        nx_vms_common
    PRIVATE_LIBS
        nx_vms_common_test_support
    PROJECT VMS
    COMPONENT Server
    FOLDER common/tests
)
