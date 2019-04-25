set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_maxminddb LIBRARY ${library_type} NO_MOC WERROR
    PUBLIC_LIBS
        maxminddb nx_utils)

target_compile_definitions(nx_maxminddb
    PRIVATE NX_MAXMINDDB_API=${API_EXPORT_MACRO}
    INTERFACE NX_MAXMINDDB_API=${API_IMPORT_MACRO})

if(withTests AND withMaxminddbTests)
    add_subdirectory(unit_tests)
endif()