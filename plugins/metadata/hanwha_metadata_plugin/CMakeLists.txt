nx_add_target(hanwha_metadata_plugin LIBRARY
    NO_PCH
    PUBLIC_LIBS
        Qt5::Core Qt5::Network
        ffmpeg
        common nx_network nx_utils nx_fusion mediaserver_core)
