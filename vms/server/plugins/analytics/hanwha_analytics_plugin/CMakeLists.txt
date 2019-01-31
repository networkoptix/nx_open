nx_add_server_plugin(hanwha_analytics_plugin
    PRIVATE_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        ffmpeg
        openssl
        nx_sdk
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_vms_server
        nx_kit
)
