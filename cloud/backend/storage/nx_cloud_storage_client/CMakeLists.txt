nx_add_target(nx_cloud_storage_client LIBRARY NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_network
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
