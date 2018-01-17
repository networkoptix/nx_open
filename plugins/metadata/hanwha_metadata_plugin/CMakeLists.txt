nx_add_server_plugin(hanwha_metadata_plugin
    NO_PCH
    PUBLIC_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        ffmpeg openssl
        common nx_network nx_utils nx_fusion mediaserver_core nx_kit)
