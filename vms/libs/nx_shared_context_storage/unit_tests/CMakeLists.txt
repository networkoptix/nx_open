## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_shared_context_storage_ut
    PUBLIC_LIBS nx_sdk nx_shared_context_storage
    PROJECT VMS
    COMPONENT Server
    FOLDER common/tests
)
