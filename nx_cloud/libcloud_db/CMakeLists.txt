set(product.title "${company.name} libcloud_db")
set(product.display.title "${company.name} Cloud Database")

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_configure_file(
    maven/filter-resources/libcloud_db_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libcloud_db LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libcloud_db_app_info_impl.cpp"
    PUBLIC_LIBS
        cloud_db_client appserver2 nx_email)

set_target_properties(libcloud_db PROPERTIES PREFIX "")

target_include_directories(libcloud_db PUBLIC ${PROJECT_SOURCE_DIR}/nx_cloud)
