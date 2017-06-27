set(product.title "${company.name} Mediator Lib")
set(product.display.title "${company.name} Mediator Lib")

nx_configure_file(
    maven/filter-resources/libconnection_mediator_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(libconnection_mediator LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/libconnection_mediator_app_info_impl.cpp"
    PRIVATE_LIBS
        qtservice qtsinglecoreapplication
    PUBLIC_LIBS
        cloud_db_client common
)

set_target_properties(libconnection_mediator PROPERTIES PREFIX "")
