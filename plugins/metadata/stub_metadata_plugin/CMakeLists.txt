nx_add_server_plugin(stub_metadata_plugin LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_sdk
        nx_kit
)

if(LINUX)
    target_link_libraries(stub_metadata_plugin PRIVATE pthread)
endif()
