nx_add_server_plugin(test_storage_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
