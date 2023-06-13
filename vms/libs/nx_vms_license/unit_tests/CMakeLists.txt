## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_license_ut
    PRIVATE_LIBS
        nx_utils
        nx_vms_license
        nx_vms_common_test_support
    PROJECT VMS
    COMPONENT Server
    FOLDER common/tests
)
