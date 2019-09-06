# nx_plugin_utils is a static lib because it uses the static lib nx_sdk, and there should be a
# dedicated instance of its context (global variables) in each Plugin and in the Server.
nx_add_target(nx_plugin_utils LIBRARY LIBRARY_TYPE STATIC
    PUBLIC_LIBS
        Qt5::Core
        nx_vms_api
        nx_utils
        nx_fusion
        nx_sdk
)

if(WINDOWS)
    target_link_libraries(nx_plugin_utils PUBLIC ws2_32) #< Needed for ntohl() and ntohs().
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
