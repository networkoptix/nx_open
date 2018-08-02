set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_update LIBRARY ${library_type} WERROR
    PUBLIC_LIBS
        nx_utils nx_fusion nx_network nx_vms_api common
)

target_compile_definitions(nx_update
    PRIVATE NX_UPDATE_API=${API_EXPORT_MACRO}
    INTERFACE NX_UPDATE_API=${API_IMPORT_MACRO})
