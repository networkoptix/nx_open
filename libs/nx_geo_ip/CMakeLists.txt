set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_geo_ip LIBRARY ${library_type} NO_MOC WERROR
    PUBLIC_LIBS
        maxminddb nx_utils nx_network)

target_compile_definitions(nx_geo_ip
    PRIVATE NX_GEO_IP_API=${API_EXPORT_MACRO}
    INTERFACE NX_GEO_IP_API=${API_IMPORT_MACRO})
