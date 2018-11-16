set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_relaying LIBRARY ${library_type} NO_MOC
    PUBLIC_LIBS
        nx_network nx_fusion nx_utils)

target_compile_definitions(nx_relaying
    PRIVATE NX_RELAYING_API=${API_EXPORT_MACRO}
    INTERFACE NX_RELAYING_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
