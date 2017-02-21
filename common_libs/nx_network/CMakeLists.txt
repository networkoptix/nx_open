nx_add_target(nx_network LIBRARY PUBLIC_LIBS nx_streaming udt)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})
