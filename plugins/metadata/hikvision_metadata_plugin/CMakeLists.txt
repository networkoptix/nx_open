nx_add_server_plugin(hikvision_metadata_plugin
    PUBLIC_LIBS
        Qt5::Core Qt5::Network
        openssl
        nx_plugin_utils
        nx_network
        nx_utils
        nx_fusion
        nx_kit
)
