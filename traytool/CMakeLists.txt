set(product.title "${traytool.name}")

nx_configure_file(maven/filter-resources/traytool_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(traytool EXECUTABLE
    ADDITIONAL_SOURCES 
        "${CMAKE_CURRENT_BINARY_DIR}/traytool_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/icons/all/logo.png"
    PUBLIC_LIBS
        Qt5::Widgets
        qtsingleapplication
        common
)
