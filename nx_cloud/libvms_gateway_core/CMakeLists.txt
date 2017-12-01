set(product.title "${company.name} vms_gateway_core")
set(product.display.title "${company.name} VMS Gateway")

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_configure_file(
    maven/filter-resources/libvms_gateway_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(vms_gateway_core LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway_core_app_info_impl.cpp"
    PUBLIC_LIBS
        common cloud_db_client nx_network nx_fusion nx_utils nx_relaying
)

target_compile_definitions(vms_gateway_core
    PRIVATE NX_VMS_GATEWAY_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_GATEWAY_API=${API_IMPORT_MACRO})
