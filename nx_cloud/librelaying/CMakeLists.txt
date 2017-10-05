set(product.title "${company.name} librelaying")
set(product.display.title "${company.name} Relaying library")
set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_configure_file(
    maven/filter-resources/librelaying_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(librelaying LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/librelaying_app_info_impl.cpp"
    PUBLIC_LIBS
        nx_network nx_utils)

target_compile_definitions(librelaying
    PRIVATE NX_RELAYING_API=${API_EXPORT_MACRO}
    INTERFACE NX_RELAYING_API=${API_IMPORT_MACRO})

set_target_properties(librelaying PROPERTIES PREFIX "")
