set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_analytics_db LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        Qt5::Concurrent
        Qt5::Network
        openssl
        nx_kit
        nx_utils nx_fusion udt common
)

set(mocableFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/analytics/db/abstract_storage.h)

nx_add_qt_mocables(nx_analytics_db ${mocableFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(nx_analytics_db
    PRIVATE NX_ANALYTICS_DB_API=${API_EXPORT_MACRO}
    INTERFACE NX_ANALYTICS_DB_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
