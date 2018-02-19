nx_add_server_plugin(vca_metadata_plugin
    PRIVATE_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        common nx_network nx_utils nx_fusion mediaserver_core nx_kit)

add_definitions(-DNX_PRINT_TO_LOG)
