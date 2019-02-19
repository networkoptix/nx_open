nx_add_target(nx_discovery_api_client LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_network
)

target_compile_definitions(nx_discovery_api_client
    PRIVATE NX_DISCOVERY_CLIENT_API=${API_EXPORT_MACRO}
    INTERFACE NX_DISCOVERY_CLIENT_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
