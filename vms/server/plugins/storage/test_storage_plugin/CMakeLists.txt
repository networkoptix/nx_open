## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Used in Combined Plugin.
nx_add_target(test_storage_plugin_integration LIBRARY
    LIBRARY_TYPE STATIC
    NO_MOC
    WERROR_IF LINUX
    PUBLIC_LIBS nx_sdk
    FOLDER server/libs
)

nx_add_server_plugin(test_storage_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS nx_sdk
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
