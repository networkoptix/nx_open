## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(vms_gateway_ut
    PUBLIC_LIBS
        vms_gateway_core
    PROJECT VMS
    COMPONENT Client
    FOLDER client/tests
)
