set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_sql LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        Qt5::Core Qt5::Sql
        nx_kit nx_utils nx_fusion
)

target_compile_definitions(nx_sql
    PRIVATE NX_SQL_API=${API_EXPORT_MACRO}
    INTERFACE NX_SQL_API=${API_IMPORT_MACRO})

if(LINUX AND NOT ANDROID)
    target_link_libraries(nx_sql PRIVATE pthread)
endif()

if(withTests)
    add_subdirectory(tests)
endif()
