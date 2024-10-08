## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_utils_ut
    PUBLIC_LIBS
        nx_vms_utils
    PROJECT VMS
    COMPONENT Client
    SUITES
    FOLDER common/tests
)
