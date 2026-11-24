## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS QuickTest REQUIRED)

# Required on Linux, since nx_add_test adds the Qt rpath only for gtest-based targets.
list(APPEND CMAKE_INSTALL_RPATH ${QT_DIR}/lib)
list(REMOVE_DUPLICATES CMAKE_INSTALL_RPATH)

file(GLOB fonts ${CMAKE_BINARY_DIR}/bin/fonts/*.ttf)
set_source_files_properties(${fonts} PROPERTIES RESOURCE_ALIAS_PREFIX fonts/)

nx_add_test(mobile_client_qml_ut
    NO_GTEST
    PUBLIC_LIBS
        mobile_client_library
        Qt6::QuickTest
    ADDITIONAL_MOCABLES
        src/qml_test_setup.h
        src/test_utils.h
    ADDITIONAL_RESOURCES
        ${fonts}
    PROJECT VMS
    COMPONENT Client
    FOLDER client/tests
)
