set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_rules
    LIBRARY ${library_type}
    PUBLIC_LIBS
        nx_vms_api)

target_compile_definitions(nx_vms_rules
    PRIVATE NX_VMS_RULES_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_RULES_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
