set(library_type)
#if(WINDOWS)
#    set(library_type LIBRARY_TYPE SHARED)
#endif()

nx_add_target(nx_sdk LIBRARY ${library_type} NO_MOC NO_PCH
    PUBLIC_LIBS
        nx_kit
)

target_compile_definitions(nx_sdk
    PRIVATE NX_SDK_API=#${API_EXPORT_MACRO}
    INTERFACE NX_SDK_API=#${API_IMPORT_MACRO}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
