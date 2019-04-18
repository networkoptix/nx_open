set(file_description "${traytool.name}")
set(file_internal_name "${traytool.name}")

find_package(Qt5
    COMPONENTS
        Widgets)

nx_configure_file(maven/filter-resources/traytool_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(traytool EXECUTABLE SIGNED
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/traytool_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/icons/all/logo.png"
    PUBLIC_LIBS
        Qt5::Widgets
        qtsingleapplication
        common
    PRIVATE_LIBS
        Shell32
        AdvApi32
)
