## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_client_core_ut
    PUBLIC_LIBS
        nx_vms_client_core
        Qt6::Test
    PROJECT VMS
    COMPONENT Client
    FOLDER client/tests
    FORCE_INCLUDE
        ${nx_utils_dir}/src/nx/utils/test_support/custom_gtest_printers.h
)
