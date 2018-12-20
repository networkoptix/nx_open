set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_cassandra LIBRARY ${library_type} WERROR
    PUBLIC_LIBS
        boost
        cassandra
        nx_utils)

target_compile_definitions(nx_cassandra
    PRIVATE NX_CASSANDRA_API=${API_EXPORT_MACRO}
    INTERFACE NX_CASSANDRA_API=${API_IMPORT_MACRO})

if(withTests AND withCassandraTests)
    add_subdirectory(unit_tests)
endif()
