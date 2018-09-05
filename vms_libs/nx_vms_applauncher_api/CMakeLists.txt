set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_applauncher_api LIBRARY ${library_type}
    PUBLIC_LIBS
        nx_utils)

target_compile_definitions(nx_vms_applauncher_api
    PRIVATE NX_VMS_APPLAUNCHER_API_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_APPLAUNCHER_API_API=${API_IMPORT_MACRO})
