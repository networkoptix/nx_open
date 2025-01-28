## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Used in Combined Plugin.
nx_add_target(sample_cloud_storage_plugin_integration LIBRARY LIBRARY_TYPE STATIC NO_MOC
    PUBLIC_LIBS nx_sdk
    FOLDER server/libs
)

nx_add_server_plugin(sample_cloud_storage_plugin NO_MOC OPTIONAL
    SOURCE_DIR "plugin"
    PRIVATE_LIBS sample_cloud_storage_plugin_integration
)
