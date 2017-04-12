set(library_type)
if(WIN32)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_network LIBRARY ${library_type}
    PUBLIC_LIBS nx_streaming udt)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})

if(LINUX)
    target_link_libraries(nx_network
        INTERFACE
            -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            -lcommon -lnx_streaming -lnx_network)
endif()

if(WIN32)
    target_link_libraries(nx_network PRIVATE Iphlpapi)
endif()