set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_api
    LIBRARY ${library_type}
    NO_PCH
    PUBLIC_LIBS
        nx_utils nx_fusion)

target_compile_definitions(nx_vms_api
    PRIVATE NX_VMS_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_API=${API_IMPORT_MACRO})
