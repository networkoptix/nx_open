## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_library(combined_plugin SHARED "plugin/plugin.cpp")
target_link_libraries(combined_plugin
    PRIVATE
        sample_analytics_plugin_integration
        sample_cloud_storage_plugin_integration
        image_library_plugin_integration
        test_storage_plugin_integration
)
