nx_add_target(nx_network LIBRARY PUBLIC_LIBS nx_streaming udt)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})

if(LINUX)
    target_link_libraries(nx_network
        INTERFACE
            -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            -lcommon -lnx_streaming -lnx_network)
endif()
