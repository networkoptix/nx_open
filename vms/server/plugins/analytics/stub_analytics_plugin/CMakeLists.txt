nx_add_server_plugin(stub_analytics_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        nx_sdk
        nx_kit
)

if(LINUX)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
