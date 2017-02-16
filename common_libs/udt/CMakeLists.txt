nx_add_target(udt LIBRARY NO_MOC
    PUBLIC_LIBS pthread)

target_compile_definitions(udt
    PRIVATE UDT_API=${API_EXPORT_MACRO}
    INTERFACE UDT_API=${API_IMPORT_MACRO})
