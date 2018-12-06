nx_add_server_plugin(it930x_plugin NO_MOC OPTIONAL
    ADDITIONAL_SOURCES
        src/dtvapi_v15/DTVAPI_15.c
    PRIVATE_LIBS
        common
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)

target_compile_definitions(it930x_plugin PRIVATE DTVAPI_V15)
