nx_add_server_plugin(dw_mtt_analytics_plugin
    PRIVATE_LIBS
        Qt5::Core
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_kit
        common
)

add_definitions(-DNX_PRINT_TO_LOG)
