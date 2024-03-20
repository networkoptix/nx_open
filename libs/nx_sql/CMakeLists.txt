## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_sql LIBRARY NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        Qt6::Core
        Qt6::Sql
        nx_kit
        nx_utils
    PRIVATE_LIBS
        nx_fusion
    FOLDER common/libs
)

target_compile_definitions(nx_sql
    PRIVATE NX_SQL_API=${API_EXPORT_MACRO}
    INTERFACE NX_SQL_API=${API_IMPORT_MACRO})

if(LINUX AND NOT ANDROID)
    target_link_libraries(nx_sql PRIVATE pthread)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
