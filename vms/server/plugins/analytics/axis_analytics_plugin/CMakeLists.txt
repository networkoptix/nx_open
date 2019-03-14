nx_add_server_plugin(axis_analytics_plugin
    PRIVATE_LIBS
        openssl
        Qt5::Core
        nx_plugin_utils
        nx_vms_api
        nx_network
        nx_utils
        nx_fusion
        nx_kit
)

# Required for GSOAP core and generated files.
include_directories(
    src/gsoap/import
    src/gsoap/aux_src
)

add_definitions(-DWITH_OPENSSL)
add_definitions(-DNX_PRINT_TO_LOG)
