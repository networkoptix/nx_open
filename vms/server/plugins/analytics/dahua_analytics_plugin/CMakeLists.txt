nx_add_server_plugin(dahua_analytics_plugin
    PRIVATE_LIBS
        Qt5::Core Qt5::Network
        nx_sdk
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_kit
)
