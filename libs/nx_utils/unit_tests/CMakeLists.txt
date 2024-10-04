## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_utils_ut
    PUBLIC_LIBS
        nx_reflect
        nx_utils
        Qt6::Network
    PROJECT NXLIB
    SUITES
    FOLDER common/tests
)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qmetaobject_helper_ut.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/enum_utils_ut.h)

nx_add_qt_mocables(nx_utils_ut ${mocableFiles}
    INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
