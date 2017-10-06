set(product.title "${company.name} libvms_gateway")
set(product.display.title "${company.name} VMS Gateway")

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_configure_file(
    maven/filter-resources/libvms_gateway_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libvms_gateway LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway_app_info_impl.cpp"
    PUBLIC_LIBS
        common cloud_db_client nx_network nx_fusion nx_utils librelaying
)

set_target_properties(libvms_gateway PROPERTIES PREFIX "")
