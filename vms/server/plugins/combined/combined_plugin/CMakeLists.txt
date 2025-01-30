## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_server_plugin(combined_plugin NO_MOC OPTIONAL
    SOURCE_DIR "plugin"
    PRIVATE_LIBS
        sample_analytics_plugin_integration
        image_library_plugin_integration
        test_storage_plugin_integration
        sample_cloud_storage_plugin_integration
)
