## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_sql_ut NO_MOC
    PUBLIC_LIBS nx_sql GTest GMock
    PROJECT NXLIB
    FOLDER common/tests
)
