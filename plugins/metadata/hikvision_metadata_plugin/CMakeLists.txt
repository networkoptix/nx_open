nx_add_target(hikvision_metadata_plugin LIBRARY
    NO_PCH
    PUBLIC_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        openssl
        common nx_network nx_utils nx_fusion mediaserver_core nx_kit)
