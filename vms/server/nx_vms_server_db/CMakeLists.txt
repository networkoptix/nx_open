nx_add_target(nx_vms_server_db LIBRARY
    PUBLIC_LIBS
        common
        appserver2
        nx_fusion
        nx_network
        nx_system_commands
)

if(LINUX)
    target_link_libraries(nx_vms_server_db PRIVATE dl)
endif()
