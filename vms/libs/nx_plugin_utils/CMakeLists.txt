set(library_type)
#if(WINDOWS)
#    set(library_type LIBRARY_TYPE SHARED)
#endif()

nx_add_target(nx_plugin_utils LIBRARY ${library_type}
    PUBLIC_LIBS
        Qt5::Core
        nx_vms_api
        nx_utils
        nx_fusion
        nx_sdk
)

target_compile_definitions(nx_plugin_utils
    PRIVATE NX_PLUGIN_UTILS_API=
    INTERFACE NX_PLUGIN_UTILS_API=
)

if(WINDOWS)
    target_link_libraries(nx_plugin_utils PUBLIC ws2_32) #< Needed for ntohl() and ntohs().
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
