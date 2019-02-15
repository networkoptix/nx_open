nx_add_target(discovery_api_client LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_network
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
