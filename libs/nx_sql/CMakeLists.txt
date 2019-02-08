set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_sql LIBRARY ${library_type} NO_MOC WERROR
    PUBLIC_LIBS
        Qt5::Core Qt5::Sql
        nx_kit nx_utils
)

target_compile_definitions(nx_sql
    PRIVATE NX_SQL_API=${API_EXPORT_MACRO}
    INTERFACE NX_SQL_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(tests)
endif()
