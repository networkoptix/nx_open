## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Used in Combined Plugin.
nx_add_target(image_library_plugin_integration LIBRARY
    LIBRARY_TYPE STATIC
    NO_MOC
    PUBLIC_LIBS nx_sdk
    FOLDER server/libs
)

nx_add_server_plugin(image_library_plugin NO_MOC NO_GTEST OPTIONAL
    SOURCE_DIR "plugin"
    PRIVATE_LIBS image_library_plugin_integration
)
