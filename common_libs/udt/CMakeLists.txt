set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

#if(LINUX)
#    add_compile_options("-Werror" "-Wall" "-Wextra")
#endif()

nx_add_target(udt LIBRARY ${library_type} NO_MOC)

target_compile_definitions(udt
    PRIVATE UDT_API=${API_EXPORT_MACRO}
    INTERFACE UDT_API=${API_IMPORT_MACRO})

if(WINDOWS)
    target_link_libraries(udt ws2_32)
endif()

if(LINUX AND NOT ANDROID)
    target_link_libraries(udt rt pthread)
endif()
