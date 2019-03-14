nx_configure_file(
    maven/filter-resources/libconnection_mediator_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libconnection_mediator LIBRARY NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libconnection_mediator_app_info_impl.cpp"
    PRIVATE_LIBS
        qtservice qtsinglecoreapplication
    PUBLIC_LIBS
        cloud_db_client common nx_network nx_fusion nx_utils nx_sql
)

set_target_properties(libconnection_mediator PROPERTIES PREFIX "")

if(withTests)
    add_subdirectory(unit_tests)
endif()
