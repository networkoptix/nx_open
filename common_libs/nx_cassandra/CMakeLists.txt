set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

if(NOT WINDOWS)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_add_target(nx_cassandra LIBRARY ${library_type}
    PUBLIC_LIBS
        nx_utils
        cassandra)

target_compile_definitions(nx_cassandra
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})
