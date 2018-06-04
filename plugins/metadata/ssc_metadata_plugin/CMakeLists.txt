find_package(Qt5 COMPONENTS SerialPort REQUIRED)

nx_add_server_plugin(ssc_metadata_plugin
    PRIVATE_LIBS
        Qt5::Core
        Qt5::SerialPort
        nx_plugin_utils
        nx_network
        nx_utils
        nx_fusion
        nx_kit
)

add_definitions(-DNX_PRINT_TO_LOG)
