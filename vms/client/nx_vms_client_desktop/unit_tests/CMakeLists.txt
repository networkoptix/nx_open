## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_vms_client_desktop_ut
    PUBLIC_LIBS
        nx_vms_common_test_support
        nx_vms_client_desktop
        Qt6::Test
    ADDITIONAL_MOCABLES
        src/qml/qml_dialog_with_state_ut.h
    PROJECT VMS
    SUITES
    COMPONENT Client
    FOLDER client/tests
)
