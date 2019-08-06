# nx_utils dependency is added as it is required by the our standard test configuration.
# This can be avoided by adding the test manually, without using nx_add_test function.
nx_add_server_plugin_test(stub_analytics_plugin_ut
    PRIVATE_LIBS
        nx_sdk
        nx_kit
        stub_analytics_plugin
        nx_utils
)
