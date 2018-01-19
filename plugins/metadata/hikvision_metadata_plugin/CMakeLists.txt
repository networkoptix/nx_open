nx_add_server_plugin(hikvision_metadata_plugin
    NO_PCH
    PUBLIC_LIBS
        Qt5::Core Qt5::Network
        openssl
        common nx_network nx_utils nx_fusion nx_kit)
