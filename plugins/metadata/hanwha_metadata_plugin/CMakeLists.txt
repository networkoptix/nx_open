nx_add_server_plugin(hanwha_metadata_plugin
    PUBLIC_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        ffmpeg
        openssl
        nx_sdk
        nx_plugin_utils
        nx_network
        nx_utils
        nx_fusion
        mediaserver_core
        nx_kit
)
