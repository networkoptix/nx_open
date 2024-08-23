## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_json_rpc_ut
    ADDITIONAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/nx/json_rpc/detail/outgoing_processor.cpp
    PUBLIC_LIBS nx_json_rpc
    PROJECT VMS
    COMPONENT Server
    FOLDER common/tests
)
