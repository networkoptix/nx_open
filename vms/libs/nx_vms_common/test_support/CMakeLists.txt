## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_common_test_support LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_vms_common
    FOLDER common/tests
)

target_compile_definitions(nx_vms_common_test_support
    PRIVATE NX_VMS_COMMON_TEST_SUPPORT_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_COMMON_TEST_SUPPORT_API=${API_IMPORT_MACRO}
)
