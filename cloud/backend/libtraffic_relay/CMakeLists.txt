nx_configure_file(
    maven/filter-resources/libtraffic_relay_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libtraffic_relay LIBRARY NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libtraffic_relay_app_info_impl.cpp"
    PUBLIC_LIBS
        cloud_db_client nx_network nx_utils nx_cassandra nx_relaying)

set_target_properties(libtraffic_relay PROPERTIES PREFIX "")

if(withTests)
    add_subdirectory(unit_tests)
endif()
