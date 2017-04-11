set(library_type)
if(WIN32)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(udt LIBRARY ${library_type} NO_MOC)

target_compile_definitions(udt
    PRIVATE UDT_API=${API_EXPORT_MACRO}
    INTERFACE UDT_API=${API_IMPORT_MACRO})

if(LINUX)
    target_link_libraries(udt PUBLIC pthread)
endif()

if(WIN32)
    target_link_libraries(udt ws2_32)
endif()