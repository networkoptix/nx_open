set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_key_value_db LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_data_sync_engine nx_sql nx_network)

target_compile_definitions(nx_key_value_db
    PRIVATE NX_KEY_VALUE_DB_API=${API_EXPORT_MACRO}
    INTERFACE NX_KEY_VALUE_DB_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(tests)
endif()
