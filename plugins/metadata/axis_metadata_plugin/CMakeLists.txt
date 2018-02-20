nx_add_server_plugin(axis_metadata_plugin
    PRIVATE_LIBS
        openssl
        Qt5::Core
        common
        nx_network
        nx_utils
        nx_fusion
        mediaserver_core
        nx_kit
)

# Required for GSOAP core and generated files.
include_directories(
    src/gsoap/import
    src/gsoap/aux_src
)

add_definitions(-DWITH_OPENSSL)
