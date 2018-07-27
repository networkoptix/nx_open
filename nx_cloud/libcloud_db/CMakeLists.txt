nx_configure_file(
    maven/filter-resources/libcloud_db_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libcloud_db LIBRARY NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libcloud_db_app_info_impl.cpp"
    PUBLIC_LIBS
        cloud_db_client nx_data_sync_engine nx_email nx_sql
)

set_target_properties(libcloud_db PROPERTIES PREFIX "")


target_include_directories(libcloud_db PUBLIC ${PROJECT_SOURCE_DIR}/nx_cloud)
