set(product.title "${company.name} libtraffic_relay")
set(product.display.title "${company.name} Traffic Relay")

if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_configure_file(
    maven/filter-resources/libtraffic_relay_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libtraffic_relay LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libtraffic_relay_app_info_impl.cpp"
    PUBLIC_LIBS
        cloud_db_client nx_network nx_utils nx_cassandra nx_relaying)

set_target_properties(libtraffic_relay PROPERTIES PREFIX "")

# target_include_directories(libtraffic_relay PUBLIC ${PROJECT_SOURCE_DIR}/nx_cloud)
