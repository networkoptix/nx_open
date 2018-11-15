set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_clusterdb_map LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_clusterdb_engine nx_sql nx_network)

target_compile_definitions(nx_clusterdb_map
    PRIVATE NX_KEY_VALUE_DB_API=${API_EXPORT_MACRO}
    INTERFACE NX_KEY_VALUE_DB_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
