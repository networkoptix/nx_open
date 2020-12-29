nx_add_target(nx_update LIBRARY
    WERROR
    PUBLIC_LIBS
        nx_utils nx_fusion nx_network nx_vms_api common
)

target_compile_definitions(nx_update
    PRIVATE NX_UPDATE_API=${API_EXPORT_MACRO}
    INTERFACE NX_UPDATE_API=${API_IMPORT_MACRO})
