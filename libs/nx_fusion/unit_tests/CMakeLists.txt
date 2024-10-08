## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_fusion_ut
    PUBLIC_LIBS
        nx_fusion
    PROJECT NXLIB
    SUITES
    FOLDER common/tests
)
target_include_directories(nx_fusion_ut PRIVATE ${open_source_root}/vms/libs/nx_vms_common/src)
