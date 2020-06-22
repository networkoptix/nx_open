nx_add_server_plugin(bosch_analytics_plugin NO_MOC
    PRIVATE_LIBS
        Qt5::Core
        openssl
        nx_sdk
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_kit
)
